set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR "x86")

set(COMPILER_PREFIX "i686-w64-mingw32")

# which compilers to use for C and C++
find_program(CMAKE_C_COMPILER NAMES ${COMPILER_PREFIX}-gcc)
set(CMAKE_C_COMPILER ${COMPILER_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${COMPILER_PREFIX}-g++)
set(CMAKE_CXX_COMPILER ${COMPILER_PREFIX}-g++)
find_program(CMAKE_RC_COMPILER NAMES ${COMPILER_PREFIX}-windres)
set(CMAKE_RC_COMPILER ${COMPILER_PREFIX}-windres)
find_program(CMAKE_AR NAMES ${COMPILER_PREFIX}-gcc-ar)
set(CMAKE_AR ${COMPILER_PREFIX}-gcc-ar)

SET(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static" CACHE STRING "executable linker flags" FORCE)
SET(CMAKE_MODULE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static" CACHE STRING "executable linker flags" FORCE)
