cmake_minimum_required(VERSION 4.0.0)
project(nuklear_sdl_test LANGUAGES C)

add_executable(${PROJECT_NAME} main.c)

# set C99 standard
set_target_properties(${PROJECT_NAME}
        PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
            nuklear_sdl::nuklear_sdl
)
