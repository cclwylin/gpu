#!/usr/bin/env bash
# Sprint 42 — Build VK-GL-CTS (`deqp-gles2`) against the in-tree gpu_glcompat
# library. Idempotent: re-running just rebuilds, preserves the cloned externals.
#
# Prereq: the main project already built (./build with libgpu_glcompat.a etc.).
#
# Usage:
#   tools/build_vkglcts.sh           # configure + build deqp-gles2
#   tools/build_vkglcts.sh --clean   # nuke build_vkglcts/ first
#   tools/build_vkglcts.sh --fetch   # ensure externals are present (one-shot)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CTS_DIR="${ROOT}/tests/VK-GL-CTS"

# Per-arch build dirs so an exFAT volume can be shared across Intel + Apple
# Silicon Macs without trampling caches. Override by exporting
# VKGLCTS_BUILD_DIR (this script's CTS out-of-tree build) and MAIN_BUILD (the
# top-level project's build dir). `source tools/setup_env.sh` populates both.
# Falls back to plain build/ + build_vkglcts/ on Linux/CI.
_arch="$(uname -m)"
case "$(uname -s)" in
    Darwin) _suffix="-${_arch}" ;;
    *)      _suffix="" ;;
esac
BUILD_DIR="${VKGLCTS_BUILD_DIR:-${ROOT}/build_vkglcts${_suffix}}"
MAIN_BUILD="${MAIN_BUILD:-${ROOT}/build${_suffix}}"

# Pinned revisions (mirror tests/VK-GL-CTS/external/fetch_sources.py).
SPIRV_HEADERS_REV="6dd7ba990830f7c15ac1345ff3b43ef6ffdad216"
VULKAN_DOCS_REV="6019efe93eca7bfa7a692316d91a9465eb457d60"

ensure_externals() {
    local sh_dir="${CTS_DIR}/external/spirv-headers/src"
    if [ ! -f "${sh_dir}/include/spirv/unified1/spirv.hpp11" ]; then
        echo "[vkglcts] cloning spirv-headers @ ${SPIRV_HEADERS_REV}"
        rm -rf "${sh_dir}"
        git clone --quiet https://github.com/KhronosGroup/SPIRV-Headers.git "${sh_dir}"
        (cd "${sh_dir}" && git fetch --quiet --depth 1 origin "${SPIRV_HEADERS_REV}" \
                       && git checkout --quiet "${SPIRV_HEADERS_REV}")
    fi
    local vd_dir="${CTS_DIR}/external/vulkan-docs/src"
    if [ ! -d "${vd_dir}" ]; then
        echo "[vkglcts] cloning vulkan-docs @ ${VULKAN_DOCS_REV}"
        git clone --quiet https://github.com/KhronosGroup/Vulkan-Docs.git "${vd_dir}"
        (cd "${vd_dir}" && git fetch --quiet --depth 1 origin "${VULKAN_DOCS_REV}" \
                       && git checkout --quiet "${VULKAN_DOCS_REV}")
    fi
}

if [ "${1:-}" = "--clean" ]; then
    rm -rf "${BUILD_DIR}"
    shift || true
fi

if [ "${1:-}" = "--fetch" ]; then
    ensure_externals
    exit 0
fi

ensure_externals

if [ ! -f "${MAIN_BUILD}/glcompat/libgpu_glcompat.a" ]; then
    echo "[vkglcts] main project not built — run 'cmake --build ${MAIN_BUILD}' first" >&2
    exit 1
fi

# macOS libc++ shim — same fallback as the top-level CMakeLists. When the
# active CLT install is half-installed (only ~12 of ~190 libc++ headers)
# even <type_traits> won't resolve. Inject the SDK's libc++ as -isystem
# so the CTS build survives without a CLT reinstall. Harmless when CLT
# is healthy.
extra_cxx=""
if [ "$(uname -s)" = "Darwin" ]; then
    sdk_path="$(xcrun --show-sdk-path 2>/dev/null || true)"
    if [ -n "${sdk_path}" ] && [ -f "${sdk_path}/usr/include/c++/v1/cstdint" ]; then
        extra_cxx="-isystem ${sdk_path}/usr/include/c++/v1"
    fi
fi

cmake -S "${CTS_DIR}" -B "${BUILD_DIR}" \
      -DDEQP_TARGET=glcompat \
      -DGPU_PROJECT_BUILD="${MAIN_BUILD}" \
      ${extra_cxx:+-DCMAKE_CXX_FLAGS="${extra_cxx}"} \
      ${extra_cxx:+-DCMAKE_C_FLAGS="${extra_cxx}"}
cmake --build "${BUILD_DIR}" --target deqp-gles2 -j 4

echo
echo "[vkglcts] deqp-gles2 binary: ${BUILD_DIR}/modules/gles2/deqp-gles2"
