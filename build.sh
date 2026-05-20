#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-4}"
RUN_TESTS="${RUN_TESTS:-1}"

for arg in "$@"; do
  case "$arg" in
    --debug) BUILD_TYPE="Debug" ;;
    --release) BUILD_TYPE="Release" ;;
    --no-test) RUN_TESTS="0" ;;
    --test) RUN_TESTS="1" ;;
    *)
      echo "Unknown arg: $arg"
      echo "Usage: ./build.sh [--debug|--release] [--test|--no-test]"
      exit 1
      ;;
  esac
done

echo "[optix] configure: $BUILD_DIR ($BUILD_TYPE)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "[optix] build"
cmake --build "$BUILD_DIR" -j"$JOBS"

if [[ "$RUN_TESTS" == "1" ]]; then
  echo "[optix] test"
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "[optix] done"
