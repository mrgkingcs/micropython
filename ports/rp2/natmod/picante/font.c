#include "font.h"

FontInfo fontRegistry[MAX_FONTS];
uint8_t numFonts;

void resetFonts() {
    numFonts = 0;
}

uint8_t allocFont() {
    uint8_t result = 0xff;
    if(numFonts < MAX_FONTS) {
        result = numFonts++;
    }
    return result;
}

uint8_t getNumFonts() {
    return numFonts;
}

FontInfo* getFont(uint8_t fontID) {
    FontInfo* result = NULL;
    if (fontID < numFonts) {
        result = fontRegistry+fontID;
    }
    return result;
}
