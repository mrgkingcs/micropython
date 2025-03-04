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
import micropython
from time import ticks_ms

import picante

SCR_WIDTH=320
SCR_HEIGHT=240

def test():
    """Test code."""
    pass

    picante.initGraphics(sck=2, mosi=3, dc=7, cs=5, rst=6, rotation=270)
    fontID = picante.loadFont("tom-thumb-new.bin")
    
    micropython.mem_info()

    message = "Hello!"
    pxLength = len(message)*4
    pxHeight = 6
    
    speed = 1
    posX = -pxLength
    posY = -pxHeight
    dirX = speed
    dirY = speed

    start = ticks_ms()

    numFrames = 1000

    for frame in range(0,numFrames):
        #print("Frame:",frame)
        #print("clear()")
        picante.clear(0xffff)
        
        posX += dirX
        if posX >= (SCR_WIDTH+pxLength):
            posX = -pxLength

        posY += dirY
        if posY >= SCR_HEIGHT:
            posY = -pxHeight
        
        
        picante.drawText("Hello!", posX, posY, 0xf800)

        #print("draw()")
        picante.draw()

    end = ticks_ms()
    count = end-start
    print(numFrames, "frames in",(count),"ms =",(numFrames*1000/count),"fps")

    picante.cleanupGraphics()

test()