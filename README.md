# AudioViz PC

Open-source desktop audio visualizer for Windows, Linux and macOS. This repository
is the **desktop-only** viewer for early user testing. It renders Spectrum first,
followed by VU and Oscilloscope modes. Windows captures the default system output
with WASAPI loopback; Linux uses a PulseAudio/PipeWire monitor; macOS uses an
explicit CoreAudio loopback input such as BlackHole.

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

## Linux and macOS builds

Install CMake 3.21+, Ninja and a C/C++ compiler, then run:

```bash
./build_unix.sh
```

On Linux, install the usual SDL build dependencies plus PulseAudio development
headers. The app automatically selects the monitor belonging to the default
PulseAudio/PipeWire output when possible.

On macOS, the build produces `build/AudioViz_PC.app`. macOS does not provide a
built-in system-output loopback device, so install and route audio through a
CoreAudio loopback input such as BlackHole, list inputs with
`AudioViz_PC --list-audio-devices`, then select one with `--audio-device NAME`.
Release artifacts are currently unsigned, so Gatekeeper may require **Open** from
the Finder context menu on first launch.

## GitHub Actions builds and releases

Every push to `main` and pull request targeting `main` builds, tests and smoke-tests
the supported desktop targets. Workflow artifacts are retained for 14 days:

- Windows x64: standalone `.exe` and a ZIP.
- Linux x64: standalone executable and a `.tar.gz` package.
- macOS arm64: `.app.zip` for Apple Silicon.
- macOS x64: `.app.zip` for Intel Macs.

Each artifact includes its own SHA-256 checksum file. Tag releases additionally
publish one combined `SHA256SUMS.txt`.

To publish a release, create and push a new `v*` tag on a tested commit:

```bash
git tag v0.0.2
git push origin v0.0.2
```

The tag run rebuilds and tests all four targets, then publishes every package and
the combined checksum to GitHub Releases using the repository's `GITHUB_TOKEN`.
Local `gh` login is not needed for this workflow. Tag pushes should be reserved
for maintainers; pull requests cannot publish a release.

## Run

```text
AudioViz_PC.exe
AudioViz_PC.exe --demo
AudioViz_PC.exe --list-audio-devices
AudioViz_PC.exe --audio-device "device name"
AudioViz_PC.exe --demo --mode 1 --style 3
```

The app opens in Spectrum. `--mode 1` selects Spectrum, `--mode 2` selects VU,
and `--mode 3` selects Oscilloscope. Press **Space** or click **MODE** to change
view. In Spectrum, click **STYLE**
to cycle styles. The Spectrum styles are Bars, Trace, Dots, Pulse, Mirror,
Waveform, Halo, Waterfall, Ridgeline, Topo Bloom, Aurora Weave and Cymatic Mesh
(`--style 0` through `--style 11`, respectively). Bars uses discrete columns,
Pulse a continuous two-sided energy ribbon, and Halo a radial spectrum with a
live waveform ring. Waterfall scrolls recent spectral energy, Ridgeline layers
five recent spectra, Topo Bloom breathes through layered contours, Aurora Weave
uses slowly drifting textured ribbons, and Cymatic Mesh reveals audio-driven
interference nodes.
[Bars](previews/spectrum-bars.png),
[Pulse](previews/spectrum-pulse.png),
[Halo](previews/spectrum-halo.png),
[Waterfall](previews/spectrum-waterfall.png),
[Ridgeline](previews/spectrum-ridgeline.png),
[Topo Bloom](previews/spectrum-topo-bloom.png),
[Aurora Weave](previews/spectrum-aurora-weave.png) and
[Cymatic Mesh](previews/spectrum-cymatic-mesh.png)
are screenshots from the same demo signal. `--demo` is a synthetic signal for UI
testing; live capture uses the playback device. `--screenshot output.bmp --frames 60` exits after
60 display frames and saves a screenshot.

Halo is an original CPU-rendered radial view inspired by the general circular
spectrum idea in [Jasper Bernaers' music visualizer](https://jasperbernaers.com/music-visualizer/);
it uses this app's own 24-band spectrum and stereo waveform, not that site's shader.

The current viewer uses SDL2/LVGL. A native Windows application shell is a
separate planned step; this repository does not claim to be that shell.

## Scope and license

Source in this repository is MIT-licensed. Third-party dependencies retain
their own licenses; see `THIRD_PARTY.md`. No purchase or device is required to
run the PC visualizer. Issues and test feedback should include Windows version,
audio output device, capture mode, screenshot and a short reproduction path.
