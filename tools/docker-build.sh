#!/usr/bin/env bash
set -euo pipefail
sdk_path="${1:?Usage: tools/docker-build.sh /absolute/path/to/OpenOrbis/PS4Toolchain}"
project_path="$(cd "$(dirname "$0")/.." && pwd)"
docker build --platform linux/amd64 -t voyaps4-builder:0.1 -f "$project_path/tools/Dockerfile" "$project_path/tools"
docker run --rm --platform linux/amd64 -e GITHUB_RUN_NUMBER="${GITHUB_RUN_NUMBER:-0}" \
    -v "$project_path:/work" -v "$sdk_path:/sdk:ro" voyaps4-builder:0.1 make ps4
