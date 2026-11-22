#!/usr/bin/env bash
# Compiles extras/test_build for every modem driver the way ESP-IDF 6.x builds a
# component (Arduino as an ESP-IDF component): -std=gnu++26 with -Wall -Wextra
# -Werror, against the headers, sdkconfig and GCC of an installed arduino-esp32
# core (3.x or 4.x).  The Arduino IDE builds sketches as C++20 with most
# warnings off, so this catches what only an ESP-IDF build would reject.
#
# Usage: extras/tests/idf_flags_check.sh [chip] [extra -D defines...]
#   chip    esp32s3 (default), esp32, esp32c3, esp32c6, esp32p4, ...
#   e.g.    extras/tests/idf_flags_check.sh esp32s3 TINY_GSM_USE_MUTEX
# Environment:
#   ARDUINO_DATA  Arduino data directory (default ~/.arduino15, or
#                 %LOCALAPPDATA%/Arduino15 on Windows)
#   MODEMS        space-separated list of modems (default: one per driver)
#   STD           C++ standard (default gnu++26)
#   JOBS          parallel compiles (default: number of CPUs)
set -euo pipefail

chip="${1:-esp32s3}"
shift || true
defines=("$@")

here="$(cd "$(dirname "$0")" && pwd)"
lib="$here/../.."
sketch="$lib/extras/test_build/test_build.ino"

if [ -z "${ARDUINO_DATA:-}" ]; then
  if [ -n "${LOCALAPPDATA:-}" ] && [ -d "$LOCALAPPDATA/Arduino15" ]; then
    ARDUINO_DATA="$LOCALAPPDATA/Arduino15"
  else
    ARDUINO_DATA="$HOME/.arduino15"
  fi
fi
pkg="$ARDUINO_DATA/packages/esp32"
core="$(ls -d "$pkg"/hardware/esp32/*/ | sort -V | tail -1)"
core="${core%/}"
ver="$(basename "$core")"
libs="$pkg/tools/${chip}-libs/$ver"
[ -d "$libs" ] || libs="$(ls -d "$pkg"/tools/esp32-arduino-libs/*/ 2>/dev/null | sort -V | tail -1)$chip"
[ -d "$libs" ] || { echo "No ${chip} libraries found for core $ver" >&2; exit 2; }
case "$chip" in
  esp32 | esp32s2 | esp32s3)
    gxx="$(ls -d "$pkg"/tools/esp-x32/*/ | sort -V | tail -1)bin/xtensa-${chip}-elf-g++" ;;
  *)
    gxx="$(ls -d "$pkg"/tools/esp-rv32/*/ | sort -V | tail -1)bin/riscv32-esp-elf-g++" ;;
esac
mem="$(ls -d "$libs"/*/ | grep -E '(qio|dio|opi)_' | head -1)"

MODEMS="${MODEMS:-SIM800 SIM808 SIM900 SIM7000 SIM7000SSL SIM7080 SIM5360 \
SIM7600 UBLOX SARAR4 SARAR5 M95 BG96 BG95SSL A6 M590 MC60 ESP32 XBEE \
SEQUANS_MONARCH A7672X}"
STD="${STD:-gnu++26}"
JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT

echo "arduino-esp32 $ver, $chip, -std=$STD, defines: ${defines[*]:-(none)}"
"$gxx" --version | head -1

compile_one() {
  local m=$1 src="$out/$1.cpp"
  {
    for d in "${defines[@]}"; do echo "#define ${d/=/ }"; done
    echo "#define TINY_GSM_MODEM_$m"
    echo "#include <Arduino.h>"
    cat "$sketch"
  } > "$src"
  # Last flags win: the -std and warning flags override the core's cpp_flags
  if (cd "$libs" && "$gxx" -c @flags/cpp_flags -Os -DF_CPU=240000000L \
      -DARDUINO=10607 -DARDUINO_ARCH_ESP32 -DESP32=ESP32 -DCORE_DEBUG_LEVEL=0 \
      -DARDUINO_RUNNING_CORE=1 -DARDUINO_EVENT_RUNNING_CORE=1 \
      -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=0 @flags/defines \
      -iprefix "$libs/include/" @flags/includes -I"${mem}include" \
      -I"$core/cores/esp32" -I"$core/variants/$chip" -I"$lib/src" \
      -std="$STD" -Wall -Wextra -Werror "$src" -o "$out/$m.o") \
      > "$out/$m.log" 2>&1; then
    echo "PASS $m"
  else
    echo "FAIL $m"
    sed 's/^/    /' "$out/$m.log" | grep -E 'error|warning' | head -20
    return 1
  fi
}
export -f compile_one
export out sketch libs gxx mem core lib chip STD
export DEFINES_SERIALIZED="${defines[*]:-}"

status=0
# shellcheck disable=SC2086
printf '%s\n' $MODEMS |
  xargs -P "$JOBS" -I{} bash -c \
    'defines=($DEFINES_SERIALIZED); compile_one "$@"' _ {} || status=1
exit $status
