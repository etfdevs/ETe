cd src

msbuild "wolf.sln" /m /p:PlatformToolset="v141_xp" /p:Configuration="%BUILD_CONFIGURATION%"

IF ERRORLEVEL 1 GOTO ERROR

set buildyear=%date:~10,4%
set buildmonth=%date:~4,2%
set buildday=%date:~7,2%
set build_date=%buildyear%%buildmonth%%buildday%

cd %APPVEYOR_BUILD_FOLDER%/src/%BUILD_CONFIGURATION%
7z a "%APPVEYOR_BUILD_FOLDER%/wolfet-2.60e-%build_date%-%BUILD_CONFIGURATION%-win32.zip" ETe.exe
cd %APPVEYOR_BUILD_FOLDER%/src/ded/%BUILD_CONFIGURATION%
7z a "%APPVEYOR_BUILD_FOLDER%/wolfet-2.60e-%build_date%-%BUILD_CONFIGURATION%-win32.zip" ETe.ded.exe
cd %APPVEYOR_BUILD_FOLDER%
7z a "%APPVEYOR_BUILD_FOLDER%/wolfet-2.60e-%build_date%-%BUILD_CONFIGURATION%-win32.zip" docs\*

IF ERRORLEVEL 1 GOTO ERROR

GOTO END

:ERROR

echo "Building ETe failed"

:END
