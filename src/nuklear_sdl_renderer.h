/* nuklear - public domain */

/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */

#ifndef NK_SDL3_RENDERER_H_
#define NK_SDL3_RENDERER_H_

#include "nuklear_sdl_renderer_config.h"
#include "nuklear.h"

#if SDL_MAJOR_VERSION < 3
    #error "nk_sdl3_renderer requires at least SDL 3.0.0"
#endif // SDL_MAJOR_VERSION < 3
#ifndef NK_INCLUDE_COMMAND_USERDATA
    #error "nk_sdl3_renderer requires the NK_INCLUDE_COMMAND_USERDATA define"
#endif // NK_INCLUDE_COMMAND_USERDATA
#ifndef NK_INCLUDE_VERTEX_BUFFER_OUTPUT
    #error "nk_sdl3_renderer requires the NK_INCLUDE_VERTEX_BUFFER_OUTPUT define"
#endif // NK_INCLUDE_VERTEX_BUFFER_OUTPUT

/* We have to redefine it because demos do not include any headers
 * This is the same default value as the one from "src/nuklear_internal.h" */
#ifndef NK_BUFFER_DEFAULT_INITIAL_SIZE
    #define NK_BUFFER_DEFAULT_INITIAL_SIZE (4*1024)
#endif // NK_BUFFER_DEFAULT_INITIAL_SIZE

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

NK_API struct nk_context*    nk_sdl_init(SDL_Window* window, SDL_GPUDevice* gpu, struct nk_allocator allocator, int max_vertex_buffer, int max_index_buffer);
NK_API void                  nk_sdl_shutdown(struct nk_context* ctx);
NK_API struct nk_allocator   nk_sdl_allocator(void);
NK_API int                   nk_sdl_handle_event(struct nk_context* ctx, SDL_Event* evt);
NK_API nk_bool               nk_sdl_render_needed(struct nk_context* ctx);
NK_API void                  nk_sdl_render(struct nk_context* ctx, enum nk_anti_aliasing AA, struct nk_colorf color);
NK_API void                  nk_sdl_update_text_input(struct nk_context* ctx);
NK_API nk_handle             nk_sdl_get_userdata(const struct nk_context* ctx);
NK_API void                  nk_sdl_set_userdata(struct nk_context* ctx, nk_handle userdata);
#ifdef NK_INCLUDE_FONT_BAKING
NK_API struct nk_font_atlas* nk_sdl_font_stash_begin(struct nk_context* ctx);
NK_API void                  nk_sdl_font_stash_end(struct nk_context* ctx);
#endif // NK_INCLUDE_FONT_BAKING

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* NK_SDL3_RENDERER_H_ */
