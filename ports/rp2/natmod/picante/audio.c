//======================================================================================================
//======================================================================================================
//
//  audio.c
//
//  Audio internals for picante
//
//  Copyright (c) 2022 Greg King
//
//======================================================================================================
//======================================================================================================

#include "audio.h"

#include "py/dynruntime.h"

int16_t sineLUT[LUT_SIZE]; 
int16_t (*waveformFuncs[NUM_WAVEFORMS])(uint16_t phase);
uint32_t noiseWaveformState;
uint16_t noiseWaveformNextPhase;

Voice voices[NUM_VOICES];

//======================================================================================================
//======================================================================================================
//
//  Waveforms
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Get sine waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t sine(uint16_t phase) {
    uint16_t idx = (phase >> INTERP_BITS) & 0xf;
    int32_t delta = sineLUT[idx+1] - sineLUT[idx];
    int32_t interp = phase & INTERP_MASK;
    int32_t interpDelta = delta*interp;
    int16_t value = sineLUT[idx] + (interpDelta>>INTERP_BITS);
    if (phase & 0x8000) {
        value = -value;
    }
    return value;
}

//======================================================================================================
// Get square waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t square(uint16_t phase) {
    return (phase & 0x8000) ? MAX_SAMPLE_VALUE : MIN_SAMPLE_VALUE;
}

//======================================================================================================
// Get triangle waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t triangle(uint16_t phase) {
    int16_t subPhase = (phase&0x3fff)<<1;
    switch(phase >> 14) {
        case 0b00:  return subPhase;
        case 0b01:  return MAX_SAMPLE_VALUE - subPhase;
        case 0b10:  return -subPhase;
        case 0b11:  return MIN_SAMPLE_VALUE + subPhase;
    }
    return 0;   // should never reach this
}


//======================================================================================================
// Get sawtooth waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t sawtooth(uint16_t phase) {
    return (phase & 0x8000) ? (phase & 0x7fff) : MIN_SAMPLE_VALUE + (phase & 0x7fff);
}

//======================================================================================================
// Get noise waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//  I owe a great deal to this site: http://www.sidmusic.org/sid/sidtech5.html
//  The clunky way I've implemented this means that it will misbehave if more than one voice
//  is set to the 'noise' waveform. :/
//======================================================================================================
int16_t noise(uint16_t phase) {
    int16_t result;

    if( phase >= noiseWaveformNextPhase) {
        noiseWaveformNextPhase += 128;
        uint32_t newBit = (noiseWaveformState >> 22);
        newBit ^= (noiseWaveformState >> 17);
        noiseWaveformState = noiseWaveformState << 1 | (newBit & 1);
    }

    result =    ((noiseWaveformState >> 7) & (1<<15)) |
                ((noiseWaveformState >> 6) & (1<<14)) |
                ((noiseWaveformState >> 3) & (1<<13)) |
                ((noiseWaveformState << 1) & (1<<12)) |
                ((noiseWaveformState     ) & (1<<11)) |
                ((noiseWaveformState << 3) & (1<<10)) |
                ((noiseWaveformState << 5) & (1<< 9)) |
                ((noiseWaveformState << 6) & (1<< 8))
            ;

    return result;
}

//======================================================================================================
//======================================================================================================
//
//  Initialisation
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Initialise the voices to sensible values
//======================================================================================================
void initVoices() {
    for(uint8_t voiceIdx = 0; voiceIdx < NUM_VOICES; voiceIdx++) {
        voices[voiceIdx].state = STATE_NOT_PLAYING;
    }
}

//======================================================================================================
// Set up the sine look-up table
//======================================================================================================
void setupSineLUT() {
    // initialise sine look-up table
    sineLUT[ 0] = 0;
    sineLUT[ 1] = 6392;
    sineLUT[ 2] = 12539;
    sineLUT[ 3] = 18204;
    sineLUT[ 4] = 23169;
    sineLUT[ 5] = 27244;
    sineLUT[ 6] = 30272;
    sineLUT[ 7] = 32137;
    sineLUT[ 8] = 32767;
    sineLUT[ 9] = 32137;
    sineLUT[10] = 30272;
    sineLUT[11] = 27244;
    sineLUT[12] = 23169;
    sineLUT[13] = 18204;
    sineLUT[14] = 12539;
    sineLUT[15] = 6392;
    sineLUT[16] = 0;
}

