#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#include "m_argv.h"
#include "d_main.h"
#include "d_loop.h"
#include "i_system.h"
#include "i_video.h"
#include "s_sound.h"
#include "v_video.h"
#include "w_checksum.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);


int doomgeneric_Create(int argc, char **argv)
{
    const size_t screen_bytes = DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t);

	// save arguments
    myargc = argc;
    myargv = argv;

	M_FindResponseFile();

	if (DG_ScreenBuffer == NULL)
    {
#ifdef ESP_PLATFORM
        DG_ScreenBuffer = heap_caps_malloc(screen_bytes,
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        DG_ScreenBuffer = malloc(screen_bytes);
#endif
    }

    if (DG_ScreenBuffer == NULL)
    {
        fprintf(stderr, "Doom framebuffer allocation failed (%u bytes)\n",
                (unsigned int)screen_bytes);
        return 0;
    }

    memset(DG_ScreenBuffer, 0, screen_bytes);

	DG_Init();

	D_DoomMain ();
    return 1;
}

void doomgeneric_Destroy(void)
{
    S_Shutdown();
    I_ShutdownGraphics();
    V_UseBuffer(NULL);
    W_Shutdown();
    W_ChecksumShutdown();
    I_ResetExitFunctions();
    Z_Shutdown();
    D_ResetGameLoop();
    D_ResetMainState();

    // OpenCalc owns the shared UI canvas supplied as Doom's framebuffer.
    DG_ScreenBuffer = NULL;
    myargc = 0;
    myargv = NULL;
}
