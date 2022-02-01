// Include the header file to get access to the MicroPython API
#include "py/dynruntime.h"

#include "py/objarray.h"

#include "py/nativeglue.h"

#define SCREEN_WIDTH (320)
#define SCREEN_HEIGHT (240)
#define NUM_STRIPES (15)
#define STRIPE_WIDTH (SCREEN_WIDTH)
#define STRIPE_HEIGHT (SCREEN_HEIGHT/NUM_STRIPES)
#define STRIPE_NUM_PX (STRIPE_WIDTH*STRIPE_HEIGHT)

//======================================================================================================
// Command structures
//======================================================================================================

// the base unit for any command
typedef struct _CmdBase {
    uint8_t opcode;
    uint8_t param8;
    uint16_t param16;
    void* nextPtr;
} CmdBase;

// the command to clear the stripe to a given colour
typedef struct _CmdClear565 {
    CmdBase base;
    // uint16_t colour;
    // uint16_t reserved;
} CmdClear565;

// the command to blit from a 4bpp source buffer to the display stripe
typedef struct _CmdBlit {
    CmdBase base;
    uint8_t* srcBuf;
    uint16_t* palette;
    uint16_t srcStartOffsetPx;
    uint16_t dstStartOffsetPx;
    uint8_t numPxInDrawRow;
    uint8_t srcStridePx;
    uint8_t numRows;
    uint8_t reserved;
} CmdBlit;


//======================================================================================================
//======================================================================================================
//
// Render commands
//
//======================================================================================================
//======================================================================================================
enum OPCODE {
    OPCODE_CLEAR565 = 1,
    OPCODE_BLIT = 2
};

//======================================================================================================
// returns the size of the command structure for the given opcode
//======================================================================================================
uint16_t getCmdSize(uint16_t opCode) {
    switch(opCode) {
        case OPCODE_CLEAR565:   return sizeof(CmdClear565);
        case OPCODE_BLIT:       return sizeof(CmdBlit);
        default:                return sizeof(CmdBase);
    }
}

//======================================================================================================
// horrible monolithic function to execute the given command
//======================================================================================================
void executeCmd(CmdBase* cmdPtr, uint16_t* displayStripe) {
    switch(cmdPtr->opcode) {
        case OPCODE_CLEAR565:
        {
            // CmdClear565* clearCmdPtr = (CmdClear565*)cmdPtr;
            uint16_t* target = displayStripe;
            uint16_t* beyondEnd = target + STRIPE_NUM_PX;
            while (target < beyondEnd) {
                *(target++) = cmdPtr->param16;//clearCmdPtr->colour;
            }
        }
        break;

        case OPCODE_BLIT:
        {
            CmdBlit* blitCmdPtr = (CmdBlit*)cmdPtr;
            uint8_t* src = blitCmdPtr->srcBuf + (blitCmdPtr->srcStartOffsetPx>>1);
            uint16_t* dst = displayStripe+blitCmdPtr->dstStartOffsetPx;
            int postRowSrcStride = (blitCmdPtr->srcStridePx - blitCmdPtr->numPxInDrawRow)>>1;
            int postRowDstStride = STRIPE_WIDTH - blitCmdPtr->numPxInDrawRow;
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
        }
        break;
    }
}

//======================================================================================================
//======================================================================================================
//
// Render command queue
//
//======================================================================================================
//======================================================================================================

#define CMD_BUFF_SIZE (4096)

