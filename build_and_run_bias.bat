@echo off
REM ===========================================================================
REM  build_and_run_bias.bat - build (if needed) and launch the BIAS GUI
REM
REM  Double-clickable: on any failure it prints why and pauses so the window
REM  stays open. Pass -n / --no-build to skip the build and just launch.
REM
REM  Toolchain: MSYS2 ucrt64 (gcc, cmake, Qt5, OpenCV 4.13)
REM  Backend  : Lucid Arena (with_arena=ON, with_spin=OFF)
REM
REM  If your MSYS2 or Arena SDK lives elsewhere, edit the three paths below.
REM ===========================================================================

setlocal EnableDelayedExpansion

set "REPO=%~dp0"
set "MINGW_BIN=C:\msys64\ucrt64\bin"
set "QT_PLUGINS=C:\msys64\ucrt64\share\qt5\plugins\platforms"
set "ARENA_BIN=C:\Program Files\Lucid Vision Labs\Arena SDK\x64Release"

REM Honor the Arena SDK env var if the SDK lives somewhere else
if defined LUCID_DEV_ROOT set "ARENA_BIN=%LUCID_DEV_ROOT%\x64Release"

set "BUILD_DIR=%REPO%build_ucrt"
set "EXE=%BUILD_DIR%\test_gui.exe"

set "SKIP_BUILD="
if /i "%~1"=="-n"         set "SKIP_BUILD=1"
if /i "%~1"=="--no-build" set "SKIP_BUILD=1"

REM ---------------------------------------------------------------- sanity ---
if not exist "%MINGW_BIN%\Qt5Core.dll" (
    echo [ERROR] MSYS2 ucrt64 runtime not found:
    echo         %MINGW_BIN%
    echo         Expected Qt5Core.dll there.
    goto :fail
)
if not exist "%ARENA_BIN%\ArenaC_v140.dll" (
    echo [ERROR] Arena SDK runtime not found:
    echo         %ARENA_BIN%
    echo         Expected ArenaC_v140.dll there.
    goto :fail
)

set "PATH=%MINGW_BIN%;%ARENA_BIN%;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%QT_PLUGINS%"

if defined SKIP_BUILD goto :launch

REM ----------------------------------------------------------------- build ---
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

if not exist "%BUILD_DIR%\Makefile" (
    echo === Configuring ^(no Makefile yet^) ...
    pushd "%BUILD_DIR%"
    cmake -G "MinGW Makefiles" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
          -DCMAKE_BUILD_TYPE=Release ^
          -DCMAKE_PREFIX_PATH="C:/msys64/ucrt64" ^
          -Dwith_spin=OFF -Dwith_arena=ON -Dwith_qt_gui=ON ..
    set "RC=!ERRORLEVEL!"
    popd
    if not "!RC!"=="0" (
        echo.
        echo [ERROR] cmake configure failed ^(exit !RC!^).
        goto :fail
    )
)

echo === Building ...
pushd "%BUILD_DIR%"
mingw32-make -j8
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" (
    echo.
    echo [ERROR] Build failed ^(exit !RC!^). See the compiler output above.
    goto :fail
)
echo === Build OK

REM ---------------------------------------------------------------- launch ---
:launch
if not exist "%EXE%" (
    echo [ERROR] Executable not found:
    echo         %EXE%
    echo         Re-run this script without -n to build it.
    goto :fail
)

echo.
echo === Launching BIAS ...
echo     exe   : %EXE%
echo     arena : %ARENA_BIN%
echo.

"%EXE%" %*
set "RC=%ERRORLEVEL%"

if not "%RC%"=="0" (
    echo.
    echo [WARN] BIAS exited with code %RC%
    if "%RC%"=="-1073741515" echo        0xC0000135 STATUS_DLL_NOT_FOUND - a DLL failed to resolve
    if "%RC%"=="-1073741819" echo        0xC0000005 STATUS_ACCESS_VIOLATION - a real crash, not a DLL issue
    goto :fail
)

endlocal
exit /b 0

:fail
echo.
pause
endlocal
exit /b 1
