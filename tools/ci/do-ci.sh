#! /usr/bin/env bash
#
# build and test stuff

set -ex

cmake -B build -DCMAKE_TOOLCHAIN_FILE=fwk_io/xmos_cmake_toolchain/xs3a.cmake
cmake --build build --target all --target test_app --target test_app_low_level_api --target simple --target i2s_slave  -j$(nproc)

pushd tests
pytest --junitxml=results.xml -rA -v --durations=0 -o junit_logging=all
ls bin
popd

