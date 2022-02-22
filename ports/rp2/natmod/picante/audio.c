#include "audio.h"


int16_t sineLUT[LUT_SIZE]; 
int16_t (*waveformFuncs[NUM_WAVEFORMS])(uint16_t phase);

Voice voices[NUM_VOICES];

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
//======================================================================================================
int16_t noise(uint16_t phase) {
    return 0x7fff;
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
// Sets the waveform for the given voice
//======================================================================================================
void setWaveform_(uint8_t voiceIdx, uint8_t waveformID) {
    voices[voiceIdx].waveform = waveformFuncs[waveformID];
}

//======================================================================================================
// Sets the base amplitude for the given voice
//======================================================================================================
void setAmplitude_(uint8_t voiceIdx, uint8_t amplitude) {
    voices[voiceIdx].baseAmplitude = ((uint16_t)amplitude)<<7;
}

//======================================================================================================
// Sets the phasePerTick for the given voice (phasePerTick = freq * 16000 / 65536) ==> 16:16 fixed point
//======================================================================================================
void setPhasePerTick_(uint8_t voiceIdx, uint32_t phasePerTick) {
    voices[voiceIdx].phasePerTickFP = phasePerTick;
}

//======================================================================================================
// Sets the envelope for the given voice
//  deltas are envelope changes per sample, in 16:16 fixed point
//  sustain level is in range 0->32767
//======================================================================================================
void setEnvelope_(  uint8_t voiceIdx, 
                    uint32_t attackDeltaFP,
                    uint32_t decayDeltaFP,
                    uint16_t sustainLevel,
                    uint32_t releaseDeltaFP
                ) 
{
    Voice* voicePtr = voices + voiceIdx;

    voicePtr->attackDeltaFP = attackDeltaFP;
    voicePtr->decayDeltaFP = decayDeltaFP;
    voicePtr->sustainLevel = sustainLevel;
    voicePtr->releaseDeltaFP = releaseDeltaFP;

}

//======================================================================================================
// Start playing a note for the given voice
//======================================================================================================
void playVoice_(uint8_t voiceIdx)  {
    Voice* voice = voices + voiceIdx;
    voice->phaseFP = 0;
    voice->prevEnvelopeFP = 0;
    voice->state = STATE_ATTACK;
}

//======================================================================================================
// Release the note for the given voice
//======================================================================================================
void releaseVoice_(uint8_t voiceIdx) {
    // set envelope to start release
    voices[voiceIdx].state = STATE_RELEASE;
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
        if (result < voice->sustainLevel) {
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
// Get the next sample from the given voice
// This is where all the MAGIC happens! :O
//======================================================================================================
int16_t voiceGetSample(Voice* voice) {
    uint16_t phase = voice->phaseFP>>16;
    int32_t sample = (voice->waveform)(phase);
    voice->phaseFP += voice->phasePerTickFP;

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
// Do the meat of filling out the buffer with synth output
//======================================================================================================
void synthFillBuffer_(int16_t* buffer, uint16_t numSamples) {
    bool bufferCleared = false;
    for(uint voiceIdx = 0; voiceIdx < NUM_VOICES; voiceIdx++) {
        if(voiceIsPlaying(voiceIdx)) {
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
}