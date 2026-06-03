cmake_minimum_required(VERSION 4.0.0)
project(nuklear_sdl)

cmake_path(SET shader_vertex_source   NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex.hlsl")
cmake_path(SET shader_vertex_output   NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/shaders/vertex.dxil")
cmake_path(SET shader_vertex_generate NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/shaders/vertex.h")
cmake_path(SET shader_pixel_source    NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/pixel.hlsl")
cmake_path(SET shader_pixel_output    NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/shaders/pixel.dxil")
cmake_path(SET shader_pixel_generate  NORMALIZE "${CMAKE_CURRENT_BINARY_DIR}/shaders/pixel.h")

cmake_path(SET shader_embed NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/embed.py")

add_custom_command(
        OUTPUT ${shader_vertex_output}
        COMMAND dxc.exe -T vs_6_0 -E vs_main -Fo ${shader_vertex_output} ${shader_vertex_source}
        DEPENDS ${shader_vertex_source}
        COMMENT "compiling vertex shader: ${shader_vertex_source} -> ${shader_vertex_output}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

add_custom_command(
        OUTPUT ${shader_pixel_output}
        COMMAND dxc.exe -T ps_6_0 -E ps_main -Fo ${shader_pixel_output} ${shader_pixel_source}
        DEPENDS ${shader_pixel_source}
        COMMENT "compiling pixel shader: ${shader_pixel_source} -> ${shader_pixel_output}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

add_custom_command(
        OUTPUT ${shader_vertex_generate} ${shader_pixel_generate}
        COMMAND uv run ${shader_embed} --files ${shader_vertex_output} ${shader_pixel_output}
        DEPENDS ${shader_embed} ${shader_vertex_output} ${shader_pixel_output}
        COMMENT "embedding shaders ${shader_vertex_output} -> ${shader_vertex_generate}, ${shader_pixel_output} -> ${shader_pixel_generate}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        VERBATIM
)

set(shaders_embedded ${shader_vertex_generate} ${shader_pixel_generate})
add_custom_target(nk_sdl_shaders DEPENDS ${shaders_embedded})

add_library(${PROJECT_NAME} STATIC)

add_dependencies(${PROJECT_NAME} nk_sdl_shaders)

target_sources(${PROJECT_NAME}
        PRIVATE
            nuklear_sdl_renderer_config.c
            nuklear_sdl_renderer.c
)

target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET nuklear_HEADERS
            TYPE HEADERS
            BASE_DIRS ${nuklear_SOURCE_DIR}
            FILES ${nuklear_SOURCE_DIR}/nuklear.h
)

target_sources(${PROJECT_NAME}
        PUBLIC
            FILE_SET nuklear_sdl_HEADERS
            TYPE HEADERS
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/nuklear_sdl_renderer.h
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# set C99 standard
set_target_properties(${PROJECT_NAME}
        PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
            POSITION_INDEPENDENT_CODE ON
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(${PROJECT_NAME}
        PUBLIC
            SDL3::SDL3
)
