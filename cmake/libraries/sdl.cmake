if(NOT USE_SDL2 AND NOT USE_SDL3)
    return()
endif()

if(USE_SDL2 AND USE_SDL3)
    message(FATAL_ERROR "Multiple SDL major versions enabled, only choose one")
    return()
endif()

if(NOT BUILD_CLIENT)
    return()
endif()

include(utils/arch)

if(WIN32)# OR APPLE)
    # On Windows (currently not macOS) we have internal SDL binaries we can use
    set(HAVE_INTERNAL_SDL true)
endif()

if(APPLE)
set(USE_INTERNAL_SDL OFF CACHE BOOL "Use internal SDL 2 or 3 binary (if available)" FORCE)
endif()

set(INTERNAL_SDL_DIR ${DEPS_DIR}/libsdl)
include("${INTERNAL_SDL_DIR}/versions.cmake")

if(USE_SDL3)
    set(SDL_VERSION 3)
    set(SDLx_VERSION "${SDL3_VERSION}")
else()
    set(SDL_VERSION 2)
    set(SDLx_VERSION "${SDL2_VERSION}")
endif()

if(USE_INTERNAL_SDL AND HAVE_INTERNAL_SDL)
    if(NOT WIN32)
        message(FATAL_ERROR "HAVE_INTERNAL_SDL set incorrectly; file a bug")
    endif()

    set(SDLx_ROOT "${INTERNAL_SDL_DIR}/sdl${SDL_VERSION}")
    if(USE_SDL3)
        # SDL3 sources include <SDL3/SDL.h>
        set(SDLx_INCLUDE_DIRS "${SDLx_ROOT}/include")
    else()
        # SDL2 sources include <SDL.h>
        set(SDLx_INCLUDE_DIRS "${SDLx_ROOT}/include/SDL2")
    endif()

    if(MSVC)
        set(SDLx_COMPILER_DIR msvc)
    elseif(MINGW)
        set(SDLx_COMPILER_DIR mingw)
    else()
        message(FATAL_ERROR "Internal SDL is only supported with MSVC or MinGW on Windows")
    endif()

    if(ARCH STREQUAL "x86_64")
        set(SDLx_ARCH x64)
        set(SDLx_MACHINE x64)
    elseif(ARCH STREQUAL "x86")
        set(SDLx_ARCH x86)
        set(SDLx_MACHINE x86)
    elseif(ARCH STREQUAL "arm64")
        set(SDLx_ARCH aarch64)
        set(SDLx_MACHINE arm64)
    else()
        message(FATAL_ERROR "Unknown ARCH ${ARCH} for internal SDL")
    endif()

    set(SDLx_DLL_NAME "SDL${SDL_VERSION}${SDLx_ARCH}")
    set(SDLx_DLL_SRC "${SDLx_ROOT}/windows/${SDLx_COMPILER_DIR}/${SDLx_ARCH}/SDL${SDL_VERSION}.dll")
    set(SDLx_DEF "${SDLx_ROOT}/def/${SDLx_DLL_NAME}.def")

    if(NOT EXISTS "${SDLx_DLL_SRC}")
        message(FATAL_ERROR "Bundled SDL DLL not found: ${SDLx_DLL_SRC}")
    endif()
    if(NOT EXISTS "${SDLx_DEF}")
        message(FATAL_ERROR "Bundled SDL def not found: ${SDLx_DEF}")
    endif()

    # The official import libraries reference SDL2.dll / SDL3.dll. Generate
    # import libraries that reference the architecture-suffixed runtime names
    # instead so 32-bit and 64-bit clients can live in the same directory.
    set(SDLx_GEN_DIR "${CMAKE_BINARY_DIR}/deps/sdl${SDL_VERSION}/${SDLx_ARCH}")
    file(MAKE_DIRECTORY "${SDLx_GEN_DIR}")

    if(MSVC)
        # CMAKE_AR can be stale when Visual Studio is updated without clearing
        # the build directory. Fall back to the lib.exe next to the compiler.
        set(SDLx_LIB "${CMAKE_AR}")
        if(NOT EXISTS "${SDLx_LIB}")
            unset(SDLx_LIB)
            get_filename_component(SDLx_COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
            find_program(SDLx_LIB NAMES lib.exe HINTS "${SDLx_COMPILER_DIR}" NO_DEFAULT_PATH)
        endif()
        if(NOT SDLx_LIB)
            message(FATAL_ERROR "lib.exe not found; cannot create the SDL import library")
        endif()

        set(SDLx_IMPLIB_TOOL "${SDLx_LIB}")
        set(SDLx_IMPLIB "${SDLx_GEN_DIR}/${SDLx_DLL_NAME}.lib")
        execute_process(
            COMMAND "${SDLx_LIB}" /nologo "/machine:${SDLx_MACHINE}"
                    "/def:${SDLx_DEF}" "/out:${SDLx_IMPLIB}"
            WORKING_DIRECTORY "${SDLx_GEN_DIR}"
            RESULT_VARIABLE SDLx_IMPLIB_RESULT
            OUTPUT_VARIABLE SDLx_IMPLIB_OUTPUT
            ERROR_VARIABLE SDLx_IMPLIB_ERROR)
    elseif(MINGW)
        if(NOT CMAKE_DLLTOOL)
            find_program(SDLx_DLLTOOL NAMES dlltool llvm-dlltool)
            if(NOT SDLx_DLLTOOL)
                message(FATAL_ERROR "dlltool not found; cannot create the SDL import library")
            endif()
        else()
            set(SDLx_DLLTOOL "${CMAKE_DLLTOOL}")
        endif()

        set(SDLx_IMPLIB_TOOL "${SDLx_DLLTOOL}")
        set(SDLx_IMPLIB "${SDLx_GEN_DIR}/lib${SDLx_DLL_NAME}.dll.a")
        execute_process(
            COMMAND "${SDLx_DLLTOOL}" -d "${SDLx_DEF}"
                    -D "${SDLx_DLL_NAME}.dll" -l "${SDLx_IMPLIB}"
            WORKING_DIRECTORY "${SDLx_GEN_DIR}"
            RESULT_VARIABLE SDLx_IMPLIB_RESULT
            OUTPUT_VARIABLE SDLx_IMPLIB_OUTPUT
            ERROR_VARIABLE SDLx_IMPLIB_ERROR)
    endif()

    if(NOT SDLx_IMPLIB_RESULT EQUAL 0 OR NOT EXISTS "${SDLx_IMPLIB}")
        message(FATAL_ERROR
            "Failed to generate SDL import library ${SDLx_IMPLIB} with ${SDLx_IMPLIB_TOOL}\n"
            "${SDLx_IMPLIB_OUTPUT}\n${SDLx_IMPLIB_ERROR}")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${SDLx_DEF}" "${SDLx_DLL_SRC}" "${INTERNAL_SDL_DIR}/versions.cmake")

    set(SDLx_LIBRARIES "${SDLx_IMPLIB}")
    list(APPEND CLIENT_DEPLOY_LIBRARY_RENAMES "${SDLx_DLL_SRC}=>${SDLx_DLL_NAME}.dll")

    message(STATUS "Using bundled SDL${SDL_VERSION} ${SDLx_VERSION} (${SDLx_ARCH})")
else()
    if(USE_SDL3)
        find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
        set(SDLx_LIBRARIES SDL3::SDL3)
    else()
        find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2-shared)
        set(SDLx_LIBRARIES SDL2::SDL2)
    endif()
endif()

if(USE_SDL3)
    list(APPEND CLIENT_DEFINITIONS USE_SDL3)
    list(APPEND RENDERER_DEFINITIONS USE_SDL3)
else()
    list(APPEND CLIENT_DEFINITIONS USE_SDL2)
    list(APPEND RENDERER_DEFINITIONS USE_SDL2)
endif()

list(APPEND CLIENT_LIBRARIES ${SDLx_LIBRARIES})
list(APPEND CLIENT_INCLUDE_DIRS ${SDLx_INCLUDE_DIRS})
#list(APPEND CLIENT_COMPILE_OPTIONS ${SDL2_CFLAGS_OTHER})
