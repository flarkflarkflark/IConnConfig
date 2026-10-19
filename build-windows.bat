@echo off
echo Building IConnConfig for Windows...

REM Check if PKControls exists
if not exist "..\PKControls" (
    echo Cloning PKControls dependency...
    git clone https://codeberg.org/dehnhardt/PKControls.git ..\PKControls
)

REM Build with qmake
echo Running qmake...
qmake iConnConfig.pro

REM Build with make/nmake
echo Building...
if exist "Makefile" (
    make
) else if exist "Makefile.Debug" (
    nmake debug
) else (
    echo Error: No Makefile found
    pause
    exit /b 1
)

echo Build complete!
echo Executable should be in debug/ or release/ folder
pause