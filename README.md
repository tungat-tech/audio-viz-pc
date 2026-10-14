# AudioViz PC

Open-source desktop audio visualizer. This repository is the **PC-only** viewer
for early user testing. It captures the default system output on Windows with
WASAPI loopback and renders VU, spectrum, oscilloscope and waveform styles.

This repository intentionally contains **no ESP32 firmware, flashing tool,
device protocol, serial sender or board-specific calibration**. The desktop
visual analysis is an independent implementation; it may not produce identical
bar movement to a device build.

## Windows build

Install Visual Studio 2022 with the Desktop development with C++ workload and
CMake 3.21 or newer. In a Developer PowerShell:

```powershell
./build_windows.ps1
```

Or run the commands manually:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The first configure downloads pinned SDL 2.30.12, LVGL 8.4.0 and miniaudio
0.11.22. Run `build\Release\AudioViz_PC.exe` (or find the executable in the
selected generator's build directory).

## GitHub Actions builds and releases

Every push to `main` and pull request targeting `main` builds and tests on a
GitHub-hosted Windows 2022 runner. The resulting `audioviz-pc-windows-x64`
artifact is available from the workflow run for 14 days. It contains a ZIP
with the executable and license files plus `SHA256SUMS.txt`.

To publish a release, create and push a new `v*` tag on a tested commit:

```bash
git tag v0.1.0-beta.1
git push origin v0.1.0-beta.1
```

The tag run rebuilds and tests the source on Windows, then publishes the ZIP
and checksum to GitHub Releases using the repository's `GITHUB_TOKEN`. Local
`gh` login is not needed for this workflow. Tag pushes should be reserved for
maintainers; pull requests cannot publish a release.

## Run

```text
AudioViz_PC.exe
AudioViz_PC.exe --demo
AudioViz_PC.exe --list-audio-devices
AudioViz_PC.exe --audio-device "device name"
AudioViz_PC.exe --demo --mode 2 --style 3
```

Press **Space** or click **MODE** to change view. In Spectrum, click **STYLE**
to cycle styles. The Spectrum styles are Bars, Trace, Dots, Pulse, Prism,
Mirror and Waveform (`--style 0` through `--style 6`, respectively). Bars uses
discrete columns, Pulse a continuous two-sided energy ribbon, and Prism
faceted frequency shards. [Bars](previews/spectrum-bars.png),
[Pulse](previews/spectrum-pulse.png) and [Prism](previews/spectrum-prism.png)
are screenshots from the same demo signal. `--demo` is a synthetic signal for UI
testing; live capture uses the playback device. `--screenshot output.bmp --frames 60` exits after
60 display frames and saves a screenshot.

The current viewer uses SDL2/LVGL. A native Windows application shell is a
separate planned step; this repository does not claim to be that shell.

## Scope and license

Source in this repository is MIT-licensed. Third-party dependencies retain
their own licenses; see `THIRD_PARTY.md`. No purchase or device is required to
run the PC visualizer. Issues and test feedback should include Windows version,
audio output device, capture mode, screenshot and a short reproduction path.
