#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "bootstrap-pi.sh must be run on Linux/Raspberry Pi OS" >&2
    exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "apt-get not found; this script expects Raspberry Pi OS or a Debian-based system" >&2
    exit 1
fi

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libcamera-dev \
    libcamera-apps

"${repo_root}/scripts/test-core-pi.sh"
