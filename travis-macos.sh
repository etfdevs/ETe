#!/bin/bash

set -o verbose

brew update
brew install scons p7zip

# Build ETe

#export BUILD_CONFIGURATION="Debug"
export BUILD_CONFIGURATION="Release"

cd src/mac
if [[ "$BUILD_CONFIGURATION" == "Release" ]] ; then
 xcodebuild -project WolfET.xcodeproj -configuration "Release"
 #scons BUILD=release || exit 1
elif [[ "$BUILD_CONFIGURATION" == "Debug" ]] ; then
 xcodebuild -project WolfET.xcodeproj -configuration "Debug"
 #scons BUILD=debug || exit 1
else
 exit 1
fi

ls -R *.app

echo "$TRAVIS_TAG"
echo "$BUILD_CONFIGURATION"

#7z a "$TRAVIS_BUILD_DIR/ETe-Linux-$TRAVIS_TAG-$BUILD_CONFIGURATION.x86.7z" ete.x86 eteded.x86
#cd "$TRAVIS_BUILD_DIR"
#7z a "ETe-Linux-$TRAVIS_TAG-$BUILD_CONFIGURATION.x86.7z" docs\*