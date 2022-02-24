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
#from ili9341 import Display, color565
#from machine import Pin, SPI

from random import randint
import micropython
from time import ticks_ms

import picante

SCR_WIDTH=320
SCR_HEIGHT=240

def test():
    """Test code."""

    picante.initGraphics(sck=2, mosi=3, dc=7, cs=5, rst=6, rotation=270)
    ballSpriteInfo = picante.loadSprite("ballsprite.bin")
    tileSetInfo = picante.loadSprite("tileset_dungeon.bin")

    micropython.mem_info()

    speed = 3
    posX = 0
    posY = 0
    dirX = speed
    dirY = speed

    start = ticks_ms()

    numFrames = 100

    for frame in range(0,numFrames):
        picante.clear(0b0)
        
        posX += dirX
        if posX >= (SCR_WIDTH-32):
            posX = (SCR_WIDTH-32)
            dirX = -speed
        elif posX <= 0:
            posX = 0
            dirX = speed

        posY += dirY
        if posY >= (SCR_HEIGHT-32):
            posY = (SCR_HEIGHT-32)
            dirY = -speed
        elif posY <= 0:
            posY = 0
            dirY = speed

        for tileRow in range(0, (SCR_HEIGHT/32)-1):
            for tileCol in range(0, SCR_WIDTH/32):
                tileId = (tileRow*8 + tileCol) % len(tileSetInfo["pixelBuffers"])
                picante.blit32(tileSetInfo["pixelBuffers"][tileId], tileCol*32, tileRow*32, tileSetInfo["palettes"][0])
            
        picante.blit32(ballSpriteInfo["pixelBuffers"][0], posX, posY, ballSpriteInfo["palettes"][0])

        picante.draw()

    end = ticks_ms()
    count = end-start
    print(numFrames, "frames in",(count),"ms =",(numFrames*1000/count),"fps")

    picante.cleanupGraphics()

test()