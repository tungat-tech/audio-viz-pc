#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")" && pwd)"
build_dir="${1:-$repo_dir/build}"

cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

if [[ "$(uname -s)" == "Darwin" ]]; then
  app="$build_dir/AudioViz_PC.app/Contents/MacOS/AudioViz_PC"
else
  app="$build_dir/AudioViz_PC"
fi

test -x "$app"
SDL_VIDEODRIVER=dummy "$app" --demo --mode 1 --style 9 --frames 4 \
  --screenshot "$build_dir/smoke.bmp"
test -s "$build_dir/smoke.bmp"
printf 'Ready: %s\n' "$app"
