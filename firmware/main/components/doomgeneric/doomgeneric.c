#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


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
