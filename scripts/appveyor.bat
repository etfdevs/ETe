cd src

msbuild "wolf.sln" /m /p:PlatformToolset="v141_xp" /p:Configuration="%BUILD_CONFIGURATION%"

IF ERRORLEVEL 1 GOTO ERROR

cd %APPVEYOR_BUILD_FOLDER%/src/%BUILD_CONFIGURATION%
7z a "%APPVEYOR_BUILD_FOLDER%/wolfet-2.60e-%APPVEYOR_REPO_COMMIT:~0,10%-%BUILD_CONFIGURATION%-win32.zip" ETe.exe
cd %APPVEYOR_BUILD_FOLDER%/src/ded/%BUILD_CONFIGURATION%
7z a "%APPVEYOR_BUILD_FOLDER%/wolfet-2.60e-%APPVEYOR_REPO_COMMIT:~0,10%-%BUILD_CONFIGURATION%-win32.zip" ETe-ded.exe
cd %APPVEYOR_BUILD_FOLDER%
7z a "%APPVEYOR_BUILD_FOLDER%/wolfet-2.60e-%APPVEYOR_REPO_COMMIT:~0,10%-%BUILD_CONFIGURATION%-win32.zip" docs\*

IF ERRORLEVEL 1 GOTO ERROR

GOTO END

:ERROR

echo "Building ETe failed"

:END
