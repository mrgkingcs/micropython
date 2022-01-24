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

    imPxWidth = im.size[0]
    imPxHeight = im.size[1]
    if imPxWidth < 32 or imPxHeight < 32:
        print(f"Failed to process file \"{filename}\" - must be at least 32x32px")
        continue

    numFramesWide = int(imPxWidth / 32)
    numFramesHigh = int(imPxHeight / 32)

    bytesPerPixel = 4

    pxBytes = im.tobytes()
    print(f"{len(pxBytes)} for image of {imPxWidth}x{imPxHeight} => {imPxWidth*imPxHeight}px ({imPxWidth*imPxHeight*bytesPerPixel} expected bytes)")
    frames = []

    frameRowStride = numFramesWide*32*bytesPerPixel
    frameColStride = 32*bytesPerPixel

    for frameIdxY in range(0, numFramesHigh):
        for frameIdxX in range(0, numFramesWide):

            outPxBytes = bytearray()

            baseIndex = frameIdxY * frameRowStride + frameIdxX * frameColStride

            for frameRowIdx in range(baseIndex, baseIndex+frameRowStride*32, frameRowStride):
                for pxIndex in range(frameRowIdx,frameRowIdx+32*bytesPerPixel,bytesPerPixel):
                    r = pxBytes[pxIndex] >> 6
                    g = pxBytes[pxIndex+1] >> 5
                    b = pxBytes[pxIndex+2] >> 6
                    a = int(pxBytes[pxIndex+3] > 0)

                    outByte = r | (g<<2) | (b<<5) | (a<<7)

                    outPxBytes.append(outByte)
            
            frames.append(outPxBytes)

    sheetName = filename.lower()[:filename.rindex('.')]
    outFileName = sheetName+".py"

    try:
        outFile = open(outFileName, "w")

        outFile.write(f"{sheetName} = [\n")

        for outPxBytes in frames:
            outFile.write("\tbytes(b\'")
            for outPxByte in outPxBytes:
                #print(outPxByte)
                value = hex(outPxByte)[2:]
                if len(value) == 1:
                    value = "0"+value
                outFile.write("\\x"+value)
            outFile.write("\'),\n")

        outFile.write("]\n")

        outFile.close()
    except:
        print(f"Failed to write to file \"{outFileName}\"")