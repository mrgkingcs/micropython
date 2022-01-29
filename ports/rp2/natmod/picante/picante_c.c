// Include the header file to get access to the MicroPython API
#include "py/dynruntime.h"

#include "py/objarray.h"

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
    uint8_t reserved;
    uint16_t reserved2;
    void* nextPtr;
} CmdBase;

// the command to clear the stripe to a given colour
typedef struct _CmdClear565 {
    CmdBase base;
    uint16_t colour;
    uint16_t reserved;
} CmdClear565;

//======================================================================================================
//======================================================================================================
//
// Render commands
//
//======================================================================================================
//======================================================================================================
enum OPCODE {
    OPCODE_CLEAR565 = 1
};

//======================================================================================================
// returns the size of the command structure for the given opcode
//======================================================================================================
uint16_t getCmdSize(uint16_t opCode) {
    switch(opCode) {
        case OPCODE_CLEAR565:   return sizeof(CmdClear565);
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
            CmdClear565* clearCmdPtr = (CmdClear565*)cmdPtr;
            uint16_t* target = displayStripe;
            uint16_t* beyondEnd = target + STRIPE_NUM_PX;
            while (target < beyondEnd) {
                *(target++) = clearCmdPtr->colour;
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

#define CMD_BUFF_SIZE (1024)

uint8_t commandBuffer[CMD_BUFF_SIZE];
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
    return (CmdBase*)alloc(getCmdSize(opCode));
} 

//======================================================================================================
// Enqueue a command for the given stripe
//======================================================================================================
void enqueueCmd(void* cmdPtr, uint8_t stripeIdx) {
    CmdBase* cmd = cmdPtr;
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
    //uint16_t colour = mp_obj_get_int(colour565);
    int result = nextFree;
    for(int stripeIdx = 0; stripeIdx < NUM_STRIPES; stripeIdx++) {
        CmdClear565* clearCmd = (CmdClear565*)allocCmd(OPCODE_CLEAR565);
         if(clearCmd != NULL) {
             result = 0;
    //         clearCmd->colour = colour;
    //         enqueueCmd(clearCmd, stripeIdx);
         }
    }
    return mp_obj_new_int(result);
}

//======================================================================================================
// blit a 32x32 bitmap to the screen (clipping as necessary)
//======================================================================================================
STATIC mp_obj_t blit32(mp_obj_t srcBuf32x32, mp_obj_t posTuple, mp_obj_t buffer232) {
    mp_obj_tuple_t* pos = (mp_obj_tuple_t*)MP_OBJ_TO_PTR(posTuple);
    mp_int_t posX = mp_obj_get_int(pos->items[0]);
    mp_int_t posY = mp_obj_get_int(pos->items[1]);

    mp_obj_array_t* srcBuf = MP_OBJ_TO_PTR(srcBuf32x32);
    mp_obj_array_t* dstBuf = MP_OBJ_TO_PTR(buffer232);

    const int dstStride = 320;
    unsigned char* dst = (unsigned char*)dstBuf->items;
    dst += posY*dstStride + posX;

    unsigned char* src = (unsigned char*)srcBuf->items;

    for(int row = 0; row < 32; row++) {
        unsigned char* beyondEnd = dst+32;
        while(dst < beyondEnd) {
            *(dst++) = *(src++);
        }
        dst += dstStride - 32;
    }

    return 0;
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

//======================================================================================================
// not long to live: encode an RGBA2321 buffer to the BGR565 screen stripe
//======================================================================================================
STATIC mp_obj_t encode(mp_obj_t sourceBuff, mp_obj_t destBuff, mp_obj_t srcOffset) {
    mp_int_t srcOffsetInt = mp_obj_get_int(srcOffset);
    mp_obj_array_t* src = MP_OBJ_TO_PTR(sourceBuff);
    mp_obj_array_t* dst = MP_OBJ_TO_PTR(destBuff);
    unsigned char* srcItem = (unsigned char*)src->items + srcOffsetInt;
    unsigned char* dstItem = (unsigned char*)dst->items;
    unsigned char* beyondEndItem = dstItem + dst->len;
    while (dstItem < beyondEndItem) {
        unsigned short srcByte = *(srcItem++);
        unsigned short result = ((srcByte & 0x3) << 14) | ((srcByte & 0x1C) << 6) | ((srcByte & 0x0060) >> 2);
        
        *(dstItem++) = result >> 8;
        *(dstItem++) = result & 0xff;
    }
    return src;
}

//======================================================================================================
// not long to live: clear an RGBA2321 buffer to the given colour
//======================================================================================================
STATIC mp_obj_t clear232(mp_obj_t buffer232, mp_obj_t colour232) {
    unsigned char colour = mp_obj_get_int(colour232) & 0xff;
    mp_obj_array_t* buf = MP_OBJ_TO_PTR(buffer232);
    unsigned char* bufItem = (unsigned char*)(buf->items);
    unsigned char* beyondEndItem = bufItem + buf->len;
    while(bufItem < beyondEndItem)
        *(bufItem++) = colour;
    return buffer232;
}





// Define a Python reference to the function above
STATIC MP_DEFINE_CONST_FUN_OBJ_0(init_obj, init);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(clear565_obj, clear565);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(blit32_obj, blit32);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(renderStripe_obj, renderStripe);
STATIC MP_DEFINE_CONST_FUN_OBJ_0(clearCmdQueue_obj, clearCmdQueue);


STATIC MP_DEFINE_CONST_FUN_OBJ_2(clear232_obj, clear232);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(encode_obj, encode);

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



    mp_store_global(MP_QSTR_clear232, MP_OBJ_FROM_PTR(&clear232_obj));
    mp_store_global(MP_QSTR_encode, MP_OBJ_FROM_PTR(&encode_obj));

    // This must be last, it restores the globals dict
    MP_DYNRUNTIME_INIT_EXIT
}
