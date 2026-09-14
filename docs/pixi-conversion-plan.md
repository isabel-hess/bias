# Plan: convert BIAS to a pixi project (dependency handling + compilation)

**Status:** planning / scoping only — nothing here is implemented yet. This
document is written for a future agent (or developer) to pick up.

**Goal:** make BIAS's build dependencies and compilation reproducible through
[pixi](https://pixi.sh) — ideally `pixi install` to get the toolchain +
Qt5 + OpenCV, and `pixi run build` / `pixi run gui` to configure, build, and
launch, on a fresh machine, cross-platform.

---

## 1. Current build (what we're converting from)

- CMake project (`cmake_minimum_required(VERSION 2.8)`), C++11.
- Windows build today: **MSYS2 UCRT64** — GCC 16, Qt 5.15, OpenCV 4.13, CMake
  4.4, generator "MinGW Makefiles", `-DCMAKE_BUILD_TYPE=Release`,
  `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. See
  [building-on-windows.md](building-on-windows.md).
- Backends selected via `-Dwith_arena/with_spin/with_fc2/with_dc1394`. GUI via
  `-Dwith_qt_gui=ON`.
- Camera SDKs are **proprietary, system-installed** and discovered by CMake
  find-modules using env vars:
  - Lucid **Arena** (`LUCID_DEV_ROOT`, `cmake/Modules/FindArena.cmake`)
  - FLIR **Spinnaker** (`cmake/Modules/FindSpinnaker.cmake`)
- Source uses GCC-isms: `__PRETTY_FUNCTION__` (pervasive) and the CMake flag
  `set(CMAKE_CXX_FLAGS "-std=gnu++0x -O2")`.

## 2. What conda-forge can and cannot provide (verified 2026-07, `pixi search`)

**Available on conda-forge (both win-64 and linux-64):**
- `cmake` 4.4, `ninja` 1.13
- `qt-main` **5.15.15** (this is Qt5)
- `opencv` (default 5.0; **4.x builds also exist — pin to 4.x** to match the
  code, e.g. `opencv=4.*`)
- Compiler activation:
  - linux-64: `gxx_linux-64` (GCC 15) — a real, modern GCC.
  - win-64: `vs2022_win-64` / `vs2019_win-64` — these **activate a
    system-installed MSVC**, they do not ship a compiler.

**NOT usable / not available:**
- A modern **MinGW** toolchain. conda-forge only has `m2w64-toolchain` =
  **GCC 5.3** (2016-era) with no matching modern Qt5/OpenCV m2w64 builds. The
  current MSYS2 UCRT64 (GCC 16) environment **cannot be reproduced via pixi**.
- **Lucid Arena** / **FLIR Spinnaker** SDKs — proprietary, not on conda-forge
  (`pixi search libarena` returns nothing). These must remain **system
  prerequisites**, referenced by env var + the existing Find modules.

### The central consequence

> On **Windows**, "pixi/conda-forge" ⇒ **MSVC compiler** (plus conda-forge
> Qt5/OpenCV, which are MSVC-built). On **Linux**, "pixi/conda-forge" ⇒ GCC,
> which is source-compatible with the code as-is.

So converting to pixi on Windows is really "port BIAS to MSVC." On Linux it's
essentially free.

## 3. Options

### Option A — Full pixi, MSVC on Windows + GCC on Linux *(recommended if committing to pixi)*
Pixi manages cmake/ninja/qt5/opencv and the compiler on both platforms. Windows
uses MSVC (via `vs2022_win-64`, requires a system VS Build Tools install — the
machine already has VS BuildTools 18). Requires a **source portability pass to
remove GCC-isms** (Section 5). Biggest up-front cost, best end state:
reproducible cross-platform build, and it sidesteps the MSYS2 GLib event-loop
issues seen with the MinGW build.

### Option B — pixi as a thin task-runner, keep MSYS2 MinGW on Windows
Pixi manages only cross-platform helpers (cmake, ninja, maybe python tooling)
and defines `[tasks]`, but the **compiler + Qt5 + OpenCV still come from MSYS2**
(a documented manual prerequisite, not pixi-managed). Low effort, but pixi does
not actually own the heavy dependencies on Windows, so reproducibility is only
partial. Not a "real" pixi conversion; mainly buys you `pixi run build/gui`
task ergonomics.

### Option C — Linux-first pixi now, Windows later
Do the clean Linux conversion first (Option A's Linux half — no code changes),
prove out the pixi.toml + tasks, and defer the Windows MSVC port. Good if BIAS
will also run on Linux (the Arena SDK has a Linux distribution). Lowest risk,
partial coverage.

**Recommendation:** Option A if the lab wants one reproducible cross-platform
build system and is willing to do the MSVC port. If the near-term need is just
"don't hand-install deps on Windows," Option B is faster but keep expectations
modest. Whoever picks this up should confirm the intended target OS(es) first —
that single answer determines most of the work.

## 4. Proposed pixi.toml (Option A sketch)

Multi-platform, multi-feature. Treat as a starting point to iterate on.

```toml
[workspace]
name = "bias"
channels = ["conda-forge"]
platforms = ["win-64", "linux-64"]

[dependencies]
cmake = "4.*"
ninja = "*"
qt-main = "5.15.*"          # Qt5
opencv = "4.*"             # pin to 4.x to match the code (default is 5.0)

# --- Windows: activate the system MSVC toolchain ---
[target.win-64.dependencies]
vs2022_win-64 = "*"        # requires VS 2022 / Build Tools installed on the machine

# --- Linux: conda-forge GCC ---
[target.linux-64.dependencies]
gxx_linux-64 = "*"

# Camera SDKs are NOT pixi-managed. Point CMake at the system install.
[target.win-64.activation.env]
LUCID_DEV_ROOT = "C:/Program Files/Lucid Vision Labs/Arena SDK"
# Ensure the Arena runtime DLLs + Qt platform plugin are found when running:
QT_QPA_PLATFORM_PLUGIN_PATH = "$CONDA_PREFIX/Library/plugins/platforms"

[tasks]
# Configure (Ninja works with both MSVC and GCC; drop the MinGW-Makefiles generator)
configure = "cmake -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Dwith_arena=ON -Dwith_spin=OFF -Dwith_qt_gui=ON -Dwith_demos=OFF -Dwith_tests=OFF"
build     = { cmd = "cmake --build build -j", depends-on = ["configure"] }
gui       = { cmd = "build/test_gui", depends-on = ["build"] }
clean     = "cmake --build build --target clean"
```

Notes / things to verify while implementing:
- On win-64, pixi activation puts conda-forge's MSVC-built Qt5/OpenCV DLLs under
  `%CONDA_PREFIX%\Library\bin` (auto on PATH inside `pixi run`/`pixi shell`), so
  the Qt/OpenCV runtime is handled — but the **Arena runtime DLLs**
  (`ArenaC_v140.dll`, GenTL producer) still come from
  `%LUCID_DEV_ROOT%\x64Release`; add that to PATH in a run task/activation.
- conda-forge Qt5 provides its own CMake config; `find_package(Qt5 ...)` should
  resolve from `$CONDA_PREFIX`. Confirm the deprecated `qt5_use_modules` still
  works with this Qt5 build (it did under MSYS2).
- Consider a `[feature]`/`[environments]` split (e.g. `arena`, `spin`) so
  `pixi run -e arena build` selects the backend + only requires that SDK.

## 5. Source/CMake changes needed for the MSVC (Windows) path

These are the known GCC-isms. Do this as a portability pass, then iterate on
whatever else MSVC's compiler flags.

1. **`CMAKE_CXX_FLAGS "-std=gnu++0x -O2"`** (top-level `CMakeLists.txt`) — MSVC
   rejects `-std=gnu++0x`. Replace the hard-coded flag with
   `set(CMAKE_CXX_STANDARD 14)` + `set(CMAKE_CXX_STANDARD_REQUIRED ON)` (drop
   `-O2`; let `CMAKE_BUILD_TYPE=Release` supply optimization), or guard the flag
   with `if(NOT MSVC)`.
2. **`__PRETTY_FUNCTION__`** (pervasive, in error-message strings) — MSVC does
   not define it. Cleanest fix: a tiny compat header
   ```cpp
   #if defined(_MSC_VER) && !defined(__PRETTY_FUNCTION__)
   #  define __PRETTY_FUNCTION__ __FUNCSIG__
   #endif
   ```
   and force-include it for MSVC in CMake:
   `if(MSVC) add_compile_options(/FI"compat_msvc.hpp") endif()` (plus the header
   dir on the include path). Avoids editing hundreds of call sites.
3. **Already fixed in this branch** (help MSVC and modern GCC alike): `const`
   comparators, the `-Wreturn-type` fall-off-the-end bugs, OpenCV-4 constant
   names, and the histogram `at<float>(i,0)` fix. Keep them.
4. **Iterate on remaining MSVC errors.** Expect a few more GCC-isms to surface
   (e.g. `__attribute__`, `and`/`or`/`not` keyword operators without
   `<ciso646>`, designated initializers, variable-length arrays, `#warning`).
   Fix as they appear.
5. **Camera SDKs under MSVC:** the Arena C API (`ArenaC_v140.lib`) and Spinnaker
   C API are MSVC-built, so they link natively — no MinGW import-lib dance, and
   the MinGW-only `SPINNAKER_DEPRECATED_API` workaround
   (`BUILD_MINGW_HANDOFF.md` fix #4) is **not needed** under MSVC.
6. **Bonus:** conda-forge Qt5 on Windows is not glib-based, so the MSYS2 MinGW
   GLib/GIO startup issue (see `lucid-arena-backend.md`) does not apply.

## 6. Handling the proprietary camera SDKs

Pixi cannot fetch them. Keep them as documented system prerequisites:
- Reference via env vars in `[target.*.activation.env]` (`LUCID_DEV_ROOT`;
  add an equivalent for Spinnaker if `with_spin`).
- The existing `FindArena.cmake` / `FindSpinnaker.cmake` already honor those.
- Add a preflight `pixi` task that checks the SDK exists and prints a clear
  message if not, e.g. verifying
  `%LUCID_DEV_ROOT%\include\ArenaC\ArenaCApi.h`.
- Ensure the SDK **runtime** dir (`x64Release`) is on PATH for the `gui` task.

## 7. Step-by-step for the implementing agent

1. **Confirm the target OS(es)** with the user (Windows-only? +Linux?). This
   decides MSVC-port scope. Confirm the OpenCV major version to pin (4 vs 5) by
   building a throwaway and checking the API the code uses.
2. Start on **linux-64** if in scope (no code changes) to validate the
   pixi.toml, dependency pins, and tasks quickly.
3. For **win-64**: add the MSVC activation + do the Section 5 portability pass.
   Build iteratively with `pixi run build`, fixing MSVC errors.
4. Wire `[tasks]` for configure/build/gui/clean; add the SDK preflight + PATH
   handling; consider `[environments]` per backend.
5. Update `run_bias.bat` (or replace with `pixi run gui`) and the docs
   ([building-on-windows.md](building-on-windows.md), README) to describe the
   pixi workflow. Keep the MSYS2 instructions as a documented fallback until the
   MSVC path is proven.
6. Add `.pixi/` to `.gitignore`; commit `pixi.toml` + `pixi.lock`.

## 8. Risks / open questions

- **MSVC port effort is unbounded until attempted.** `__PRETTY_FUNCTION__` and
  the CXX flag are known; the "iterate on the rest" step could be small or
  moderate. Time-box and reassess.
- **OpenCV 4 vs 5 pin.** Code was fixed against OpenCV 4; conda-forge default is
  5.0. Pin 4.x, or do a small pass to confirm 5.x compatibility.
- **`vs2022_win-64` needs a system VS install.** Present here (VS BuildTools
  18); document it as a prerequisite (pixi activates it, doesn't install it).
- **Qt5 is legacy on conda-forge.** `qt-main` 5.15 is available now; long-term
  the project may need a Qt6 port (separate, larger effort).
- **Two Windows toolchains coexist.** Decide whether pixi/MSVC *replaces* the
  MSYS2/MinGW build or lives beside it. Recommend replacing once proven, to
  avoid maintaining two.

## 9. Verification (definition of done)

- `pixi install` then `pixi run build` produces `test_gui` on each target
  platform from a clean checkout, with only the camera SDK as an external
  prerequisite.
- `pixi run gui` launches the GUI; a camera window appears (titled by IP for
  Arena).
- Headless smoke test via the HTTP control API (see
  [building-on-windows.md](building-on-windows.md#4-http-control-api-headless--scripting)):
  `connect` → `start-capture` → `get-frame-count` increments → clean
  `stop-capture`/`disconnect`, no crash.
