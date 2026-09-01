if(NOT BUILD_CLIENT)
    return()
endif()

include(utils/arch)
include(utils/set_target_platform_details)
include(shared_sources)

include(renderer_common)

set(CLIENT_SOURCES
	"${SOURCE_DIR}/client/cl_avi.c"
	"${SOURCE_DIR}/client/cl_cgame.c"
	"${SOURCE_DIR}/client/cl_cin.c"
	"${SOURCE_DIR}/client/cl_console.c"
	"${SOURCE_DIR}/client/cl_curl.c"
	"${SOURCE_DIR}/client/cl_curl.h"
	"${SOURCE_DIR}/client/cl_input.c"
	"${SOURCE_DIR}/client/cl_jpeg.c"
	"${SOURCE_DIR}/client/cl_keys.c"
	"${SOURCE_DIR}/client/cl_main.c"
	"${SOURCE_DIR}/client/cl_net_chan.c"
	"${SOURCE_DIR}/client/cl_parse.c"
	"${SOURCE_DIR}/client/cl_scrn.c"
	"${SOURCE_DIR}/client/cl_tc_vis.c"
	"${SOURCE_DIR}/client/cl_ui.c"
	"${SOURCE_DIR}/client/client.h"
	"${SOURCE_DIR}/client/keys.h"
	"${SOURCE_DIR}/client/keycodes.h"
	"${SOURCE_DIR}/client/snd_adpcm.c"
	"${SOURCE_DIR}/client/snd_codec.c"
	"${SOURCE_DIR}/client/snd_codec.h"
	"${SOURCE_DIR}/client/snd_codec_wav.c"
	"${SOURCE_DIR}/client/snd_dma.c"
	"${SOURCE_DIR}/client/snd_local.h"
	"${SOURCE_DIR}/client/snd_main.c"
	"${SOURCE_DIR}/client/snd_mem.c"
	"${SOURCE_DIR}/client/snd_mix.c"
	"${SOURCE_DIR}/client/snd_public.h"
	"${SOURCE_DIR}/client/snd_wavelet.c"
	"${SOURCE_DIR}/cgame/cg_public.h"
	"${SOURCE_DIR}/ui/ui_public.h"
    ${CLIENT_PLATFORM_SOURCES}
)

if(USE_DISCORD)
	list(APPEND CLIENT_SOURCES "${SOURCE_DIR}/client/cl_discord.c")
endif()

if(USE_OPENAL)
	list(APPEND CLIENT_SOURCES "${SOURCE_DIR}/client/snd_openal.c")
endif()

set(CLIENT_BINARY ${CLIENT_NAME})

if(USE_ARCHLESS_FILENAMES)
    list(APPEND CLIENT_DEFINITIONS USE_ARCHLESS_FILENAMES)
endif()

if(USE_RENDERER_DLOPEN)
    list(APPEND CLIENT_DEFINITIONS USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})
endif()

if(DEFAULT_MOD AND NOT DEFAULT_MOD STREQUAL ${BASEGAME})
    list(APPEND CLIENT_DEFINITIONS DEFAULT_GAME="${DEFAULT_MOD}")
endif()

if(BUILD_RENDERER_OPENGL)
    list(APPEND CLIENT_DEFINITIONS USE_OPENGL_API)
	if(NOT USE_RENDERER_DLOPEN)
		list(APPEND CLIENT_DEFINITIONS RENDERER_OPENGL)
	endif()
endif()

if(BUILD_RENDERER_VULKAN)
    list(APPEND CLIENT_DEFINITIONS USE_VULKAN_API)
	if(NOT USE_RENDERER_DLOPEN)
		list(APPEND CLIENT_DEFINITIONS RENDERER_VULKAN)
	endif()
endif()

list(APPEND CLIENT_DEFINITIONS USE_JOYSTICK)

list(APPEND CLIENT_BINARY_SOURCES
    ${SERVER_SOURCES}
    ${CLIENT_SOURCES}
    ${COMMON_SOURCES}
    ${SYSTEM_SOURCES}
    ${ASM_SOURCES}
    ${ASM_CLIENT_SOURCES}
)

add_executable(${CLIENT_BINARY} ${CLIENT_EXECUTABLE_OPTIONS} ${CLIENT_BINARY_SOURCES})

target_link_libraries(${CLIENT_BINARY} PRIVATE c_compiler_opts)

target_include_directories(     ${CLIENT_BINARY} PRIVATE ${CLIENT_INCLUDE_DIRS})
target_compile_definitions(     ${CLIENT_BINARY} PRIVATE ${CLIENT_DEFINITIONS})
target_compile_options(         ${CLIENT_BINARY} PRIVATE ${CLIENT_COMPILE_OPTIONS})
target_link_libraries(          ${CLIENT_BINARY} PRIVATE ${COMMON_LIBRARIES} ${CLIENT_LIBRARIES})
target_link_options(            ${CLIENT_BINARY} PRIVATE ${CLIENT_LINK_OPTIONS})

set_target_properties(${CLIENT_BINARY} PROPERTIES
	FOLDER Engine
	RUNTIME_OUTPUT_DIRECTORY "${BASE_DIR_PATH}"
	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BASE_DIR_PATH}"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BASE_DIR_PATH}")
set_target_platform_details(${CLIENT_BINARY})

if (CMAKE_GENERATOR MATCHES "Visual Studio")
	get_target_property(CLIENT_EXE ${CLIENT_BINARY} OUTPUT_NAME)
	set_target_properties(${CLIENT_BINARY} PROPERTIES
			VS_DEBUGGER_COMMAND "${ET_PATH}\\${CLIENT_EXE}.exe"
			VS_DEBUGGER_COMMAND_ARGUMENTS "+set fs_basepath . +set fs_homepath \"${ET_PATH}\" +set fs_game ${ET_MOD}"
			VS_DEBUGGER_WORKING_DIRECTORY "$(SolutionDir)"
	)
endif ()

if(NOT USE_RENDERER_DLOPEN)
    target_sources(${CLIENT_BINARY} PRIVATE
        # These are never simultaneously populated
        ${RENDERER_OPENGL_BINARY_SOURCES}
        ${RENDERER_VULKAN_BINARY_SOURCES})
endif()

foreach(LIBRARY IN LISTS CLIENT_DEPLOY_LIBRARIES)
    add_custom_command(TARGET ${CLIENT_BINARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${LIBRARY}
            $<TARGET_FILE_DIR:${CLIENT_BINARY}>)
endforeach()

if(POST_CLIENT_CONFIGURE_FUNCTION)
    cmake_language(CALL ${POST_CLIENT_CONFIGURE_FUNCTION})
endif()

install(TARGETS ${CLIENT_BINARY}
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .)
