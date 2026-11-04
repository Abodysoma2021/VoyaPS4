#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk_path="${1:?Usage: tools/publish-release.sh /absolute/path/to/OpenOrbis/PS4Toolchain}"
[[ "$(git branch --show-current)" == main ]] || { printf 'Release from main\n' >&2; exit 1; }
[[ -z "$(git status --porcelain)" ]] || { printf 'Commit your changes before releasing\n' >&2; exit 1; }
version="$(cat VERSION)"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
repository="Abodysoma2021/VoyaPS4"
[[ "$(gh repo view --json nameWithOwner --jq .nameWithOwner)" == "$repository" ]] || exit 1
if gh release view "v$version" --repo "$repository" >/dev/null 2>&1; then
    printf 'This version already exists. Bump VERSION; release assets are immutable.\n' >&2
    exit 1
fi
if git rev-parse --verify "refs/tags/v$version" >/dev/null 2>&1; then
    [[ "$(git rev-parse "v$version^{commit}")" == "$(git rev-parse HEAD)" ]]
fi
python3 tools/verify-deps.py
make test
tools/docker-build.sh "$sdk_path"
git push origin main
if ! git rev-parse --verify "refs/tags/v$version" >/dev/null 2>&1; then
    git tag -a "v$version" -m "VoyaPS4 $version"
fi
git push origin "v$version"
gh release create "v$version" dist/VoyaPS4.pkg dist/update.json dist/SHA256SUMS \
    --repo "$repository" --verify-tag --title "VoyaPS4 $version" --notes-file docs/RELEASE-NOTES.md
