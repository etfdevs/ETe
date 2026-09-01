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

if(USE_SDL3)
set(SDL_VERSION 3)
else()
set(SDL_VERSION 2)
endif()

if(USE_INTERNAL_SDL AND HAVE_INTERNAL_SDL)
    set(SDLx_INCLUDE_DIRS ${INTERNAL_SDL_DIR}/include)
    #list(APPEND CLIENT_DEFINITIONS USE_INTERNAL_SDL_HEADERS)
    #list(APPEND RENDERER_DEFINITIONS USE_INTERNAL_SDL_HEADERS)

    if(WIN32)
        if(MSVC)
            set(SDLx_COMPILER_DIR vs2017)
        elseif(MINGW)
            set(SDLx_COMPILER_DIR mingw)
        else()
        endif()

        if(ARCH STREQUAL "x86_64")
            set(LIB_DIR ${INTERNAL_SDL_DIR}/windows/${SDLx_COMPILER_DIR}/lib64)
            set(LIB_SUFFIX 64)
        elseif(ARCH STREQUAL "x86")
            set(LIB_DIR ${INTERNAL_SDL_DIR}/windows/${SDLx_COMPILER_DIR}/lib32)
            set(LIB_SUFFIX )
        else()
            message(FATAL_ERROR "Unknown ARCH")
        endif()

        if(MINGW)
            set(SDLx_LIBRARIES
                ${LIB_DIR}/libSDL${SDL_VERSION}${LIB_SUFFIX}.dll.a)
        elseif(MSVC)
            set(SDLx_LIBRARIES
                ${LIB_DIR}/SDL${SDL_VERSION}${LIB_SUFFIX}.lib)
        endif()

        list(APPEND CLIENT_DEPLOY_LIBRARIES ${LIB_DIR}/SDL${SDL_VERSION}${LIB_SUFFIX}.dll)
    #elseif(APPLE)
    #    set(SDLx_LIBRARIES
    #        ${SOURCE_DIR}/thirdparty/libs/macos/libSDL2main.a
    #        ${SOURCE_DIR}/thirdparty/libs/macos/libSDL2-2.0.0.dylib)
    #    list(APPEND CLIENT_DEPLOY_LIBRARIES
    #        ${SOURCE_DIR}/thirdparty/libs/macos/libSDL2-2.0.0.dylib)
    else()
        message(FATAL_ERROR "HAVE_INTERNAL_SDL set incorrectly; file a bug")
    endif()
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