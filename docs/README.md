# BIAS documentation

Local documentation for building and extending BIAS.

## Contents

- **[lucid-arena-backend.md](lucid-arena-backend.md)** — the Lucid Vision Labs
  **Arena (GigE)** camera backend: how it works, how cameras are identified by
  IP, and the full list of source/build changes made to compile BIAS with a
  modern MSYS2 (GCC 16 / Qt5 / OpenCV 4) toolchain.

- **[building-on-windows.md](building-on-windows.md)** — step-by-step
  **installation and compilation guide** for Windows using the MSYS2 / MinGW
  **UCRT64** toolchain, plus how to run the GUI and drive it over the HTTP
  control API.

## Quick start (Windows, Lucid GigE camera)

```bash
# 1. Install the MSYS2 UCRT64 toolchain + deps (once) - see building-on-windows.md
# 2. Configure + build (from an MSYS2 UCRT64 shell, repo root):
export CMAKE_POLICY_VERSION_MINIMUM=3.5
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release \
      -Dwith_arena=ON -Dwith_spin=OFF -Dwith_qt_gui=ON \
      -B build_ucrt .
cmake --build build_ucrt -j
```

Then launch the GUI with the provided helper (from the repo root):

```bat
run_bias.bat
```

Each connected Lucid camera opens its own window, titled with the camera's IP
address. Click **Connect**, then **Start Capture**.
