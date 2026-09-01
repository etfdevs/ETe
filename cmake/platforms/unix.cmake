# Unix specific settings (this includes macOS and emscripten)

if(NOT UNIX)
    return()
endif()

list(APPEND SYSTEM_PLATFORM_SOURCES
    "${SOURCE_DIR}/unix/unix_main.c"
    "${SOURCE_DIR}/unix/unix_shared.c"
    "${SOURCE_DIR}/unix/linux_signals.c"
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
    list(APPEND CLIENT_PLATFORM_SOURCES 
        "${SOURCE_DIR}/unix/linux_snd.c"
        "${SOURCE_DIR}/unix/x11_dga.c"
        "${SOURCE_DIR}/unix/x11_randr.c"
        "${SOURCE_DIR}/unix/x11_vidmode.c"
    )
    if(BUILD_RENDERER_OPENGL)
        list(APPEND CLIENT_PLATFORM_SOURCES "${SOURCE_DIR}/unix/linux_glimp.c" "${SOURCE_DIR}unix/linux_qgl.c")
    endif()
    if(BUILD_RENDERER_VULKAN)
        list(APPEND CLIENT_PLATFORM_SOURCES "${SOURCE_DIR}/unix/linux_qvk.c")
    endif()
endif()

list(APPEND COMMON_LIBRARIES
    ${CMAKE_DL_LIBS}   # Dynamic loader
    m                  # Math library
)

if(USE_SDL2 or USE_SDL3)
    list(APPEND CLIENT_DEFINITIONS USE_ICON)
endif()