uint8_t* commandBuffer[CMD_BUFF_SIZE] __attribute__ ((aligned (4)));
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
    size = (size+3) & (~3);
    if(nextFree+size < CMD_BUFF_SIZE) {
         result = (void*)(commandBuffer+nextFree);
         nextFree += size;
    } else {

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
//======================================================================================================
//
// Micropython interface
//
//======================================================================================================
//======================================================================================================

//======================================================================================================
// Initialise the system
//======================================================================================================
STATIC mp_obj_t init() {
    resetCmdBuffer();
    return mp_obj_new_int(0);
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
    int result = 0;

    mp_obj_tuple_t* pos = (mp_obj_tuple_t*)MP_OBJ_TO_PTR(posTuple);
    mp_int_t posX = mp_obj_get_int(pos->items[0]);
    mp_int_t posY = mp_obj_get_int(pos->items[1]);

    mp_obj_array_t* srcBuf = MP_OBJ_TO_PTR(srcBuf32x32);
    uint8_t* src = (uint8_t*)srcBuf->items;

    mp_obj_array_t* paletteBuf = MP_OBJ_TO_PTR(paletteObj);
    uint16_t* palette = (uint16_t*)paletteBuf->items;

    // don't worry about clipping yet
    
    int startStripe = posY / STRIPE_HEIGHT;
    int endStripe = (posY+31) / STRIPE_HEIGHT;

    uint16_t srcOffsetPx = 0; // this should be X offset

    // start stripe
    {
        CmdBlit* blitCmd = (CmdBlit*)allocCmd(OPCODE_BLIT);
        if(blitCmd != NULL) {
            blitCmd->srcBuf = src;
            blitCmd->palette = palette;
            blitCmd->srcStartOffsetPx = srcOffsetPx;
            blitCmd->dstStartOffsetPx = (posY % STRIPE_HEIGHT)*STRIPE_WIDTH + posX;
            blitCmd->numPxInDrawRow = 32;
            blitCmd->srcStridePx = 32;
            blitCmd->numRows = STRIPE_HEIGHT - (posY % STRIPE_HEIGHT);
            enqueueCmd(blitCmd, startStripe);

            srcOffsetPx += blitCmd->numRows * blitCmd->srcStridePx;
        } else {
            result = -1;
        }
    }

    // middle stripe
    if(endStripe > startStripe+1)
    {
        CmdBlit* blitCmd = (CmdBlit*)allocCmd(OPCODE_BLIT);
        if(blitCmd != NULL) {
            blitCmd->srcBuf = src;
            blitCmd->palette = palette;
            blitCmd->srcStartOffsetPx = srcOffsetPx;
            blitCmd->dstStartOffsetPx = posX;
            blitCmd->numPxInDrawRow = 32;
            blitCmd->srcStridePx = 32;
            blitCmd->numRows = STRIPE_HEIGHT;
            enqueueCmd(blitCmd, startStripe+1);

            srcOffsetPx += blitCmd->numRows * blitCmd->srcStridePx;
        } else {
            result = -2;
        }
    }

    // end stripe
    if(endStripe > startStripe)
    {
        CmdBlit* blitCmd = (CmdBlit*)allocCmd(OPCODE_BLIT);
        if(blitCmd != NULL) {
            blitCmd->srcBuf = src;
            blitCmd->palette = palette;
            blitCmd->numRows = (posY+32)-(endStripe*STRIPE_HEIGHT);
            blitCmd->srcStartOffsetPx = srcOffsetPx;
            blitCmd->dstStartOffsetPx = posX;
            blitCmd->numPxInDrawRow = 32;
            blitCmd->srcStridePx = 32;
            enqueueCmd(blitCmd, endStripe);
        } else {
            result = -3;
        }
    }

    return mp_obj_new_int(result);
}


//======================================================================================================
// Render all the enqueued commands for the given stripe into the given display stripe buffer 
//======================================================================================================
STATIC mp_obj_t renderStripe(mp_obj_t stripeIdxObj, mp_obj_t displayStripeObj) {
    CmdBase* cmdPtr = stripeCmdQueueHead[mp_obj_get_int(stripeIdxObj)];
    mp_obj_array_t* displayStripeObjPtr = MP_OBJ_TO_PTR(displayStripeObj);
    uint16_t* displayStripe = (uint16_t*)(displayStripeObjPtr->items);
    while(cmdPtr != NULL) {
        executeCmd(cmdPtr, displayStripe);
        cmdPtr = cmdPtr->nextPtr;
    }
    return mp_obj_new_int(0);
}

//======================================================================================================
// Reset the render commands
//======================================================================================================
STATIC mp_obj_t clearCmdQueue() {
    resetCmdBuffer();
    return mp_obj_new_int(0);
}

// //======================================================================================================
// // not long to live: encode an RGBA2321 buffer to the BGR565 screen stripe
// //======================================================================================================
// STATIC mp_obj_t encode(mp_obj_t sourceBuff, mp_obj_t destBuff, mp_obj_t srcOffset) {
//     mp_int_t srcOffsetInt = mp_obj_get_int(srcOffset);
//     mp_obj_array_t* src = MP_OBJ_TO_PTR(sourceBuff);
//     mp_obj_array_t* dst = MP_OBJ_TO_PTR(destBuff);
//     unsigned char* srcItem = (unsigned char*)src->items + srcOffsetInt;
//     unsigned char* dstItem = (unsigned char*)dst->items;
//     unsigned char* beyondEndItem = dstItem + dst->len;
//     while (dstItem < beyondEndItem) {
//         unsigned short srcByte = *(srcItem++);
//         unsigned short result = ((srcByte & 0x3) << 14) | ((srcByte & 0x1C) << 6) | ((srcByte & 0x0060) >> 2);
        
//         *(dstItem++) = result >> 8;
//         *(dstItem++) = result & 0xff;
//     }
//     return src;
// }

// //======================================================================================================
// // not long to live: clear an RGBA2321 buffer to the given colour
// //======================================================================================================
// STATIC mp_obj_t clear232(mp_obj_t buffer232, mp_obj_t colour232) {
//     unsigned char colour = mp_obj_get_int(colour232) & 0xff;
//     mp_obj_array_t* buf = MP_OBJ_TO_PTR(buffer232);
//     unsigned char* bufItem = (unsigned char*)(buf->items);
//     unsigned char* beyondEndItem = bufItem + buf->len;
//     while(bufItem < beyondEndItem)
//         *(bufItem++) = colour;
//     return buffer232;
// }





// Define a Python reference to the function above
STATIC MP_DEFINE_CONST_FUN_OBJ_0(init_obj, init);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(clear565_obj, clear565);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(blit32_obj, blit32);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(renderStripe_obj, renderStripe);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(clearCmdQueue_obj, clearCmdQueue);


// STATIC MP_DEFINE_CONST_FUN_OBJ_2(clear232_obj, clear232);
// STATIC MP_DEFINE_CONST_FUN_OBJ_3(encode_obj, encode);

// This is the entry point and is called when the module is imported
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    // This must be first, it sets up the globals dict and other things
    MP_DYNRUNTIME_INIT_ENTRY

    // Make the function available in the module's namespace
    mp_store_global(MP_QSTR_init, MP_OBJ_FROM_PTR(&init_obj));
    mp_store_global(MP_QSTR_blit32, MP_OBJ_FROM_PTR(&blit32_obj));
    mp_store_global(MP_QSTR_clear565, MP_OBJ_FROM_PTR(&clear565_obj));
    mp_store_global(MP_QSTR_renderStripe, MP_OBJ_FROM_PTR(&renderStripe_obj));
    mp_store_global(MP_QSTR_clearCmdQueue, MP_OBJ_FROM_PTR(&clearCmdQueue_obj));



    // mp_store_global(MP_QSTR_clear232, MP_OBJ_FROM_PTR(&clear232_obj));
    // mp_store_global(MP_QSTR_encode, MP_OBJ_FROM_PTR(&encode_obj));

    // This must be last, it restores the globals dict
    MP_DYNRUNTIME_INIT_EXIT
}
