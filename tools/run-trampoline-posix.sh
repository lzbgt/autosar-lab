#!/usr/bin/env bash
# Build and run real Trampoline OS periodic example in ignored third_party.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
osroot="$root/third_party/trampoline"
if [[ ! -d "$osroot/.git" ]]; then
  bash "$root/tools/fetch-upstreams.sh" trampoline
fi
expected=ff287023252bf73273067f00d2dbc8bbc9a20401
if [[ "$(git -C "$osroot" rev-parse HEAD)" != "$expected" ]]; then
  echo "Trampoline revision mismatch; refusing to run unknown source" >&2
  exit 1
fi
goil="$osroot/goil/makefile-unix/goil"
if [[ ! -x "$goil" ]]; then
  (cd "$osroot/goil/makefile-unix" && python3 build.py all 4)
fi
make -C "$osroot/viper" -j4
cd "$osroot/examples/posix/periodic"
"$goil" --target=posix/linux --templates=../../../goil/templates/ periodic.oil
PATH="$osroot/goil/makefile-unix:$PATH" ./make.py -s
echo "Executing pinned Trampoline OSEK/AUTOSAR OS example:"
output="$(VIPER_PATH="$osroot/viper" timeout 20 ./periodic_exe)"
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -q 'Activation #1'
printf '%s\n' "$output" | grep -q 'Shutdown'
echo "PASS: real Trampoline POSIX task scheduling and orderly shutdown"
