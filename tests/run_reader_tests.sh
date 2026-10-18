#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")/.."
PYTHON=${PYTHON:-python3}
READER_REPO=${FM1_READER_DIR:-../fm1-reader}
[ -f "$READER_REPO/tools/audit.mjs" ] || { echo 'Clone charlesvestal/fm1-reader beside this repository.' >&2; exit 2; }
mkdir -p build/sr
"$PYTHON" tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h
"$READER_REPO/tools/sync_announce.sh" --check "$PWD"
cmp "$READER_REPO/web/words.js" web/emu/reader-words.js
sh "$READER_REPO/tests/run_tests.sh"
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src tests/pigeon_reader_host.c -lm -o build/pigeon-reader-host
rm -f build/sr/said.tsv
PIGEON_SAID_LOG="$PWD/build/sr/said.tsv" build/pigeon-reader-host tests/scenarios/reader-main.sr
PIGEON_SAID_LOG="$PWD/build/sr/said.tsv" build/pigeon-reader-host tests/scenarios/reader-v04.sr
PIGEON_SAID_LOG="$PWD/build/sr/said.tsv" build/pigeon-reader-host tests/scenarios/reader-concise.sr
PIGEON_SAID_LOG="$PWD/build/sr/said.tsv" build/pigeon-reader-host tests/scenarios/reader-upstream.sr
build/pigeon-reader-host tests/scenarios/reader-dump.sr > build/sr/dump.tsv
cat build/sr/dump.tsv build/sr/said.tsv > build/sr/all.tsv
WORDS=$(sed -n 's/.*\/\([a-f0-9]*\.json\)"/\1/p' build/gen/ann_words.h)
node "$READER_REPO/tools/audit.mjs" build/sr/all.tsv --words "web/words/$WORDS" --allow tests/reader-allow.txt > build/sr/audit.log
cat build/sr/audit.log
grep -q "0 errors, 0 warnings" build/sr/audit.log
node "$READER_REPO/tools/audit.mjs" build/sr/all.tsv --words "web/words/$WORDS" --spoken > build/sr/spoken.tsv
node tests/reader-browser-test.mjs
printf '%s\n' 'ALL PIGEON READER TESTS PASSED'
