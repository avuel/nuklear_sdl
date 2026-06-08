#include "nuklear_sdl_gpu.h"

#define NK_SIZEOF(st,m) sizeof(((st*)0)->m)

/* embed stbrp statically */
#define STBRP_STATIC

/* nuklear includes */
#include "../vendor/nuklear/src/nuklear_9slice.c"
#include "../vendor/nuklear/src/nuklear_buffer.c"
#include "../vendor/nuklear/src/nuklear_button.c"
#include "../vendor/nuklear/src/nuklear_chart.c"
#include "../vendor/nuklear/src/nuklear_color.c"
#include "../vendor/nuklear/src/nuklear_color_picker.c"
#include "../vendor/nuklear/src/nuklear_combo.c"
#include "../vendor/nuklear/src/nuklear_context.c"
#include "../vendor/nuklear/src/nuklear_contextual.c"
#include "../vendor/nuklear/src/nuklear_draw.c"
#include "../vendor/nuklear/src/nuklear_edit.c"
#include "../vendor/nuklear/src/nuklear_font.c"
#include "../vendor/nuklear/src/nuklear_group.c"
#include "../vendor/nuklear/src/nuklear_image.c"
#include "../vendor/nuklear/src/nuklear_input.c"
#include "../vendor/nuklear/src/nuklear_knob.c"
#include "../vendor/nuklear/src/nuklear_layout.c"
#include "../vendor/nuklear/src/nuklear_list_view.c"
#include "../vendor/nuklear/src/nuklear_math.c"
#include "../vendor/nuklear/src/nuklear_menu.c"
#include "../vendor/nuklear/src/nuklear_page_element.c"
#include "../vendor/nuklear/src/nuklear_panel.c"
#include "../vendor/nuklear/src/nuklear_pool.c"
#include "../vendor/nuklear/src/nuklear_popup.c"
#include "../vendor/nuklear/src/nuklear_progress.c"
#include "../vendor/nuklear/src/nuklear_property.c"
#include "../vendor/nuklear/src/nuklear_scrollbar.c"
#include "../vendor/nuklear/src/nuklear_selectable.c"
#include "../vendor/nuklear/src/nuklear_slider.c"
#include "../vendor/nuklear/src/nuklear_string.c"
#include "../vendor/nuklear/src/nuklear_style.c"
#include "../vendor/nuklear/src/nuklear_table.c"
#include "../vendor/nuklear/src/nuklear_text.c"
#include "../vendor/nuklear/src/nuklear_text_editor.c"
#include "../vendor/nuklear/src/nuklear_toggle.c"
#include "../vendor/nuklear/src/nuklear_tooltip.c"
#include "../vendor/nuklear/src/nuklear_tree.c"
#include "../vendor/nuklear/src/nuklear_utf8.c"
#include "../vendor/nuklear/src/nuklear_util.c"
#include "../vendor/nuklear/src/nuklear_vertex.c"
#include "../vendor/nuklear/src/nuklear_widget.c"
#include "../vendor/nuklear/src/nuklear_window.c"

/* third party includes */
#include <SDL3/SDL_gpu.h>

/* embedded character buffers of shader programs */
#include "shaders/hlsl/vertex.hlsl.h"
#include "shaders/hlsl/pixel.hlsl.h"

/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */

struct nk_sdl_device {
    SDL_GPUDevice* gpu;
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUBuffer* buffer_vertex;
    SDL_GPUBuffer* buffer_index;
    int buffer_vertex_max;
    int buffer_index_max;
    SDL_GPUTransferBuffer* buffer_transfer;
    SDL_GPUFence* fence;
    SDL_GPUTexture* font_texture;
    SDL_GPUSampler* font_sampler;
    struct nk_draw_null_texture tex_null;
    struct nk_buffer cmds;
};

struct nk_sdl_vertex {
    float position[2];
    float uv[2];
    float col[4];
};

struct nk_sdl {
    SDL_Window* window;
    struct nk_sdl_device device;
    struct nk_context ctx;
#ifdef NK_INCLUDE_FONT_BAKING
    struct nk_font_atlas atlas;
#endif /* NK_INCLUDE_FONT_BAKING */
    struct nk_allocator allocator;
    nk_handle userdata;
    Uint64 last_render;
    bool insert_toggle;
    bool edit_was_active;
};

NK_INTERN void nk_sdl_clipboard_paste(nk_handle usr, struct nk_text_edit* edit);
NK_INTERN void nk_sdl_clipboard_copy(nk_handle usr, const char* text, int len);
NK_INTERN void nk_sdl_device_create(struct nk_sdl* sdl);
NK_INTERN void nk_sdl_device_destroy(struct nk_sdl* sdl);

NK_API struct nk_context* nk_sdl_init(SDL_Window* window, SDL_GPUDevice* gpu, const struct nk_allocator allocator, const int max_vertex_buffer, const int max_index_buffer)
{
    NK_ASSERT(window);
    NK_ASSERT(gpu);
    NK_ASSERT(allocator.alloc);
    NK_ASSERT(allocator.free);
    struct nk_sdl* sdl = allocator.alloc(allocator.userdata, 0, sizeof(*sdl));
    NK_ASSERT(sdl);
    SDL_zerop(sdl);

    sdl->allocator = (struct nk_allocator) {
        .userdata = allocator.userdata,
        .alloc    = allocator.alloc,
        .free     = allocator.free,
    };

    sdl->window = window;

    nk_init(&sdl->ctx, &sdl->allocator, NULL);
    sdl->ctx.userdata = nk_handle_ptr(sdl);

    sdl->ctx.clip = (struct nk_clipboard) {
        .userdata = nk_handle_ptr(sdl),
        .paste    = nk_sdl_clipboard_paste,
        .copy     = nk_sdl_clipboard_copy,
    };

    sdl->device.gpu = gpu;
    sdl->device.buffer_vertex_max = max_vertex_buffer;
    sdl->device.buffer_index_max  = max_index_buffer;
    nk_sdl_device_create(sdl);

    sdl->edit_was_active = false;
    sdl->insert_toggle = false;

    return &sdl->ctx;
}

/*
 * Helper for retrieving the entries from nuklear_sdl3_gpu_data.c
 * Returns 'true' and populates 'out' upon success, otherwise returns 'false'
 * Failure means that the 'stage' and/or 'format' is either unsupported or invalid
 * 'out==NULL' is allowed, and can be useful for checking the support of specific format
 */
NK_INTERN bool nk_sdl_get_shader_info(
        SDL_GPUShaderCreateInfo*  out,
        const SDL_GPUShaderStage  stage,
        const SDL_GPUShaderFormat format)
{
    SDL_assert(SDL_HasExactlyOneBitSet32(format));

    /* FIXME: add SDL error here (?) */
    return false;
}

