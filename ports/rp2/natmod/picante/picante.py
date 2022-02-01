from ili9341 import Display, color565
from machine import Pin, SPI
import picante_c

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

# this is going to die
renderBuffer=bytearray(320*240)

####################################################################################
#
# Initialise the rendering system
#
# sck, mosi = pin choices for the SPI
# dc, cs, rst = pin choices for the ILI9341 display
# rotation = angle of rotation for the screen
#
####################################################################################
def init(sck, mosi, dc, cs, rst, rotation):
    global spi, display
    spi = SPI(0, baudrate=65000000, sck=Pin(sck), mosi=Pin(mosi))
    display = Display(spi, dc=Pin(dc), cs=Pin(cs), rst=Pin(rst), rotation=rotation, width=SCREEN_WIDTH, height=SCREEN_HEIGHT)
    picante_c.init()

####################################################################################
#
# Cleans up any resources used
#
####################################################################################
def cleanup():
    global spi, display

    display.cleanup()
    display = None

    spi = None

    displayStripe = None
    renderBuffer = None

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
# Apply the current render instructions to the actual display
#
####################################################################################
def draw():
    for stripeIdx in range(0, NUM_STRIPES):
        #print("Rendering stripe",stripeIdx)
        picante_c.renderStripe(stripeIdx, displayStripe)
        display.block(0, stripeIdx*STRIPE_HEIGHT, 319, (stripeIdx+1)*STRIPE_HEIGHT-1, displayStripe)
    picante_c.clearCmdQueue()