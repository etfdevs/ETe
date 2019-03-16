#!/bin/bash

set -o verbose

sudo apt-get -qq update
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install mesa-common-dev:i386 libxxf86dga-dev:i386 libasound2-dev:i386 libxrandr-dev:i386 libxxf86vm-dev:i386 libbsd-dev:i386
sudo apt-get install p7zip-full

# Build ETe

#export BUILD_CONFIGURATION="Debug"
export BUILD_CONFIGURATION="Release"

cd src
if [[ "$BUILD_CONFIGURATION" == "Release" ]] ; then
 scons BUILD=release || exit 1
elif [[ "$BUILD_CONFIGURATION" == "Debug" ]] ; then
 scons BUILD=debug || exit 1
else
 exit 1
fi

ls -R *.x86

echo "$TRAVIS_TAG"
echo "$BUILD_CONFIGURATION"

7z a "$TRAVIS_BUILD_DIR/ETe-Linux-$TRAVIS_TAG-$BUILD_CONFIGURATION.x86.7z" ete.x86 eteded.x86
cd "$TRAVIS_BUILD_DIR"
7z a "ETe-Linux-$TRAVIS_TAG-$BUILD_CONFIGURATION.x86.7z" docs\*