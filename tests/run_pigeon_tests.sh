#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# PIGEON product checks; inherited all-engine screen lint remains in run_tests.sh.
set -eu
cd "$(dirname "$0")/.."
OUT=build/pigeon-tests
mkdir -p "$OUT"
CC=${CC:-cc}
PYTHON=${PYTHON:-python3}
PROFILE='-DFELUCCA_SAMPLER_ONLY=1 -DFELUCCA_KO_DUAL=1'
for test in pigeon_workspace_test ko_test ko_sequence_test pigeon_ui_test pigeon_piano_test pigeon_poly_test pigeon_note_timing_test pigeon_note_screen_test pigeon_upstream_backport_test pigeon_pattern_edit_test pigeon_queue_test pigeon_scratch_test pigeon_rec_button_test pigeon_ui_isolation_test pigeon_boot_test ko_arrange_test pigeon_workflow_test pigeon_expansion_test pigeon_performance_test pigeon_screen_test ko_playpoint_test ko_chop_workflow_test pigeon_chop_finish_test; do
    $CC -O1 -w $PROFILE -Ibuild/gen -Ifirmware/src "tests/$test.c" -lm -o "$OUT/$test"
    "$OUT/$test"
done
# Packed MIDI state retains ownership, sustain and burst/overflow behavior.
$CC -O1 -w -Ibuild/gen -Ifirmware/src tests/pigeon_memory_test.c -lm -o "$OUT/pigeon_memory_test"
"$OUT/pigeon_memory_test"
# Legacy synth objects remain opaque backup bytes, without active preset banks.
$CC -O1 -w -Ibuild/gen -Ifirmware/src tests/pigeon_legacy_backup_test.c -lm -o "$OUT/pigeon_legacy_backup_test"
"$OUT/pigeon_legacy_backup_test"
# Serialized old projects, recovery, malformed requests and DMA servicing.
for test in project_test storage_test backup_test robust_test fun10_test lcd_audio_test input_test; do
    $CC -O1 -w -Ibuild/gen -Ifirmware/src "tests/$test.c" -lm -o "$OUT/$test"
    "$OUT/$test"
done
$CC -O2 -w -Ibuild/gen -Ifirmware/src tests/regress.c -lm -o "$OUT/regress"
"$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
SAN='-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow -fno-sanitize-recover=undefined'
for test in pigeon_workspace_test pigeon_ui_test pigeon_piano_test pigeon_poly_test pigeon_note_timing_test pigeon_note_screen_test pigeon_upstream_backport_test pigeon_pattern_edit_test pigeon_queue_test pigeon_scratch_test pigeon_rec_button_test pigeon_boot_test pigeon_performance_test ko_playpoint_test ko_chop_workflow_test pigeon_chop_finish_test robust_test; do
    test_profile=$PROFILE
    if [ "$test" = robust_test ]; then test_profile=; fi
    $CC $SAN -w -Ibuild/gen -Ifirmware/src $test_profile "tests/$test.c" -lm -o "$OUT/$test-san"
    ASAN_OPTIONS=detect_leaks=0 "$OUT/$test-san"
done
FELUCCA_SAMPLER_ONLY=1 FELUCCA_KO_SCRATCH=0 "$PYTHON" tests/target_budget.py
"$PYTHON" tests/brand_splash_test.py
"$PYTHON" tests/icon_atlas_test.py
"$PYTHON" tests/pigeon_library_test.py
FELUCCA_SAMPLER_ONLY=1 FELUCCA_KO_DUAL=1 "$PYTHON" tests/ko_storage_test.py
"$PYTHON" tests/pigeon_gui_codec_test.py --node "${NODE:-node}"
# USB receive and real NOR persistence, including failure and cancellation paths.
$CC -O1 -w -Ifirmware/src tests/usb_sample_test.c -lm -o "$OUT/usb_sample_test"
$CC -O1 -Wall -Wextra tests/usb_sample_flash_test.c -o "$OUT/usb_sample_flash_test"
"$OUT/usb_sample_flash_test"
"$OUT/usb_sample_test"
$CC -O1 -w -Ibuild/gen -Ifirmware/src tests/ko_usb_sample_test.c build/ko-host/*.o -lm -o "$OUT/ko_usb_sample_test"
"$OUT/ko_usb_sample_test"
$CC $SAN -w -Ibuild/gen -Ifirmware/src tests/ko_usb_sample_test.c build/ko-host-san/*.o -lm -o "$OUT/ko_usb_sample_test-san"
ASAN_OPTIONS=detect_leaks=0 "$OUT/ko_usb_sample_test-san"
printf '%s\n' 'ALL PIGEON PRODUCT TESTS PASSED'
