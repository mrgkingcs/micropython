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

notes = [ "C4", "D4", "E4", "F4", "G4", "A5", "B5", "C5" ]
#notes = [ "C1", "D1", "E1", "F1", "G1", "A2", "B2", "C2" ]

bpm = 120
msPerBeat = int(1000*60/bpm)

micropython.mem_info()

def test():
    """Test code."""

    try:
        picante.initGraphics(sck=2, mosi=3, dc=7, cs=5, rst=6, rotation=270)

        picante.initAudio(bclk=12, wsel=13, din=11)
        

        picante.setVoice(0, picante.WAVEFORM_SINE, (4, 8, 192, 32))
        picante.setVoice(1, picante.WAVEFORM_SINE, (0, 0, 64, 0))
        #picante.setModulation(0, picante.MODULATION_LINEAR, 4, 8)
        picante.setModulation(0, picante.MODULATION_EXPONENTIAL, 3, 64)
        #picante.setModulation(0, picante.MODULATION_LINEAR, 24, 4)
        picante.setLowPassFilterLevel(3)
        picante.releaseNote(0)
        
        start = ticks_ms()

        for beatIdx in range(0, len(notes)):
            if notes[beatIdx] != "":
                picante.playNote(0, notes[beatIdx], 64)
            
            while (ticks_ms()-start) < (beatIdx+1)*msPerBeat - msPerBeat*0.4:
                pass

            picante.releaseNote(0)

            while (ticks_ms()-start) < (beatIdx+1)*msPerBeat:
                pass
    finally:
        picante.cleanupAudio()
        picante.cleanupGraphics()

test()