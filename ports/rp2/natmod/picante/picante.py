# ======================================================================================================
# ======================================================================================================
# 
#   picante.py
# 
#   Python wrapper around C interface
# 
#   Copyright (c) 2022 Greg King
# 
# ======================================================================================================
# ======================================================================================================

from micropython import schedule
from ili9341 import Display, color565
from machine import Pin, SPI, I2S


####################################################################################
####################################################################################
##
##
##  Picante Graphics System
##
##
####################################################################################
####################################################################################

## constants
SCREEN_WIDTH = 320
SCREEN_HEIGHT = 240
BYTES_PER_PIXEL = 2
NUM_STRIPES = 15
STRIPE_HEIGHT = int(SCREEN_HEIGHT // NUM_STRIPES)
STRIPE_BYTES = SCREEN_WIDTH * STRIPE_HEIGHT * BYTES_PER_PIXEL

## globals
displayStripe = bytearray(STRIPE_BYTES)
spi = None
display = None



####################################################################################
#
# Initialise the graphics system
#
# sck, mosi = pin numbers for the SPI
# dc, cs, rst = pin numbers for the ILI9341 display
# rotation = angle of rotation for the screen
#
####################################################################################
def initGraphics(sck, mosi, dc, cs, rst, rotation):
    global spi, display
    spi = SPI(0, baudrate=65000000, sck=Pin(sck), mosi=Pin(mosi))
    display = Display(spi, dc=Pin(dc), cs=Pin(cs), rst=Pin(rst), rotation=rotation, width=SCREEN_WIDTH, height=SCREEN_HEIGHT)
    gfxInit()

####################################################################################
#
# Cleans up any graphics resources used
#
####################################################################################
def cleanupGraphics():
    global spi, display, displayStripe, renderBuffer

    display.cleanup()
    display = None

    spi = None

    displayStripe = None
    renderBuffer = None

####################################################################################
#
# loads palette and pixel data from a binary file
#
####################################################################################
def loadSprite(filename):
    result = {"palettes":[], "pixelBuffers":[] }

    paletteHeader = "pale".encode('ascii')
    bmp32Header = "bp32".encode('ascii')    

    inFile = open(filename, "rb")

    inHeader = inFile.read(4)

    while len(inHeader) == 4:
        if inHeader == paletteHeader:
            result["palettes"].append(inFile.read(32))
        elif inHeader == bmp32Header:
            result["pixelBuffers"].append(inFile.read(32*32//2))
        else:
            print("Unrecognised header found:", inHeader.decode('ascii'), ":", list(inHeader))
            break

        inHeader = inFile.read(4)

    inFile.close()

    return result


####################################################################################
#
# loads a bitmap font from a binary file, returning a FontID to use when rendering 
#
####################################################################################
def loadFont(filename):
    fontID = -1

    fontHeader = "fnt1".encode('ascii')

    inFile = open(filename, "rb")
    inHeader = inFile.read(4)
    if inHeader == fontHeader:
        charSizeByte = int.from_bytes(inFile.read(1), "little")
        charWidth = charSizeByte & 0xf
        charHeight = charSizeByte>>4
        
        advanceByte = int.from_bytes(inFile.read(1), "little")
        advanceX = advanceByte & 0xf
        advanceY = advanceByte >> 4

        firstCharCode = int.from_bytes(inFile.read(1), "little")
        finalCharCode = int.from_bytes(inFile.read(1), "little")
        numChars = (finalCharCode+1)-firstCharCode

        bufferBytes = charHeight*numChars
        buffer = inFile.read(bufferBytes)

        fontInfo = (charWidth, charHeight, advanceX, advanceY, firstCharCode, finalCharCode, buffer)
        
        # print("Loaded font: ",charWidth,"x",charHeight,":",advanceX,advanceY,firstCharCode, finalCharCode, buffer)

        fontID = gfxAddFont( fontInfo )
    else:
        print("Failed to load font:",filename)

    return fontID
    

####################################################################################
#
# sets the index to use as transparency
#
####################################################################################
def setTransparentColour(index):
    if index < 0 or index >= 16:
        index = 0xff
        
    return gfxSetTransparentColour(index)

####################################################################################
#
# sets the font to use for text rendering
#
####################################################################################
def setFont(fontBuffer):
    return setFont(fontBuffer)

####################################################################################
#
# adds commands to clear the entire screen to the given RGB565 colour
#
####################################################################################
def clear(colour565 = 0):
    return gfxClear565(colour565)

####################################################################################
#
# adds commands to copy a byte-based object to the screen
#
# bitmap = the bytes-based object containing a 32x32 ARGB1232-encoded bitmap
# x, y = the screen-position of the top-left corner of the bitmap
#
####################################################################################
def blit32(bitmap, x, y, palette):
    return gfxBlit32(bitmap, (x, y), palette)

####################################################################################
#
# Draws the given string at the given coords
#
####################################################################################
def drawText(string, x, y, colour565):
    return gfxDrawText(string, (x, y), colour565)

####################################################################################
#
# Apply the current render instructions to the actual display
#
####################################################################################
def draw():
    for stripeIdx in range(0, NUM_STRIPES):
        #print("Rendering stripe",stripeIdx)
        gfxRenderStripe(stripeIdx, displayStripe)
        display.block(0, stripeIdx*STRIPE_HEIGHT, 319, (stripeIdx+1)*STRIPE_HEIGHT-1, displayStripe)
    gfxClearCmdQueue()


####################################################################################
####################################################################################
##
##
##  Picante Audio System
##
##  (owes hella much to sources distributed under the MIT licence:
##   https://github.com/miketeachman/micropython-i2s-examples/blob/master/examples/play_tone.py
##   https://github.com/miketeachman/micropython-i2s-examples/blob/master/examples/play_wav_from_sdcard_non_blocking.py
##  )
##
##
####################################################################################
####################################################################################

# constants
I2D_ID = 0
I2S_BUFFER_BYTES = 1024
FORMAT = I2S.MONO

SAMPLE_RATE_HZ = 16000
SAMPLE_SIZE_BITS = 16

SYNTH_BUFFER_BYTES = 1024

WAVEFORM_SINE = 0
WAVEFORM_SQUARE = 1
WAVEFORM_TRIANGLE = 2
WAVEFORM_SAWTOOTH = 3
WAVEFORM_NOISE = 4

MODULATION_NONE = 0
MODULATION_LINEAR = 1
MODULATION_EXPONENTIAL = 2

# globals
audioOut = None
synthBuffer = None
playingBufferIdx = 0

A1FreqHz = 55

semitoneFreqFactors = [
    1, 1.059, 1.122, 1.189, 1.260, 1.335, 1.414, 1.498, 1.587, 1.682, 1.782, 1.888
]

semitoneIdxMap = {  "A" : 0, 
                    "A#" : 1, "Bb" : 1,
                    "B" : 2,
                    "C" : 3,
                    "C#" : 4, "Db" : 4,
                    "D" : 5,
                    "D#" : 6, "Eb" : 6,
                    "E" : 7,
                    "F" : 8,
                    "F#" : 9, "Gb" : 9,
                    "G" : 10,
                    "G#" : 11, "Ab" : 11
}

####################################################################################
#
# Initialise the audio system
#
# bclk = GPIO pin number for BCLK/SCK pin
# wsel = GPIO pin number for WSEL/WS pin
# din = GPIO pin number for DIN/SD pin
#
####################################################################################
def initAudio(bclk, wsel, din):
    global audioOut, synthBuffer, playingBufferIdx
    audioOut = I2S(
        I2D_ID,
        sck=Pin(bclk),
        ws=Pin(wsel),
        sd=Pin(din),
        mode=I2S.TX,
        bits=SAMPLE_SIZE_BITS,
        format=FORMAT,
        rate=SAMPLE_RATE_HZ,
        ibuf=I2S_BUFFER_BYTES
    )
    audioOut.irq(i2s_callback)

    synthBuffer = bytearray(SYNTH_BUFFER_BYTES)
    playingBufferIdx = 0
    audioOut.write(synthBuffer)

    audInit()


####################################################################################
#
# clean up any audio resources used
#
####################################################################################
def cleanupAudio():
    global audioOut, synthBuffer
    if audioOut is not None:
        audioOut.deinit()
        audioOut = None
    synthBuffer = None

####################################################################################
#
# sets up the parameters for the voice
#  0 = sine, 1 = square, 2 = triangle, 3 = sawtooth, 4 = noise
#  envelope is tuple of 4 values:
#   - attack time in 64 frame units (roughly 1/256th of a second)
#   - decay time in 64 frame units (roughly 1/256th of a second)
#   - sustain level in 1/255ths of full amplitude
#   - release time in 64 frame units (roughly 1/256th of a second)
#
####################################################################################
def setVoice(voiceIdx, waveform, envelope):
    audSetWaveform(voiceIdx, waveform)

    attackTime = int(envelope[0]) * 64
    decayTime = int(envelope[1]) * 64
    sustainLevel = envelope[2] * 32767 / 255
    releaseTime = int(envelope[3]) * 64

    if attackTime == 0:
        attackDelta = 32767
    else:
        attackDelta = 32767 / attackTime

    if decayTime == 0:
        decayDelta = 32767
    else:
        maxDecayDelta = (32767 - sustainLevel)
        decayDelta = maxDecayDelta / decayTime
    
    if releaseTime == 0:
        releaseDelta = 32767
    else:
        releaseDelta = sustainLevel / releaseTime

    # encode these values into 16:16 fixed point
    encodedEnvelope = (
            int(attackDelta * 65536),
            int(decayDelta * 65536),
            int(sustainLevel),
            int(releaseDelta * 65536)
        )

    audSetEnvelope(voiceIdx, encodedEnvelope)

####################################################################################
#
# sets up a voice to be modulated by the next voice along
# (modulatorIdx = (carrierIdx+1)%numVoices )
# modulationType: 0 = none, 1 = linear
# freqMultiplier: modulator frequency = this * carrier frequency
# amplitude: 0-255 amplitude of modulation voice
#
####################################################################################
def setModulation(voiceIdx, modulationType, freqMultiplier, amplitude):
    freqMultiplierFP = int(freqMultiplier * (1<<4))
    audSetModulation(voiceIdx, (modulationType, freqMultiplierFP, amplitude))

####################################################################################
#
# starts playing a note (e.g. pressing a key on a keyboard)
#   Note string is in range "A1" (55Hz) to "Ab7" (3322Hz)
#
####################################################################################
def playNote(voiceIdx, noteString, amplitude):
    octave = int(noteString[-1])
    noteIdx = semitoneIdxMap[noteString[:-1]]

    octaveFreq = 2**(octave-2) * A1FreqHz
    noteFreq = semitoneFreqFactors[noteIdx] * octaveFreq

    # 1Hz = 65536 phase /16000 sample = 4.096 phase per sample
    # represented as 16:16 fixed point
    phasePerSampleFP = int(4.096 * noteFreq * 65536)
    print(noteFreq,"==>",phasePerSampleFP)
    audSetPhasePerTick(voiceIdx, phasePerSampleFP)

    audSetAmplitude(voiceIdx, amplitude)

    audPlayVoice(voiceIdx)

####################################################################################
#
# stops playing a note (e.g. releasing a key on a keyboard)
#
####################################################################################
def releaseNote(voiceIdx):
    audReleaseVoice(voiceIdx)


####################################################################################
#
# interrupt handler to provide audio samples
#
####################################################################################
def i2s_callback(arg):
    global audioOut, synthBuffer, playingBufferIdx
    audioOut.write(synthBuffer)
    audSynthFillBuffer(synthBuffer)

####################################################################################
#
# callback to fill out next buffer
#
####################################################################################
def fillBuffer(bufferIdx):
    audSynthFillBuffer(synthBuffer[bufferIdx])