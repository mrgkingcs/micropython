#!/usr/bin/python3

from PIL import Image

im = Image.new("P", (32,32))


colours = []


for blue in range(0,4):
    byteBlue = blue*85
    for green in range(0,8):
        byteGreen = int(0.5+green*36.42857)
        for red in range(0,4):
            byteRed = red*85

            colours.append(byteRed)
            colours.append(byteGreen)
            colours.append(byteBlue)

im.putdata(list(range(0,128)))

im.putpalette(colours, rawmode="RGB")

im.save("picantePalette.png")