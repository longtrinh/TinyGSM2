#!/usr/bin/env bash
# Builds and runs the host-side tests (g++ on Linux, used by the CI workflow).
#
# Usage: extras/tests/run_tests.sh <PubSubClient src dir>...
#   e.g. extras/tests/run_tests.sh deps/pubsubclient/src deps/pubsubclient3/src
# The PubSubClient test is built once against each PubSubClient given.
# Set CXX to use another compiler and OUT_DIR to keep the binaries.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
src="$here/../../src"
out="${OUT_DIR:-$(mktemp -d)}"
mkdir -p "$out"
CXX="${CXX:-g++}"
# C++11, as the oldest supported cores (AVR) use; sanitizers catch buffer
# overruns in the FIFO / socket code
CXXFLAGS=(-std=gnu++11 -g -O1 -Wall -Wextra
  -fsanitize=address,undefined -fno-sanitize-recover=all
  -I"$here/stubs" -I"$here" -I"$src")

status=0
run() {
  if ! "$@"; then status=1; fi
}

echo "::group::Build test_sslclient"
"$CXX" "${CXXFLAGS[@]}" "$here/test_sslclient.cpp" -o "$out/test_sslclient"
echo "::endgroup::"
run "$out/test_sslclient"

n=0
for psc in "$@"; do
  n=$((n + 1))
  echo "::group::Build test_pubsubclient against $psc"
  "$CXX" "${CXXFLAGS[@]}" -I"$psc" -DPSC_LABEL="\"$psc\"" \
    "$here/test_pubsubclient.cpp" "$psc/PubSubClient.cpp" \
    -o "$out/test_pubsubclient_$n"
  echo "::endgroup::"
  run "$out/test_pubsubclient_$n"
done

exit $status
