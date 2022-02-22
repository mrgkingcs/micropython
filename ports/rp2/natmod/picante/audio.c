#include "audio.h"


int16_t sineLUT[LUT_SIZE]; 
int16_t (*waveformFuncs[NUM_WAVEFORMS])(uint16_t phase);

Voice voices[NUM_VOICES];

uint32_t globalPhase; // just until proper instruments/timing is set up

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
// Returns true if the given voice should be added to the playback buffer
//======================================================================================================
bool voiceIsPlaying(uint8_t voiceIdx) {
    return (voices[voiceIdx].envelopePhaseTicks != 0xffff);
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
    voices[voiceIdx].baseAmplitude = amplitude<<7;
}

//======================================================================================================
// Sets the phasePerTick for the given voice (phasePerTick = freq * 16000 / 65536) ==> 16:16 fixed point
//======================================================================================================
void setPhasePerTick_(uint8_t voiceIdx, uint32_t phasePerTick) {
    voices[voiceIdx].phasePerTickFP = phasePerTick;
}

//======================================================================================================
// Sets the envelope for the given voice
//  times are in units of 64 samples (roughly 1/256th of a second)
//  sustain level is in 1/256ths of base amplitude
//======================================================================================================
void setEnvelope_(  uint8_t voiceIdx, 
                    uint8_t attackTime,
                    uint8_t decayTime,
                    uint8_t sustainLevel,
                    uint8_t releaseTime
                ) 
{
    Voice* voicePtr = voices + voiceIdx;

    voicePtr->attackTime = attackTime;
    voicePtr->decayTime = decayTime;
    voicePtr->sustainLevel = sustainLevel;
    voicePtr->releaseTime = releaseTime;
}

//======================================================================================================
// Start playing a note for the given voice
//======================================================================================================
void playVoice_(uint8_t voiceIdx)  {
    voices[voiceIdx].phaseFP = 0;
    voices[voiceIdx].envelopePhaseTicks = 0;
}

//======================================================================================================
// Release the note for the given voice
//======================================================================================================
void releaseVoice_(uint8_t voiceIdx) {
    // just instant cut-off for now
    voices[voiceIdx].envelopePhaseTicks = 0xffff;//0b1100000000000000;
}

//======================================================================================================
// Get the next sample from the given voice
// This is where all the MAGIC happens! :O
//======================================================================================================
int16_t voiceGetSample(Voice* voice) {
    int16_t sample = sine(voice->phaseFP>>16);
    voice->phaseFP += voice->phasePerTickFP;
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
// Initialise the voices to sensible values
//======================================================================================================
void initVoices() {
    for(uint8_t voiceIdx = 0; voiceIdx < NUM_VOICES; voiceIdx++) {
        voices[voiceIdx].envelopePhaseTicks = 0xffff;
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