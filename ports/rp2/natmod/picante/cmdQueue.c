
#include "cmdQueue.h"


//======================================================================================================
// returns the size of the command structure for the given opcode
//======================================================================================================
uint16_t getCmdSize(uint16_t opCode) {
    switch(opCode) {
        case OPCODE_CLEAR565:   return sizeof(CmdClear565);
        case OPCODE_BLIT:       return sizeof(CmdBlit);
        case OPCODE_TEXT:       return sizeof(CmdText);
        default:                return sizeof(CmdBase);
    }
}

//======================================================================================================
// Render util: blit just the high colour from the source column to dest column
//======================================================================================================
void blitHiCol(uint8_t* src, uint16_t* dst, CmdBlit* blitCmdPtr) {
    if(blitCmdPtr->transparentColour == 0xff) {
        for(int row = 0; row < blitCmdPtr->numRows; row++) {
            uint colIndex = (*src) >> 4;
            uint16_t colValue = blitCmdPtr->palette[colIndex];
            *dst = colValue;

            src += blitCmdPtr->srcStridePx>>1;
            dst += STRIPE_WIDTH;                 
        }
    } else {
        for(int row = 0; row < blitCmdPtr->numRows; row++) {
            uint colIndex = (*src) >> 4;
            if(colIndex != blitCmdPtr->transparentColour) {
                uint16_t colValue = blitCmdPtr->palette[colIndex];
                *dst = colValue;
            }

            src += blitCmdPtr->srcStridePx>>1;
            dst += STRIPE_WIDTH;                 
        }      
    }
}

//======================================================================================================
// Render util: blit just the low colour from the source column to dest column
//======================================================================================================
void blitLoCol(uint8_t* src, uint16_t* dst, CmdBlit* blitCmdPtr) {
    if(blitCmdPtr->transparentColour == 0xff) {
        for(int row = 0; row < blitCmdPtr->numRows; row++) {
            uint colIndex = (*src) & 0xf;
            uint16_t colValue = blitCmdPtr->palette[colIndex];
            *dst = colValue;

            src += blitCmdPtr->srcStridePx>>1;
            dst += STRIPE_WIDTH;                 
        }
    } else {
        for(int row = 0; row < blitCmdPtr->numRows; row++) {
            uint colIndex = (*src) & 0xf;
            if(colIndex != blitCmdPtr->transparentColour) {
                uint16_t colValue = blitCmdPtr->palette[colIndex];
                *dst = colValue;
            }

            src += blitCmdPtr->srcStridePx>>1;
            dst += STRIPE_WIDTH;                 
        }      
    }
}


//======================================================================================================
// Render util: blit both pixels from source to dest
//======================================================================================================
void blitRect(uint8_t* src, uint16_t* dst, CmdBlit* blitCmdPtr) {
    int postRowSrcStride = (blitCmdPtr->srcStridePx - blitCmdPtr->numPxInDrawRow)>>1;
    int postRowDstStride = STRIPE_WIDTH - blitCmdPtr->numPxInDrawRow;
    if(blitCmdPtr->transparentColour == 0xff) {
        for(int row = 0; row < blitCmdPtr->numRows; row++) {
            for(int srcByte = 0; srcByte < blitCmdPtr->numPxInDrawRow>>1; srcByte++) {
                uint colIndex = (*src) & 0xf;
                uint16_t colValue = blitCmdPtr->palette[colIndex];
                (*dst++) = colValue;

                colIndex = (*src) >> 4;
                colValue = blitCmdPtr->palette[colIndex];
                (*dst++) = colValue;

                src++;
            }
            dst += postRowDstStride;
            src += postRowSrcStride;
        }
    } else {
        for(int row = 0; row < blitCmdPtr->numRows; row++) {
            for(int srcByte = 0; srcByte < blitCmdPtr->numPxInDrawRow>>1; srcByte++) {
                uint colIndex = (*src) & 0xf;
                if(colIndex != blitCmdPtr->transparentColour) {
                    uint16_t colValue = blitCmdPtr->palette[colIndex];
                    *dst = colValue;
                }
                dst++;

                colIndex = (*src) >> 4;
                if(colIndex != blitCmdPtr->transparentColour) {
                    uint16_t colValue = blitCmdPtr->palette[colIndex];
                    *dst = colValue;
                }
                dst++;

                src++;
            }
            dst += postRowDstStride;
            src += postRowSrcStride;
        }
    }

}

