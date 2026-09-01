include_guard(GLOBAL)

set(RENDERER_COMMON_SOURCES
	"${SOURCE_DIR}/renderercommon/anorms256.h"
	"${SOURCE_DIR}/renderercommon/iqm.h"
	"${SOURCE_DIR}/renderercommon/puff.c"
	"${SOURCE_DIR}/renderercommon/puff.h"
	"${SOURCE_DIR}/renderercommon/renderImage.h"
	"${SOURCE_DIR}/renderercommon/tr_font.c"
	"${SOURCE_DIR}/renderercommon/tr_image_bmp.c"
	#"${SOURCE_DIR}/renderercommon/tr_image_buffer.c"
	"${SOURCE_DIR}/renderercommon/tr_image_jpg.c"
	#"${SOURCE_DIR}/renderercommon/tr_image_load.c"
	"${SOURCE_DIR}/renderercommon/tr_image_pcx.c"
	"${SOURCE_DIR}/renderercommon/tr_image_png.c"
	"${SOURCE_DIR}/renderercommon/tr_image_tga.c"
	"${SOURCE_DIR}/renderercommon/tr_noise.c"
	"${SOURCE_DIR}/renderercommon/tr_public.h"
	"${SOURCE_DIR}/renderercommon/tr_types.h"
)

set(DYNAMIC_RENDERER_SOURCES
    "${SOURCE_DIR}/qcommon/q_shared.c"
    "${SOURCE_DIR}/qcommon/q_shared.h"
    "${SOURCE_DIR}/qcommon/q_math.c"
)

list(APPEND DYNAMIC_RENDERER_SOURCES ${ASM_RENDERER_DLOPEN_SOURCES})

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_DEFINITIONS USE_RENDERER_DLOPEN)
elseif(BUILD_RENDERER_OPENGL AND BUILD_RENDERER_VULKAN)
    message(FATAL_ERROR "Multiple static renderers enabled; choose one")
elseif(NOT BUILD_RENDERER_OPENGL AND NOT BUILD_RENDERER_VULKAN)
    message(FATAL_ERROR "Zero static renderers enabled; choose one")
endif()

list(APPEND RENDERER_LIBRARIES ${COMMON_LIBRARIES})
