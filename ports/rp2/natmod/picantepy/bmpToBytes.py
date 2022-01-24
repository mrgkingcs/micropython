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

    pxBytes = im.tobytes()

    outPxBytes = bytearray()

    for pxIndex in range(0,len(pxBytes),3):
        a = 0
        r = pxBytes[pxIndex] >> 6
        g = pxBytes[pxIndex+1] >> 5
        b = pxBytes[pxIndex+2] >> 6

        outByte = r | (g<<2) | (b<<5) | (a<<7)

        outPxBytes.append(outByte)

    spriteName = filename.lower()[:filename.rindex('.')]
    outFileName = spriteName+".py"

    try:
        outFile = open(outFileName, "w")

        outFile.write(f"{spriteName} = bytes(b\'")
        for outPxByte in outPxBytes:
            #print(outPxByte)
            value = hex(outPxByte)[2:]
            if len(value) == 1:
                value = "0"+value
            outFile.write("\\x"+value)
        outFile.write("\')")

        outFile.close()
    except:
        print(f"Failed to write to file \"{outFileName}\"")