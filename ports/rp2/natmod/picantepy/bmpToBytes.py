#!/usr/bin/python3
import sys
from PIL import Image

if len(sys.argv) == 1:
    print("USAGE: bmpToBytes <image filename>")
    exit(-1)

for filename in sys.argv[1:]:
    try:
        im = Image.open(filename)
    except:
        print(f"Failed to open file \"{filename}\"")
        continue

    if im.size[0] != 32 or im.size[1] != 32:
        print(f"Failed to process file \"{filename}\" - must be 32x32px")
        continue

    # get image bytes
    pxBytes = im.tobytes()
    
    # encode image bytes
    outPxBytes = bytearray()

    for pxIndex in range(0, len(pxBytes), 2):
        outByte = (pxBytes[pxIndex] & 0xf) + ((pxBytes[pxIndex+1] & 0xf) << 4)
        outPxBytes.append(outByte)

    # process colour table
    usedColours = im.getcolors()
    maxUsedColour = usedColours[0][1]
    for colourEntry in usedColours:
        if colourEntry[1] > maxUsedColour:
            maxUsedColour = colourEntry[1]

    palette = im.getpalette()[0:(maxUsedColour+1)*3]        
    
    # encode colour table bytes
    outColBytes = bytearray()

    for colIndex in range(0, len(palette), 3):
        # encode as BGR565
        r = palette[colIndex] >> 3
        g = palette[colIndex+1] >> 2
        b = palette[colIndex+2] >> 3

        outBytes = (b) | (g << 5) | (r << 11)
        #outBytes = (r) | (g << 5) | (b << 11)

        outColBytes.append(outBytes & 0xff) 
        outColBytes.append(outBytes >> 8) 


    # output to .py file
    spriteName = filename.lower()[:filename.rindex('.')]
    outFileName = spriteName+".py"

    try:
        outFile = open(outFileName, "w")

        outFile.write(f"{spriteName} = bytes(b\'")
        for outPxByte in outPxBytes:
            value = hex(outPxByte)[2:]
            if len(value) == 1:
                value = "0"+value
            outFile.write("\\x"+value)
        outFile.write("\')\n")

        outFile.write(f"{spriteName}_pal = bytes(b\'")
        for outColByte in outColBytes:
            value = hex(outColByte)[2:]
            if len(value) == 1:
                value = "0"+value
            outFile.write("\\x"+value)
        outFile.write("\')\n")

        outFile.close()
    except:
        print(f"Failed to write to file \"{outFileName}\"")