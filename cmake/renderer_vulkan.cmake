if(NOT BUILD_CLIENT OR NOT BUILD_RENDERER_VULKAN)
    return()
endif()

include(utils/arch)
include(utils/set_target_platform_details)
include(renderer_common)

set(RENDERER_VULKAN_SOURCES
	"${SOURCE_DIR}/renderervk/tr_animation_mdm.c"
	"${SOURCE_DIR}/renderervk/tr_animation_mds.c"
	"${SOURCE_DIR}/renderervk/tr_backend.c"
	"${SOURCE_DIR}/renderervk/tr_bsp.c"
	"${SOURCE_DIR}/renderervk/tr_cmds.c"
	"${SOURCE_DIR}/renderervk/tr_cmesh.c"
	"${SOURCE_DIR}/renderervk/tr_common.h"
	"${SOURCE_DIR}/renderervk/tr_curve.c"
	"${SOURCE_DIR}/renderervk/tr_decals.c"
	"${SOURCE_DIR}/renderervk/tr_image.c"
	"${SOURCE_DIR}/renderervk/tr_init.c"
	"${SOURCE_DIR}/renderervk/tr_light.c"
	"${SOURCE_DIR}/renderervk/tr_local.h"
	"${SOURCE_DIR}/renderervk/tr_main.c"
	"${SOURCE_DIR}/renderervk/tr_marks.c"
	"${SOURCE_DIR}/renderervk/tr_mesh.c"
	"${SOURCE_DIR}/renderervk/tr_model_iqm.c"
	"${SOURCE_DIR}/renderervk/tr_model.c"
	"${SOURCE_DIR}/renderervk/tr_scene.c"
	"${SOURCE_DIR}/renderervk/tr_shade_calc.c"
	"${SOURCE_DIR}/renderervk/tr_shade.c"
	"${SOURCE_DIR}/renderervk/tr_shader.c"
	"${SOURCE_DIR}/renderervk/tr_shadows.c"
	"${SOURCE_DIR}/renderervk/tr_sky.c"
	"${SOURCE_DIR}/renderervk/tr_surface.c"
	"${SOURCE_DIR}/renderervk/tr_world.c"
	"${SOURCE_DIR}/renderervk/vk.c"
	"${SOURCE_DIR}/renderervk/vk.h"
	"${SOURCE_DIR}/renderervk/vk_flares.c"
	"${SOURCE_DIR}/renderervk/vk_vbo.c"
)

if(MSVC)
	set(RENDERER_VULKAN_SHADER_SOURCES
		"${SOURCE_DIR}/renderervk/shaders/spirv/shader_data.c"
		"${SOURCE_DIR}/renderervk/shaders/blend.frag"
		"${SOURCE_DIR}/renderervk/shaders/bloom.frag"
		"${SOURCE_DIR}/renderervk/shaders/blur.frag"
		"${SOURCE_DIR}/renderervk/shaders/color.frag"
		"${SOURCE_DIR}/renderervk/shaders/color.vert"
		"${SOURCE_DIR}/renderervk/shaders/dot.frag"
		"${SOURCE_DIR}/renderervk/shaders/dot.vert"
		"${SOURCE_DIR}/renderervk/shaders/fog.frag"
		"${SOURCE_DIR}/renderervk/shaders/fog.vert"
		"${SOURCE_DIR}/renderervk/shaders/gamma.frag"
		"${SOURCE_DIR}/renderervk/shaders/gamma.vert"
		"${SOURCE_DIR}/renderervk/shaders/gen_frag.tmpl"
		"${SOURCE_DIR}/renderervk/shaders/gen_vert.tmpl"
		"${SOURCE_DIR}/renderervk/shaders/light_frag.tmpl"
		"${SOURCE_DIR}/renderervk/shaders/light_vert.tmpl"
	)

	set_property(SOURCE ${RENDERER_VULKAN_SHADER_SOURCES} APPEND PROPERTY VS_SETTINGS "ExcludedFromBuild=true" HEADER_FILE_ONLY ON)
endif()

set(RENDERER_VULKAN_BASENAME ${RENDERER_PREFIX}_vulkan)

#set(RENDERER_VULKAN_TYPE STATIC)

set(RENDERER_VULKAN_BINARY ${RENDERER_VULKAN_BASENAME})

if(USE_ARCHLESS_FILENAMES)
    list(APPEND RENDERER_DEFINITIONS USE_ARCHLESS_FILENAMES)
endif()

list(APPEND RENDERER_VULKAN_BINARY_SOURCES
    ${RENDERER_COMMON_SOURCES}
    ${RENDERER_VULKAN_SOURCES}
    ${RENDERER_VULKAN_SHADER_SOURCES}
    ${RENDERER_LIBRARY_SOURCES})

if(USE_RENDERER_DLOPEN)
    list(APPEND RENDERER_VULKAN_BINARY_SOURCES ${DYNAMIC_RENDERER_SOURCES})
    set(RENDERER_VULKAN_TYPE MODULE)

    add_library(${RENDERER_VULKAN_BINARY} ${RENDERER_VULKAN_TYPE} ${RENDERER_VULKAN_BINARY_SOURCES})

    target_link_libraries(      ${RENDERER_VULKAN_BINARY} PRIVATE c_compiler_opts)
    target_link_libraries(      ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_LIBRARIES})
    target_include_directories( ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_INCLUDE_DIRS})
    target_compile_definitions( ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_DEFINITIONS} RENDERER_VULKAN)
    target_compile_options(     ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_COMPILE_OPTIONS})
    target_link_options(        ${RENDERER_VULKAN_BINARY} PRIVATE ${RENDERER_LINK_OPTIONS})

	set_target_properties(${RENDERER_VULKAN_BINARY} PROPERTIES
		FOLDER Renderers
		LIBRARY_OUTPUT_DIRECTORY "${BASE_DIR_PATH}"
		LIBRARY_OUTPUT_DIRECTORY_DEBUG "${BASE_DIR_PATH}"
		LIBRARY_OUTPUT_DIRECTORY_RELEASE "${BASE_DIR_PATH}")
	set_target_platform_details(${RENDERER_VULKAN_BINARY})

    install(TARGETS ${RENDERER_VULKAN_BINARY} LIBRARY DESTINATION .)
endif()

