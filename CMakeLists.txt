cmake_minimum_required(VERSION 4.0.0)
project(nuklear_sdl_top)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
add_subdirectory(vendor/sdl3 EXCLUDE_FROM_ALL)

cmake_path(SET nuklear_SOURCE_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nuklear/src")

add_subdirectory(src)

add_subdirectory(test EXCLUDE_FROM_ALL)
