#include "py/dynruntime.h"

//======================================================================================================
// FontInfo registry
//======================================================================================================
#define MAX_FONTS (4)

typedef struct _FontInfo {
    uint8_t charPxW;
    uint8_t charPxH;
    uint8_t advanceX;
    uint8_t advanceY;   // i.e. line-height
    uint8_t firstCharCode;
    uint8_t finalCharCode;
    uint8_t* buffer;
} FontInfo;


void resetFonts();
uint8_t allocFont();

uint8_t getNumFonts();
FontInfo* getFont(uint8_t fontID);
