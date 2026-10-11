# KO memory and storage follow-up

**The original KO profile is described below.** The new opt-in
`FELUCCA_KO_STORAGE=1` profile changes the application/sample layout and supports
minute-long banks; see [Expanded KO storage](KO_STORAGE.md) for its map, limits
and validation requirements. The statements below about unchanged partitions
apply only to the original profile.

This source-only work does not change the flash map, bootloader, updater or firmware package format.
Both firmware profiles have now been built. KO host tests, upload-capacity tests, ASan/UBSan
checks and compiled target DSP budgets pass. Physical CPU/audio deadline measurements remain pending.
The complete host suite also passes, including 92 unchanged reference renders and 720 engine-switch
cases. Web tests were skipped because Node is unavailable; browser-emulator tests were skipped
because Emscripten is unavailable. No hardware was accessed or flashed.

## Immediate storage change

`tools/fm1_sample_upload.py:build_song` splits a mono 22.05 kHz IMA ADPCM song into USR1 and USR3.
`firmware/src/eng_sample.c:smp_ko_pair_valid` validates their tags, pair identity and sample rate.
`firmware/src/eng_slice.c:slc_user_scan_one` joins their flash-backed segments for playback and
transient analysis. `ko_prepare` binds chop metadata to both payloads. The decoder already resets
at segment boundaries, so no whole-song PCM allocation is required.

| Allocation | Flash range | Purpose |
|---|---|---|
| Application reservation | 0x4000–0x92FFF | Firmware and built-in assets |
| USR1 | 0xA0000–0xB3FFF | Song first chunk; final 4 KiB reserved for metadata |
| USR2 | 0xB4000–0xC7FFF | Drum kit |
| USR3 | 0xC8000–0xDBFFF | Song continuation |

The song gets 77,312 + 81,408 = 158,720 encoded bytes, approximately 14.40 seconds.
The kit retains 81,408 encoded bytes shared by its sixteen hits, approximately 7.38 seconds total.
This increases available song storage without expanding RAM or moving any flash partition.

## Reclaiming factory assets: next candidate

The existing generated `build/gen/felucca_samples.h` contains 126,244 bytes of ADPCM assets.
Removing them could substantially shrink an optional sampler-only firmware. It does not make
those bytes accessible to the existing sample uploader: they lie in the application reservation.
The current KO image is 503,956 bytes, RAM 56,084 / 73,728 bytes and pool 327,800 / 368,640 bytes.
The standard image is 495,640 bytes. KO has 17,644 bytes of static RAM headroom and 40,840 bytes
of pool headroom. These are separate budgets; they are not sample storage allocations.

| Hypothetical song format / allocation | Approximate maximum | Status |
|---|---|---|
| Current 22.05 kHz mono IMA ADPCM, USR1 + USR3 | 14.40 seconds | Exact encoder capacity tested |
| 11.025 kHz mono IMA ADPCM, same allocation | 28.79 seconds | Needs importer changes; less high-frequency detail |
| 8 kHz mono IMA ADPCM, same allocation | 39.68 seconds | Needs importer changes; telephone-band audio |
| Current format plus approximately 126 KiB reclaimed factory data | 25.85 seconds | Estimate; requires application/sample layout and updater changes |

These figures describe stored source duration. Varispeed pitch changes playback duration but
does not increase how much source audio fits. The last row assumes essentially all factory
sample bytes become usable song storage; partition alignment, metadata and code changes may
reduce the result. No factory assets or flash partitions were removed in this change.

A future profile should preserve engine, preset and sample-set IDs and provide explicit empty
factory sets. Removing engines outright risks stored projects and preset conversion paths.
Reclaiming application flash for songs needs a reviewed common layout shared by the linker,
packer, updater, XIP mapping, upload protocol and backup/restore. Firmware installation must
not overwrite samples placed in reclaimed space. Do not allocate unverified top-of-flash gaps:
the device's prior JEDEC check identified the expected 1 MiB part.

## Required validation before installation

1. Run ordinary and KO host suites, including upload interruption and continuation mismatch cases.
2. Build both profiles and check image, static RAM and runtime pool limits.
3. Verify both halves after reboot, chops spanning the seam, loop/latch stop and voice stealing.
4. Measure worst-case CPU/audio deadlines with eight voices, pitch changes, effects and compression.
   Longer sources increase transient scan time and reverse-seek costs; keep reverse out of the
   initial hardware stress setup, then assess it separately before claiming full compatibility.
5. For any future layout change, test firmware update and rollback, project loading, sample
   backup/restore, interrupted writes and factory reset against an explicit partition map.