NK_API SDL_GPUShaderFormat nk_sdl_get_shader_formats(void)
{
    const SDL_GPUShaderFormat gpu_shader_formats = SDL_ShaderCross_GetHLSLShaderFormats() | SDL_ShaderCross_GetSPIRVShaderFormats();

#if 0
    if (0 != (SDL_GPU_SHADERFORMAT_PRIVATE  & gpu_shader_formats)) { SDL_Log("gpu format supported: SDL_GPU_SHADERFORMAT_PRIVATE");  }
    if (0 != (SDL_GPU_SHADERFORMAT_SPIRV    & gpu_shader_formats)) { SDL_Log("gpu format supported: SDL_GPU_SHADERFORMAT_SPIRV");    }
    if (0 != (SDL_GPU_SHADERFORMAT_DXBC     & gpu_shader_formats)) { SDL_Log("gpu format supported: SDL_GPU_SHADERFORMAT_DXBC");     }
    if (0 != (SDL_GPU_SHADERFORMAT_DXIL     & gpu_shader_formats)) { SDL_Log("gpu format supported: SDL_GPU_SHADERFORMAT_DXIL");     }
    if (0 != (SDL_GPU_SHADERFORMAT_MSL      & gpu_shader_formats)) { SDL_Log("gpu format supported: SDL_GPU_SHADERFORMAT_MSL");      }
    if (0 != (SDL_GPU_SHADERFORMAT_METALLIB & gpu_shader_formats)) { SDL_Log("gpu format supported: SDL_GPU_SHADERFORMAT_METALLIB"); }
#endif

    return gpu_shader_formats;
}

NK_INTERN void nk_sdl_shader_load_programs(char** gpu_shader_vertex_program, size_t* gpu_shader_vertex_program_size, char** gpu_shader_pixel_program, size_t* gpu_shader_pixel_program_size)
{
    NK_ASSERT(gpu_shader_vertex_program);
    NK_ASSERT(gpu_shader_vertex_program_size);
    NK_ASSERT(gpu_shader_pixel_program);
    NK_ASSERT(gpu_shader_pixel_program_size);

    *gpu_shader_vertex_program_size = (size_t)vertex_hlsl_len + 1;
    *gpu_shader_pixel_program_size  = (size_t)pixel_hlsl_len  + 1;

    *gpu_shader_vertex_program = SDL_malloc(*gpu_shader_vertex_program_size);
    if (NULL == *gpu_shader_vertex_program)
    {
        NK_ASSERT(!"vertex shader program buffer allocation failed");
    }

    *gpu_shader_pixel_program = SDL_malloc(*gpu_shader_pixel_program_size);
    if (NULL == *gpu_shader_pixel_program)
    {
        SDL_free(*gpu_shader_vertex_program);
        NK_ASSERT(!"pixel shader program buffer allocation failed");
    }

    memcpy(*gpu_shader_vertex_program, vertex_hlsl, vertex_hlsl_len);
    (*gpu_shader_vertex_program)[vertex_hlsl_len] = '\0';

    memcpy(*gpu_shader_pixel_program, pixel_hlsl, pixel_hlsl_len);
    (*gpu_shader_pixel_program)[pixel_hlsl_len] = '\0';
}

NK_INTERN void nk_sdl_shader_compile_hlsl_to_spirv(void** gpu_shader_vertex_code, size_t* gpu_shader_vertex_code_size, void** gpu_shader_pixel_code, size_t* gpu_shader_pixel_code_size)
{
    char* gpu_shader_vertex_program = NULL;
    size_t gpu_shader_vertex_program_size = 0;
    char* gpu_shader_pixel_program = NULL;
    size_t gpu_shader_pixel_program_size = 0;
    nk_sdl_shader_load_programs(&gpu_shader_vertex_program, &gpu_shader_vertex_program_size, &gpu_shader_pixel_program, &gpu_shader_pixel_program_size);

    const SDL_ShaderCross_HLSL_Info shadercross_hlsl_info_vertex_shader = {
        .source = gpu_shader_vertex_program,
        .entrypoint = "main_vs",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
        .props = 0,
    };
    *gpu_shader_vertex_code = SDL_ShaderCross_CompileSPIRVFromHLSL(&shadercross_hlsl_info_vertex_shader, gpu_shader_vertex_code_size);
    if (NULL == *gpu_shader_vertex_code)
    {
        SDL_free(gpu_shader_vertex_program);
        SDL_free(gpu_shader_pixel_program);
        NK_ASSERT(!"vertex shader compilation from hlsl to spirv failed");
    }

    const SDL_ShaderCross_HLSL_Info shadercross_hlsl_info_pixel_shader = {
        .source = gpu_shader_pixel_program,
        .entrypoint = "main_ps",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
        .props = 0,
    };
    *gpu_shader_pixel_code = SDL_ShaderCross_CompileSPIRVFromHLSL(&shadercross_hlsl_info_pixel_shader, gpu_shader_pixel_code_size);
    SDL_free(gpu_shader_vertex_program);
    SDL_free(gpu_shader_pixel_program);
    if (NULL == *gpu_shader_pixel_code)
    {
        SDL_free(gpu_shader_vertex_code);
        NK_ASSERT(!"pixel shader compilation from hlsl to spirv failed");
    }
}

NK_INTERN void nk_sdl_shader_compile_hlsl_to_dxbc(void** gpu_shader_vertex_code, size_t* gpu_shader_vertex_code_size, void** gpu_shader_pixel_code, size_t* gpu_shader_pixel_code_size)
{
    char* gpu_shader_vertex_program = NULL;
    size_t gpu_shader_vertex_program_size = 0;
    char* gpu_shader_pixel_program = NULL;
    size_t gpu_shader_pixel_program_size = 0;
    nk_sdl_shader_load_programs(&gpu_shader_vertex_program, &gpu_shader_vertex_program_size, &gpu_shader_pixel_program, &gpu_shader_pixel_program_size);

    const SDL_ShaderCross_HLSL_Info shadercross_hlsl_info_vertex_shader = {
        .source = gpu_shader_vertex_program,
        .entrypoint = "main_vs",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
        .props = 0,
    };
    *gpu_shader_vertex_code = SDL_ShaderCross_CompileDXBCFromHLSL(&shadercross_hlsl_info_vertex_shader, gpu_shader_vertex_code_size);
    if (NULL == *gpu_shader_vertex_code)
    {
        SDL_free(gpu_shader_vertex_program);
        SDL_free(gpu_shader_pixel_program);
        NK_ASSERT(!"vertex shader compilation from hlsl to dxil failed");
    }

    const SDL_ShaderCross_HLSL_Info shadercross_hlsl_info_pixel_shader = {
        .source = gpu_shader_pixel_program,
        .entrypoint = "main_ps",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
        .props = 0,
    };
    *gpu_shader_pixel_code = SDL_ShaderCross_CompileDXBCFromHLSL(&shadercross_hlsl_info_pixel_shader, gpu_shader_pixel_code_size);
    SDL_free(gpu_shader_vertex_program);
    SDL_free(gpu_shader_pixel_program);
    if (NULL == *gpu_shader_pixel_code)
    {
        SDL_free(gpu_shader_vertex_code);
        NK_ASSERT(!"pixel shader compilation from hlsl to dxil failed");
    }
}

