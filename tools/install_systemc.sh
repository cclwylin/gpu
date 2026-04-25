#!/usr/bin/env bash
# Sprint 35 — install SystemC with the project's compiler.
#
# Background: the macOS-prebuilt /usr/local/systemc-2.3.4 ships with
# AppleClang/libc++. Our project compiles with GCC-15/libstdc++ for
# unrelated reasons; the resulting libc++/libstdc++ ABI mismatch
# means SystemC test executables can't link locally (they're gated
# to Docker by default).
#
# This script builds SystemC from the official Accellera source with
# whatever compiler the project itself uses, installs to
# $HOME/.local/systemc-<version>-gcc/ (no sudo needed), and prints
# the env-var line you should add to your shell rc.
#
# Usage:
#     tools/install_systemc.sh
#     # then:
#     export SYSTEMC_HOME=$HOME/.local/systemc-2.3.4-gcc
#     cmake -S . -B build -DGPU_BUILD_SYSTEMC=ON \
#           -DGPU_FORCE_SYSTEMC_TESTS=ON \
#           -DCMAKE_CXX_COMPILER=g++-15
#     cmake --build build -j8
#     ctest --test-dir build --output-on-failure
#
# Knobs (all optional):
#     SYSTEMC_VERSION   default 2.3.4 (matches the existing macOS path style)
#     SYSTEMC_PREFIX    default $HOME/.local/systemc-<version>-gcc
#     CXX               default g++-15  (Homebrew GCC; matches project)
#
# This script is idempotent: if the install already exists, it
# refuses to overwrite and prints what to do.

set -euo pipefail

VERSION="${SYSTEMC_VERSION:-2.3.4}"
CXX_COMPILER="${CXX:-g++-15}"
PREFIX="${SYSTEMC_PREFIX:-$HOME/.local/systemc-${VERSION}-gcc}"

echo "==> SystemC ${VERSION}"
echo "    compiler  = ${CXX_COMPILER}"
echo "    prefix    = ${PREFIX}"

if [ -d "${PREFIX}/lib/cmake/SystemCLanguage" ]; then
    echo
    echo "Already installed at ${PREFIX}."
    echo "  - To reinstall: rm -rf '${PREFIX}' && rerun."
    echo "  - To use it:    export SYSTEMC_HOME='${PREFIX}'"
    exit 0
fi

if ! command -v "${CXX_COMPILER}" >/dev/null 2>&1; then
    echo "error: '${CXX_COMPILER}' not found on PATH." >&2
    echo "       (on macOS try: brew install gcc)" >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake not found on PATH (brew install cmake)." >&2
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT
echo "    workdir   = ${WORK}"

echo
echo "==> cloning Accellera systemc v${VERSION}"
git clone --depth 1 --branch "${VERSION}" \
    https://github.com/accellera-official/systemc.git "${WORK}/src"

echo
echo "==> configuring (Release, C++20, ${CXX_COMPILER})"
cmake -S "${WORK}/src" -B "${WORK}/build" \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DCMAKE_CXX_COMPILER="${CXX_COMPILER}" \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    >"${WORK}/configure.log" 2>&1 || {
        echo "configure failed — see ${WORK}/configure.log" >&2
        tail -20 "${WORK}/configure.log" >&2
        exit 1
    }

echo "==> building (this is the slow part — ~1–2 minutes)"
cmake --build "${WORK}/build" -j"$(getconf _NPROCESSORS_ONLN || echo 4)" \
    >"${WORK}/build.log" 2>&1 || {
        echo "build failed — see ${WORK}/build.log" >&2
        tail -40 "${WORK}/build.log" >&2
        exit 1
    }

echo "==> installing to ${PREFIX}"
cmake --install "${WORK}/build" >"${WORK}/install.log" 2>&1

echo
echo "==> done."
echo
echo "Next steps:"
echo "  export SYSTEMC_HOME='${PREFIX}'                       # add to ~/.zshrc"
echo "  cmake -S . -B build -DGPU_BUILD_SYSTEMC=ON \\"
echo "        -DGPU_FORCE_SYSTEMC_TESTS=ON \\"
echo "        -DCMAKE_CXX_COMPILER='${CXX_COMPILER}'"
echo "  cmake --build build -j$(getconf _NPROCESSORS_ONLN || echo 8)"
echo "  ctest --test-dir build --output-on-failure"
echo
echo "Then:"
echo "  python3 tools/run_pattern.py triangle_white"
echo "  # → out/triangle_white.swref.ppm + out/triangle_white.sc.ppm + cycle counts"
