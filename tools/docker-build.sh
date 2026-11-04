#!/usr/bin/env bash
set -euo pipefail
sdk_path="${1:?Usage: tools/docker-build.sh /absolute/path/to/OpenOrbis/PS4Toolchain}"
project_path="$(cd "$(dirname "$0")/.." && pwd)"
docker build --platform linux/amd64 -t voyaps4-builder:0.1 -f "$project_path/tools/Dockerfile" "$project_path/tools"
build_number="$(git -C "$project_path" rev-list --count HEAD)"
docker run --rm --platform linux/amd64 -e VOYA_BUILD_NUMBER="$build_number" \
    -v "$project_path:/work" -v "$sdk_path:/sdk:ro" voyaps4-builder:0.1 make ps4
