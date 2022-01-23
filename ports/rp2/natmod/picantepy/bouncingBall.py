"""
This file is by Greg King, distributed under the MIT license.

MIT License

Copyright (c) Greg King 2022

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
from time import sleep
from ili9341 import Display, color565
from machine import Pin, SPI

from random import randint
from picante import encode, clear232, blit32
import micropython
from time import ticks_ms

from ballSprite import ballSprite

SCR_WIDTH=320
SCR_HEIGHT=240
renderBufferBytes = SCR_WIDTH*SCR_HEIGHT

NUM_STRIPES = 8

buffWidth = int(320)
buffHeight = int(240/NUM_STRIPES)

testSprite = bytearray()
for count in range(0,32*32):
    testSprite.append(0b01100000)

def test():
    """Test code."""
    spi = SPI(0, baudrate=65000000, sck=Pin(2), mosi=Pin(3))
    display = Display(spi, dc=Pin(7), cs=Pin(5), rst=Pin(6), rotation=270, width=320, height=240)

    renderBuffer = bytearray(renderBufferBytes)
    
    
    numBytes = buffWidth*buffHeight*2
    displayBuffer = bytearray(numBytes)
    
    micropython.mem_info()

    speed = 3
    posX = 0
    posY = 0
    dirX = speed
    dirY = speed

    start = ticks_ms()

    while True:
        #print("Frame:",frameNum)
        clear232(renderBuffer, 0b00000000)
        
        posX += dirX
        if posX >= (320-32):
            posX = (320-32)
            dirX = -speed
        elif posX <= 0:
            posX = 0
            dirX = speed

        posY += dirY
        if posY >= (240-32):
            posY = (240-32)
            dirY = -speed
        elif posY <= 0:
            posY = 0
            dirY = speed

        blit32(ballSprite, (posX, posY), renderBuffer)

        for stripeIdx in range(0, NUM_STRIPES):
            encode(renderBuffer, displayBuffer, stripeIdx*numBytes//2)
            display.block(0, stripeIdx*buffHeight, 319, (stripeIdx+1)*buffHeight-1, displayBuffer)

    end = ticks_ms()
    count = end-start
    print("100 frames in",(count),"ms =",(100000/count),"fps")

    display.cleanup()

test()