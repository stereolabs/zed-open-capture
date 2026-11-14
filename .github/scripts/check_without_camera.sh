#!/bin/bash -e

# Builds every configuration of the library and verifies what can be verified without a camera.
# Used by the CI, it can also be started on a development computer:
#   CXX=g++ .github/scripts/check_without_camera.sh

SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${WORK_DIR:-$(mktemp -d)}"
JOBS="$(nproc)"

echo "::group::Full build with tests, warnings are errors"
cmake -S "$SRC" -B "$WORK/full" -DBUILD_TESTS=ON -DDEBUG_CAM_REG=ON -DZED_OC_WARNINGS_AS_ERRORS=ON \
      -DCMAKE_INSTALL_PREFIX="$WORK/install"
cmake --build "$WORK/full" -j"$JOBS"
echo "::endgroup::"

echo "::group::Run without a camera"
# The tests that need a camera are skipped: the others must pass and nothing must crash
for t in test_video test_sensors test_sync; do
    "$WORK/full/tests/$t" --gtest_filter=-Setup.*
done

# The tools must report the missing camera
out="$("$WORK/full/zed_open_capture_benchmark" 2>&1 || true)"
echo "$out"
echo "$out" | grep -q "No Stereolabs camera detected"
echo "::endgroup::"

echo "::group::Installed package used by an application: CMake"
cmake --install "$WORK/full" > /dev/null
cmake -S "$SRC/tests/package" -B "$WORK/package" -DCMAKE_PREFIX_PATH="$WORK/install"
cmake --build "$WORK/package"
LD_LIBRARY_PATH="$WORK/install/lib" "$WORK/package/package_test"
echo "::endgroup::"

echo "::group::Installed package used by an application: pkg-config"
export PKG_CONFIG_PATH="$WORK/install/lib/pkgconfig"
# shellcheck disable=SC2046
${CXX:-c++} -std=c++11 "$SRC/tests/package/main.cpp" $(pkg-config --cflags --libs zed_open_capture) -o "$WORK/pkgconfig_test"
LD_LIBRARY_PATH="$WORK/install/lib" "$WORK/pkgconfig_test"
echo "::endgroup::"

for options in "-DBUILD_SENSORS=OFF" "-DBUILD_VIDEO=OFF" "-DBUILD_EXAMPLES=OFF"; do
    echo "::group::Build with $options"
    cmake -S "$SRC" -B "$WORK/partial" $options -DBUILD_TESTS=ON -DZED_OC_WARNINGS_AS_ERRORS=ON
    cmake --build "$WORK/partial" -j"$JOBS"
    rm -rf "$WORK/partial"
    echo "::endgroup::"
done

if [ "${SANITIZERS:-0}" = "1" ]; then
    for sanitizer in address thread; do
        echo "::group::Build and run with the $sanitizer sanitizer"
        cmake -S "$SRC" -B "$WORK/$sanitizer" -DBUILD_TESTS=ON -DBUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo \
              -DZED_OC_SANITIZER=$sanitizer
        cmake --build "$WORK/$sanitizer" -j"$JOBS"
        for t in test_video test_sensors test_sync; do
            "$WORK/$sanitizer/tests/$t" --gtest_filter=-Setup.*
        done
        echo "::endgroup::"
    done
fi

echo "All checks passed"
