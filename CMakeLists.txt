cmake_minimum_required(VERSION 4.0.0)
project(nuklear_sdl_top)

# add sdl3 if needed
if(NOT TARGET SDL3::SDL3)
    # only using static sdl3
    set(SDL_SHARED OFF)
    set(SDL_STATIC ON)
    add_subdirectory(vendor/sdl3 EXCLUDE_FROM_ALL)
endif()

if (NOT DirectXShaderCompiler_FOUND)
    if(NOT DEFINED ENV{DirectXShaderCompiler_ROOT} AND NOT DEFINED DirectXShaderCompiler_ROOT)
        message(FATAL_ERROR "variable DirectXShaderCompiler_ROOT not set (as environment or cmake variable)")
    endif()

    if(NOT DEFINED DirectXShaderCompiler_ROOT)
        cmake_path(SET DirectXShaderCompiler_ROOT NORMALIZE $ENV{DirectXShaderCompiler_ROOT})
    endif()
    cmake_path(SET sdl3_shadercross_module_path NORMALIZE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdl3_shadercross/cmake)
    list(APPEND CMAKE_MODULE_PATH ${sdl3_shadercross_module_path})
    find_package(DirectXShaderCompiler REQUIRED)
endif()

if(NOT TARGET spirv-cross-c)
    set(SPIRV_CROSS_CLI OFF CACHE BOOL "disable spirv cross cli" FORCE)
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "disable spirv cross tests" FORCE)
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "disable spirv cross json reflection" FORCE)
    set(SPIRV_CROSS_FORCE_PIC ON CACHE BOOL "enable spirv cross position-independent code" FORCE)

    add_subdirectory(vendor/spirv-cross spriv-cross)

    # spirv cross defines targets with - not _
    # sdl3_shadercross expects _ not -
    # so... add alias library targets to fix
    add_library(spirv_cross_core ALIAS spirv-cross-core)
    add_library(spirv_cross_glsl ALIAS spirv-cross-glsl)
    add_library(spirv_cross_cpp  ALIAS spirv-cross-cpp)
    add_library(spirv_cross_msl  ALIAS spirv-cross-msl)
    add_library(spirv_cross_hlsl ALIAS spirv-cross-hlsl)
    add_library(spirv_cross_util ALIAS spirv-cross-util)
    add_library(spirv_cross_c    ALIAS spirv-cross-c)
endif()

# add sdl3_shadercross target if needed
if(NOT TARGET SDL3_shadercross::SDL3_shadercross)
    set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF)
    set(SDLSHADERCROSS_INSTALL OFF)
    # add sdl3_shadercross
    add_subdirectory(vendor/sdl3_shadercross EXCLUDE_FROM_ALL)
endif()

# nuklear uses makefiles. just target the include directory
cmake_path(SET nuklear_SOURCE_DIR NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nuklear/src")

# add the src subdirectory (nuklear_sdl3 library)
add_subdirectory(src)

# add the demo example
add_subdirectory(demo EXCLUDE_FROM_ALL)
