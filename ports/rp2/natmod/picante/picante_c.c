// Include the header file to get access to the MicroPython API
#include "py/dynruntime.h"

#include "py/objarray.h"

#include "py/nativeglue.h"

#include "cmdQueue.h"
#include "font.h"


//======================================================================================================
//======================================================================================================
//
// Render state
//
//======================================================================================================
//======================================================================================================

uint8_t currFontID;
uint8_t transparentColour;



//======================================================================================================
//======================================================================================================
//
//
//  Graphics system internals
//
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Initialise the graphics system
//======================================================================================================
STATIC mp_obj_t initGraphics() {
    transparentColour = 0xff;
    currFontID = 0xff;
    resetCmdBuffer();
    resetFonts();
    return mp_obj_new_int(0);
}

//======================================================================================================
// Set the palette index to treat as transparent 
//======================================================================================================
STATIC mp_obj_t setTransparentColour(mp_obj_t colourIndex) {
    transparentColour = (uint8_t)mp_obj_get_int(colourIndex);
    return NULL;
}

//======================================================================================================
// Set the font buffer to use for text rendering 
//======================================================================================================
STATIC mp_obj_t setFont(mp_obj_t fontIDObj) {
    uint8_t newFontID = mp_obj_get_int(fontIDObj);
    if(newFontID < getNumFonts())
        currFontID = newFontID;
    return NULL;
}

//======================================================================================================
// Add a font to the internal list of fonts
//======================================================================================================
STATIC mp_obj_t addFont(mp_obj_t fontInfoObj) {
    mp_obj_tuple_t* fontInfo = (mp_obj_tuple_t*)MP_OBJ_TO_PTR(fontInfoObj);
    uint8_t fontID = allocFont();
    if(fontID != 0xff) {
        if(currFontID == 0xff) {
            currFontID = fontID;
        }
        FontInfo* newFont = getFont(fontID);
        newFont->charPxW = mp_obj_get_int(fontInfo->items[0]);
        newFont->charPxH = mp_obj_get_int(fontInfo->items[1]);
        newFont->advanceX = mp_obj_get_int(fontInfo->items[2]);
        newFont->advanceY = mp_obj_get_int(fontInfo->items[3]);
        newFont->firstCharCode = mp_obj_get_int(fontInfo->items[4]);
        newFont->finalCharCode = mp_obj_get_int(fontInfo->items[5]);

        mp_obj_array_t* buffer = MP_OBJ_TO_PTR(fontInfo->items[6]);
        newFont->buffer = (uint8_t*)buffer->items;
    }
    return mp_obj_new_int(fontID);
}

//======================================================================================================
// Clear the whole screen to a given colour
//======================================================================================================
STATIC mp_obj_t clear565(mp_obj_t colour565) {
    uint16_t colour = mp_obj_get_int(colour565);
    int result = 0;

    for(int stripeIdx = 0; stripeIdx < NUM_STRIPES; stripeIdx++) {
        CmdClear565* clearCmd = (CmdClear565*)allocCmd(OPCODE_CLEAR565);
        if(clearCmd != NULL) {
            result = 0;
            //clearCmd->colour = colour;
            clearCmd->base.param16 = (colour << 8) | (colour >> 8);
            enqueueCmd(clearCmd, stripeIdx);
        } else {
            result = -1;
        }
    }
    return mp_obj_new_int(result);
}

