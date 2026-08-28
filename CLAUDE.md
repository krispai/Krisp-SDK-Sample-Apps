# CLAUDE.md

This is the **public** sample-apps repo for the Krisp Audio SDK: standalone example apps (Android,
iOS, JavaScript, native C++, Python) that show how to integrate a pre-built Krisp SDK / language
binding — it does not build the SDK itself, only consumes SDK artifacts (xcframework, `.so`/`.a`
libs, `krisp_audio` Python package, etc.) supplied separately. There is a parallel internal-only
repo (`Krisp-SDK-Sample-Apps-Internal`) covering the same platforms; this one is what external
customers see, so don't assume internal tooling or conventions apply here.

## Directory layout

| Path | Contents |
|---|---|
| `android/KrispTestApp/` | Android Studio project (Kotlin + NDK/CMake JNI bridge) — loads a `.kef` model and runs NC on a WAV file picked via SAF |
| `ios/KrispTestApp/` | Xcode/SwiftUI project — file-based NC processing view + live audio capture view, wraps the SDK via an Obj-C++ bridge |
| `javascript/sample-vt/` | A single `main.js` file — see caveat below, not a runnable project |
| `native-cpp/` | CMake-based desktop samples (Linux/macOS/Windows): NC, VAD, AR (voice/accent), VT, a plain WAV CLI, a Node addon, and a Python (pybind11) module |
| `python/` | One CLI script (`krisp_audio_test.py`) exercising the `krisp_audio` PyPI/wheel package directly (no C++ build) |

## android/KrispTestApp

Kotlin app; native glue lives in `app/src/main/cpp/krisp.cpp` + `CMakeLists.txt`, JNI declared in
`WavProcessActivity.kt` (`krispLoadModel`, `krispStartNcSessionPcm16/Float`, `krispNcFramePcm16/Float`).

**Setup** (per `android/KrispTestApp/README.md`): unpack `krisp-audio-sdk-9.9.0-android.zip`, rename
the root to `krisp-audio-sdk`, and place it at the Android project root. The Gradle build then reads
`krisp.sdk.dir` from `local.properties` (or `KRISP_SDK_DIR` env var) — the CMake configure step fails
with `error("Set krisp.sdk.dir in local.properties or KRISP_SDK_DIR in env")` if neither is set.
Expected SDK layout: `<sdk>/include`, `<sdk>/lib/<abi>/{static,dynamic}/libkrisp-audio-sdk.{a,so}`.

- `KRISP_LINK_TYPE` is hardcoded to `dynamic` in `app/build.gradle.kts` (static path exists in
  `CMakeLists.txt` but is commented out in the Gradle args).
- ABIs built: `arm64-v8a`, `armeabi-v7a`, `x86_64`. `compileSdk`/`targetSdk` 35, `minSdk` 21, NDK
  `28.0.13004108`, CMake `3.22.1`.
- **Build**: open in Android Studio, or `./gradlew assembleDebug` from `android/KrispTestApp/` once
  `local.properties`/`KRISP_SDK_DIR` is set.
- **Model at runtime**: the app has no bundled model — pick a `.kef` file via the file/folder picker
  in the UI (long-press "Select Model" opens a folder browser that filters for `*.kef`).

## ios/KrispTestApp

SwiftUI app (`WelcomeView` → `FileProcessingView` for file-based NC, `AudioCaptureView` for live
capture) over an Obj-C++ bridge (`KrispAudioProcessor.h/.mm`, `KrispAudioSDK.h/.mm`,
`KrispTestApp-Bridging-Header.h`).

**Setup** (per `ios/KrispTestApp/README.md`): copy `KrispAudioSDK.xcframework` (v9.2+) into
`ios/KrispTestApp/KrispTestApp/` (same directory as the `.swift` sources) before opening the
project — `project.pbxproj` references it at that exact path
(`$(PROJECT_DIR)/KrispTestApp/KrispAudioSDK.xcframework/...`). No Podfile/SPM — the framework is a
plain Xcode file reference.

- **Build**: open `ios/KrispTestApp/KrispTestApp.xcodeproj` in Xcode and run.
- **Model at runtime**: no bundled model — `FileProcessingView` loads a `.kef` via a file importer
  (`FileTypes.model`) at runtime, same as Android.

## javascript/sample-vt

Contains only `main.js` — no `package.json`, no build config, nothing else in the folder. It imports
`from "@anthropic/krisp-vt-sdk"` (note: `@anthropic` scope, not a Krisp package) and uses a
`KrispVTSDK` class with a hardcoded placeholder `API_KEY`. This does not correspond to any package
published in this umbrella (no such npm package exists in `krisp-audio-sdk-node` or elsewhere in this
workspace) — treat it as illustrative pseudocode for a browser VT integration, not a runnable sample.

## native-cpp

Desktop CMake project; see `native-cpp/README.md` for the authoritative option/flag table (reproduced
below only where it affects how to invoke Claude). Supported: Linux x64/arm64 (GCC 9.4+), macOS
x64/arm64 (Clang 15+), Windows x64/arm64 (VS 2019+), against Krisp SDK Desktop/Server v9.9+.

Apps (each gated by its own `-DBUILD_SAMPLE_*=ON` CMake option, off by default):

