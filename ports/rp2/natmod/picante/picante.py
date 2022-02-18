from ili9341 import Display, color565
from machine import Pin, SPI, I2S
import picante_c

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
    picante_c.initGraphics()

####################################################################################
#
# Cleans up any graphics resources used
#
####################################################################################
def cleanupGraphics():
    global spi, display, displayStripe, renderBuffer

    display.cleanupGraphics()
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
            #print("Loaded palette",len(result["palettes"]))
        elif inHeader == bmp32Header:
            result["pixelBuffers"].append(inFile.read(32*32//2))
            #print("Loaded pixelBuffer",len(result["pixelBuffers"]))
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
        
        print("Loaded font: ",charWidth,"x",charHeight,":",advanceX,advanceY,firstCharCode, finalCharCode, buffer)

        fontID = picante_c.addFont( fontInfo )
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
        
    return picante_c.setTransparentColour(index)

####################################################################################
#
# sets the font to use for text rendering
#
####################################################################################
def setFont(fontBuffer):
    return picante_c.setFont(fontBuffer)

####################################################################################
#
# adds commands to clear the entire screen to the given RGB565 colour
#
####################################################################################
def clear(colour565 = 0):
    return picante_c.clear565(colour565)

####################################################################################
#
# adds commands to copy a byte-based object to the screen
#
# bitmap = the bytes-based object containing a 32x32 ARGB1232-encoded bitmap
# x, y = the screen-position of the top-left corner of the bitmap
#
####################################################################################
def blit32(bitmap, x, y, palette):
    return picante_c.blit32(bitmap, (x, y), palette)

####################################################################################
#
# Draws the given string at the given coords
#
####################################################################################
def drawText(string, x, y, colour565):
    return picante_c.drawText(string, (x, y), colour565)

####################################################################################
#
# Apply the current render instructions to the actual display
#
####################################################################################
def draw():
    for stripeIdx in range(0, NUM_STRIPES):
        #print("Rendering stripe",stripeIdx)
        picante_c.renderStripe(stripeIdx, displayStripe)
        display.block(0, stripeIdx*STRIPE_HEIGHT, 319, (stripeIdx+1)*STRIPE_HEIGHT-1, displayStripe)
    picante_c.clearCmdQueue()


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

# globals
audioOut = None
synthBuffer = None

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
    global audioOut
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
    synthByffer = bytes(SYNTH_BUFFER_BYTES)
    audioOut.irq = i2s_callback

####################################################################################
#
# clean up any audio resources used
#
####################################################################################
def cleanupAudio():
    global audioOut, synthBuffer
    audioOut = None
    synthBuffer = None

####################################################################################
#
# interrupt handler to provide audio samples
#
####################################################################################
def i2s_callback(arg):
    picante_c.synthFillBuffer(synthBuffer)
    audioOut.write(synthBuffer)