//======================================================================================================
// blit a 32x32 bitmap to the screen (clipping as necessary)
//======================================================================================================
STATIC mp_obj_t blit32(mp_obj_t srcBuf32x32, mp_obj_t posTuple, mp_obj_t paletteObj) {
    const int PX_WIDTH = 32;
    const int PX_HEIGHT = 32;
    int result = 0;

    mp_obj_tuple_t* pos = (mp_obj_tuple_t*)MP_OBJ_TO_PTR(posTuple);
    mp_int_t posX = mp_obj_get_int(pos->items[0]);
    mp_int_t posY = mp_obj_get_int(pos->items[1]);

    // if the whole thing is off screen, just give up now
    if (posX > -PX_WIDTH && posX < SCREEN_WIDTH && posY > -PX_HEIGHT && posY < SCREEN_HEIGHT) {

        // we're going to draw _something_ so get the buffer info now
        mp_obj_array_t* srcBuf = MP_OBJ_TO_PTR(srcBuf32x32);
        uint8_t* src = (uint8_t*)srcBuf->items;

        mp_obj_array_t* paletteBuf = MP_OBJ_TO_PTR(paletteObj);
        uint16_t* palette = (uint16_t*)paletteBuf->items;

        // calculate which stripes we're going to draw into
        int startStripe = ((posY+PX_HEIGHT) / STRIPE_HEIGHT) - (PX_HEIGHT/STRIPE_HEIGHT);   // relies on PX_HEIGHT being multiple of STRIPE_HEIGHT
        int endStripe = (posY+(PX_HEIGHT-1)) / STRIPE_HEIGHT; // nb. posY+31 >= 0 or sprite would be off-screen culled already

        // calculate left clipping
        uint16_t srcOffsetPx = (posX >= 0) ? 0 : -posX;
        uint16_t numPxInDrawRow = PX_WIDTH - srcOffsetPx;
        uint16_t clippedPosX = (posX >= 0) ? posX : 0;

        // calculate right clipping
        if(posX + numPxInDrawRow > SCREEN_WIDTH) {
            numPxInDrawRow = SCREEN_WIDTH - posX;
        }

        // start stripe - only if it's not off the top of the screen
        int numRows = STRIPE_HEIGHT - (posY & STRIPE_PX_ROW_MASK);
        if(startStripe >= 0)
        {
            CmdBlit* blitCmd = (CmdBlit*)allocCmd(OPCODE_BLIT);
            if(blitCmd != NULL) {
                blitCmd->srcBuf = src;
                blitCmd->palette = palette;
                blitCmd->srcStartOffsetPx = srcOffsetPx;
                blitCmd->dstStartOffsetPx = (posY & STRIPE_PX_ROW_MASK)*STRIPE_WIDTH + clippedPosX;
                blitCmd->numPxInDrawRow = numPxInDrawRow;
                blitCmd->srcStridePx = PX_WIDTH;
                blitCmd->numRows = numRows;
                blitCmd->transparentColour = transparentColour;
                enqueueCmd(blitCmd, startStripe);
            } else {
                result = -1;
            }
        }
        srcOffsetPx += numRows * PX_WIDTH;

        // middle stripe
        numRows = STRIPE_HEIGHT;
        if(endStripe > startStripe+1) {
            int midStripe = startStripe+1;
            if(midStripe >= 0 && midStripe < NUM_STRIPES)
            {
                CmdBlit* blitCmd = (CmdBlit*)allocCmd(OPCODE_BLIT);
                if(blitCmd != NULL) {
                    blitCmd->srcBuf = src;
                    blitCmd->palette = palette;
                    blitCmd->srcStartOffsetPx = srcOffsetPx;
                    blitCmd->dstStartOffsetPx = clippedPosX;
                    blitCmd->numPxInDrawRow = numPxInDrawRow;
                    blitCmd->srcStridePx = PX_WIDTH;
                    blitCmd->numRows = numRows;
                    blitCmd->transparentColour = transparentColour;
                    enqueueCmd(blitCmd, startStripe+1);
                } else {
                    result = -2;
                }
            }
            srcOffsetPx += numRows * PX_WIDTH;
        }

        // end stripe
        numRows = (posY+PX_HEIGHT)-(endStripe*STRIPE_HEIGHT);
        if(endStripe > startStripe && endStripe < NUM_STRIPES)
        {
            CmdBlit* blitCmd = (CmdBlit*)allocCmd(OPCODE_BLIT);
            if(blitCmd != NULL) {
                blitCmd->srcBuf = src;
                blitCmd->palette = palette;
                blitCmd->numRows = numRows;
                blitCmd->srcStartOffsetPx = srcOffsetPx;
                blitCmd->dstStartOffsetPx = clippedPosX;
                blitCmd->numPxInDrawRow = numPxInDrawRow;
                blitCmd->srcStridePx = PX_WIDTH;
                blitCmd->transparentColour = transparentColour;
                enqueueCmd(blitCmd, endStripe);
            } else {
                result = -3;
            }
        }
    }
    return mp_obj_new_int(result);
}