//======================================================================================================
// Set up the waveform function look-up table
//======================================================================================================
void setupWaveformLUT() {
    waveformFuncs[0] = &sine;
    waveformFuncs[1] = &square;
    waveformFuncs[2] = &triangle;
    waveformFuncs[3] = &sawtooth;
    waveformFuncs[4] = &noise;
}

//======================================================================================================
// Set up the noise waveform
//======================================================================================================
void setupNoiseWaveformTable() {
    noiseWaveformState = 0x7ffff8;
    noiseWaveformNextPhase = 0;
}



//======================================================================================================
//======================================================================================================
//
//  Voice Status
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Returns true if the given voice should be added to the playback buffer
//======================================================================================================
bool voiceIsPlaying(uint8_t voiceIdx) {
    return (voices[voiceIdx].state != STATE_NOT_PLAYING);
}

//======================================================================================================
// Returns true if the given voice is being used as a modulator
//======================================================================================================
bool voiceIsModulator(uint8_t voiceIdx) {
    uint8_t carrier = (voiceIdx>0) ? (voiceIdx-1) : (NUM_VOICES-1);
    return (voices[carrier].modulationType != MODULATION_NONE);
}


//======================================================================================================
//======================================================================================================
//
//  Internal Synth Stuff
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Get the next envelope value for the voice (and advance envelope)
//======================================================================================================
uint16_t getEnvelope(Voice* voice) {
    uint16_t result = 32767;

    if (voice->state == STATE_ATTACK) {
        voice->prevEnvelopeFP += voice->attackDeltaFP;
        if(voice->prevEnvelopeFP > MAX_SAMPLE_VALUE_FP) {
            voice->prevEnvelopeFP = MAX_SAMPLE_VALUE_FP;
            voice->state = STATE_DECAY;
        } else {
            result = voice->prevEnvelopeFP >> 16;
        }
    } else if (voice->state == STATE_DECAY) {
        voice->prevEnvelopeFP -= voice->decayDeltaFP;
        result = voice->prevEnvelopeFP >> 16;
        if (result <= voice->sustainLevel) {
            result = voice->sustainLevel;
            voice->prevEnvelopeFP = ((uint32_t)voice->sustainLevel) << 16;
            voice->state = STATE_SUSTAIN;
        }
    } else if (voice->state == STATE_SUSTAIN) {
        result = voice->sustainLevel;
    } else if (voice->state == STATE_RELEASE) {
        if(voice->prevEnvelopeFP < voice->releaseDeltaFP) {
            result = 0;
            voice->state = STATE_NOT_PLAYING;
            voice->prevEnvelopeFP = 0;
        } else {
            voice->prevEnvelopeFP -= voice->releaseDeltaFP;
            result = voice->prevEnvelopeFP >> 16;
        }
    } else if (voice->state == STATE_NOT_PLAYING) {
        result = 0;
    }
    return result;
}

//======================================================================================================
// Get the modulator voice for the given carrier voice
//======================================================================================================
Voice* getModulator(Voice* carrierVoice) {
    Voice* modulator = carrierVoice+1;
    if (modulator > voices+NUM_VOICES) {
        modulator = voices;
    }
    return modulator;
}

//======================================================================================================
// Get the next sample from the given voice
// This is where all the MAGIC happens! :O
//======================================================================================================
int16_t voiceGetSample(Voice* voice) {
    uint16_t phase = voice->phaseFP>>16;
    int32_t sample = (voice->waveform)(phase);

    // apply modulation here
    if(voice->modulationType == MODULATION_NONE) {
        voice->phaseFP += voice->phasePerTickFP;
    } else if (voice->modulationType == MODULATION_LINEAR) {
        Voice* modulator = getModulator(voice);
        int32_t modulatorSampleFP = ((int32_t)voiceGetSample(modulator)) << 16;
        int32_t signedPhasePerTickFP = voice->phasePerTickFP >> 1;
        int32_t phaseDeltaFP = signedPhasePerTickFP + modulatorSampleFP;
        if (phaseDeltaFP > 0) {
            voice->phaseFP += ((uint32_t)phaseDeltaFP) << 1;
        }
    } else if (voice->modulationType == MODULATION_EXPONENTIAL) {
        Voice* modulator = getModulator(voice);
        int32_t modulatorSampleFP = ((int32_t)voiceGetSample(modulator));
        int32_t signedPhasePerTickFP = voice->phasePerTickFP >> 1;
        int32_t phaseDeltaFP = (signedPhasePerTickFP>>4) * modulatorSampleFP;
        if (phaseDeltaFP > 0) {
            voice->phaseFP += ((uint32_t)phaseDeltaFP) << 1;
        }
    }

    sample *= voice->baseAmplitude;
    sample >>= 15;

    sample *= getEnvelope(voice);
    sample >>= 15;

    return sample;
}

