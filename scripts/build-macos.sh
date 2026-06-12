#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
INSTALL_PREFIX="${VINYL_REEL_INSTALL_PREFIX:-${HOME}/Library/Application Support/obs-studio/plugins}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
  -DVINYL_REEL_OBS_APP="/Applications/OBS.app"

cmake --build "${BUILD_DIR}" --target install
