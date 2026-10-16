# AudioViz UI Core

Shared, platform-neutral display model and LVGL visualizer used by AudioViz PC
and AudioViz device firmware. This directory in the public PC repository is the
canonical source; private device builds pin the PC repository revision.

## Scope

This repository owns:

- normalized visual and utility frames;
- mode/style layout and drawing code;
- shared fonts and color palettes;
- the optional indexed canvas used by constrained display targets.

It intentionally does **not** contain audio capture, DSP calibration, the
device wire protocol, UART/USB transport, firmware drivers, OTA, flashing, or
licensing code. Those belong to their application repositories.

## Consuming from CMake

The parent project must create an `lvgl` target first, then add this directory:

```cmake
add_subdirectory(libs/audioviz-ui)
target_link_libraries(my_app PRIVATE AudioViz::UiCore)
```

Device builds consume this directory through their pinned public-PC submodule.

## Data boundary

Applications convert their input into `audio::AudioFrame` and
`data::UtilityFrame`. The UI never reads WASAPI, microphone, network, serial,
or firmware state directly.

## License

MIT. Private device applications may link this library while keeping their
protocol, transport, firmware and calibration implementations private.