| App | Binary | Notes |
|---|---|---|
| `sample-nc` | `bin/sample-nc` | CLI WAV denoiser (PCM16/FLOAT32), optional `-s` call-stats |
| `sample-vad` | `bin/sample-vad` | Voice activity detection, writes per-frame scores to a text file |
| `wav-cli` | `bin/krisp-wav-cli` | Generic WAV processor; `-DENABLE_ACCENT=ON` adds Accent API |
| `sample-ar` | `bin/sample-ar` | Voice enrollment + accent-reduction-style processing (CMake option is `BUILD_SAMPLE_AL`, not `BUILD_SAMPLE_AR` — inconsistent naming vs. the binary/source dir; not documented in the repo's own README table) |
| `sample-vt` | `bin/sample-vt` | Voice translation CLI (PCM16/FLOAT32) — **needs a Krisp-dashboard auth token**, see gotcha below |
| `sample-dll` | `bin/sample-dll` (+ `libkrispdll`) | Builds a dynamic lib from Krisp static libs, plus a trivial test app; built via CMake like the others despite the README mentioning a "VS solution" |
| `sample-node` | `src/sample-node/nodejs-module.node` + `app.js`/`index.js` | Node N-API addon; CMake auto-runs `npm install` for you |
| `sample-python` | `bin/krisp_module*.so` + `process_wav.py` | pybind11 CPython module |

Common CMake flow:
```
cmake -S native-cpp/cmake -B build -DCOPY_KRISP_SDK=<path-to-unpacked-sdk> -DBUILD_SAMPLE_NC=ON \
      -DLIBSNDFILE_INC=<dir> -DLIBSNDFILE_LIB=<dir>
cmake --build build -j
```
- `-DCOPY_KRISP_SDK=<path>` copies the SDK into `native-cpp/krisp-sdk/` **once**; a second run with
  this flag set fails with "Krisp SDK already exists" — omit it (or delete `native-cpp/krisp-sdk/`
  first) on subsequent configures. Expected SDK layout under there: `include/`,
  `lib/{static,dynamic}[/external]`.
- `-DUSE_KRISP_DYNAMIC_LIBRARY=ON|OFF` (default OFF, static).
- Windows: `-DCMAKE_MSVC_RUNTIME_LIBRARY` **must** match the SDK build variant (`_mt`/`_mtd` →
  static `MultiThreaded[Debug]`; `_md`/`_mdd` → dynamic `MultiThreadedDLL[Debug]`) or you get linker
  errors.
- macOS: `brew install libsndfile` (and `brew install pybind11` for the Python module).
- Node: v20+, N-API 9; set `NODE_INC` to the Node headers dir (e.g.
  `/opt/homebrew/include/node` on ARM Mac with Homebrew, or `$NVM_DIR/versions/node/<v>/include/node`).
- Python module: needs Python3 dev headers + NumPy + pybind11 (`-DPYTHON3_PATH=<path>` optional).
- All built binaries land in `native-cpp/bin/`.
- Test assets: `native-cpp/test/input/*.wav`; `native-cpp/test/nc-sample-test-driver.sh` and
  `test-python.sh` show example invocations (they expect a `model.kef` alongside them, not provided
  in-repo).

**Model files**: every native-cpp sample takes a `.kef` model path via `-m`/`--model_path` (or
`-cfg` for the Python CLI sample) — none are bundled in the repo; supply your own.

**Licensing/auth**:
- `sample-vt/main.cpp` hardcodes a placeholder `const std::string authToken = "your-auto-token";`
  with the comment "get your auth token from Krisp dashboard" — this is the only sample requiring a
  credential beyond a model file, and it also needs the SDK built/linked with VT support (pulls in
  `Security`/`SystemConfiguration` frameworks on macOS).
- `wav-cli` has an `-DENABLE_LICENSING=ON` build option (sets `ENABLE_LICENSING` compile define) but
  no further licensing detail is in this repo.

## python/

Not tied to `native-cpp` — a plain pip-installable sample:
```
pip install -r python/requirements.txt   # krisp_audio>=1.0.0, soundfile
python3 python/krisp_audio_test.py -cfg <model.kef> -i <input.wav> [-d <frame_ms>] [-nsl <0-100>]
```
`krisp_audio` is the SDK's Python binding (published from the sibling `krisp-audio-sdk-py` repo in
this umbrella) — this sample consumes it as an ordinary dependency, it does not build it. Mono
PCM16/FLOAT WAV only (raises on multi-channel input); output is written next to the input as
`<name>_nvc_<frame>ms_<rate>hz.wav`.

This umbrella's shared-venv convention (one Python env for everything except `kms-scripts`, path
per-developer) is not required here — this sample only needs `krisp_audio` + `soundfile` in
whatever interpreter you use; ask the developer for the shared venv path only if they want to reuse
that env instead of a fresh one.

## Gotchas

- **`javascript/sample-vt` is not a real sample** — no package.json, imports a nonexistent
  `@anthropic/krisp-vt-sdk` package, placeholder API key. Don't try to `npm install`/run it as-is.
- **`-DCOPY_KRISP_SDK` is one-shot**: re-running CMake with it set after the first successful copy
  aborts with `Krisp SDK already exists at: .../native-cpp/krisp-sdk`; drop the flag or delete that
  directory.
- **Windows MSVC runtime must match the SDK variant exactly** (`win_x64_mt`/`_mtd`/`_md`/`_mdd`
  naming in the SDK archive tells you which); mismatches fail at link time, not configure time.
- **`sample-ar`'s CMake option is `BUILD_SAMPLE_AL`**, not `BUILD_SAMPLE_AR` — easy to typo, and it's
  omitted from the option table in `native-cpp/README.md` entirely.
- **No sample in this repo bundles a model** (`.kef`) or an SDK build — every platform expects you to
  supply the SDK artifact (zip/xcframework/wheel/static libs) and a model file yourself; none of that
  is checked into this repo.
