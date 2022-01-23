from PIL import Image

# needs to be a 32x32 png - no checking is done atm
im = Image.open("BallSprite.png")

pxBytes = im.tobytes()
print(len(pxBytes))

for row in range(0,32):
    for column in range(0,32):
        baseIndex = (row*32 + column) * 3
        #print(f"{hex(pxBytes[baseIndex])[2:]}{hex(pxBytes[baseIndex+1])[2:]}{hex(pxBytes[baseIndex+2])[2:]},", end="")
    print()


#input()

outPxBytes = bytearray()

for pxIndex in range(0,len(pxBytes),3):
    a = 0
    r = pxBytes[pxIndex] >> 6
    g = pxBytes[pxIndex+1] >> 5
    b = pxBytes[pxIndex+2] >> 6

    outByte = r | (g<<2) | (b<<5) | (a<<7)

    #print(r,g,b, hex(outByte))

    outPxBytes.append(outByte)

#input()

print(len(outPxBytes))
#print(outPxBytes)

outFile = open("ballSprite.py", "w")

outFile.write("ballSprite = bytearray(b\'")
for outPxByte in outPxBytes:
    #print(outPxByte)
    value = hex(outPxByte)[2:]
    if len(value) == 1:
        value = "0"+value
    outFile.write("\\x"+value)
outFile.write("\')")

outFile.close()
