# Windows specific settings

if(NOT WIN32)
    return()
endif()

# Disable SDL support for now
set(USE_SDL2 OFF CACHE BOOL "SDL2 for client binary" FORCE)
set(USE_SDL3 OFF CACHE BOOL "SDL3 for client binary" FORCE)

list(APPEND SYSTEM_PLATFORM_SOURCES
    "${SOURCE_DIR}/win32/win_dpi.c"
    "${SOURCE_DIR}/win32/win_local.h"
    "${SOURCE_DIR}/win32/win_main.c"
    "${SOURCE_DIR}/win32/win_shared.c"
    "${SOURCE_DIR}/win32/win_syscon.c"
    "${SOURCE_DIR}/win32/winquake.rc"
)
if(USE_SDL2)
    list(APPEND CLIENT_PLATFORM_SOURCES 
        "${SOURCE_DIR}/sdl/sdl_gamma.c"
        "${SOURCE_DIR}/sdl/sdl_glimp.c"
        "${SOURCE_DIR}/sdl/sdl_glw.h"
        "${SOURCE_DIR}/sdl/sdl_icon.h"
        "${SOURCE_DIR}/sdl/sdl_input.c"
        "${SOURCE_DIR}/sdl/sdl_local.h"
        "${SOURCE_DIR}/sdl/sdl_snd.c"
        "${SOURCE_DIR}/sdl/sdl_version.c"
    )
elseif(USE_SDL3)
    list(APPEND CLIENT_PLATFORM_SOURCES 
        "${SOURCE_DIR}/sdl3/sdl_gamma.c"
        "${SOURCE_DIR}/sdl3/sdl_glimp.c"
        "${SOURCE_DIR}/sdl3/sdl_glw.h"
        "${SOURCE_DIR}/sdl3/sdl_icon.h"
        "${SOURCE_DIR}/sdl3/sdl_input.c"
        "${SOURCE_DIR}/sdl3/sdl_local.h"
        "${SOURCE_DIR}/sdl3/sdl_snd.c"
        "${SOURCE_DIR}/sdl3/sdl_version.c"
    )
else()
    option(USE_WASAPI "Use WASAPI" OFF)
    if(USE_WASAPI)
        list(APPEND CLIENT_DEFINITIONS USE_WASAPI=1)
    else()
        list(APPEND CLIENT_DEFINITIONS USE_WASAPI=0)
    endif()
    list(APPEND CLIENT_PLATFORM_SOURCES 
        "${SOURCE_DIR}/win32/win_gamma.c"
        "${SOURCE_DIR}/win32/win_input.c"
        "${SOURCE_DIR}/win32/win_minimize.c"
        "${SOURCE_DIR}/win32/win_snd.c"
        "${SOURCE_DIR}/win32/win_wndproc.c"
    )
    if(BUILD_RENDERER_OPENGL)
        list(APPEND CLIENT_PLATFORM_SOURCES "${SOURCE_DIR}/win32/win_qgl.c")
    endif()
    if(BUILD_RENDERER_VULKAN)
        list(APPEND CLIENT_PLATFORM_SOURCES "${SOURCE_DIR}/win32/win_qvk.c")
    endif()
    if(BUILD_RENDERER_OPENGL OR BUILD_RENDERER_VULKAN)
        list(APPEND CLIENT_PLATFORM_SOURCES "${SOURCE_DIR}/win32/win_glimp.c")
    endif()
endif()

list(APPEND COMMON_LIBRARIES
    winmm     # timeBeginPeriod/timeEndPeriod
    comctl32  # Windows Common Controls for viewlog screen
    wsock32   # Windows Sockets
    ws2_32    # Windows Sockets 2
    Crypt32   # Crypto Functions
    Iphlpapi
    Secur32
) #psapi)

if(MINGW)
    list(APPEND COMMON_LIBRARIES mingw32)
endif()

if(USE_SDL2 OR USE_SDL3)
    list(APPEND CLIENT_DEFINITIONS USE_ICON)
endif()

list(APPEND CLIENT_DEFINITIONS USE_WIN32_ASM) # for snd_mix

#if(MSVC)
#    # We have our own manifest, disable auto creation
#    list(APPEND SERVER_LINK_OPTIONS "/MANIFEST:NO")
#    list(APPEND CLIENT_LINK_OPTIONS "/MANIFEST:NO")
#endif()

set(CLIENT_EXECUTABLE_OPTIONS WIN32)
set(SERVER_EXECUTABLE_OPTIONS WIN32) # because we support the viewlog dedicated server we also still need to be a Win32 gui application
