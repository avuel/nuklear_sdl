cmake_minimum_required(VERSION 4.0.0)
project(nuklear_sdl_gpu)

cmake_path(SET shader_vertex_source NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/vertex.hlsl")
cmake_path(SET shader_vertex_embed  NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/shaders/hlsl/vertex.hlsl.c")
cmake_path(SET shader_pixel_source  NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/hlsl/pixel.hlsl")
cmake_path(SET shader_pixel_embed   NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/shaders/hlsl/pixel.hlsl.c")

# call xxd on vertex shader to embed into .c file
add_custom_command(
        OUTPUT ${shader_vertex_embed}
        # TODO: windows xxd does not have -t option for null termination...
        COMMAND xxd -i -n vertex_hlsl ${shader_vertex_source} ${shader_vertex_embed}
        DEPENDS ${shader_vertex_source}
        COMMENT "embedding shader ${shader_vertex_source} -> ${shader_vertex_embed}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

# call xxd on pixel shader to embed into .c file
add_custom_command(
        OUTPUT ${shader_pixel_embed}
        # TODO: windows xxd does not have -t option for null termination...
        COMMAND xxd -i -n pixel_hlsl ${shader_pixel_source} ${shader_pixel_embed}
        DEPENDS ${shader_pixel_source}
        COMMENT "embedding shader ${shader_pixel_source} -> ${shader_pixel_embed}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

add_library(${PROJECT_NAME} STATIC)

# add source files
target_sources(${PROJECT_NAME}
        PRIVATE
            nuklear_sdl_gpu_config.c
            nuklear_sdl_gpu.c
            # embeded shader code
            ${shader_vertex_embed}
            ${shader_pixel_embed}
)

# expose nuklear header
target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET nuklear_HEADERS
            TYPE HEADERS
            BASE_DIRS ${nuklear_SOURCE_DIR}
            FILES ${nuklear_SOURCE_DIR}/nuklear.h
)

# expose nuklear_sdl3_gpu headers
target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET nuklear_sdl_HEADERS
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/nuklear_sdl_gpu_config.h
                ${CMAKE_CURRENT_SOURCE_DIR}/nuklear_sdl_gpu.h
)

# create nuklear_sdl_gpu::nuklear_sdl_gpu alias target
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}
        PROPERTIES
            # set C99 standard, required standard, no compiler extensions
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
            # enable for static lib
            POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# link against sdl3 and sdl3_shadercross default targets
target_link_libraries(${PROJECT_NAME}
        PUBLIC
            SDL3::SDL3
            SDL3_shadercross::SDL3_shadercross
)
