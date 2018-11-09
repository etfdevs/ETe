#!/bin/sh

# Setups 32-bit build environment on Debian Stretch 64-bit

if ! dpkg-query -l gcc-multilib g++-multilib make scons > /dev/null ; then
    echo "run: sudo apt install gcc-multilib g++-multilib make scons"
    exit 1
fi

if ! dpkg --print-foreign-architectures | grep -q i386 ; then
    echo "run: sudo dpkg --add-architecture i386 && sudo apt update"
    exit 1
fi 

if ! dpkg-query -l mesa-common-dev:i386 libxxf86dga-dev:i386 libasound2-dev:i386 libxrandr-dev:i386 libxxf86vm-dev:i386 libbsd-dev:i386 > /dev/null ; then
    echo "run: sudo apt install mesa-common-dev:i386 libxxf86dga-dev:i386 libasound2-dev:i386 libxrandr-dev:i386 libxxf86vm-dev:i386 libbsd-dev:i386"
    exit 1
fi

scons
