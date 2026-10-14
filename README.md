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
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target AudioViz_PC
ctest --test-dir build -C Release --output-on-failure
```

The first configure downloads pinned SDL 2.30.12, LVGL 8.4.0 and miniaudio
0.11.22. Run `build\Release\AudioViz_PC.exe` (or find the executable in the
selected generator's build directory).

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