//======================================================================================================
// Synthesise 16bpp samples from voice and mix into given buffer
//======================================================================================================
void voiceMixToBuffer(Voice* voice, int16_t* buffer, uint16_t numSamples) {
    int16_t* currSample = buffer;
    int16_t* beyondEnd = buffer + numSamples;
    while(currSample < beyondEnd) {
        int16_t mixValue = *currSample;
        *(currSample++) = mixValue + voiceGetSample(voice);
    }
}

//======================================================================================================
// Synthesise 16bpp samples from voice overwriting given buffer
//======================================================================================================
void voiceSetBuffer(Voice* voice, int16_t* buffer, uint16_t numSamples) {
    int16_t* currSample = buffer;
    int16_t* beyondEnd = buffer + numSamples;
    while(currSample < beyondEnd) {
        *(currSample++) = voiceGetSample(voice);
    }
}



//======================================================================================================
//======================================================================================================
//
// Micropython bindings
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Initialise the audio internals
//======================================================================================================
STATIC mp_obj_t audInit() {
    initVoices();
    setupSineLUT();
    setupWaveformLUT();
    setupNoiseWaveformTable();

    return mp_obj_new_int(0);
}

//======================================================================================================
// Fill the given buffer with the next audio samples
//======================================================================================================
STATIC mp_obj_t audSynthFillBuffer(mp_obj_t bufferObj) {
    mp_obj_array_t* bufferArr = (mp_obj_array_t*)MP_OBJ_TO_PTR(bufferObj);

    int16_t* buffer = (int16_t*)bufferArr->items;
    uint32_t numSamples = bufferArr->len>>1;

    bool bufferCleared = false;
    for(uint voiceIdx = 0; voiceIdx < NUM_VOICES; voiceIdx++) {
        if(voiceIsPlaying(voiceIdx) && !voiceIsModulator(voiceIdx)) {
            if(!bufferCleared) {
                voiceSetBuffer(voices+voiceIdx, buffer, numSamples);
                bufferCleared = true;
            } else {
                voiceMixToBuffer(voices+voiceIdx, buffer, numSamples);
            }
        }
    }

    if(!bufferCleared) {
        int16_t* dst = buffer;
        int16_t* beyondEnd = dst + numSamples;
        while(dst < beyondEnd) {
            *(dst++) = 0;
        }
    }
    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the waveform of the given voice
//  0 = sine, 1 = square, 2 = triangle, 3 = sawtooth, 4 = noise
//======================================================================================================
STATIC mp_obj_t audSetWaveform(mp_obj_t voiceIdxObj, mp_obj_t waveformIDObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);
    uint8_t waveformID = mp_obj_get_int(waveformIDObj);

    voices[voiceIdx].waveform = waveformFuncs[waveformID];

    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the frequency of the note (as phasePerTick)
// Python code can convert between freq and phasePerTick
//======================================================================================================
STATIC mp_obj_t audSetPhasePerTick(mp_obj_t voiceIdxObj, mp_obj_t phasePerTickObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);
    uint32_t phasePerTick = mp_obj_get_int(phasePerTickObj);

    Voice* voice = voices + voiceIdx;
    voice->phasePerTickFP = phasePerTick;

    if(voices[voiceIdx].modulationType != MODULATION_NONE) {
        Voice* modulator = getModulator(voice);
        uint32_t freqFactor = voice->modulationFreqFactorFP;
        modulator->phasePerTickFP = (phasePerTick >> 4) * freqFactor;
    }

    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the envelope values of the voice (tuple of attack, release, sustain, decay)
//  deltas are envelope changes per sample, in 16:16 fixed point
//  sustain level is in range 0->32767
//======================================================================================================
STATIC mp_obj_t audSetEnvelope(mp_obj_t voiceIdxObj, mp_obj_t envelopeTupleObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);
    mp_obj_tuple_t* envelopeTuple = MP_OBJ_TO_PTR(envelopeTupleObj);

    Voice* voicePtr = voices + voiceIdx;

    voicePtr->attackDeltaFP = mp_obj_get_int(envelopeTuple->items[0]);
    voicePtr->decayDeltaFP = mp_obj_get_int(envelopeTuple->items[1]);
    voicePtr->sustainLevel = mp_obj_get_int(envelopeTuple->items[2]);
    voicePtr->releaseDeltaFP = mp_obj_get_int(envelopeTuple->items[3]);

    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the base amplitude of the given voice
//======================================================================================================
STATIC mp_obj_t audSetAmplitude(mp_obj_t voiceIdxObj, mp_obj_t amplitudeObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);
    uint16_t amplitude = mp_obj_get_int(amplitudeObj) & 0xff;

    voices[voiceIdx].baseAmplitude = ((uint16_t)amplitude)<<7;

    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the modulation of the given voice
// Modulation Tuple = (type(0 = None, 1 = linear), frequencyFactor(fixed point 12:4)
//======================================================================================================
STATIC mp_obj_t audSetModulation(mp_obj_t voiceIdxObj, mp_obj_t modulationTupleObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);
    mp_obj_tuple_t* modulationTuple = MP_OBJ_TO_PTR(modulationTupleObj);
    
    uint8_t modulationType = mp_obj_get_int( modulationTuple->items[0] );

    uint16_t modulationFreqFactorFP = mp_obj_get_int( modulationTuple->items[1]);

    uint16_t modulationAmplitude = mp_obj_get_int( modulationTuple->items[2]) << 8;

    Voice* voice = voices+voiceIdx;

    voice->modulationType = modulationType;
    voice->modulationFreqFactorFP = modulationFreqFactorFP;

    getModulator(voice)->baseAmplitude = modulationAmplitude;

    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the voice playing a new note
//======================================================================================================
STATIC mp_obj_t audPlayVoice(mp_obj_t voiceIdxObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);

    Voice* voice = voices + voiceIdx;
    voice->phaseFP = 0;
    voice->prevEnvelopeFP = 0;
    voice->state = STATE_ATTACK;
    if(voice->waveform == &noise) {
        noiseWaveformNextPhase = 0;
    }

    if(voice->modulationType != MODULATION_NONE) {
        Voice* modulator = getModulator(voice);
        modulator->phaseFP = 0;
        modulator->prevEnvelopeFP = 0;
        modulator->state = STATE_ATTACK;
    }

    return mp_obj_new_int(0);
}

