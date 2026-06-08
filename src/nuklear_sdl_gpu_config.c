#include "nuklear_sdl_gpu_config.h"

char* nk_sdl_dtoa(char* str, const double d)
{
    NK_ASSERT(str);

    if (NULL == str)
    {
        return NULL;
    }

    (void)SDL_snprintf(str, 99999, "%.17g", d);

    return str;
}
