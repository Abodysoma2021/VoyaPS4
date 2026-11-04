#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
sdk="${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to OpenOrbis v0.5.4}"
compiler="${CXX:-clang++}"
linker="${LD:-ld.lld}"
version="$(cat VERSION)"
content_id="IV0000-VOYA00001_00-VOYAPS4APP000001"
out="$PWD/build/ps4"
mkdir -p "$out/objects" "$out/package/sce_sys/about" "$out/package/assets/fonts" dist
flags=(--target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -std=c++17 -D_DEFAULT_SOURCE -O2
    -Wall -Wextra -Werror -Wno-unused-command-line-argument
    -isysroot "$sdk" -isystem "$sdk/include" -isystem "$sdk/include/c++/v1"
    -Isrc -Ivendor "-DVOYA_VERSION=\"$version\"")
for source in src/{metadata,sha256,transfer,library,ui}.cpp src/platform/{http_ps4,main_ps4}.cpp; do
    object="$out/objects/$(basename "$source" .cpp).o"
    extra=()
    if [[ "$source" == *main_ps4.cpp ]]; then extra=(-fshort-wchar); fi
    "$compiler" "${flags[@]}" "${extra[@]}" -c "$source" -o "$object"
done
"$linker" -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
    -L"$sdk/lib" "$out"/objects/*.o "$sdk/lib/crt1.o" \
    -lc++ -lc++abi -lc -lkernel -lSceVideoOut -lScePad -lSceUserService \
    -lSceSysmodule -lSceNet -lSceHttp -lSceSsl -lSceImeDialog -lSceCommonDialog \
    -lSceAppInstUtil -o "$out/VoyaPS4.elf"
"$sdk/bin/linux/create-fself" -in="$out/VoyaPS4.elf" -out="$out/VoyaPS4.oelf" \
    --eboot "$out/package/eboot.bin" --paid 0x3800000000000011
cp assets/catalog.json "$out/package/assets/"
cp assets/fonts/{SpaceGrotesk.ttf,OFL.txt} "$out/package/assets/fonts/"
cp LICENSE "$out/package/assets/LICENSE.txt"
cp docs/THIRD-PARTY.md "$out/package/assets/THIRD-PARTY.txt"
cp vendor/{json.LICENSE,stb.LICENSE} "$out/package/assets/"
cp assets/icon0.png "$out/package/sce_sys/icon0.png"
cp "$sdk/samples/hello_world/sce_sys/about/right.sprx" "$out/package/sce_sys/about/right.sprx"
pkgtool="$sdk/bin/linux/PkgTool.Core"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
"$pkgtool" sfo_new "$out/package/sce_sys/param.sfo"
# SFO version is monotonic across CI builds; semantic version is compiled into the app.
# A release rebuild requires a new tag/version. Local development defaults to 01.00.
app_ver="$(python3 -c 'import os; n=100+int(os.environ.get("GITHUB_RUN_NUMBER","0")); assert 100<=n<10000; print("%02d.%02d"%(n//100,n%100))')"
set_sfo() { "$pkgtool" sfo_setentry "$out/package/sce_sys/param.sfo" "$1" --type "$2" --maxsize "$3" --value "$4"; }
set_sfo APP_TYPE Integer 4 1
set_sfo APP_VER Utf8 8 "$app_ver"
set_sfo ATTRIBUTE Integer 4 0
set_sfo CATEGORY Utf8 4 gd
set_sfo CONTENT_ID Utf8 48 "$content_id"
set_sfo DOWNLOAD_DATA_SIZE Integer 4 0
set_sfo SYSTEM_VER Integer 4 0
set_sfo TITLE Utf8 128 VoyaPS4
set_sfo TITLE_ID Utf8 12 VOYA00001
set_sfo VERSION Utf8 8 "$app_ver"
cd "$out/package"
files="eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx assets/catalog.json assets/fonts/SpaceGrotesk.ttf assets/fonts/OFL.txt assets/LICENSE.txt assets/THIRD-PARTY.txt assets/json.LICENSE assets/stb.LICENSE"
"$sdk/bin/linux/create-gp4" -out "$out/package/package.gp4" --content-id="$content_id" --files "$files"
"$pkgtool" pkg_build "$out/package/package.gp4" "$out"
cp "$out/$content_id.pkg" "$OLDPWD/dist/VoyaPS4.pkg"
"$pkgtool" pkg_validate "$OLDPWD/dist/VoyaPS4.pkg"
cd "$OLDPWD"
python3 tools/release-manifest.py
