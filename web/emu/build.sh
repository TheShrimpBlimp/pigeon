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
VERSION=Pigeon-0.1-dev
mkdir -p build/emu/fonts
python3 - <<'GEN'
import re
from pathlib import Path
source=Path("build/gen/felucca_samples.h").read_text()
data=re.search(r"static const uint8_t SMP_DATA\[\d{2,}\] = \{(.*?)\};",source,re.S)
if not data: raise SystemExit("Missing generated CC0 factory audio")
Path("build/gen/web_demo.h").write_text("static const uint8_t WEB_DEMO_DATA[] = {"+data[1]+"};\n")
GEN
# shellcheck disable=SC2086
MAD_SOURCES=""
for unit in bit fixed frame huffman layer12 layer3 stream synth timer; do
    MAD_SOURCES="$MAD_SOURCES firmware/vendor/libmad/$unit.c"
done
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    -DFPM_64BIT -DNDEBUG -DHAVE_LIMITS_H -DSIZEOF_INT=4 -DSIZEOF_LONG=4 -DSIZEOF_LONG_LONG=8 -fwrapv \
    -Dmalloc=ko_mad_malloc -Dcalloc=ko_mad_calloc -Dfree=ko_mad_free \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c firmware/src/ko_mp3.c $MAD_SOURCES
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js web/emu/controller.js build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, $VERSION)"
