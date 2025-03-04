//======================================================================================================
//======================================================================================================
//
//  font.c
//
//  Font registry (see graphics.h/.c for font rendering)
//
//  Copyright (c) 2022 Greg King
//
//======================================================================================================
//======================================================================================================

#include "font.h"

FontInfo fontRegistry[MAX_FONTS];
uint8_t numFonts;

//======================================================================================================
// Reset font registry
//======================================================================================================
void resetFonts() {
    numFonts = 0;
}

//======================================================================================================
// Allocate a font slot (0xff on fail)
//======================================================================================================
uint8_t allocFont() {
    uint8_t result = 0xff;
    if(numFonts < MAX_FONTS) {
        result = numFonts++;
    }
    return result;
}

//======================================================================================================
// return the number of currently allocated fonts
//======================================================================================================
uint8_t getNumFonts() {
    return numFonts;
}

//======================================================================================================
// get the FontInfo struct for an allocated font slot
//======================================================================================================
FontInfo* getFont(uint8_t fontID) {
    FontInfo* result = NULL;
    if (fontID < numFonts) {
        result = fontRegistry+fontID;
    }
    return result;
}

