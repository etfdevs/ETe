# MSVC compiler specific settings

if(NOT CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    return()
endif()

include(utils/arch)

if(ARCH MATCHES "x86_64")
	enable_language(ASM_MASM)

	set(ASM_SOURCES
		"${SOURCE_DIR}/asm/common_x64.asm"
	)

	set(ASM_RENDERER_DLOPEN_SOURCES
		"${SOURCE_DIR}/asm/common_x64.asm"
	)

	set(ASM_CLIENT_SOURCES
		"${SOURCE_DIR}/asm/snd_mix_x64.asm"
	)
endif()

# provide options for Visual Studio to automatically setup debugger paths
set(ET_PATH "" CACHE PATH "Path to ET installation used during development")
set(ET_MOD "${BASEGAME}" CACHE STRING "Mod to startup used during development")
if(BUILD_CLIENT)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CLIENT_NAME})
elseif(BUILD_SERVER)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${SERVER_NAME})
endif()

# setup directories for IDEs that use them (Visual Studio, XCode...)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER CMake)

set(CMAKE_FILES_DIR ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_SOURCES
	"${CMAKE_FILES_DIR}/compilers/all.cmake"
	"${CMAKE_FILES_DIR}/compilers/clang.cmake"
	"${CMAKE_FILES_DIR}/compilers/gcc.cmake"
	"${CMAKE_FILES_DIR}/compilers/gnu.cmake"
	"${CMAKE_FILES_DIR}/compilers/msvc.cmake"
	"${CMAKE_FILES_DIR}/libraries/all.cmake"
	"${CMAKE_FILES_DIR}/libraries/curl.cmake"
	#"${CMAKE_FILES_DIR}/libraries/discord.cmake"
	"${CMAKE_FILES_DIR}/libraries/jpeg.cmake"
	#"${CMAKE_FILES_DIR}/libraries/openal.cmake"
	"${CMAKE_FILES_DIR}/libraries/sdl.cmake"
	"${CMAKE_FILES_DIR}/platforms/all.cmake"
	#"${CMAKE_FILES_DIR}/platforms/emscripten.cmake"
	"${CMAKE_FILES_DIR}/platforms/linux.cmake"
	"${CMAKE_FILES_DIR}/platforms/macos.cmake"
	"${CMAKE_FILES_DIR}/platforms/unix.cmake"
	"${CMAKE_FILES_DIR}/platforms/windows.cmake"
	"${CMAKE_FILES_DIR}/utils/arch.cmake"
	"${CMAKE_FILES_DIR}/utils/set_target_platform_details.cmake"
	"${CMAKE_FILES_DIR}/utils/setup_sanitizers.cmake"
	"${CMAKE_FILES_DIR}/utils/source_group.cmake"
	"${CMAKE_FILES_DIR}/client.cmake"
	"${CMAKE_FILES_DIR}/identity.cmake"
	"${CMAKE_FILES_DIR}/renderer_common.cmake"
	"${CMAKE_FILES_DIR}/renderer_opengl.cmake"
	"${CMAKE_FILES_DIR}/renderer_vulkan.cmake"
	"${CMAKE_FILES_DIR}/server.cmake"
	"${CMAKE_FILES_DIR}/shared_sources.cmake"
)

source_group(TREE ${CMAKE_FILES_DIR} FILES ${CMAKE_SOURCES})

add_custom_target(CMake-Scripts SOURCES ${CMAKE_SOURCES})
set_property(SOURCE ${CMAKE_SOURCES} APPEND PROPERTY VS_SETTINGS "ExcludedFromBuild=true")
set_target_properties(CMake-Scripts PROPERTIES FOLDER CMake EXCLUDE_FROM_ALL TRUE EXCLUDE_FROM_DEFAULT_BUILD TRUE)

function(create_compiler_opts target)
	set(WARN_LEVEL 3)

	# parse named arguments
	set(options "")
	set(args WARN)
	set(list_args DEFINE)
	cmake_parse_arguments(PARSE_ARGV 1 arg "${options}" "${args}" "${list_args}")

	# todo validate
	if (NOT ${arg_WARN} STREQUAL "")
		set(WARN_LEVEL ${arg_WARN})	
	endif()

	# MSVC flags
	set(MSVC_LINK_FLAGS 
		$<$<CONFIG:Release>:
			/LTCG>) # perform link time optimizations

	set(MSVC_C_FLAGS
		/wd4068                # ignore GCC pragmas
		/wd4267                # ignore integer narrowing warnings
		/wd4250                # ignore function hiding with virtual
		/wd4714                # ignore function marked as __forceinline not inlined
		/EHsc                  # standard C++ exception handling
		/MP                    # build with Multiple Processes
		$<IF:$<STREQUAL:${WARN_LEVEL},0>,/W0,/W${WARN_LEVEL}>
		$<$<CONFIG:Release>:
			/MT                # use static runtime
			/O2                # max optimizations
			/GL                # full exe/dll optimization
			/Gy                # generate useful information for optimizer
			/Ob2               # let compiler inline freely
			/fp:fast           # fast floating point math
			/FC>               # full path of source code file in diagnostics (/Zi in debug implies this)
		$<$<CONFIG:Debug>:
			/Ob0               # no inlining
			/Od                # no optimizations
			/Zi                # generate complete debug information
			/MDd               # link against dynamic runtime 
			/RTC1>)            # run-time checking

	add_library(${target} INTERFACE)

	# we use this to print out relative path to a source code file for __FILE__ macro replacement
	string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
	target_compile_definitions(${target} INTERFACE "SOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

	target_compile_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:C>:${MSVC_C_FLAGS}>)
	target_link_options(${target} INTERFACE $<$<COMPILE_LANGUAGE:C>:${MSVC_LINK_FLAGS}>)
	
	target_compile_definitions(${target} INTERFACE 
		$<$<CONFIG:Release>:NDEBUG>
		$<$<CONFIG:Debug>:_DEBUG>
		#WIN32_LEAN_AND_MEAN
		_CRT_SECURE_NO_DEPRECATE
		_CRT_SECURE_NO_WARNINGS
		_CRT_NONSTDC_NO_WARNING
		_SCL_SECURE_NO_WARNINGS
		${arg_DEFINE})

	target_compile_features(${target} INTERFACE c_std_11)
endfunction()
