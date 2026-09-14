@echo off
REM ===========================================================================
REM  build_and_run_bias.bat - build (if needed) and launch the BIAS GUI
REM
REM  Usage:
REM     build_and_run_bias.bat                 Lucid Arena backend (default)
REM     build_and_run_bias.bat --spin          FLIR/Teledyne Spinnaker backend
REM     build_and_run_bias.bat -n              skip the build, just launch
REM     build_and_run_bias.bat --spin -n       launch the Spinnaker build
REM     build_and_run_bias.bat --help          show this help
REM
REM  Any other arguments are passed through to test_gui.exe.
REM
REM  Double-clickable: on failure it explains why and pauses so the window
REM  stays open.
REM
REM  Prerequisites (one-time) - see docs\building-on-windows.md:
REM    * MSYS2 ucrt64 toolchain (gcc, cmake, make, Qt5, OpenCV)
REM    * the camera SDK for your backend (Arena, or Spinnaker)
REM ===========================================================================

setlocal EnableDelayedExpansion

set "REPO=%~dp0"
set "BACKEND=arena"
set "SKIP_BUILD="
set "EXTRA="

REM ------------------------------------------------------------ arguments ---
:parse
if "%~1"=="" goto :parsed
if /i "%~1"=="--spin" (
    set "BACKEND=spin"
    shift
    goto :parse
)
if /i "%~1"=="--arena" (
    set "BACKEND=arena"
    shift
    goto :parse
)
if /i "%~1"=="-n" (
    set "SKIP_BUILD=1"
    shift
    goto :parse
)
if /i "%~1"=="--no-build" (
    set "SKIP_BUILD=1"
    shift
    goto :parse
)
if /i "%~1"=="-h"     goto :usage
if /i "%~1"=="--help" goto :usage
set "EXTRA=!EXTRA! %1"
shift
goto :parse
:parsed

REM ------------------------------------------------- locate MSYS2 ucrt64 ---
set "UCRT="
if defined MSYS2_ROOT (
    if exist "%MSYS2_ROOT%\ucrt64\bin\Qt5Core.dll" set "UCRT=%MSYS2_ROOT%\ucrt64"
)
if not defined UCRT (
    for %%D in ("C:\msys64" "C:\msys2" "%SystemDrive%\msys64") do (
        if not defined UCRT if exist "%%~D\ucrt64\bin\Qt5Core.dll" set "UCRT=%%~D\ucrt64"
    )
)
if not defined UCRT (
    echo [ERROR] Could not find an MSYS2 ucrt64 toolchain.
    echo         Looked for ucrt64\bin\Qt5Core.dll under MSYS2_ROOT, C:\msys64, C:\msys2.
    echo         Install it, or set MSYS2_ROOT to your MSYS2 install.
    echo         See docs\building-on-windows.md section 1b.
    goto :fail
)
set "MINGW_BIN=%UCRT%\bin"
set "QT_PLUGINS=%UCRT%\share\qt5\plugins\platforms"

REM ------------------------------------------------ backend configuration ---
if /i "%BACKEND%"=="spin" (
    set "BUILD_DIR=%REPO%build_spin"
    set "CMAKE_BACKEND=-Dwith_spin=ON -Dwith_arena=OFF"
    set "SDK_NAME=Spinnaker SDK"
    set "SDK_DLL=SpinnakerC_v140.dll"
    call :find_spinnaker
) else (
    set "BUILD_DIR=%REPO%build_ucrt"
    set "CMAKE_BACKEND=-Dwith_arena=ON -Dwith_spin=OFF"
    set "SDK_NAME=Arena SDK"
    set "SDK_DLL=ArenaC_v140.dll"
    call :find_arena
)
set "EXE=!BUILD_DIR!\test_gui.exe"

REM ------------------------------------------------------------- sanity ---
if not defined SDK_BIN (
    echo [ERROR] Could not locate the %SDK_NAME% runtime directory.
    echo         See docs\building-on-windows.md section 1a.
    goto :fail
)
if not exist "!SDK_BIN!\!SDK_DLL!" (
    echo [ERROR] %SDK_NAME% runtime not found:
    echo             !SDK_BIN!
    echo         Expected !SDK_DLL! there.
    echo         See docs\building-on-windows.md section 1a.
    goto :fail
)

REM Toolchain first on PATH so our Qt5/OpenCV win over any DLLs the camera
REM SDK ships in its own bin directory.
set "PATH=%MINGW_BIN%;!SDK_BIN!;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%QT_PLUGINS%"

if defined SKIP_BUILD goto :launch

