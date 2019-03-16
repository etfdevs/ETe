cd src

msbuild "wolf.sln" /m /p:PlatformToolset="v141_xp" /p:Configuration="%BUILD_CONFIGURATION%"

IF ERRORLEVEL 1 GOTO ERROR

7z a ../ETe-Win32-%ETE_VERSION%.7z "%BUILD_CONFIGURATION%/ETe.exe"
7z a ../ETe-Win32-%ETE_VERSION%.7z "ded/%BUILD_CONFIGURATION%/ETe-ded.exe"
7z a ../ETe-Win32-%ETE_VERSION%.7z ../docs\

IF ERRORLEVEL 1 GOTO ERROR

GOTO END

:ERROR

echo "Building ETe failed"

:END
