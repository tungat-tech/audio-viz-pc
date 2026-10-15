# Third-party dependencies

CMake fetches these pinned upstream source versions on the first configure:

- [SDL 2.30.12](https://github.com/libsdl-org/SDL/tree/release-2.30.12) —
  zlib license (`licenses/SDL2-LICENSE.txt`).
- [LVGL 8.4.0](https://github.com/lvgl/lvgl/tree/v8.4.0) — MIT license
  (`licenses/LVGL-LICENCE.txt`).
- [miniaudio 0.11.22](https://github.com/mackron/miniaudio/tree/0.11.22) —
  public domain or MIT No Attribution option (`licenses/miniaudio-LICENSE.txt`).
- [curl](https://curl.se/) — curl license; used through the system library for
  HTTPS market-data requests on Linux and macOS. Windows uses the operating
  system WinHTTP API instead.

Distribution packages include these notices alongside the project license.

## Data services

- Public cryptocurrency market data is requested from the Binance Spot REST API.
- Weather and place search data are requested from Open-Meteo; location search
  data is based on GeoNames. Review the provider terms before commercial use.