//======================================================================================================
// horrible monolithic function to execute the given command
//======================================================================================================
void executeCmd(CmdBase* cmdPtr, uint16_t* displayStripe) {
    switch(cmdPtr->opcode) {
        case OPCODE_CLEAR565:
        {
            uint16_t* target = displayStripe;
            uint16_t* beyondEnd = target + STRIPE_NUM_PX;
            while (target < beyondEnd) {
                *(target++) = cmdPtr->param16;
            }
        }
        break;

        case OPCODE_BLIT:
        {
            CmdBlit* blitCmdPtr = (CmdBlit*)cmdPtr;
            uint16_t* dst = displayStripe+blitCmdPtr->dstStartOffsetPx;
            uint8_t* src = blitCmdPtr->srcBuf + (blitCmdPtr->srcStartOffsetPx>>1);

            if(blitCmdPtr->srcStartOffsetPx & 1) {
                blitHiCol(src, dst, blitCmdPtr);

                blitCmdPtr->srcStartOffsetPx++;
                src++;
                dst++;
                blitCmdPtr->numPxInDrawRow--;
            }

            if(blitCmdPtr->numPxInDrawRow & 1) {
                // blit final column pixels
                blitLoCol(src+(blitCmdPtr->numPxInDrawRow>>1), dst + blitCmdPtr->numPxInDrawRow-1, blitCmdPtr);
                blitCmdPtr->numPxInDrawRow--;
            }

            // then do main / central rect
            if(blitCmdPtr->numPxInDrawRow > 0)
            {
                src = blitCmdPtr->srcBuf + (blitCmdPtr->srcStartOffsetPx>>1);
                blitRect(src, dst, blitCmdPtr);
            }
        }
        break;

        case OPCODE_TEXT:
        {
            // CmdText* textCmdPtr = (CmdText*)cmdPtr;

            // uint16_t* dst = displayStripe+textCmdPtr->dstStartOffsetPx;
            // uint8_t* chars = (uint8_t*)(textCmdPtr + 1);
            // if(textCmdPtr->numChars & 0x80) {
            //     chars += sizeof(CmdText);
            // }
            // FontInfo* fontInfo = fontRegistry+textCmdPtr->fontID;

            // uint8_t numChars = textCmdPtr->numChars & 0x7f;
            // for(int idx = 0; idx < numChars; idx++) {
            //     if(chars[idx] != 0xff) {
            //         uint8_t* glyph = fontInfo->buffer + chars[idx]*fontInfo->charPxH + textCmdPtr->charStartRow;
            //         for(int row = 0; row < textCmdPtr->charNumRows; row++) {
            //             uint8_t mask = 1;
            //             uint16_t* rowDst = dst;
            //             for(int col = 0; col < fontInfo->charPxW; col++)
            //             {
            //                 if(*glyph & mask) {
            //                     *rowDst = textCmdPtr->colour;
            //                 }
            //                 rowDst += STRIPE_WIDTH;
            //                 mask <<= 1;
            //             }
            //             glyph++;
            //         }
            //     }
            //     dst += fontInfo->advanceX;
            // }
        }
        break;
    }
}





uint8_t* commandBuffer[CMD_BUFF_SIZE] __attribute__ ((aligned (CMD_ALIGNMENT)));
uint16_t nextFree;

CmdBase* stripeCmdQueueHead[NUM_STRIPES];
CmdBase* stripeCmdQueueTail[NUM_STRIPES];

//======================================================================================================
// Resets the command buffer
//======================================================================================================
void resetCmdBuffer() {
    nextFree = 0;
    for(int stripeIdx = 0; stripeIdx < NUM_STRIPES; stripeIdx++ ) {
        stripeCmdQueueHead[stripeIdx] = NULL;
        stripeCmdQueueTail[stripeIdx] = NULL;
    }
}

//======================================================================================================
// Allocate a piece of memory from the buffer
//======================================================================================================

void* alloc(uint16_t size) {
    void* result = NULL;
    size = 32;//(size+CMD_ALIGN_MASK) & (~CMD_ALIGN_MASK);
    if(nextFree+size < CMD_BUFF_SIZE) {
         result = (void*)(commandBuffer+nextFree);
         nextFree += size;
    }
    return result;
}

//======================================================================================================
// Allocate a command from the buffer
//======================================================================================================
CmdBase* allocCmd(uint16_t opCode) {
    CmdBase* cmd = (CmdBase*)alloc(getCmdSize(opCode));
    cmd->opcode = opCode;
    return cmd;
} 

//======================================================================================================
// Enqueue a command for the given stripe
//======================================================================================================
void enqueueCmd(void* cmdPtr, uint8_t stripeIdx) {
    CmdBase* cmd = (CmdBase*)cmdPtr;
    cmd->nextPtr = NULL;
    if (stripeCmdQueueHead[stripeIdx] == NULL) {
        stripeCmdQueueHead[stripeIdx] = cmd;
        stripeCmdQueueTail[stripeIdx] = cmd;
    } else {
        stripeCmdQueueTail[stripeIdx]->nextPtr = cmd;
        stripeCmdQueueTail[stripeIdx] = cmd;
    }
}

//======================================================================================================
// Execute all the commands for the given stripe
//======================================================================================================
void executeStripeCommands(uint8_t stripeIdx, uint16_t* displayStripe) {
    CmdBase* cmdPtr = stripeCmdQueueHead[stripeIdx];
    while(cmdPtr != NULL) {
        executeCmd(cmdPtr, displayStripe);
        cmdPtr = cmdPtr->nextPtr;
    }
}