//======================================================================================================
// do unsigned int division because micropython doesn't link it properly!
//======================================================================================================
uint uidiv(uint a, uint b) {
    uint count = 0;
    uint total = b;
    while (total <= a) {
        total += b;
        count ++;
    }
    return count;
}

//======================================================================================================
// Queue up commands to draw the given string at the given location
//======================================================================================================
STATIC mp_obj_t drawText(mp_obj_t stringObj, mp_obj_t posTuple, mp_obj_t colourObj) {
    int result = -1;

    // check we actually have a font
    if (currFontID != 0xff) {
        result = 0; // don't return -1 if we _could_ have drawn it, but it was clipped

        // decode parameters
        uint8_t strLen = mp_obj_get_int(mp_obj_len(stringObj));
        const char* str = mp_obj_str_get_str(stringObj);

        mp_obj_tuple_t* pos = (mp_obj_tuple_t*)MP_OBJ_TO_PTR(posTuple);
        mp_int_t posX = mp_obj_get_int(pos->items[0]);
        mp_int_t posY = mp_obj_get_int(pos->items[1]);

        uint16_t colour = mp_obj_get_int(colourObj);

        // get info for clipping
        FontInfo* fontInfo = getFont(currFontID);
        uint16_t totalPxWidth = fontInfo->advanceX*strLen;

        // check text would actually appear on-screen
        if(posX < SCREEN_WIDTH && posX > -totalPxWidth && posY < SCREEN_HEIGHT && posY > -fontInfo->charPxH) {
            // clip left and right to whole chars
            // (the very idea of clipping within a char just makes me tired)
            if (posX < 0) {
                uint8_t numClipCharsLeft = uidiv(-posX, fontInfo->advanceX)+1;
                posX += numClipCharsLeft * fontInfo->advanceX;
                str += numClipCharsLeft;
                strLen -= numClipCharsLeft;
            }

            int16_t rightOverlap = (posX + totalPxWidth) - SCREEN_WIDTH;
            if (rightOverlap > 0) {
                uint16_t numClipCharsRight = uidiv(rightOverlap + fontInfo->charPxW, fontInfo->advanceX);
                strLen -= numClipCharsRight;
            }

            // work out useful preliminaries
            int8_t stripeIdx = ((posY+STRIPE_HEIGHT) / STRIPE_HEIGHT) - 1; // need to add STRIPE_HEIGHT for correct rounding of -ve posY
            uint8_t stripeRow = posY & STRIPE_PX_ROW_MASK;

            // allocate space for command(s) and string
            CmdText* textCmd = NULL;
            CmdText* textCmd2 = NULL;

            // calculate numRows to draw, clipping to bottom of stripe
            uint8_t numRowsToDraw = fontInfo->charPxH;

            // if first stripe is not off the top of the screen, allocate the command struct
            if(stripeIdx >= 0) {
               textCmd = (CmdText*)allocCmd(OPCODE_TEXT);
            }

            // if text would overlap into next stripe...
            if (stripeRow + numRowsToDraw > STRIPE_HEIGHT) {
                // adjust number of rows to draw
                numRowsToDraw = STRIPE_HEIGHT - stripeRow;

                // if second stripe is not off bottom of the screen, allocate the command struct 
                if(stripeIdx+1 < NUM_STRIPES) {
                    textCmd2 = (CmdText*)allocCmd(OPCODE_TEXT);
                }
            }

            // allocate space for the characters
            uint8_t* charCodes = (uint8_t*)alloc(strLen);

            // fill out the first stripe's CmdText struct and enqueue it (if it's needed)
            if(textCmd != NULL) {
                textCmd->dstStartOffsetPx = stripeRow*STRIPE_WIDTH + posX;
                textCmd->colour = colour;
                textCmd->charStartRow = 0;
                textCmd->charNumRows = numRowsToDraw;
                textCmd->numChars = strLen;  
                textCmd->fontID = currFontID;

                // if we've got a second command, flag the extra string offset in numChars
                if(textCmd2 != NULL) {
                    textCmd->numChars |= 0x80;
                }

                enqueueCmd(textCmd, stripeIdx);
            }

            // fill out second stripe's CmdText struce and enqueue it (if it's needed)
            if(textCmd2 != NULL) {
                // fill out the CmdText struct and enqueue it
                textCmd2->dstStartOffsetPx = posX;
                textCmd2->colour = colour;
                textCmd2->charStartRow = numRowsToDraw;
                textCmd2->charNumRows = fontInfo->charPxH - numRowsToDraw;
                textCmd2->numChars = strLen;
                textCmd2->fontID = currFontID;

                enqueueCmd(textCmd2, stripeIdx+1);
            }

            // copy chars into command buffer
            for(int index = 0; index < strLen; index++) {
                uint8_t charCode = str[index] - fontInfo->firstCharCode;
                // replace any invalid ones with SPACE
                if(charCode > fontInfo->finalCharCode) {
                    charCode = 0xff;
                }
                charCodes[index] = charCode;
            }

            // set return value to number of characters enqueued
            result = strLen;
        }
    }

    return mp_obj_new_int(result);
}


