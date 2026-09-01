# macOS specific settings

if(NOT APPLE)
    return()
endif()

# setup directories for IDEs that use them (Visual Studio, XCode...)
if(XCODE)
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

    add_custom_target(CMake-Scripts SOURCES ${CMAKE_SOURCES})
    set_target_properties(CMake-Scripts PROPERTIES FOLDER CMake)
endif()

# Including the arch in the filename doesn't really make sense
# on macOS where we're building Universal Binaries
set(USE_ARCHLESS_FILENAMES ON CACHE INTERNAL "")

option(BUILD_MACOS_APP "Deploy as a macOS .app" ON)

enable_language(OBJC)

#list(APPEND SYSTEM_PLATFORM_SOURCES ${SOURCE_DIR}/sys/sys_osx.m)

list(APPEND COMMON_LIBRARIES "-framework Cocoa")
list(APPEND CLIENT_LIBRARIES "-framework IOKit")

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
set(CMAKE_OSX_ARCHITECTURES arm64;x86_64)

if(BUILD_MACOS_APP)
    set(CLIENT_EXECUTABLE_OPTIONS MACOSX_BUNDLE)
    set(POST_CLIENT_CONFIGURE_FUNCTION finish_macos_app)
endif()

function(finish_macos_app)
    get_filename_component(MACOS_ICON_FILE ${MACOS_ICON_PATH} NAME)

    set(MACOS_APP_BUNDLE_NAME ${CLIENT_NAME})
    set(MACOS_APP_EXECUTABLE_NAME ${CLIENT_BINARY})
    set(MACOS_APP_GUI_IDENTIFIER ${MACOS_BUNDLE_ID})
    set(MACOS_APP_ICON_FILE ${MACOS_ICON_FILE})
    set(MACOS_APP_SHORT_VERSION_STRING ${PRODUCT_VERSION})
    set(MACOS_APP_BUNDLE_VERSION ${PRODUCT_VERSION})
    set(MACOS_APP_DEPLOYMENT_TARGET ${CMAKE_OSX_DEPLOYMENT_TARGET})
    set(MACOS_APP_COPYRIGHT ${COPYRIGHT})

    if(PROTOCOL_HANDLER_SCHEME)
        set(MACOS_APP_PLIST_URL_TYPES
        "<key>CFBundleURLTypes</key>
        <array>
            <dict>
                <key>CFBundleURLName</key>
                <string>${MACOS_APP_BUNDLE_NAME}</string>
                <key>CFBundleURLSchemes</key>
                <array>
                    <string>${PROTOCOL_HANDLER_SCHEME}</string>
                </array>
            </dict>
        </array>")
    else()
        set(MACOS_APP_PLIST_URL_TYPES "")
    endif()

    configure_file(${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
        ${CMAKE_BINARY_DIR}/Info.plist @ONLY)

    set_target_properties(${CLIENT_BINARY} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/Info.plist)

    set(RESOURCES_DIR $<TARGET_FILE_DIR:${CLIENT_BINARY}>/../Resources)
    add_custom_command(TARGET ${CLIENT_BINARY} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCES_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${MACOS_ICON_PATH} ${RESOURCES_DIR})

    if(USE_RENDERER_DLOPEN)
        set(MACOS_APP_BINARY_DIR ${CLIENT_BINARY}.app/Contents/MacOS)

        if(BUILD_RENDERER_OPENGL)
            set_output_dirs(${RENDERER_OPENGL_BINARY} SUBDIRECTORY ${MACOS_APP_BINARY_DIR})
        endif()

        if(BUILD_RENDERER_VULKAN)
            set_output_dirs(${RENDERER_VULKAN_BINARY} SUBDIRECTORY ${MACOS_APP_BINARY_DIR})
        endif()
    endif()
endfunction()
