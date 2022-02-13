#!/usr/bin/python3
import sys
from PIL import Image

if len(sys.argv) != 7:
    print("USAGE: bmpToFont <image filename> <charWxcharH> <advanceX> <lineheight> <firstCharCode> <finalCharCode>")
    exit(-1)

filename = sys.argv[1]

sizeAsStrPair = sys.argv[2].split('x')
charSizePx = ( int(sizeAsStrPair[0]), int(sizeAsStrPair[1]))

advance = ( int(sys.argv[3]), int(sys.argv[4]))

firstCharCode = int(sys.argv[5])
finalCharCode = int(sys.argv[6])

try:
    im = Image.open(filename)
except:
    print(f"Failed to open file \"{filename}\"")
    exit()


# get image bytes
pxBytes = im.tobytes()

# encoded font bytes
outFntBytes = bytearray()


charSizeOutByte = (charSizePx[0]) | (charSizePx[1] << 4)
outFntBytes.append(charSizeOutByte)

advanceOutByte = (advance[0]) | (advance[1] << 4)
outFntBytes.append(advanceOutByte)

outFntBytes.append(firstCharCode)
outFntBytes.append(finalCharCode)



# font layout
imWidthPx, imHeightPx = im.size
numCharsWide = imWidthPx // advance[0]
numCharsHigh = imHeightPx // advance[1]

charRowStride = imWidthPx*advance[1]
print(charRowStride)

for charRowIdx in range(0, numCharsHigh):
    for charColIdx  in range(0, numCharsWide):
        pxIndex = charRowIdx*charRowStride + charColIdx*advance[0]

        for rowPxIndex in range(pxIndex, pxIndex+imWidthPx*charSizePx[1], imWidthPx):
            currByte = 0
            bitValue = 1
            for px in pxBytes[rowPxIndex:rowPxIndex+charSizePx[0]]:
                if px != 0:
                    currByte |= bitValue
                bitValue *= 2
            outFntBytes.append(currByte)



# output to raw block file
fontHeader = "fnt1".encode('ascii')

fontName = filename.lower()[:filename.rindex('.')]
outFileName = fontName+".bin"

try:
    outFile = open(outFileName, "wb")

    outFile.write(fontHeader)
    outFile.write(outFntBytes)

    outFile.close()
except:
    print(f"Failed to write to file \"{outFileName}\"")