//======================================================================================================
// Render all the enqueued commands for the given stripe into the given display stripe buffer 
//======================================================================================================
STATIC mp_obj_t renderStripe(mp_obj_t stripeIdxObj, mp_obj_t displayStripeObj) {
    uint8_t stripeIdx = mp_obj_get_int(stripeIdxObj);
    mp_obj_array_t* displayStripeObjPtr = MP_OBJ_TO_PTR(displayStripeObj);
    uint16_t* displayStripe = (uint16_t*)(displayStripeObjPtr->items);
    executeStripeCommands(stripeIdx, displayStripe);
    return mp_obj_new_int(0);
}

//======================================================================================================
// Reset the render commands
//======================================================================================================
STATIC mp_obj_t clearCmdQueue() {
    resetCmdBuffer();
    return mp_obj_new_int(0);
}


//======================================================================================================
//======================================================================================================
//
//
//  Audio system internals
//
//
//======================================================================================================
//======================================================================================================

#define SAMPLE_RATE 16000
#define LUT_SIZE 17
#define INTERP_BITS 11
#define INTERP_MASK ((1<<INTERP_BITS)-1)
int16_t sineLUT[LUT_SIZE]; 

uint32_t globalPhase; // just until proper instruments/timing is set up

//======================================================================================================
// Get sine (-32767 -> 32767) value for 'phase' (0 -> 65535)
//======================================================================================================
int16_t sine(uint16_t phase) {
    uint16_t idx = (phase >> INTERP_BITS) & 0xf;
    int32_t delta = sineLUT[idx+1] - sineLUT[idx];
    int32_t interp = phase & INTERP_MASK;
    int32_t interpDelta = delta*interp;
    int16_t value = sineLUT[idx] + (interpDelta>>INTERP_BITS);
    if (phase & 0x8000) {
        value = -value;
    }
    return value;
}