//======================================================================================================
// Release the note playing on the given voice
//======================================================================================================
STATIC mp_obj_t audReleaseVoice(mp_obj_t voiceIdxObj) {
    uint8_t voiceIdx = mp_obj_get_int(voiceIdxObj);

    voices[voiceIdx].state = STATE_RELEASE;

    return mp_obj_new_int(0);
}

//======================================================================================================
//
// Micropython bindings
//
//======================================================================================================
STATIC MP_DEFINE_CONST_FUN_OBJ_0(audInit_obj, audInit);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(audSynthFillBuffer_obj, audSynthFillBuffer);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(audSetWaveform_obj, audSetWaveform);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(audSetAmplitude_obj, audSetAmplitude);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(audSetModulation_obj, audSetModulation);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(audSetPhasePerTick_obj, audSetPhasePerTick);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(audSetEnvelope_obj, audSetEnvelope);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(audPlayVoice_obj, audPlayVoice);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(audReleaseVoice_obj, audReleaseVoice);

void mpy_audio_init() {
    mp_store_global(MP_QSTR_audInit, MP_OBJ_FROM_PTR(&audInit_obj));
    mp_store_global(MP_QSTR_audSynthFillBuffer, MP_OBJ_FROM_PTR(&audSynthFillBuffer_obj));
    mp_store_global(MP_QSTR_audSetWaveform, MP_OBJ_FROM_PTR(&audSetWaveform_obj));
    mp_store_global(MP_QSTR_audSetAmplitude, MP_OBJ_FROM_PTR(&audSetAmplitude_obj));
    mp_store_global(MP_QSTR_audSetModulation, MP_OBJ_FROM_PTR(&audSetModulation_obj));
    mp_store_global(MP_QSTR_audSetPhasePerTick, MP_OBJ_FROM_PTR(&audSetPhasePerTick_obj));
    mp_store_global(MP_QSTR_audSetEnvelope, MP_OBJ_FROM_PTR(&audSetEnvelope_obj));
    mp_store_global(MP_QSTR_audPlayVoice, MP_OBJ_FROM_PTR(&audPlayVoice_obj));
    mp_store_global(MP_QSTR_audReleaseVoice, MP_OBJ_FROM_PTR(&audReleaseVoice_obj));
}