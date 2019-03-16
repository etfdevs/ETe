#!/bin/bash

set -o verbose

#export BUILD_CONFIGURATION "Debug"
#export BUILD_CONFIG_FOLDER "debug"
export BUILD_CONFIGURATION "Release"
export BUILD_CONFIG_FOLDER "release"

sudo apt-get -qq update
sudo apt-get install gcc-multilib g++-multilib
sudo apt-get install mesa-common-dev:i386 libxxf86dga-dev:i386 libasound2-dev:i386 libxrandr-dev:i386 libxxf86vm-dev:i386 libbsd-dev:i386
sudo apt-get install p7zip-full

# Build ETe

cd src
scons || exit 1

ls -R *.x86

#7z a "ETe-Linux-$TRAVIS_TAG-$BUILD_CONFIGURATION.x86.7z" ete.x86 eteded.x86
#cd $TRAVIS_BUILD_DIR
#7z a "ETe-Linux-$TRAVIS_TAG-$BUILD_CONFIGURATION.x86.7z" docs\*