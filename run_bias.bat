@echo off
REM ---------------------------------------------------------------------------
REM run_bias.bat - launch the BIAS GUI (test_gui) built with the MSYS2 MinGW
REM toolchain against Lucid Arena (GigE) cameras.
REM
REM Puts the required runtime DLLs on PATH:
REM   * MinGW runtime + Qt5 + OpenCV       (C:\msys64\mingw64\bin)
REM   * Arena C API + GenTL producer       (Arena SDK\x64Release)
REM and points Qt at its Windows platform plugin.
REM ---------------------------------------------------------------------------

setlocal

REM Location of this script (repo root, with trailing backslash)
set "REPO_DIR=%~dp0"

REM --- toolchain / SDK locations (edit here if installed elsewhere) ---
REM Uses the MSYS2 UCRT64 toolchain (verified working build in build_ucrt\).
set "MINGW_BIN=C:\msys64\ucrt64\bin"
set "QT_PLUGINS=C:\msys64\ucrt64\share\qt5\plugins\platforms"
set "ARENA_BIN=C:\Program Files\Lucid Vision Labs\Arena SDK\x64Release"

REM Honor the Arena SDK env var if the SDK lives somewhere else
if defined LUCID_DEV_ROOT set "ARENA_BIN=%LUCID_DEV_ROOT%\x64Release"

set "TEST_GUI=%REPO_DIR%build_ucrt\test_gui.exe"

if not exist "%TEST_GUI%" (
    echo ERROR: %TEST_GUI% not found.
    echo Build it first, e.g. from an MSYS2 MinGW64 shell:
    echo     export CMAKE_POLICY_VERSION_MINIMUM=3.5
    echo     cmake -G "MinGW Makefiles" -Dwith_arena=ON -Dwith_spin=OFF -Dwith_qt_gui=ON -B build .
    echo     cmake --build build -j
    exit /b 1
)

set "PATH=%MINGW_BIN%;%ARENA_BIN%;%PATH%"
set "QT_QPA_PLATFORM_PLUGIN_PATH=%QT_PLUGINS%"

echo Launching BIAS (test_gui) ...
"%TEST_GUI%" %*

endlocal