NK_INTERN void nk_sdl_shader_compile_hlsl_to_dxil(void** gpu_shader_vertex_code, size_t* gpu_shader_vertex_code_size, void** gpu_shader_pixel_code, size_t* gpu_shader_pixel_code_size)
{
    char* gpu_shader_vertex_program = NULL;
    size_t gpu_shader_vertex_program_size = 0;
    char* gpu_shader_pixel_program = NULL;
    size_t gpu_shader_pixel_program_size = 0;
    nk_sdl_shader_load_programs(&gpu_shader_vertex_program, &gpu_shader_vertex_program_size, &gpu_shader_pixel_program, &gpu_shader_pixel_program_size);

    const SDL_ShaderCross_HLSL_Info shadercross_hlsl_info_vertex_shader = {
        .source = gpu_shader_vertex_program,
        .entrypoint = "main_vs",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_VERTEX,
        .props = 0,
    };
    *gpu_shader_vertex_code = SDL_ShaderCross_CompileDXILFromHLSL(&shadercross_hlsl_info_vertex_shader, gpu_shader_vertex_code_size);
    if (NULL == *gpu_shader_vertex_code)
    {
        SDL_free(gpu_shader_vertex_program);
        SDL_free(gpu_shader_pixel_program);
        NK_ASSERT(!"vertex shader compilation from hlsl to dxil failed");
    }

    const SDL_ShaderCross_HLSL_Info shadercross_hlsl_info_pixel_shader = {
        .source = gpu_shader_pixel_program,
        .entrypoint = "main_ps",
        .include_dir = NULL,
        .defines = NULL,
        .shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT,
        .props = 0,
    };
    *gpu_shader_pixel_code = SDL_ShaderCross_CompileDXILFromHLSL(&shadercross_hlsl_info_pixel_shader, gpu_shader_pixel_code_size);
    SDL_free(gpu_shader_vertex_program);
    SDL_free(gpu_shader_pixel_program);
    if (NULL == *gpu_shader_pixel_code)
    {
        SDL_free(gpu_shader_vertex_code);
        NK_ASSERT(!"pixel shader compilation from hlsl to dxil failed");
    }
}

NK_INTERN void nk_sdl_shader_compile(const SDL_GPUShaderFormat gpu_shader_format, void** gpu_shader_vertex_code, size_t* gpu_shader_vertex_code_size, void** gpu_shader_pixel_code, size_t* gpu_shader_pixel_code_size)
{
    if (!SDL_HasExactlyOneBitSet32(gpu_shader_format))
    {
        NK_ASSERT(!"gpu format more than one bit set");
    }

    /* TODO: when windows vim install updates xxd to have -t option, do not memcpy the program buffers, just use buffer directly... */
    switch (gpu_shader_format)
    {
        case SDL_GPU_SHADERFORMAT_INVALID:  { NK_ASSERT("!gpu format invalid"); break; }
        case SDL_GPU_SHADERFORMAT_PRIVATE:  { NK_ASSERT(!"gpu format not implemented"); break; }
        case SDL_GPU_SHADERFORMAT_SPIRV:    { nk_sdl_shader_compile_hlsl_to_spirv(gpu_shader_vertex_code, gpu_shader_vertex_code_size, gpu_shader_pixel_code, gpu_shader_pixel_code_size); break; }
        case SDL_GPU_SHADERFORMAT_DXBC:     { nk_sdl_shader_compile_hlsl_to_dxbc(gpu_shader_vertex_code, gpu_shader_vertex_code_size, gpu_shader_pixel_code, gpu_shader_pixel_code_size);  break; }
        case SDL_GPU_SHADERFORMAT_DXIL:     { nk_sdl_shader_compile_hlsl_to_dxil(gpu_shader_vertex_code, gpu_shader_vertex_code_size, gpu_shader_pixel_code, gpu_shader_pixel_code_size);  break; }
        case SDL_GPU_SHADERFORMAT_MSL:      { NK_ASSERT(!"gpu format not implemented"); break; }
        case SDL_GPU_SHADERFORMAT_METALLIB: { NK_ASSERT(!"gpu format not implemented"); break; }
        default: { NK_ASSERT(!"gpu format unknown"); break; }
    }
}

