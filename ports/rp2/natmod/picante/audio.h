//======================================================================================================
//======================================================================================================
//
//  audio.h
//
//  Audio internals for picante
//
//  Copyright (c) 2022 Greg King
//
//======================================================================================================
//======================================================================================================

#include "py/dynruntime.h"

#include "py/objarray.h"

#include "py/nativeglue.h"

//======================================================================================================
//======================================================================================================
//
//
//  Audio system internals
//
//
//======================================================================================================
//======================================================================================================

#define SAMPLE_RATE 16000
#define MAX_SAMPLE_VALUE ((int16_t)32767)
#define MIN_SAMPLE_VALUE ((int16_t)-32768)
#define NUM_VOICES (4)

#define WAVEFORM_SINE (0)
#define WAVEFORM_SQUARE (1)
#define WAVEFORM_TRIANGLE (2)
#define WAVEFORM_SAWTOOTH (3)
#define WAVEFORM_NOISE (4)

#define NUM_WAVEFORMS (5)

#define LUT_SIZE 17
#define INTERP_BITS 11
#define INTERP_MASK ((1<<INTERP_BITS)-1)

#define MAX_SAMPLE_VALUE_FP (((uint32_t)MAX_SAMPLE_VALUE)<<16)

#define STATE_NOT_PLAYING (0)
#define STATE_ATTACK (1)
#define STATE_DECAY (2)
#define STATE_SUSTAIN (3)
#define STATE_RELEASE (4)

//======================================================================================================
//  Structure to hold current state of a synth voice
//======================================================================================================
typedef struct _Voice {
    int16_t (*waveform)(uint16_t phase);    // waveform function
    uint32_t phaseFP;                       // in 16:16 fixed point (1 cycle is 0:0 -> 65535:65535)
    uint32_t phasePerTickFP;                // in 16:16 fixed point (1 cycle is 0:0 -> 65535:65535)

    uint32_t attackDeltaFP;                 // amount envelope should increase by each sample
                                            // in 16:16 fixed point
    uint32_t decayDeltaFP;                  // amount envelope should decrease by each sample
                                            // in 16:16 fixed point
    uint32_t releaseDeltaFP;                // amount envelope should decrease by each sample
                                            // in 16:16 fixed point

    uint32_t prevEnvelopeFP;                // previous envelope value in 16:16 fixed point

    uint16_t baseAmplitude;                 // 0 to 32767

    uint16_t sustainLevel;                   // 0 to 32767

    uint16_t state;
} Voice;


//======================================================================================================
// do Micropython binding stuffs
//======================================================================================================
void mpy_audio_init();