// void *memcpy(void *restrict s1, const void *restrict s2, size_t n) {
//   char *c1 = (char *)s1;
//   const char *c2 = (const char *)s2;
//   for (size_t i = 0; i < n; ++i)
//     c1[i] = c2[i];
//   return s1;
// }
//======================================================================================================
// Initialise the audio internals
//======================================================================================================
STATIC mp_obj_t initAudio() {
    // initialise sine look-up table
    sineLUT[ 0] = 0;
    sineLUT[ 1] = 6392;
    sineLUT[ 2] = 12539;
    sineLUT[ 3] = 18204;
    sineLUT[ 4] = 23169;
    sineLUT[ 5] = 27244;
    sineLUT[ 6] = 30272;
    sineLUT[ 7] = 32137;
    sineLUT[ 8] = 32767;
    sineLUT[ 9] = 32137;
    sineLUT[10] = 30272;
    sineLUT[11] = 27244;
    sineLUT[12] = 23169;
    sineLUT[13] = 18204;
    sineLUT[14] = 12539;
    sineLUT[15] = 6392;
    sineLUT[16] = 0;

    globalPhase = 0;

    return mp_obj_new_int(0);
}

//======================================================================================================
// Fill the given buffer with the next audio samples
//======================================================================================================
STATIC mp_obj_t synthFillBuffer(mp_obj_t bufferObj) {
    mp_obj_array_t* buffer = (mp_obj_array_t*)MP_OBJ_TO_PTR(bufferObj);
    int8_t* dstSample = (int8_t*)(buffer->items);
    int8_t* beyondEnd = dstSample + buffer->len;

    uint32_t deltaPhase = (220<<16) / SAMPLE_RATE;  // in fractions of a cycle (0-1) per sample interval
    deltaPhase <<= 16;  // in 'phase' (0->65535) per sample interval

    while(dstSample < beyondEnd) {
        uint16_t value = sine(globalPhase>>16) >> 1;
        *(dstSample++) = value & 0xff;
        *(dstSample++) = value>>8;
        globalPhase += deltaPhase;
    }

    return mp_obj_new_int(0);
}



// Define a Python reference to the functions above
STATIC MP_DEFINE_CONST_FUN_OBJ_0(initGraphics_obj, initGraphics);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(setTransparentColour_obj, setTransparentColour);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(setFont_obj, setFont);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(addFont_obj, addFont);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(clear565_obj, clear565);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(blit32_obj, blit32);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(drawText_obj, drawText);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(renderStripe_obj, renderStripe);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(clearCmdQueue_obj, clearCmdQueue);

STATIC MP_DEFINE_CONST_FUN_OBJ_0(initAudio_obj, initAudio);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(synthFillBuffer_obj, synthFillBuffer);



// STATIC MP_DEFINE_CONST_FUN_OBJ_2(clear232_obj, clear232);
// STATIC MP_DEFINE_CONST_FUN_OBJ_3(encode_obj, encode);

// This is the entry point and is called when the module is imported
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    // This must be first, it sets up the globals dict and other things
    MP_DYNRUNTIME_INIT_ENTRY

    // Make the function available in the module's namespace
    mp_store_global(MP_QSTR_initGraphics, MP_OBJ_FROM_PTR(&initGraphics_obj));
    mp_store_global(MP_QSTR_setTransparentColour, MP_OBJ_FROM_PTR(&setTransparentColour_obj));
    mp_store_global(MP_QSTR_setFont, MP_OBJ_FROM_PTR(&setFont_obj));
    mp_store_global(MP_QSTR_addFont, MP_OBJ_FROM_PTR(&addFont_obj));
    mp_store_global(MP_QSTR_clear565, MP_OBJ_FROM_PTR(&clear565_obj));
    mp_store_global(MP_QSTR_blit32, MP_OBJ_FROM_PTR(&blit32_obj));
    mp_store_global(MP_QSTR_drawText, MP_OBJ_FROM_PTR(&drawText_obj));
    mp_store_global(MP_QSTR_renderStripe, MP_OBJ_FROM_PTR(&renderStripe_obj));
    mp_store_global(MP_QSTR_clearCmdQueue, MP_OBJ_FROM_PTR(&clearCmdQueue_obj));

    mp_store_global(MP_QSTR_initAudio, MP_OBJ_FROM_PTR(&initAudio_obj));
    mp_store_global(MP_QSTR_synthFillBuffer, MP_OBJ_FROM_PTR(&synthFillBuffer_obj));

    // This must be last, it restores the globals dict
    MP_DYNRUNTIME_INIT_EXIT
}