NK_INTERN void nk_sdl_device_create(struct nk_sdl* sdl)
{
    NK_ASSERT(sdl);
    nk_buffer_init(&sdl->device.cmds, &sdl->allocator, NK_BUFFER_DEFAULT_INITIAL_SIZE);

    const SDL_GPUShaderFormat gpu_shader_formats = SDL_GetGPUShaderFormats(sdl->device.gpu);

    SDL_GPUShaderFormat gpu_shader_format = SDL_GPU_SHADERFORMAT_INVALID;

    /* prefer directX */
    if      (0 != (SDL_GPU_SHADERFORMAT_DXIL     & gpu_shader_formats)) { gpu_shader_format = SDL_GPU_SHADERFORMAT_DXIL;     }
    else if (0 != (SDL_GPU_SHADERFORMAT_DXBC     & gpu_shader_formats)) { gpu_shader_format = SDL_GPU_SHADERFORMAT_DXBC;     }
    /* then vulkan */
    else if (0 != (SDL_GPU_SHADERFORMAT_SPIRV    & gpu_shader_formats)) { gpu_shader_format = SDL_GPU_SHADERFORMAT_SPIRV;    }
    /* then metal */
    else if (0 != (SDL_GPU_SHADERFORMAT_METALLIB & gpu_shader_formats)) { gpu_shader_format = SDL_GPU_SHADERFORMAT_METALLIB; }
    else if (0 != (SDL_GPU_SHADERFORMAT_MSL      & gpu_shader_formats)) { gpu_shader_format = SDL_GPU_SHADERFORMAT_MSL;      }

    void* gpu_shader_vertex_code = NULL;
    size_t gpu_shader_vertex_code_size = 0;

    void* gpu_shader_pixel_code = NULL;
    size_t gpu_shader_pixel_code_size = 0;

    nk_sdl_shader_compile(gpu_shader_format, &gpu_shader_vertex_code, &gpu_shader_vertex_code_size, &gpu_shader_pixel_code, &gpu_shader_pixel_code_size);

    /* upload vertex shader program */
    const SDL_GPUShaderCreateInfo shader_vertex_create_info = {
        .code_size = gpu_shader_vertex_code_size,
        .code = gpu_shader_vertex_code,
        .entrypoint = "main_vs",
        .format = gpu_shader_format,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
        .num_samplers = 0,
        .num_storage_textures = 0,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 1,
        .props = 0,
    };

    SDL_GPUShader* gpu_shader_vertex = SDL_CreateGPUShader(sdl->device.gpu, &shader_vertex_create_info);
    if (NULL == gpu_shader_vertex)
    {
        NK_ASSERT(!"vertex shader creation failed");
    }

    /* upload pixel shader program */
    const SDL_GPUShaderCreateInfo shader_pixel_create_info = {
        .code_size = gpu_shader_pixel_code_size,
        .code = gpu_shader_pixel_code,
        .entrypoint = "main_ps",
        .format = gpu_shader_format,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 1,
        .num_storage_textures = 1,
        .num_storage_buffers = 0,
        .num_uniform_buffers = 0,
        .props = 0,
    };

    SDL_GPUShader* gpu_shader_pixel = SDL_CreateGPUShader(sdl->device.gpu, &shader_pixel_create_info);
    if (NULL == gpu_shader_pixel)
    {
        NK_ASSERT(!"pixel shader creation failed");
    }

    const SDL_GPUColorTargetBlendState color_target_blend = {
        .src_color_blendfactor   = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor   = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op          = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor   = SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor   = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op          = SDL_GPU_BLENDOP_ADD,
        .enable_blend            = true,
        .enable_color_write_mask = true,
        .color_write_mask        = 0xF,
    };

    const SDL_GPUGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .vertex_shader   = gpu_shader_vertex,
        .fragment_shader = gpu_shader_pixel,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_input_state = {
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (const SDL_GPUVertexBufferDescription[]) {
                { .slot = 0, .pitch = sizeof(struct nk_sdl_vertex), .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
            },
            .num_vertex_attributes = 3,
            .vertex_attributes = (const SDL_GPUVertexAttribute[]) {
                    { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = NK_OFFSETOF(struct nk_sdl_vertex, position), },
                    { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = NK_OFFSETOF(struct nk_sdl_vertex, uv),       },
                    { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = NK_OFFSETOF(struct nk_sdl_vertex, col),      },
            },
        },
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .depth_bias_constant_factor = 0.0f,
            .depth_bias_clamp = 0.0f,
            .depth_bias_slope_factor = 0.0f,
            .enable_depth_bias = false,
            .enable_depth_clip = false,
        },
        .multisample_state = {
            .sample_count = 0,
            .sample_mask  = 0,
            .enable_mask  = false,
            .enable_alpha_to_coverage = false,
        },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_INVALID,
            .back_stencil_state = {
                .fail_op       = SDL_GPU_STENCILOP_INVALID,
                .pass_op       = SDL_GPU_STENCILOP_INVALID,
                .depth_fail_op = SDL_GPU_STENCILOP_INVALID,
                .compare_op    = SDL_GPU_COMPAREOP_INVALID,
            },
            .front_stencil_state = {
                .fail_op       = SDL_GPU_STENCILOP_INVALID,
                .pass_op       = SDL_GPU_STENCILOP_INVALID,
                .depth_fail_op = SDL_GPU_STENCILOP_INVALID,
                .compare_op    = SDL_GPU_COMPAREOP_INVALID,
            },
            .compare_mask = 0,
            .write_mask   = 0,
            .enable_depth_test   = false,
            .enable_depth_write  = false,
            .enable_stencil_test = false,
        },
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (const SDL_GPUColorTargetDescription[]) {
                { .format = SDL_GetGPUSwapchainTextureFormat(sdl->device.gpu, sdl->window), .blend_state = color_target_blend, },
            },
            .has_depth_stencil_target = false,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
        },
    };

    /* create graphics pipeline */
    sdl->device.pipeline = SDL_CreateGPUGraphicsPipeline(sdl->device.gpu, &graphics_pipeline_create_info);
    if (NULL == sdl->device.pipeline)
    {
        NK_ASSERT(!"graphics pipeline creation failed");
    }

    /* free shaders after creating pipeline */
    SDL_ReleaseGPUShader(sdl->device.gpu, gpu_shader_vertex);
    SDL_ReleaseGPUShader(sdl->device.gpu, gpu_shader_pixel);
    SDL_free(gpu_shader_vertex_code);
    SDL_free(gpu_shader_pixel_code);

    /* create vertex buffer */
    const SDL_GPUBufferCreateInfo buffer_vertex_create_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size  = sdl->device.buffer_vertex_max,
        .props = 0,
    };
    sdl->device.buffer_vertex = SDL_CreateGPUBuffer(sdl->device.gpu, &buffer_vertex_create_info);
    if (NULL == sdl->device.buffer_vertex)
    {
        NK_ASSERT(!"vertex buffer creation failed");
    }

    /* create index buffer */
    const SDL_GPUBufferCreateInfo buffer_index_create_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size  = sdl->device.buffer_index_max,
        .props = 0,
    };
    sdl->device.buffer_index = SDL_CreateGPUBuffer(sdl->device.gpu, &buffer_index_create_info);
    if (NULL == sdl->device.buffer_index)
    {
        NK_ASSERT(!"index buffer creation failed");
    }

    /* create gpu transfer buffer */
    const SDL_GPUTransferBufferCreateInfo gpu_transfer_buffer_create_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = buffer_vertex_create_info.size + buffer_index_create_info.size,
        .props = 0
    };
    sdl->device.buffer_transfer = SDL_CreateGPUTransferBuffer(sdl->device.gpu, &gpu_transfer_buffer_create_info);
    if (NULL == sdl->device.buffer_transfer)
    {
        NK_ASSERT(!"transfer buffer creation failed");
    }

    const SDL_GPUSamplerCreateInfo sampler_create_info = {
        .min_filter        = SDL_GPU_FILTER_LINEAR,
        .mag_filter        = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .mip_lod_bias      = 0.0f,
        .max_anisotropy    = 0.0f,
        .compare_op        = SDL_GPU_COMPAREOP_NEVER,
        .min_lod           = 0.0f,
        .max_lod           = 0.0f,
        .enable_anisotropy = false,
        .enable_compare    = false,
        .padding1          = 0,
        .padding2          = 0,

        .props = 0,
    };
    sdl->device.font_sampler = SDL_CreateGPUSampler(sdl->device.gpu, &sampler_create_info);
}

NK_INTERN void nk_sdl_device_destroy(struct nk_sdl* sdl)
{
    if (NULL != sdl->device.fence)
    {
        const bool success = SDL_WaitForGPUFences(sdl->device.gpu, true, &sdl->device.fence, 1);
        NK_ASSERT(success);
        SDL_ReleaseGPUFence(sdl->device.gpu, sdl->device.fence);
        sdl->device.fence = NULL;
    }

    if (NULL != sdl->device.font_sampler)
    {
        SDL_ReleaseGPUSampler(sdl->device.gpu, sdl->device.font_sampler);
        sdl->device.font_sampler = NULL;
    }

    if (NULL != sdl->device.font_texture)
    {
        SDL_ReleaseGPUTexture(sdl->device.gpu, sdl->device.font_texture);
        sdl->device.font_texture = NULL;
    }

    if (NULL != sdl->device.pipeline)
    {
        SDL_ReleaseGPUGraphicsPipeline(sdl->device.gpu, sdl->device.pipeline);
        sdl->device.pipeline = NULL;
    }

    if (NULL != sdl->device.buffer_transfer)
    {
        SDL_ReleaseGPUTransferBuffer(sdl->device.gpu, sdl->device.buffer_transfer);
        sdl->device.buffer_transfer = NULL;
    }

    if (NULL != sdl->device.buffer_index)
    {
        SDL_ReleaseGPUBuffer(sdl->device.gpu, sdl->device.buffer_index);
        sdl->device.buffer_index = NULL;
    }

    if (NULL != sdl->device.buffer_vertex)
    {
        SDL_ReleaseGPUBuffer(sdl->device.gpu, sdl->device.buffer_vertex);
        sdl->device.buffer_vertex = NULL;
    }
}

