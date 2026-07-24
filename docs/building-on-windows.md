# Building BIAS on Windows (MSYS2 / MinGW UCRT64)

This guide builds BIAS and the `test_gui` application on Windows using the
**MSYS2 UCRT64** toolchain. GCC, CMake, Qt5 and OpenCV all come from `pacman`,
and the camera SDKs' **C** APIs (`ArenaC` for Lucid, `SpinnakerC` for FLIR)
link against MinGW-GCC because they are plain `extern "C"` interfaces — no need
for MSVC or MSVC builds of Qt5/OpenCV.

> Verified with GCC 16.1, CMake 4.4, Qt 5.15, OpenCV 4.13 against a Lucid
> PHX004S-M GigE camera.

---

## 1. Prerequisites

### 1a. Camera SDK

Install the vendor SDK for your camera and note its location:

- **Lucid (GigE):** Arena SDK, default
  `C:\Program Files\Lucid Vision Labs\Arena SDK`.
  The installer sets `LUCID_DEV_ROOT` and puts the runtime DLLs in
  `…\Arena SDK\x64Release` (usually added to the system `PATH`).
- **FLIR (USB3):** Spinnaker SDK, default
  `C:\Program Files\FLIR Systems\Spinnaker` (see `FindSpinnaker.cmake`).

Sanity-check the hardware first with the SDK's own tools (e.g. Arena's
`ArenaView`, or the Spinnaker `Enumeration`/`Acquisition` example binaries)
before building BIAS.

### 1b. MSYS2 + toolchain

1. Install MSYS2 (e.g. `winget install MSYS2.MSYS2`) — installs to `C:\msys64`.
2. From an **MSYS2 UCRT64** shell, install the toolchain and dependencies:

   ```bash
   pacman -Syu   # update (may ask to re-open the shell, then run again)
   pacman -S --needed \
       mingw-w64-ucrt-x86_64-gcc \
       mingw-w64-ucrt-x86_64-cmake \
       mingw-w64-ucrt-x86_64-make \
       mingw-w64-ucrt-x86_64-pkgconf \
       mingw-w64-ucrt-x86_64-qt5-base \
       mingw-w64-ucrt-x86_64-qt5-serialport \
       mingw-w64-ucrt-x86_64-opencv \
       mingw-w64-ucrt-x86_64-gdb        # optional, for debugging
   ```

---

## 2. Configure & build

From the repository root, in an **MSYS2 UCRT64** shell (or any shell with
`C:\msys64\ucrt64\bin` first on `PATH`):

```bash
export CMAKE_POLICY_VERSION_MINIMUM=3.5     # required for the old cmake_minimum_required(2.8)

cmake -G "MinGW Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      -Dwith_arena=ON \
      -Dwith_spin=OFF \
      -Dwith_fc2=OFF \
      -Dwith_dc1394=OFF \
      -Dwith_qt_gui=ON \
      -Dwith_demos=OFF \
      -Dwith_tests=OFF \
      -B build_ucrt .

cmake --build build_ucrt -j
```

Produces `build_ucrt/test_gui.exe`.

**Backend options** (`-D…=ON/OFF`): `with_arena` (Lucid), `with_spin` (FLIR
Spinnaker), `with_fc2` (FlyCapture2), `with_dc1394`. At least one is required.
`with_qt_gui=ON` is needed to build the GUI (the plugins depend on Qt being
found). `with_demos`/`with_tests` are OFF because those targets still contain
OpenCV-2-era code.

Notes:
- `-DCMAKE_BUILD_TYPE=Release` (i.e. `-DNDEBUG`) is important — see the build-type
  note in [lucid-arena-backend.md](lucid-arena-backend.md#build-type).
- If CMake can't find the Arena SDK, pass it explicitly:
  `-DArena_INCLUDE_DIR="C:/Program Files/Lucid Vision Labs/Arena SDK/include/ArenaC"`
  `-DArena_LIBRARY="C:/Program Files/Lucid Vision Labs/Arena SDK/lib64/ArenaC/ArenaC_v140.lib"`

---

## 3. Running the GUI

### Easiest: the helper script

From the repo root:

```bat
run_bias.bat
```

It puts the correct directories on `PATH`, sets the Qt platform-plugin path,
and launches `build_ucrt\test_gui.exe`. Edit the paths at the top of the script
if your toolchain or SDK live elsewhere.

### Manual launch

The executable needs these on `PATH` **in this order** (your toolchain first):

```bash
export PATH="/c/msys64/ucrt64/bin:/c/Program Files/Lucid Vision Labs/Arena SDK/x64Release:$PATH"
export QT_QPA_PLATFORM_PLUGIN_PATH="C:/msys64/ucrt64/share/qt5/plugins/platforms"
./build_ucrt/test_gui.exe
```

Each discovered camera opens its own window, titled with its **IP address**.
Click **Connect**, then **Start Capture**.

> **PATH ordering matters.** Some camera SDK `bin` directories ship their own
> (mismatched) copies of `Qt5*.dll`. Keep your MSYS2 `ucrt64\bin` **first** so
> the loader uses your Qt/OpenCV/MinGW DLLs. A wrong-DLL mix typically fails
> with `STATUS_DLL_NOT_FOUND` (0xC0000135), which is not an obviously
> Qt-related message.

---

## 4. HTTP control API (headless / scripting)

Each camera window runs a small HTTP control server on port
`5000 + 10 × (cameraNumber + 1)` — so camera 0 = **5010**, camera 1 = **5020**, …
Commands are GET requests of the form `http://localhost:5010/?<command>`
(hyphenated names). Useful for scripted capture or verifying a build without
clicking:

```bash
curl "http://localhost:5010/?get-camera-guid"     # -> the camera IP
curl "http://localhost:5010/?connect"
curl "http://localhost:5010/?start-capture"
curl "http://localhost:5010/?get-frame-count"     # increments while streaming
curl "http://localhost:5010/?get-frames-per-sec"
curl "http://localhost:5010/?stop-capture"
curl "http://localhost:5010/?disconnect"
```

Other commands include `get-status`, `get-time-stamp`, `enable-logging` /
`disable-logging`, `set-video-file=<path>`, `load-configuration=<path>`,
`save-configuration=<path>`, `get-configuration`, `set-configuration=<json>`.

---

## 5. Troubleshooting

- **CMake configure error about `cmake_minimum_required` / CMP0046** — you
  didn't set `CMAKE_POLICY_VERSION_MINIMUM=3.5` (CMake ≥ 4.0).
- **`STATUS_DLL_NOT_FOUND` (0xC0000135) at launch** — a DLL (usually a Qt DLL)
  isn't found or a wrong copy loaded; check PATH ordering (section 3).
- **`STATUS_HEAP_CORRUPTION` (0xC0000374) at startup / random crashes** — a
  fall-off-the-end (`-Wreturn-type`) bug. Rebuild clean and scan:
  `cmake --build build_ucrt --clean-first 2>&1 | grep -i return-type`.
- **Crash during capture in a non-Release build** — likely an OpenCV debug
  assertion (e.g. an out-of-range `Mat::at`). Build with
  `-DCMAKE_BUILD_TYPE=Release`, and fix the underlying access.
- **Debugging:** `gdb` from `C:\msys64\mingw64\bin\gdb.exe` debugs a
  ucrt64-built binary fine:
  `gdb --batch -ex run -ex "bt 40" build_ucrt/test_gui.exe`.
- **Don't commit build artifacts** — add `build/` and `build_ucrt/` to
  `.gitignore`.
