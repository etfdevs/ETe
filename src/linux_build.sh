#!/bin/sh

# Setups 32-bit build environment on Debian Stretch 64-bit

if ! dpkg-query -l gcc-multilib g++-multilib make cmake > /dev/null ; then
    echo "run: sudo apt install gcc-multilib g++-multilib make cmake"
    exit 1
fi

if ! dpkg --print-foreign-architectures | grep -q i386 ; then
    echo "run: sudo dpkg --add-architecture i386 && sudo apt update"
    exit 1
fi 

if ! dpkg-query -l libglib2.0-dev:i386 libgl1-mesa-dev:i386 libasound2-dev:i386 libpulse-dev:i386 libjpeg-dev:i386 libsdl2-dev:i386 libcurl4-openssl-dev:i386 ; then
    echo "run: sudo apt install libglib2.0-dev:i386 libgl1-mesa-dev:i386 libasound2-dev:i386 libpulse-dev:i386 libjpeg-dev:i386 libsdl2-dev:i386 libcurl4-openssl-dev:i386"
    exit 1
fi

mkdir -p build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DUSE_SDL2=TRUE -DCMAKE_TOOLCHAIN_FILE="../cmake/toolchains/linux-i686.cmake" ..
cmake --build . --config Release -- -j8
cd ..