NK_API void nk_sdl_shutdown(struct nk_context* ctx)
{
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);

#ifdef NK_INCLUDE_FONT_BAKING
    if (sdl->atlas.font_num > 0)
    {
        nk_font_atlas_clear(&sdl->atlas);
    }
#endif /* NK_INCLUDE_FONT_BAKING */

    nk_sdl_device_destroy(sdl);
    nk_buffer_free(&sdl->device.cmds);

    nk_free(ctx);
    sdl->allocator.free(sdl->allocator.userdata, sdl);
}

NK_INTERN void* nk_sdl_alloc(const nk_handle user, void* old, const nk_size size)
{
    NK_UNUSED(user);
    /* FIXME: nk_sdl_alloc should use SDL_realloc here, not SDL_malloc
     * but this could cause a double-free due to bug within Nuklear, see:
     * https://github.com/Immediate-Mode-UI/Nuklear/issues/768
     * */
#if 0
    return SDL_realloc(old, size);
#else
    NK_UNUSED(old);
    return SDL_malloc(size);
#endif
}

NK_INTERN void nk_sdl_free(const nk_handle user, void* old)
{
    NK_UNUSED(user);
    SDL_free(old);
}

NK_API struct nk_allocator nk_sdl_allocator()
{
    struct nk_allocator allocator = { 0 };

    allocator.userdata.ptr = 0;
    allocator.alloc = nk_sdl_alloc;
    allocator.free = nk_sdl_free;

    return allocator;
}

NK_API int nk_sdl_handle_event(struct nk_context* ctx, SDL_Event *evt)
{
    NK_ASSERT(ctx);
    NK_ASSERT(evt);

    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);

    /* We only care about Window currently used by Nuklear */
    if (sdl->window != SDL_GetWindowFromEvent(evt))
    {
        return 0;
    }

    switch (evt->type)
    {
        case SDL_EVENT_KEY_UP: /* KEYUP & KEYDOWN share same routine */
        case SDL_EVENT_KEY_DOWN:
        {
            const int down = evt->type == SDL_EVENT_KEY_DOWN;
            const int ctrl_down = (int)(evt->key.mod & SDL_KMOD_CTRL);

            switch(evt->key.key)
            {
                case SDLK_LALT:
                case SDLK_RALT:      { nk_input_key(ctx, NK_KEY_ALT,             down); break; }
                case SDLK_RSHIFT:    /* RSHIFT & LSHIFT share same routine */
                case SDLK_LSHIFT:    { nk_input_key(ctx, NK_KEY_SHIFT,           down); break; }
                case SDLK_DELETE:    { nk_input_key(ctx, NK_KEY_DEL,             down); break; }
                case SDLK_RETURN:    { nk_input_key(ctx, NK_KEY_ENTER,           down); break; }
                case SDLK_TAB:       { nk_input_key(ctx, NK_KEY_TAB,             down); break; }
                case SDLK_BACKSPACE: { nk_input_key(ctx, NK_KEY_BACKSPACE,       down); break; }
                case SDLK_HOME:      { nk_input_key(ctx, NK_KEY_TEXT_START,      down); nk_input_key(ctx, NK_KEY_SCROLL_START, down); break; }
                case SDLK_END:       { nk_input_key(ctx, NK_KEY_TEXT_END,        down); nk_input_key(ctx, NK_KEY_SCROLL_END,   down); break; }
                case SDLK_PAGEDOWN:  { nk_input_key(ctx, NK_KEY_SCROLL_DOWN,     down); break; }
                case SDLK_PAGEUP:    { nk_input_key(ctx, NK_KEY_SCROLL_UP,       down); break; }
                case SDLK_F1:        { nk_input_key(ctx, NK_KEY_F1,              down); break; }
                case SDLK_F2:        { nk_input_key(ctx, NK_KEY_F2,              down); break; }
                case SDLK_F3:        { nk_input_key(ctx, NK_KEY_F3,              down); break; }
                case SDLK_F4:        { nk_input_key(ctx, NK_KEY_F4,              down); break; }
                case SDLK_F5:        { nk_input_key(ctx, NK_KEY_F5,              down); break; }
                case SDLK_F6:        { nk_input_key(ctx, NK_KEY_F6,              down); break; }
                case SDLK_F7:        { nk_input_key(ctx, NK_KEY_F7,              down); break; }
                case SDLK_F8:        { nk_input_key(ctx, NK_KEY_F8,              down); break; }
                case SDLK_F9:        { nk_input_key(ctx, NK_KEY_F9,              down); break; }
                case SDLK_F10:       { nk_input_key(ctx, NK_KEY_F10,             down); break; }
                case SDLK_F11:       { nk_input_key(ctx, NK_KEY_F11,             down); break; }
                case SDLK_F12:       { nk_input_key(ctx, NK_KEY_F12,             down); break; }
                case SDLK_A:         { nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, down && ctrl_down); break; }
                case SDLK_Z:         { nk_input_key(ctx, NK_KEY_TEXT_UNDO,       down && ctrl_down); break; }
                case SDLK_R:         { nk_input_key(ctx, NK_KEY_TEXT_REDO,       down && ctrl_down); break; }
                case SDLK_C:         { nk_input_key(ctx, NK_KEY_COPY,            down && ctrl_down); break; }
                case SDLK_V:         { nk_input_key(ctx, NK_KEY_PASTE,           down && ctrl_down); break; }
                case SDLK_X:         { nk_input_key(ctx, NK_KEY_CUT,             down && ctrl_down); break; }
                case SDLK_B:         { nk_input_key(ctx, NK_KEY_TEXT_LINE_START, down && ctrl_down); break; }
                case SDLK_E:         { nk_input_key(ctx, NK_KEY_TEXT_LINE_END,   down && ctrl_down); break; }
                case SDLK_UP:        { nk_input_key(ctx, NK_KEY_UP,              down); break;              }
                case SDLK_DOWN:      { nk_input_key(ctx, NK_KEY_DOWN,            down); break;              }
                case SDLK_ESCAPE:    { nk_input_key(ctx, NK_KEY_TEXT_RESET_MODE, down); break;              }
                case SDLK_INSERT:
                {
                    if (down)
                    {
                        sdl->insert_toggle = !sdl->insert_toggle;
                    }
                    nk_input_key(ctx, (sdl->insert_toggle ? NK_KEY_TEXT_INSERT_MODE : NK_KEY_TEXT_REPLACE_MODE), down);
                    break;
                }
                case SDLK_LEFT:  { nk_input_key(ctx, (ctrl_down ? NK_KEY_TEXT_WORD_LEFT  : NK_KEY_LEFT),  down); break; }
                case SDLK_RIGHT: { nk_input_key(ctx, (ctrl_down ? NK_KEY_TEXT_WORD_RIGHT : NK_KEY_RIGHT), down); break; }
                default: { return 0; }
            }

            return 1;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: /* MOUSEBUTTONUP & MOUSEBUTTONDOWN share same routine */
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            const int x = (int)evt->button.x;
            const int y = (int)evt->button.y;
            const int down = evt->button.down;
            switch (evt->button.button)
            {
                case SDL_BUTTON_LEFT:
                {
                    if (evt->button.clicks > 1)
                    {
                        nk_input_button(ctx, NK_BUTTON_DOUBLE, x, y, down);
                    }
                    nk_input_button(ctx, NK_BUTTON_LEFT, x, y, down);
                    break;
                }
                case SDL_BUTTON_MIDDLE: { nk_input_button(ctx, NK_BUTTON_MIDDLE, x, y, down); break; }
                case SDL_BUTTON_RIGHT:  { nk_input_button(ctx, NK_BUTTON_RIGHT,  x, y, down); break; }
                case SDL_BUTTON_X1:     { nk_input_button(ctx, NK_BUTTON_X1,     x, y, down); break; }
                case SDL_BUTTON_X2:     { nk_input_button(ctx, NK_BUTTON_X2,     x, y, down); break; }
                default: { return 0; }
            }

            return 1;
        }

        case SDL_EVENT_MOUSE_MOTION:
        {
            ctx->input.mouse.pos.x = evt->motion.x;
            ctx->input.mouse.pos.y = evt->motion.y;
            ctx->input.mouse.delta.x = ctx->input.mouse.pos.x - ctx->input.mouse.prev.x;
            ctx->input.mouse.delta.y = ctx->input.mouse.pos.y - ctx->input.mouse.prev.y;
            return 1;
        }

        case SDL_EVENT_TEXT_INPUT:
        {
            NK_ASSERT(evt->text.text);
            const nk_size len = SDL_strlen(evt->text.text);
            NK_ASSERT(len <= NK_UTF_SIZE);
            nk_glyph glyph;
            NK_MEMCPY(glyph, evt->text.text, len);
            nk_input_glyph(ctx, glyph);
            return 1;
        }

        case SDL_EVENT_MOUSE_WHEEL:
        {
            nk_input_scroll(ctx, nk_vec2(evt->wheel.x, evt->wheel.y));
            return 1;
        }

        default: { break;}
    }

    return 0;
}

