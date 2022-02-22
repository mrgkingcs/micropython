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
#define NUM_WAVEFORMS (5)

#define LUT_SIZE 17
#define INTERP_BITS 11
#define INTERP_MASK ((1<<INTERP_BITS)-1)


//======================================================================================================
//  Structure to hold current state of a synth voice
//======================================================================================================
typedef struct _Voice {
    int16_t (*waveform)(uint16_t phase);    // waveform function
    uint32_t phaseFP;                       // in 16:16 fixed point (1 cycle is 0:0 -> 65535:65535)
    uint32_t phasePerTickFP;                // in 16:16 fixed point (1 cycle is 0:0 -> 65535:65535)

    int16_t envelopeCurrAmplitude;          // 0 to 32767 (negative values ignored)
    int16_t envelopeDeltaPerTick;           // how much to change the envelope by each sample
    uint16_t envelopePhaseTicks;            // top two bits=envelope phase
                                            // (00=A,01=R,10=S,11=D) => 0 = start playing; 0xffff = not playing
                                            // low 14 bits are no. of samples elapsed since phase start 
    uint16_t baseAmplitude;                 // 0 to 32767


    uint8_t attackTime;                     // in units of 64 samples (roughly 1/256th of a second)
    uint8_t decayTime;                      // in units of 1024 samples (roughly 1/256th of a second)
    uint8_t sustainLevel;                   // in 1/256ths of full amplitude
    uint8_t releaseTime;                    // in units of 64 samples (roughly 1/256th of a second)
} Voice;


//======================================================================================================
// Initialise the voices to sensible values
//======================================================================================================
void initVoices();

//======================================================================================================
// Set up the sine look-up table
//======================================================================================================
void setupSineLUT();

//======================================================================================================
// Set up the waveform function look-up table
//======================================================================================================
void setupWaveformLUT();

//======================================================================================================
// Get sine waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t sine(uint16_t phase);

//======================================================================================================
// Get square waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t square(uint16_t phase);

//======================================================================================================
// Get triangle waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t triangle(uint16_t phase);

//======================================================================================================
// Get sawtooth waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t sawtooth(uint16_t phase);

//======================================================================================================
// Get noise waveform (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t noise(uint16_t phase);

//======================================================================================================
// Returns true if the given voice should be added to the playback buffer
//======================================================================================================
bool voiceIsPlaying(uint8_t voiceIdx);

//======================================================================================================
// Sets the waveform for the given voice
//======================================================================================================
void setWaveform_(uint8_t voiceIdx, uint8_t waveformID);

//======================================================================================================
// Sets the base amplitude for the given voice
//======================================================================================================
void setAmplitude_(uint8_t voiceIdx, uint8_t amplitude);

//======================================================================================================
// Sets the phasePerTick for the given voice (phasePerTick = freq * 16000 / 65536) ==> 16:16 fixed point
//======================================================================================================
void setPhasePerTick_(uint8_t voiceIdx, uint32_t phasePerTick);

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
                );

//======================================================================================================
// Start playing a note for the given voice
//======================================================================================================
void playVoice_(uint8_t voiceIdx);   

//======================================================================================================
// Release the note for the given voice
//======================================================================================================
void releaseVoice_(uint8_t voiceIdx);     

//======================================================================================================
// Get the next sample from the given voice
//======================================================================================================
int16_t voiceGetSample(Voice* voice);

//======================================================================================================
// Synthesise 16bpp samples from voice and mix into given buffer
//======================================================================================================
void voiceMixToBuffer(Voice* voice, int16_t* buffer, uint16_t numSamples);

//======================================================================================================
// Synthesise 16bpp samples from voice overwriting given buffer
//======================================================================================================
void voiceSetBuffer(Voice* voice, int16_t* buffer, uint16_t numSamples);

//======================================================================================================
// Do the meat of filling out the buffer with synth output
//======================================================================================================
void synthFillBuffer_(int16_t* buffer, uint16_t numSamples);