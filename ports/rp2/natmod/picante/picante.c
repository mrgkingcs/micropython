// Include the header file to get access to the MicroPython API
#include "py/dynruntime.h"

#include "py/objarray.h"

STATIC mp_obj_t clear232(mp_obj_t buffer232, mp_obj_t colour232) {
    unsigned char colour = mp_obj_get_int(colour232) & 0xff;
    mp_obj_array_t* buf = MP_OBJ_TO_PTR(buffer232);
    unsigned char* bufItem = (unsigned char*)(buf->items);
    unsigned char* beyondEndItem = bufItem + buf->len;
    while(bufItem < beyondEndItem)
        *(bufItem++) = colour;
    return buffer232;
}

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

// This is the function which will be called from Python
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


// Define a Python reference to the function above
STATIC MP_DEFINE_CONST_FUN_OBJ_2(clear232_obj, clear232);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(blit32_obj, blit32);
STATIC MP_DEFINE_CONST_FUN_OBJ_3(encode_obj, encode);

// This is the entry point and is called when the module is imported
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    // This must be first, it sets up the globals dict and other things
    MP_DYNRUNTIME_INIT_ENTRY

    // Make the function available in the module's namespace
    mp_store_global(MP_QSTR_encode, MP_OBJ_FROM_PTR(&encode_obj));
    mp_store_global(MP_QSTR_clear232, MP_OBJ_FROM_PTR(&clear232_obj));
    mp_store_global(MP_QSTR_blit32, MP_OBJ_FROM_PTR(&blit32_obj));

    // This must be last, it restores the globals dict
    MP_DYNRUNTIME_INIT_EXIT
}