NK_INTERN void nk_sdl_device_upload_atlas(struct nk_context* ctx, const void* image, const int width, const int height)
{
    NK_ASSERT(ctx);
    NK_ASSERT(image);
    NK_ASSERT(width > 0);
    NK_ASSERT(height > 0);

    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);

    /* Clean up if the texture already exists. */
    if (NULL != sdl->device.font_texture)
    {
        SDL_ReleaseGPUTexture(sdl->device.gpu, sdl->device.font_texture);
        sdl->device.font_texture = NULL;
    }

    const SDL_GPUTextureCreateInfo texture_create_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = 0,
        .props = 0,
    };

    sdl->device.font_texture = SDL_CreateGPUTexture(sdl->device.gpu, &texture_create_info);
    if (NULL == sdl->device.font_texture)
    {
        NK_ASSERT(!"font texture creation failed");
    }

    const int pitch = 4 * width;
    const SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = height * pitch,
        .props = 0,
    };

    SDL_GPUTransferBuffer* font_tex_transfer_buffer = SDL_CreateGPUTransferBuffer(sdl->device.gpu, &transfer_buffer_create_info);
    if (NULL == font_tex_transfer_buffer)
    {
        NK_ASSERT(!"font texture transfer buffer creation failed");
    }

    void* font_fex_data = SDL_MapGPUTransferBuffer(sdl->device.gpu, font_tex_transfer_buffer, false);

    SDL_memcpy(font_fex_data, image, transfer_buffer_create_info.size);

    SDL_UnmapGPUTransferBuffer(sdl->device.gpu, font_tex_transfer_buffer);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(sdl->device.gpu);
    NK_ASSERT(command_buffer);

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    {
        const SDL_GPUTextureTransferInfo texture_transfer_info = {
            .transfer_buffer = font_tex_transfer_buffer,
            .offset          = 0,
            .pixels_per_row  = width,
            .rows_per_layer  = height,
        };

        const SDL_GPUTextureRegion texture_region = {
            .texture = sdl->device.font_texture,
            .mip_level = 0,
            .layer = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .w = width,
            .h = height,
            .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &texture_transfer_info, &texture_region, false);
    }
    SDL_EndGPUCopyPass(copy_pass);

    /* submit command buffer */
    const bool success = SDL_SubmitGPUCommandBuffer(command_buffer);
    if (!success)
    {
        NK_ASSERT(!"command buffer submission failed");
    }

    SDL_ReleaseGPUTransferBuffer(sdl->device.gpu, font_tex_transfer_buffer);
}

NK_API nk_bool nk_sdl_render_needed(struct nk_context* ctx)
{
    static void* cmds_last = NULL;
    static nk_size cmds_last_size = 0;

    const void* cmds = nk_buffer_memory(&ctx->memory);
    const nk_size cmds_size = ctx->memory.allocated;

    if (cmds_size != cmds_last_size || 0 != memcmp(cmds_last, cmds, cmds_size))
    {
        if (cmds_size > cmds_last_size)
        {
            cmds_last = nk_sdl_alloc(ctx->userdata, cmds_last, cmds_size);
        }

        memcpy(cmds_last, cmds, cmds_size);
        cmds_last_size = cmds_size;

        return true;
    }

    nk_clear(ctx);
    return false;
}

