cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(identity)

project(${PROJECT_NAME} LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_SERVER "Build dedicated server" ON)
option(BUILD_CLIENT "Build client" ON)
option(BUILD_RENDERER_OPENGL "Build the Vanilla OpenGL renderer" ON)
option(BUILD_RENDERER_VULKAN "Build the Vulkan renderer" ON)

if(NOT BUILD_RENDERER_OPENGL AND RENDERER_DEFAULT STREQUAL "opengl")
    message(FATAL_ERROR "Cannot default to OpenGL renderer with OpenGL Renderer disabled")
    return()
endif()

if(NOT BUILD_RENDERER_VULKAN AND RENDERER_DEFAULT STREQUAL "vulkan")
    message(FATAL_ERROR "Cannot default to Vulkan renderer with Vulkan Renderer disabled")
    return()
endif()

#option(BUILD_STANDALONE "Build binaries for standalone games" OFF)

option(USE_ARCHLESS_FILENAMES "Don't include the architecture in binary filenames" OFF)
option(USE_RENDERER_DLOPEN "Dynamically load the renderer(s). NOTE: Requires a cache rebuild if changing on active project" ON)

option(USE_SDL2 "SDL2 for client binary" OFF)
option(USE_SDL3 "SDL3 for client binary" ON)
#option(USE_OPENAL "OpenAL audio" OFF)
#option(USE_OPENAL_DLOPEN "Dynamically load OpenAL" OFF)
#option(USE_DISCORD "Add Discord Rich Presence Support" OFF)

option(USE_INTERNAL_LIBS "Use internally packaged libraries" ON)
option(USE_INTERNAL_SDL "Use internal SDL 2 or 3 binary (if available)" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_JPEG "Use internal copy of libjpeg" ${USE_INTERNAL_LIBS})
option(USE_INTERNAL_DISCORD "Use internal copy of discord-rpc" ${USE_INTERNAL_LIBS})

# provide individual options too in case a compiler only supports some sanitizers
option(USE_SANITIZERS "Enable address, leak and undefined behavior sanitizers on supported compilers" OFF)
option(USE_ASAN "Enable address sanitizer on supported compilers" OFF)
option(USE_LSAN "Enable leak sanitizer on supported compilers" OFF)
option(USE_UBSAN "Enable undefined behavior sanitizer on supported compilers" OFF)

# Release build by default, set externally if you want something else
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

include(compilers/all)
include(platforms/all)

include(utils/source_group)
include(utils/setup_sanitizers)

create_compiler_opts(c_compiler_opts WARN 3)
create_compiler_opts(c_compiler_opts_w0 WARN 0)

include(libraries/all)

include(server)
include(renderer_opengl)
include(renderer_vulkan)
include(client)

if (USE_SANITIZERS)
	set(USE_ASAN ON CACHE BOOL "Enable address sanitizer" FORCE)

	# don't enable these on MSVC as they are not supported
	if (NOT CMAKE_C_COMPILER_ID MATCHES "MSVC")
		set(USE_LSAN ON CACHE BOOL "Enable leak sanitizer" FORCE)
		set(USE_UBSAN ON CACHE BOOL "Enable undefined behavior sanitizer" FORCE)
	endif ()
endif ()

if (USE_ASAN OR USE_LSAN OR USE_UBSAN)
	setup_sanitizers()
endif ()
