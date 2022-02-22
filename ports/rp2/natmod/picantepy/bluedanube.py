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

notes = [ "C4",
          "C4", "E4", "G4",
          "G4",  "",  "G5",
          "G5",  "",  "E5",
          "E5",  ""
    ]

bpm = 120
msPerBeat = int(1000*60/bpm)

micropython.mem_info()

def test():
    """Test code."""


    
    picante.initGraphics(sck=2, mosi=3, dc=7, cs=5, rst=6, rotation=270)

    picante.initAudio(bclk=12, wsel=13, din=11)
    

    picante.setVoice(0, picante.WAVEFORM_TRIANGLE, (1, 1, 128, 1))
    picante.releaseNote(0)
    
    start = ticks_ms()

    for beatIdx in range(0, len(notes)):
        if notes[beatIdx] != "":
            print(notes[beatIdx])
            picante.playNote(0, notes[beatIdx], 255)
        
        while (ticks_ms()-start) < (beatIdx+1)*msPerBeat:
            pass

        picante.releaseNote(0)

    picante.cleanupAudio()
    picante.cleanupGraphics()

test()