NK_API void nk_sdl_render(struct nk_context* ctx, const enum nk_anti_aliasing AA, const struct nk_colorf color)
{
    /* setup global state */
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);

    {
        /* setup internal delta time that Nuklear needs for animations */
        const Uint64 ticks = SDL_GetTicksNS();
        ctx->delta_time_seconds = SDL_NS_TO_SECONDS((float)(ticks - sdl->last_render));
        sdl->last_render = ticks;
    }

    /* load draw vertices & elements directly into vertex + element buffer */
    {
        /* Wait until GPU is done with buffer */
        void* transfer_buffer_data = SDL_MapGPUTransferBuffer(sdl->device.gpu, sdl->device.buffer_transfer, false);
        if (NULL == transfer_buffer_data)
        {
            NK_ASSERT(!"transfer buffer map failed");
        }

        /* fill converting configuration */
        static const struct nk_draw_vertex_layout_element vertex_layout[] = {
            {NK_VERTEX_POSITION,    NK_FORMAT_FLOAT,              NK_OFFSETOF(struct nk_sdl_vertex, position)},
            {NK_VERTEX_TEXCOORD,    NK_FORMAT_FLOAT,              NK_OFFSETOF(struct nk_sdl_vertex, uv)      },
            {NK_VERTEX_COLOR,       NK_FORMAT_R32G32B32A32_FLOAT, NK_OFFSETOF(struct nk_sdl_vertex, col)     },
            {NK_VERTEX_LAYOUT_END}
        };
        const struct nk_convert_config config = {
            .global_alpha = 1.0f,
            .line_AA = AA,
            .shape_AA = AA,
            .circle_segment_count = 22,
            .arc_segment_count = 22,
            .curve_segment_count = 22,
            .tex_null = sdl->device.tex_null,
            .vertex_layout = vertex_layout,
            .vertex_size = sizeof(struct nk_sdl_vertex),
            .vertex_alignment = NK_ALIGNOF(struct nk_sdl_vertex),
        };

        memset(transfer_buffer_data, 0, sdl->device.buffer_vertex_max + sdl->device.buffer_index_max);
        struct nk_sdl_vertex* vertices = transfer_buffer_data;
        Uint16* indices = transfer_buffer_data + (size_t)sdl->device.buffer_vertex_max;

        /* setup buffers to load vertices and elements */
        struct nk_buffer vbuf = { 0 };
        struct nk_buffer ebuf = { 0 };
        nk_buffer_init_fixed(&vbuf, vertices, (size_t)sdl->device.buffer_vertex_max);
        nk_buffer_init_fixed(&ebuf, indices, (size_t)sdl->device.buffer_index_max);
        nk_convert(ctx, &sdl->device.cmds, &vbuf, &ebuf, &config);

        SDL_UnmapGPUTransferBuffer(sdl->device.gpu, sdl->device.buffer_transfer);

        SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(sdl->device.gpu);
        if (NULL == command_buffer)
        {
            NK_ASSERT(!"command buffer acquisition failed");
        }

        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
        {
            /* copy vertex data */
            const SDL_GPUTransferBufferLocation gpu_transfer_buffer_location_vertex = {
                .transfer_buffer = sdl->device.buffer_transfer,
                .offset = 0,
            };
            const SDL_GPUBufferRegion gpu_transfer_buffer_region_vertex = {
                .buffer = sdl->device.buffer_vertex,
                .offset = 0,
                .size   = sdl->device.buffer_vertex_max
            };
            SDL_UploadToGPUBuffer(copy_pass, &gpu_transfer_buffer_location_vertex, &gpu_transfer_buffer_region_vertex, false);

            /* copy index data */
            const SDL_GPUTransferBufferLocation gpu_transfer_buffer_location_index = {
                .transfer_buffer = sdl->device.buffer_transfer,
                .offset          = sdl->device.buffer_vertex_max,
            };
            const SDL_GPUBufferRegion gpu_transfer_buffer_region_index = {
                .buffer = sdl->device.buffer_index,
                .offset = 0,
                .size   = sdl->device.buffer_index_max,
            };
            SDL_UploadToGPUBuffer(copy_pass, &gpu_transfer_buffer_location_index, &gpu_transfer_buffer_region_index, false);
        }
        SDL_EndGPUCopyPass(copy_pass);

        /* submit command buffer */
        const bool success = SDL_SubmitGPUCommandBuffer(command_buffer);
        if (!success)
        {
            NK_ASSERT(!"command buffer submission failed");
        }
    }

    float window_width = 0.0f;
    float window_height = 0.0f;
    {
        int w;
        int h;
        const bool success = SDL_GetWindowSize(sdl->window, &w, &h);
        NK_ASSERT(success);
        window_width = (float)w;
        window_height = (float)h;
    }

    float projection_orthographic[4][4] = {
        {  2.0f,  0.0f,  0.0f,  0.0f },
        {  0.0f, -2.0f,  0.0f,  0.0f },
        {  0.0f,  0.0f, -1.0f,  0.0f },
        { -1.0f,  1.0f,  0.0f,  1.0f },
    };
    projection_orthographic[0][0] = projection_orthographic[0][0] / window_width;
    projection_orthographic[1][1] = projection_orthographic[1][1] / window_height;

    /* convert from command queue into draw list and draw to screen */

    /* convert shapes into vertices */

    /* iterate over and execute each draw command */

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(sdl->device.gpu);
    if (NULL == command_buffer)
    {
        NK_ASSERT(!"command buffer acquisition failed");
    }

#if 0
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    {
        /* copy vertex data */
        const SDL_GPUTransferBufferLocation gpu_transfer_buffer_location_vertex = {
            .transfer_buffer = sdl->device.buffer_transfer,
            .offset = 0,
        };
        const SDL_GPUBufferRegion gpu_transfer_buffer_region_vertex = {
            .buffer = sdl->device.buffer_vertex,
            .offset = 0,
            .size   = sdl->device.buffer_vertex_max
        };
        SDL_UploadToGPUBuffer(copy_pass, &gpu_transfer_buffer_location_vertex, &gpu_transfer_buffer_region_vertex, false);

        /* copy index data */
        const SDL_GPUTransferBufferLocation gpu_transfer_buffer_location_index = {
            .transfer_buffer = sdl->device.buffer_transfer,
            .offset          = sdl->device.buffer_vertex_max,
        };
        const SDL_GPUBufferRegion gpu_transfer_buffer_region_index = {
            .buffer = sdl->device.buffer_index,
            .offset = 0,
            .size   = sdl->device.buffer_index_max,
        };
        SDL_UploadToGPUBuffer(copy_pass, &gpu_transfer_buffer_location_index, &gpu_transfer_buffer_region_index, false);
    }
    SDL_EndGPUCopyPass(copy_pass);
