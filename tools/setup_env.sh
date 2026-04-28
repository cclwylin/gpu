#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# tools/setup_env.sh — pick per-host build dir + brew prefix
# -----------------------------------------------------------------------------
# This repo lives on an exFAT external volume that's shared between an Intel
# iMac (x86_64) and a Mac mini M4 (arm64). CMake caches the host arch + abs
# compiler paths into the build dir, so the two hosts MUST not share one
# build/. This script computes:
#
#   ARCH        — uname -m (x86_64 or arm64)
#   BUILD_DIR   — absolute path of build-${ARCH}/
#   VKGLCTS_BUILD_DIR — absolute path of build_vkglcts-${ARCH}/
#   BREW_PREFIX — /usr/local on Intel, /opt/homebrew on Apple Silicon
#   CMAKE_PREFIX_PATH — points at brew so find_package() works
#
# Usage:
#     source tools/setup_env.sh           # exports the vars into your shell
#     ./tools/setup_env.sh --print        # just print what it would set
#     ./tools/setup_env.sh --configure    # source + run cmake configure
# -----------------------------------------------------------------------------
# Note: no `set -u` so the script can be sourced from zsh (where BASH_SOURCE
# is unset). `set -e` still applies when executed; when sourced, an `eval`-ed
# error would kill the user's shell, so we keep failures contained to commands
# that we explicitly check.
set -eo pipefail

# Resolve "this file's path" portably across bash (BASH_SOURCE) and zsh (%x).
if [ -n "${ZSH_VERSION:-}" ]; then
    _SELF="${(%):-%x}"
else
    _SELF="${BASH_SOURCE[0]:-$0}"
fi
ROOT="$(cd "$(dirname "${_SELF}")/.." && pwd)"
ARCH="$(uname -m)"
OS="$(uname -s)"

if [ "${OS}" = "Darwin" ]; then
    SUFFIX="-${ARCH}"
    case "${ARCH}" in
        arm64)  BREW_PREFIX="/opt/homebrew" ;;
        x86_64) BREW_PREFIX="/usr/local"    ;;
        *)      BREW_PREFIX=""              ;;
    esac
else
    SUFFIX=""
    BREW_PREFIX=""
fi

export ARCH OS
export BUILD_DIR="${ROOT}/build${SUFFIX}"
export VKGLCTS_BUILD_DIR="${ROOT}/build_vkglcts${SUFFIX}"
export MAIN_BUILD="${BUILD_DIR}"  # alias used by tools/build_vkglcts.sh

# Per-arch Python venv lives off the exFAT volume because macOS keeps
# creating AppleDouble `._*` shadows inside `site-packages/` which
# Python's site loader chokes on as bogus `.pth` files. Each Mac's
# `~/.local/share/gpu-venv/<arch>/` holds its own copy; the `.venv/`
# stub on the shared drive dispatches via `uname -m`.
export VENV_DIR="${HOME}/.local/share/gpu-venv/${ARCH}"

if [ -n "${BREW_PREFIX}" ] && [ -d "${BREW_PREFIX}" ]; then
    export BREW_PREFIX
    export CMAKE_PREFIX_PATH="${BREW_PREFIX}${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"
    export PATH="${BREW_PREFIX}/bin:${PATH}"
fi

print_env() {
    cat <<EOF
[setup_env] OS=${OS}  ARCH=${ARCH}
[setup_env] BUILD_DIR=${BUILD_DIR}
[setup_env] VKGLCTS_BUILD_DIR=${VKGLCTS_BUILD_DIR}
[setup_env] BREW_PREFIX=${BREW_PREFIX:-<n/a>}
[setup_env] CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-<unset>}
[setup_env] VENV_DIR=${VENV_DIR}  $([ -f "${VENV_DIR}/bin/python" ] && echo "(ready)" || echo "(not bootstrapped — run --bootstrap-venv)")
EOF
}

bootstrap_venv() {
    if [ ! -d "${ROOT}/third_party" ] || [ ! -f "${ROOT}/third_party/requirements.txt" ]; then
        echo "[setup_env] requirements.txt not found at ${ROOT}/third_party/" >&2
        return 1
    fi
    mkdir -p "$(dirname "${VENV_DIR}")"
    if [ ! -f "${VENV_DIR}/bin/python" ]; then
        echo "[setup_env] creating venv at ${VENV_DIR}"
        python3 -m venv "${VENV_DIR}"
    else
        echo "[setup_env] venv already at ${VENV_DIR} — refreshing pip + deps"
    fi
    "${VENV_DIR}/bin/pip" install --quiet --upgrade pip
    "${VENV_DIR}/bin/pip" install --quiet -r "${ROOT}/third_party/requirements.txt"
    echo "[setup_env] done — activate with:  source ${ROOT}/.venv/activate"
}

run_configure() {
    print_env
    local gen="Unix Makefiles"
    if command -v ninja >/dev/null 2>&1; then
        gen="Ninja"
    fi
    echo "[setup_env] cmake -G '${gen}' -S '${ROOT}' -B '${BUILD_DIR}'"
    cmake -G "${gen}" -S "${ROOT}" -B "${BUILD_DIR}" \
          -DCMAKE_BUILD_TYPE=Release \
          ${CMAKE_PREFIX_PATH:+-DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}"}
}

# When sourced, return after exporting. When executed, fall through to the
# flag handler. The `(return 0 2>/dev/null)` subshell trick distinguishes
# sourced (returns 0) vs executed (returns nonzero) in both bash and zsh.
if (return 0 2>/dev/null); then
    return 0
fi

case "${1:---print}" in
    --print)           print_env ;;
    --configure)       run_configure ;;
    --bootstrap-venv)  bootstrap_venv ;;
    *) echo "usage: $0 [--print|--configure|--bootstrap-venv]" >&2; exit 2 ;;
esac
