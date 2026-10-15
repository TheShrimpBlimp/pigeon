#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Felucca in the browser: the firmware's sources as the host tests build them (web/emu/felucca_web.c), compiled
# to WebAssembly with Emscripten, plus the page. After X0X's web/emu/build.sh (charlesvestal/fm1-x0x, GPL-3.0).
#   web/emu/build.sh   ->  build/emu/{index.html, worklet.js, felucca.wasm, fonts/}
# Run after one ./build.sh (build/gen: the tables, the fonts, the CC0 samples).
# -ffp-contract=off: no fused multiply-adds, as the host build. EMU_OPT replaces -O2, EMU_CFLAGS adds flags
# (EMU_CFLAGS=-msimd128: let the compiler vectorise; every browser with AudioWorklet has wasm SIMD).
# The page shows the version (FELUCCA_VERSION) marked "(preview)" before its release; EMU_PREVIEW=0 drops the mark.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: run ./build.sh once first (build/gen)"; exit 1; }
command -v emcc >/dev/null 2>&1 || { echo "emu: no emcc (Emscripten)"; exit 1; }
VERSION=PIGEON-0.3
mkdir -p build/emu/fonts
rm -f build/emu/fonts/DotGothic16-subset.woff build/emu/fonts/OFL.txt
# shellcheck disable=SC2086
MAD_SOURCES=""
for unit in bit fixed frame huffman layer12 layer3 stream synth timer; do
    MAD_SOURCES="$MAD_SOURCES firmware/vendor/libmad/$unit.c"
done
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    -DFPM_64BIT -DNDEBUG -DHAVE_LIMITS_H -DSIZEOF_INT=4 -DSIZEOF_LONG=4 -DSIZEOF_LONG_LONG=8 -fwrapv \
    -Dmalloc=ko_mad_malloc -Dcalloc=ko_mad_calloc -Dfree=ko_mad_free \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2147483648 -sFILESYSTEM=0 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c firmware/src/ko_mp3.c $MAD_SOURCES
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js web/emu/controller.js web/emu/loader.js web/emu/file-editor.js web/emu/trial-count.js build/emu/
cp web/pigeon/fonts/LittleCharacterRegular.ttf web/pigeon/fonts/LittleCharacter-OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, $VERSION)"