#endif

    SDL_GPUTexture* swapchain_texture = NULL;
    {
        const bool success = SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, sdl->window, &swapchain_texture, NULL, NULL);
        if (!success)
        {
            NK_ASSERT(!"swapchain texture acquisition failed");
        }
    }

    if (NULL != swapchain_texture)
    {
        const SDL_GPUColorTargetInfo color_target_info = {
            .texture = swapchain_texture,
            .mip_level = 0,
            .layer_or_depth_plane = 0,
            .clear_color = { .r = color.r, .g = color.g, .b = color.b, .a = color.a },
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE,
            .resolve_texture = NULL,
            .resolve_mip_level = 0,
            .resolve_layer = 0,
            .cycle = false,
            .cycle_resolve_texture = false,
        };

        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, NULL);
        {
            /* bind vertex buffer */
            const SDL_GPUBufferBinding buffer_vertex_binding = { .buffer = sdl->device.buffer_vertex, .offset = 0, };
            SDL_BindGPUVertexBuffers(render_pass, 0, &buffer_vertex_binding, 1);

            /* bind index buffer */
            const SDL_GPUBufferBinding buffer_index_binding = { .buffer = sdl->device.buffer_index, .offset = 0, };
            SDL_BindGPUIndexBuffer(render_pass, &buffer_index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            /* bind graphics pipeline */
            SDL_BindGPUGraphicsPipeline(render_pass, sdl->device.pipeline);

            /* push vertex uniform */
            SDL_PushGPUVertexUniformData(command_buffer, 0, projection_orthographic, sizeof(projection_orthographic));

            /* bind gpu fragment texture */
            SDL_BindGPUFragmentStorageTextures(render_pass, 0, &sdl->device.font_texture, 1);

            /* push fragment texture sampler */
            const SDL_GPUTextureSamplerBinding texture_sampler_binding = {
                .texture = sdl->device.font_texture,
                .sampler = sdl->device.font_sampler,
            };
            SDL_BindGPUFragmentSamplers(render_pass, 0, &texture_sampler_binding, 1);

            Uint32 offset = 0;
            const struct nk_draw_command* cmd = { 0 };

            nk_draw_foreach(cmd, &sdl->ctx, &sdl->device.cmds)
            {
                if (0 == cmd->elem_count)
                {
                    continue;
                }

                /* scissor */
                {
                    const SDL_Rect r = {
                        .x = (int)cmd->clip_rect.x,
                        .y = (int)cmd->clip_rect.y,
                        .w = (int)cmd->clip_rect.w,
                        .h = (int)cmd->clip_rect.h,
                    };
                    SDL_SetGPUScissor(render_pass, &r);
                }

                /* draw */
                SDL_DrawGPUIndexedPrimitives(render_pass, cmd->elem_count, 1, offset, 0, 0);
                offset += (Uint32)cmd->elem_count;
            }
        }
        SDL_EndGPURenderPass(render_pass);
    }

    sdl->device.fence = SDL_SubmitGPUCommandBufferAndAcquireFence(command_buffer);
    if (NULL == sdl->device.fence)
    {
        NK_ASSERT(!"command buffer submission and acquire fence failed");
    }

    nk_buffer_clear(&sdl->device.cmds);
    nk_clear(&sdl->ctx);

    /* wait for last frame to submit */
    {
        const bool success = SDL_WaitForGPUFences(sdl->device.gpu, true, &sdl->device.fence, 1);
        if (!success)
        {
            NK_ASSERT(!"wait for fence failed");
        }
    }
    SDL_ReleaseGPUFence(sdl->device.gpu, sdl->device.fence);
    sdl->device.fence = NULL;
}

NK_INTERN void nk_sdl_clipboard_paste(const nk_handle usr, struct nk_text_edit* edit)
{
    NK_UNUSED(usr);

    /* this function returns empty string on failure, not NULL */
    char* text = SDL_GetClipboardText();

    NK_ASSERT(text);

    if ('\0' != text[0])
    {
        /* FIXME: there is a bug in Nuklear that affects UTF8 clipboard handling
         * "len" should be a buffer length, but due to bug it must be a glyph count
         * see: https://github.com/Immediate-Mode-UI/Nuklear/pull/841 */
#if 0
        const int len = nk_strlen(text);
#else
        const int len = (int)SDL_utf8strlen(text);
#endif
        nk_textedit_paste(edit, text, len);
    }

    SDL_free(text);
}

NK_INTERN void nk_sdl_clipboard_copy(const nk_handle usr, const char* text, const int len)
{
    struct nk_sdl* sdl = usr.ptr;
    NK_ASSERT(sdl);

    if (len <= 0 || NULL == text)
    {
        return;
    }

    /* FIXME: there is a bug in Nuklear that affects UTF8 clipboard handling
     * "len" is expected to be a buffer length, but due to bug it actually is a glyph count
     * see: https://github.com/Immediate-Mode-UI/Nuklear/pull/841 */
#if 0
    buflen = len + 1;
    NK_UNUSED(ptext);
#else
    const char* ptext = text;

    for (int i = len; i > 0; i--)
    {
        (void)SDL_StepUTF8(&ptext, NULL);
    }

    const size_t buflen = (size_t)(ptext - text) + 1;
#endif

    char* str = sdl->allocator.alloc(sdl->allocator.userdata, NULL, buflen);

    if (NULL == str)
    {
        return;
    }

    SDL_strlcpy(str, text, buflen);
    SDL_SetClipboardText(str);
    sdl->allocator.free(sdl->allocator.userdata, str);
}

NK_API void nk_sdl_update_text_input(struct nk_context* ctx)
{
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);

    /* Determine if Nuklear is using any top-level "edit" widget.
     * Popups take higher priority because they block any incomming input.
     * This will not work, if the widget is not updating context state properly. */
    bool active;
    if (NULL == ctx->active)
    {
        active = false;
    }
    else if (NULL != ctx->active->popup.win)
    {
        active = ctx->active->popup.win->edit.active;
    }
    else
    {
        active = ctx->active->edit.active;
    }

    /* decide, if TextInputActive should be unchanged/stoped/started
     * and change its state accordingly for owned SDL Window */
    if (active != sdl->edit_was_active)
    {
        const bool window_edit_active = SDL_TextInputActive(sdl->window);

        /* If you ever hit this check, it means that the demo and your app
         * (or something else) are all trying to manage TextInputActive state.
         * This can cause subtle bugs where the state won't be what you expect.
         * You can safely remove this assert and the demo will keep working,
         * but make sure it does not cause any issues for you */
        NK_ASSERT(window_edit_active == sdl->edit_was_active && "something else changed TextInputActive state for this Window");

        if (!window_edit_active && !sdl->edit_was_active && active)
        {
            SDL_StartTextInput(sdl->window);
        }
        else if (window_edit_active && sdl->edit_was_active && !active)
        {
            SDL_StopTextInput(sdl->window);
        }

        sdl->edit_was_active = active;
    }

    /* FIXME:
     * for full SDL3 integration, you also need to find current edit widget
     * bounds and the text cursor offset, and pass this data into SDL_SetTextInputArea.
     * This is currently not possible to do safely as Nuklear does not support it.
     * https://wiki.libsdl.org/SDL3/SDL_SetTextInputArea
     * https://github.com/Immediate-Mode-UI/Nuklear/pull/857
     */
}

NK_API nk_handle nk_sdl_get_userdata(const struct nk_context* ctx)
{
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);
    return sdl->userdata;
}

NK_API void nk_sdl_set_userdata(struct nk_context* ctx, const nk_handle userdata)
{
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);
    sdl->userdata = userdata;
}

#ifdef NK_INCLUDE_FONT_BAKING
NK_API struct nk_font_atlas* nk_sdl_font_stash_begin(struct nk_context* ctx)
{
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);
    nk_font_atlas_init(&sdl->atlas, &sdl->allocator);
    nk_font_atlas_begin(&sdl->atlas);

    return &sdl->atlas;
}

NK_API void nk_sdl_font_stash_end(struct nk_context* ctx)
{
    NK_ASSERT(ctx);
    struct nk_sdl* sdl = ctx->userdata.ptr;
    NK_ASSERT(sdl);
    int w = 0;
    int h = 0;
    const void* image = nk_font_atlas_bake(&sdl->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    NK_ASSERT(image);
    nk_sdl_device_upload_atlas(&sdl->ctx, image, w, h);
    nk_font_atlas_end(&sdl->atlas, nk_handle_ptr(sdl->device.font_texture), &sdl->device.tex_null);

    if (NULL != sdl->atlas.default_font)
    {
        nk_style_set_font(&sdl->ctx, &sdl->atlas.default_font->handle);
    }
}
#endif /* NK_INCLUDE_FONT_BAKING */
