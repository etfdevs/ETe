# Linux-like specific settings, i.e. including FreeBSD etc.

if(NOT UNIX OR APPLE)
    return()
endif()

if(USE_SDL2 or USE_SDL3)
    list(APPEND CLIENT_DEFINITIONS USE_ICON)
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE /opt/enemy-territory)
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})