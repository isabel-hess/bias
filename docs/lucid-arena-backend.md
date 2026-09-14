# Lucid Arena (GigE) backend & modern-toolchain build changes

This document summarizes the work done to (1) add a camera backend for **Lucid
Vision Labs GigE cameras** via the **Arena SDK**, and (2) get the full BIAS GUI
to compile and run with a current MSYS2 (GCC 16 / Qt5 / OpenCV 4) toolchain.

Verified end-to-end against a Lucid **PHX004S-M** (mono, 720×540): enumerate by
IP → connect → stream (~271 fps) → clean stop/disconnect, both from the GUI and
via the HTTP control API.

---

## 1. The Arena backend

BIAS selects a camera backend at build time and dispatches to it at runtime
through a facade (`Camera` / `Guid` / `CameraFinder`). The new backend lives in
`src/backend/arena/` and mirrors the existing Spinnaker (`spin`) backend.

**Key design decisions**

- **Arena C API, not C++.** Links against `ArenaC_v140.lib` / `ArenaC_v140.dll`
  (headers under `…/Arena SDK/include/ArenaC`). The flat C ABI links cleanly
  with MinGW-GCC — the MSVC-built Arena **C++** API would not. This also means
  the entire Spinnaker-style GenICam node-wrapper layer is unnecessary: the
  Arena C node map exposes typed by-name accessors
  (`acNodeMapGetFloatValue(nodeMap, "ExposureTime", …)` etc.).
- **Cameras identified by IP address.** The `Guid` value for a Lucid camera is
  its dotted IP (e.g. `169.254.227.159`), read at enumeration time via
  `acSystemGetDeviceIpAddressStr`. `GuidDevice_arena` orders cameras by
  **numeric** IP, so a given physical camera keeps a stable position in the
  enumerated set — and therefore a stable camera number, HTTP control port,
  window, and default filename prefix — across runs (unlike unstable USB
  enumeration order).
- **Single shared Arena system.** `acOpenSystem` refuses a second concurrent
  open (`AC_ERR_RESOURCE_IN_USE`), so `system_arena.{hpp,cpp}` provides a
  reference-counted `acquireArenaSystem()` / `releaseArenaSystem()` shared by
  the `CameraFinder` and every `CameraDevice_arena` (mirrors Spinnaker's
  singleton semantics).

**Acquisition spine** (`CameraDevice_arena`):
`acOpenSystem` → `acSystemUpdateDevices` / match IP → `acSystemCreateDevice` →
node-map config → `acDeviceStartStream` → `acDeviceGetBuffer` →
`acImageFactoryConvert` → wrap in `cv::Mat` → `acDeviceRequeueBuffer` →
`acDeviceStopStream` → `acSystemDestroyDevice` → `acCloseSystem`.

**Scope:** full acquisition, property parity (exposure/shutter, gain, frame
rate, gamma, brightness, trigger delay, temperature), internal/external trigger,
`camera_info`, pixel-format/`isColor` enumeration, and Format7 ROI read/write.
Deferred: faithful legacy `VideoMode`/`FrameRate` enum lists (kept as
`spin`-style single-element stubs) and software-trigger execution.

### Files added

```
src/backend/arena/
  guid_device_arena.{hpp,cpp}     # GUID = IP string, numeric-IP ordering
  system_arena.{hpp,cpp}          # refcounted shared acSystem
  camera_info_arena.{hpp,cpp}     # vendor/model/serial/IP introspection
  node_map_utils_arena.{hpp,cpp}  # typed node get/set + access-mode helpers
  utils_arena.{hpp,cpp}           # PFNC <-> OpenCV <-> BIAS pixel-format mapping
  camera_device_arena.{hpp,cpp}   # the main CameraDevice implementation
  CMakeLists.txt
cmake/Modules/FindArena.cmake     # locates ArenaC headers + import lib
```

### Facade / build wiring (mirrors every `spin` block)

- `src/facade/basic_types.hpp` — `CAMERA_LIB_ARENA` enum value + `ERROR_ARENA_*`
  codes.
- `src/facade/guid.{hpp,cpp}` — tagged `Guid(std::string ip, CameraLib lib)`
  ctor + `getValue_arena()`.
- `src/facade/camera.{hpp,cpp}` — `createCameraDevice_arena` + dispatch case.
- `src/facade/camera_finder.{hpp,cpp}` — `update_arena` IP enumeration.
- `src/facade/exception.{hpp,cpp}` — `throw_ERROR_NO_ARENA`.
- top-level `CMakeLists.txt` + `src/facade/CMakeLists.txt` — `with_arena`
  option, `-DWITH_ARENA`, `find_package(Arena)`, include/link, subdirectory.

