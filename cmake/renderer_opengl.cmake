if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_OPENGL)
    return()
endif()

include(utils/arch)
include(utils/set_target_platform_details)
include(renderer_common)

set(RENDERER_OPENGL_SOURCES
	"${SOURCE_DIR}/renderer/tr_animation_mdm.c"
	"${SOURCE_DIR}/renderer/tr_animation_mds.c"
	"${SOURCE_DIR}/renderer/tr_arb.c"
	"${SOURCE_DIR}/renderer/tr_backend.c"
	"${SOURCE_DIR}/renderer/tr_bsp.c"
	"${SOURCE_DIR}/renderer/tr_cmds.c"
	"${SOURCE_DIR}/renderer/tr_cmesh.c"
	"${SOURCE_DIR}/renderer/tr_common.h"
	"${SOURCE_DIR}/renderer/tr_curve.c"
	"${SOURCE_DIR}/renderer/tr_decals.c"
	"${SOURCE_DIR}/renderer/tr_flares.c"
	"${SOURCE_DIR}/renderer/tr_image.c"
	"${SOURCE_DIR}/renderer/tr_init.c"
	"${SOURCE_DIR}/renderer/tr_light.c"
	"${SOURCE_DIR}/renderer/tr_local.h"
	"${SOURCE_DIR}/renderer/tr_main.c"
	"${SOURCE_DIR}/renderer/tr_marks.c"
	"${SOURCE_DIR}/renderer/tr_mesh.c"
	"${SOURCE_DIR}/renderer/tr_model.c"
	"${SOURCE_DIR}/renderer/tr_model_iqm.c"
	"${SOURCE_DIR}/renderer/tr_scene.c"
	"${SOURCE_DIR}/renderer/tr_shade.c"
	"${SOURCE_DIR}/renderer/tr_shade_calc.c"
	"${SOURCE_DIR}/renderer/tr_shader.c"
	"${SOURCE_DIR}/renderer/tr_shadows.c"
	"${SOURCE_DIR}/renderer/tr_sky.c"
	"${SOURCE_DIR}/renderer/tr_surface.c"
	"${SOURCE_DIR}/renderer/tr_vbo.c"
	"${SOURCE_DIR}/renderer/tr_world.c"
)

set(RENDERER_OPENGL_BASENAME ${RENDERER_PREFIX}_opengl)

#set(RENDERER_OPENGL_TYPE STATIC)

set(RENDERER_OPENGL_BINARY ${RENDERER_OPENGL_BASENAME})

if(USE_ARCHLESS_FILENAMES)
    list(APPEND RENDERER_DEFINITIONS USE_ARCHLESS_FILENAMES)
endif()

list(APPEND RENDERER_OPENGL_BINARY_SOURCES
    ${RENDERER_COMMON_SOURCES}
    ${RENDERER_OPENGL_SOURCES}
    ${RENDERER_LIBRARY_SOURCES})

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_OPENGL_BINARY_SOURCES ${DYNAMIC_RENDERER_SOURCES})
    set(RENDERER_OPENGL_TYPE MODULE)

    add_library(${RENDERER_OPENGL_BINARY} ${RENDERER_OPENGL_TYPE} ${RENDERER_OPENGL_BINARY_SOURCES})

    target_link_libraries(      ${RENDERER_OPENGL_BINARY} PRIVATE c_compiler_opts)
    target_link_libraries(      ${RENDERER_OPENGL_BINARY} PRIVATE ${RENDERER_LIBRARIES})
    target_include_directories( ${RENDERER_OPENGL_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions( ${RENDERER_OPENGL_BINARY} PRIVATE ${RENDERER_DEFINITIONS} RENDERER_OPENGL)
    target_compile_options(     ${RENDERER_OPENGL_BINARY} PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_options(        ${RENDERER_OPENGL_BINARY} PRIVATE ${RENDERER_LINK_OPTIONS})

	set_target_properties(${RENDERER_OPENGL_BINARY} PROPERTIES
		FOLDER Renderers
		LIBRARY_OUTPUT_DIRECTORY "${BASE_DIR_PATH}"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${BASE_DIR_PATH}"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${BASE_DIR_PATH}")
	set_target_platform_details(${RENDERER_OPENGL_BINARY})

    install(TARGETS ${RENDERER_OPENGL_BINARY} LIBRARY DESTINATION .)
endif()

