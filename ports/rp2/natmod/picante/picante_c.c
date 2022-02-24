//======================================================================================================
//======================================================================================================
//
//  picante_c.c
//
//  One file to bring them all and in the darkness bind them
//
//  Copyright (c) 2022 Greg King
//
//======================================================================================================
//======================================================================================================

// Include the header file to get access to the MicroPython API
#include "py/dynruntime.h"

#include "py/nativeglue.h"

#include "audio.h"
#include "cmdQueue.h"
#include "graphics.h"
#include "font.h"


// This is the entry point and is called when the module is imported
mp_obj_t mpy_init(mp_obj_fun_bc_t *self, size_t n_args, size_t n_kw, mp_obj_t *args) {
    // This must be first, it sets up the globals dict and other things
    MP_DYNRUNTIME_INIT_ENTRY

    mpy_graphics_init();

    mpy_audio_init();

    // This must be last, it restores the globals dict
    MP_DYNRUNTIME_INIT_EXIT
}