Enable with `-Dwith_arena=ON` (see [building-on-windows.md](building-on-windows.md)).

---

## 2. Changes required to build with a modern toolchain

BIAS was originally built with an older compiler/libraries. Building with
current MSYS2 (GCC 16, Qt 5.15, OpenCV 4.13, CMake 4.x) surfaced several
pre-existing latent bugs and API drifts. All of the following were fixed.

### Latent bugs (real defects, newly exposed)

- **`-Wreturn-type` fall-off-the-end functions.** Several non-void functions
  never returned a value. Under GCC 16 the return slot held garbage; for types
  owning heap resources (`RtnStatus` owns a `QString`) this **corrupted the
  heap at startup** and crashed nondeterministically. Fixed:
  - `src/plugin/stampede/stampede_plugin_config.cpp` —
    `appendDisplayEvent()`, `setDisplayEventList()` (this was the startup crash)
  - `src/backend/base/camera_device.cpp` — `getImageTimeStamp()`
  - `src/backend/base/camera_device.hpp` — `getCameraLib()`
  - `src/gui/fps_estimator.cpp` — `setCutOffFreq()`
- **Histogram out-of-range read.** `CameraWindow::updateHistogramPixmap`
  (`src/gui/camera_window.cpp`) read `hist.at<float>(0, i)`, but `cv::calcHist`
  returns a 256×1 (row×col) matrix, so the column index was out of range.
  Fixed to `hist.at<float>(i, 0)`. (OpenCV's debug assertion caught this as an
  uncaught exception during capture in non-Release builds.)

### C++ / standard-library conformance

- **`std::set`/`map` comparators must be `const`-callable.** Modern libstdc++
  (and MSVC) `static_assert` that the comparator's `operator()` is `const`.
  Added `const` to: `GuidCmp`, `GuidPtrCmp` (`src/facade/guid.*`),
  `CameraPtrCmp` (`src/facade/camera.*`), `CompressedFrameCmp_jpg`,
  `CompressedFrameCmp_ufmf` (`src/gui/compressed_frame_*.*`).

### OpenCV 2 → 4 API drift

- Legacy `CV_*` constants replaced with `cv::` enums where the compiler flagged
  them: `CV_BGR2GRAY`→`cv::COLOR_BGR2GRAY`, `CV_GRAY2BGR`→`cv::COLOR_GRAY2BGR`,
  `CV_THRESH_BINARY`→`cv::THRESH_BINARY`, `CV_RETR_EXTERNAL`/
  `CV_CHAIN_APPROX_NONE`/`CV_FILLED`, `CV_FONT_HERSHEY_SIMPLEX`,
  `CV_IMWRITE_JPEG_QUALITY`, `CV_FOURCC(...)`→`cv::VideoWriter::fourcc(...)`.
  Files: `src/utility/{basic_image_proc,blob_finder,mat_to_qimage}.cpp`,
  `src/gui/{compressed_frame_jpg,video_writer_avi}.cpp`,
  `src/plugin/grab_detector/grab_detector_plugin.cpp`.
  *(Alternatively, OpenCV 4 still ships the old names via the legacy compat
  headers `opencv2/imgproc/imgproc_c.h` and
  `opencv2/imgcodecs/legacy/constants_c.h`.)*
- Removed obsolete `#include <cv.h>` (OpenCV 1.x umbrella, gone in 4) in the
  `signal_slot_demo` and `grab_detector` plugins.

### CMake / Qt5

- Deleted obsolete `add_dependencies(<target> ${..._FORMS})` lines (which
  treated `.ui` filenames as targets) in `src/gui/CMakeLists.txt` and the
  `stampede` / `grab_detector` / `signal_slot_demo` plugin CMakeLists. The UI
  headers are already added to each target by `qt5_wrap_ui`.
- CMake ≥ 4.0 refuses to parse the project's `cmake_minimum_required(VERSION
  2.8)` without `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

### Build type

- Build **Release** (`-DCMAKE_BUILD_TYPE=Release`, i.e. `-DNDEBUG`). Besides
  performance, this disables OpenCV's debug assertions, matching the
  configuration the code was historically validated against.

---

## 3. Note on the original Spinnaker (FLIR) build

An earlier effort documented building BIAS against a Spinnaker/FLIR camera with
the same MSYS2 approach in [`../BUILD_MINGW_HANDOFF.md`](../BUILD_MINGW_HANDOFF.md).
Several fixes here (the `-Wreturn-type` bugs, `const` comparators, OpenCV 4
constants, the `.ui` `add_dependencies` removal, `CMAKE_POLICY_VERSION_MINIMUM`)
were independently confirmed by that work — it's a useful cross-reference,
especially its notes on Spinnaker-specific SDK API drift.
