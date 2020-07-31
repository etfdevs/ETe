#!/bin/sh

if [ -e "/etc/arch-release" ] || [ -e "/etc/manjaro-release" ]; then
    # Setups 32-bit build environment on Arch/Manjaro 64-bit

    echo "Arch/Manjaro Detected"

    if ! pacman -Qi gcc make cmake > /dev/null 2>&1 ; then
        echo "run: sudo pacman -S gcc make cmake"
        exit 1
    fi

    if ! pacman -Qi lib32-alsa-lib lib32-libpulse lib32-jack lib32-sdl2 lib32-libjpeg-turbo lib32-curl > /dev/null 2>&1 ; then
        echo "run: sudo pacman -S lib32-alsa-lib lib32-libpulse lib32-jack lib32-sdl2 lib32-libjpeg-turbo lib32-curl"
    fi
elif [ -e "/etc/debian_version" ] || [ -e "/etc/debian_release" ] || [ -e "/etc/ubuntu-release" ] ; then
    # Setups 32-bit build environment on Debian 64-bit

    echo "Debian/Ubuntu Detected"

    if ! dpkg-query -l gcc-multilib g++-multilib make cmake > /dev/null 2>&1 ; then
        echo "run: sudo apt install gcc-multilib g++-multilib make cmake"
        exit 1
    fi

    if ! dpkg --print-foreign-architectures | grep -q i386 ; then
        echo "run: sudo dpkg --add-architecture i386 && sudo apt update"
        exit 1
    fi 

    if ! dpkg-query -l libglib2.0-dev:i386 libgl1-mesa-dev:i386 libasound2-dev:i386 libpulse-dev:i386 libjpeg-dev:i386 libsdl2-dev:i386 libcurl4-openssl-dev:i386 > /dev/null 2>&1 ; then
        echo "run: sudo apt install libglib2.0-dev:i386 libgl1-mesa-dev:i386 libasound2-dev:i386 libpulse-dev:i386 libjpeg-dev:i386 libsdl2-dev:i386 libcurl4-openssl-dev:i386"
        exit 1
    fi
else
    echo "Unsupported Linux Distro detected. Manually install 32-bit SDL2+deps and sound deps, libjpeg-turbo, libcurl with SSL then run the necessary cmake steps below in script"
    exit 1
fi

set -ex

mkdir -p build
cd build
if ! [ -e "CMakeCache.txt" ]; then
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DUSE_SDL2=TRUE -DCMAKE_TOOLCHAIN_FILE="../cmake/toolchains/linux-i686.cmake" ..
fi
cmake --build . --config Release -- -j8
cd ..
