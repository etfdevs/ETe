set(PROJECT_NAME ETe)
set(PROJECT_VERSION "2.60e")

set(SERVER_NAME "${PROJECT_NAME}.ded" CACHE STRING "Ded Server Binary Name")
set(CLIENT_NAME "${PROJECT_NAME}" CACHE STRING "Client Binary Name")

string(TOLOWER ${CLIENT_NAME} RENDERER_PREFIX_init)
set(RENDERER_PREFIX "${RENDERER_PREFIX_init}" CACHE STRING "Renderer Prefix")
set(RENDERER_DEFAULT "opengl" CACHE STRING "Default renderer. valid options are opengl, vulkan") # valid options: opengl, vulkan, ~~opengl2~~
set_property(CACHE RENDERER_DEFAULT PROPERTY STRINGS opengl vulkan)

if(NOT RENDERER_DEFAULT STREQUAL "opengl" AND NOT RENDERER_DEFAULT STREQUAL "vulkan")
    set(RENDERER_DEFAULT "opengl" CACHE STRING "Default renderer. valid options are opengl, vulkan") # valid options: opengl, vulkan, ~~opengl2~~
endif()

set(BASEGAME "etmain")
set(DEFAULT_MOD "${BASEGAME}" CACHE STRING "fs_game mod to launch on startup, if differs from base will be added")

set(BASE_DIR "${PROJECT_NAME}")
set(BASE_DIR_PATH "${CMAKE_BINARY_DIR}/${BASE_DIR}")
file(MAKE_DIRECTORY "${BASE_DIR_PATH}")

set(BUNDLED_TARGETS_FOLDER Bundled)
set(PACKING_TARGETS_FOLDER Package)

set(WINDOWS_ICON_PATH ${CMAKE_SOURCE_DIR}/src/win32/wolfet.ico)

set(MACOS_ICON_PATH ${CMAKE_SOURCE_DIR}/src/mac/Icon.icns)
set(MACOS_BUNDLE_ID org.etfdevs.${CLIENT_NAME})

#set(COPYRIGHT "Wolfenstein: Enemy Territory Copyright © 1999-2000 id Software, Inc. All rights reserved.")

set(PROTOCOL_HANDLER_SCHEME et)