REM -------------------------------------------------------------- build ---
if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

if not exist "!BUILD_DIR!\Makefile" (
    echo === Configuring ^(%BACKEND% backend^) ...
    pushd "!BUILD_DIR!"
    cmake -G "MinGW Makefiles" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
          -DCMAKE_BUILD_TYPE=Release ^
          -DCMAKE_PREFIX_PATH="%UCRT:\=/%" ^
          !CMAKE_BACKEND! -Dwith_qt_gui=ON -Dwith_demos=OFF -Dwith_tests=OFF ..
    set "RC=!ERRORLEVEL!"
    popd
    if not "!RC!"=="0" (
        echo.
        echo [ERROR] cmake configure failed ^(exit !RC!^).
        goto :fail
    )
)

echo === Building ...
pushd "!BUILD_DIR!"
mingw32-make -j8
set "RC=!ERRORLEVEL!"
popd
if not "!RC!"=="0" (
    echo.
    echo [ERROR] Build failed ^(exit !RC!^). See the compiler output above.
    goto :fail
)
echo === Build OK

REM ------------------------------------------------------------- launch ---
:launch
if not exist "!EXE!" (
    echo [ERROR] Executable not found:
    echo             !EXE!
    echo         Re-run without -n to build it.
    goto :fail
)

echo.
echo === Launching BIAS ...
echo     backend : %BACKEND%
echo     exe     : !EXE!
echo     sdk     : !SDK_BIN!
echo.

"!EXE!" !EXTRA!
set "RC=!ERRORLEVEL!"

if not "!RC!"=="0" (
    echo.
    echo [WARN] BIAS exited with code !RC!
    if "!RC!"=="-1073741515" echo        0xC0000135 STATUS_DLL_NOT_FOUND - a DLL failed to resolve
    if "!RC!"=="-1073741819" echo        0xC0000005 STATUS_ACCESS_VIOLATION - a real crash, not a DLL issue
    if "!RC!"=="-1073740940" echo        0xC0000374 STATUS_HEAP_CORRUPTION - see docs\building-on-windows.md section 5
    goto :fail
)

endlocal
exit /b 0

REM ============================================================ helpers ===

:find_arena
REM Arena: honor LUCID_DEV_ROOT, else the default install location.
set "SDK_BIN="
if defined LUCID_DEV_ROOT (
    set "_R=%LUCID_DEV_ROOT%"
    if "!_R:~-1!"=="\" set "_R=!_R:~0,-1!"
    if exist "!_R!\x64Release" set "SDK_BIN=!_R!\x64Release"
)
if not defined SDK_BIN (
    if exist "C:\Program Files\Lucid Vision Labs\Arena SDK\x64Release" (
        set "SDK_BIN=C:\Program Files\Lucid Vision Labs\Arena SDK\x64Release"
    )
)
goto :eof

:find_spinnaker
REM Spinnaker: honor SPINNAKER_INSTALL_PATH, else known roots newest-first
REM (4.x Teledyne, 2.x-3.x FLIR Systems, 1.x Point Grey).
set "SDK_BIN="
if defined SPINNAKER_INSTALL_PATH (
    set "_R=%SPINNAKER_INSTALL_PATH%"
    if "!_R:~-1!"=="\" set "_R=!_R:~0,-1!"
    if exist "!_R!\bin64\vs2015" set "SDK_BIN=!_R!\bin64\vs2015"
)
if not defined SDK_BIN (
    for %%R in (
        "C:\Program Files\Teledyne\Spinnaker"
        "C:\Program Files\FLIR Systems\Spinnaker"
        "C:\Program Files\Point Grey Research\Spinnaker"
    ) do (
        if not defined SDK_BIN if exist "%%~R\bin64\vs2015" set "SDK_BIN=%%~R\bin64\vs2015"
    )
)
goto :eof

:usage
echo.
echo build_and_run_bias.bat - build and launch the BIAS GUI
echo.
echo   build_and_run_bias.bat                Lucid Arena backend (default)
echo   build_and_run_bias.bat --spin         FLIR/Teledyne Spinnaker backend
echo   build_and_run_bias.bat -n^|--no-build  skip the build, just launch
echo   build_and_run_bias.bat --help         this help
echo.
echo   Arena    builds into build_ucrt\ , Spinnaker into build_spin\
echo   Any other arguments are passed through to test_gui.exe.
echo.
echo   Prerequisites: see docs\building-on-windows.md
echo.
endlocal
exit /b 0

:fail
echo.
pause
endlocal
exit /b 1
