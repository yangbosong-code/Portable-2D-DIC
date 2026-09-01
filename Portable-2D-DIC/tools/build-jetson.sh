#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"
cd "${project_root}"

if [[ -z "${GALAXY_SDK_ROOT:-}" ]]; then
  echo "Set GALAXY_SDK_ROOT to the ARM64 GalaxySDK development directory." >&2
  exit 2
fi

cmake --preset jetson-release
cmake --build --preset build-jetson --parallel "$(nproc)"
ctest --preset test-jetson --output-on-failure
