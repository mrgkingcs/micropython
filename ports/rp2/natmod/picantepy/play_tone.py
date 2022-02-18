# Modified from source:
# https://github.com/miketeachman/micropython-i2s-examples/blob/master/examples/play_tone.py
#
# The MIT License (MIT)
# Copyright (c) 2021 Mike Teachman
# https://opensource.org/licenses/MIT

# Purpose:  Play a pure audio tone out of a speaker or headphones
# 
# - write audio samples containing a pure tone to an I2S amplifier or DAC module
# - tone will play continuously in a loop until
#   a keyboard interrupt is detected or the board is reset
#
# Blocking version
# - the write() method blocks until the entire sample buffer is written to I2S

import os
import math
import struct
from machine import I2S
from machine import Pin



# ======= I2S CONFIGURATION =======
SCK_PIN = 12#16
WS_PIN = 13#17
SD_PIN = 11#18
I2S_ID = 0
BUFFER_LENGTH_IN_BYTES = 1000
# ======= I2S CONFIGURATION =======


# ======= AUDIO CONFIGURATION =======
TONE_FREQUENCY_IN_HZ = 4000#440
SAMPLE_SIZE_IN_BITS = 16
FORMAT = I2S.MONO  # only MONO supported in this example
SAMPLE_RATE_IN_HZ = 16000#22050
# ======= AUDIO CONFIGURATION =======

audio_out = I2S(
    I2S_ID,
    sck=Pin(SCK_PIN),
    ws=Pin(WS_PIN),
    sd=Pin(SD_PIN),
    mode=I2S.TX,
    bits=SAMPLE_SIZE_IN_BITS,
    format=FORMAT,
    rate=SAMPLE_RATE_IN_HZ,
    ibuf=BUFFER_LENGTH_IN_BYTES,
)

# create a buffer containing the pure tone samples
samples_per_cycle = SAMPLE_RATE_IN_HZ // TONE_FREQUENCY_IN_HZ
sample_size_in_bytes = SAMPLE_SIZE_IN_BITS // 8
samples = bytearray(samples_per_cycle * sample_size_in_bytes)
volume_reduction_factor = 32
range = pow(2, SAMPLE_SIZE_IN_BITS) // 2 // volume_reduction_factor

if SAMPLE_SIZE_IN_BITS == 16:
    format = "<h"
else:  # assume 32 bits
    format = "<l"

sineLUT = []
for idx in range(0,16):
    sineLUT.append( int(math.sin(math.pi*idx/16)*32767) )
    #print(sineLUT[idx])
sineLUT.append(0)

interpMask = (1<<11)-1

def sinewave(phase):
    intPhase = int(phase * 65536) & 0xffffffff
    idx = (intPhase >> 11) & 0xf
    delta = (sineLUT[idx+1]-sineLUT[idx])
    interp = intPhase&interpMask
    value = sineLUT[idx]+((delta*interp)>>11)
    if intPhase > 32767:
        value = -value
    print(value)
    value/=32767
    return value

def square(phase):
    if phase < 0.5:
        return 1
    else:
        return 0

def sawtooth(phase):
    return phase

def triangle(phase):
    if phase < 0.25:
        return phase*4
    elif phase < 0.75:
        return 1-(phase-0.25)*2
    else:
        return -1+(phase-0.75)*4

# this waveform needs delays set in it to represent
# SID frequency input for waveform
noiseReg = 0x7ffff8
def nextNoise():
    global noiseReg
    result  = (noiseReg & (1<<22)) >> 15
    result |= (noiseReg & (1<<20)) >> 14
    result |= (noiseReg & (1<<16)) >> 11
    result |= (noiseReg & (1<<13)) >>  9
    result |= (noiseReg & (1<<11)) >>  8
    result |= (noiseReg & (1<< 7)) >>  5
    result |= (noiseReg & (1<< 4)) >>  3
    result |= (noiseReg & (1<< 2)) >>  2
    
    newBit0 = ((noiseReg >> 22) ^ (noiseReg >> 17)) & 1
    noiseReg = noiseReg << 1
    noiseReg |= newBit0
    
    return (result & 0xff) << 8

xorshift32state = 13431;

def xorshift32():
    global xorshift32state
    x = xorshift32state
    x ^= x << 13
    x ^= x >> 17
    x ^= x << 5
    xorshift32state = x
    return x

def noise(phase):
    return ((xorshift32() & 0xffffff) - 0x800000) / 0x800000

for i in range(samples_per_cycle):
#    sample = range + int((range - 1) * math.sin(2 * math.pi * i / samples_per_cycle))
    sample = range + int((range - 1) * sinewave(i / samples_per_cycle))
    struct.pack_into(format, samples, i * sample_size_in_bytes, sample)

# continuously write tone sample buffer to an I2S DAC
print("==========  START PLAYBACK ==========")
try:
    while True:
        num_written = audio_out.write(samples)

except (KeyboardInterrupt, Exception) as e:
    print("caught exception {} {}".format(type(e).__name__, e))

# cleanup
audio_out.deinit()
print("Done")