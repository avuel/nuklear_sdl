cmake_minimum_required(VERSION 4.0.0)
project(nuklear_sdl)

add_library(${PROJECT_NAME} STATIC)

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

target_link_libraries(${PROJECT_NAME}
        PUBLIC
            SDL3::SDL3
)
