#include "py/dynruntime.h"

#define SCREEN_WIDTH (320)
#define SCREEN_HEIGHT (240)
#define NUM_STRIPES (15)
#define STRIPE_WIDTH (SCREEN_WIDTH)
// stripe height MUST be power of 2
#define STRIPE_HEIGHT (SCREEN_HEIGHT/NUM_STRIPES)
#define STRIPE_NUM_PX (STRIPE_WIDTH*STRIPE_HEIGHT)
#define STRIPE_PX_ROW_MASK (STRIPE_HEIGHT-1)

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
    uint8_t transparentColour;
} CmdBlit;

// the command to draw a clipped string to the display stripe
// (actual raw character codes directly follow command struct in command buffer)
typedef struct _CmdText {
    CmdBase base;
    uint16_t dstStartOffsetPx;
    uint16_t colour;
    uint8_t charStartRow;
    uint8_t charNumRows;
    uint8_t numChars;   // if top bit is set, chars are stored after a SECOND CmdText structure
    uint8_t fontID;
} CmdText;

//======================================================================================================
//======================================================================================================
//
// Render commands
//
//======================================================================================================
//======================================================================================================
enum OPCODE {
    OPCODE_CLEAR565 = 1,
    OPCODE_BLIT = 2,
    OPCODE_TEXT = 3,
};


//======================================================================================================
//======================================================================================================
//
// Render command queue
//
//======================================================================================================
//======================================================================================================

#define CMD_BUFF_SIZE (8192)
#define CMD_ALIGNMENT (4)
#define CMD_ALIGN_MASK (CMD_ALIGNMENT-1)


//======================================================================================================
// Resets the command buffer
//======================================================================================================
void resetCmdBuffer();

//======================================================================================================
// Allocate a piece of memory from the buffer
//======================================================================================================
void* alloc(uint16_t size);

//======================================================================================================
// Allocate a command from the buffer
//======================================================================================================
CmdBase* allocCmd(uint16_t opCode);

//======================================================================================================
// Enqueue a command for the given stripe
//======================================================================================================
void enqueueCmd(void* cmdPtr, uint8_t stripeIdx);

//======================================================================================================
// Execute all the commands for the given stripe
//======================================================================================================
void executeStripeCommands(uint8_t stripeIdx, uint16_t* displayStripe);