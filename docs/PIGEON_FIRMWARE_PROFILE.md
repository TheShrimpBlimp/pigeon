# PIGEON firmware profile

Build PIGEON with `PYTHON=.venv/bin/python ./build-sampler.sh`.
The product profile is mandatory: `FELUCCA_SAMPLER_ONLY=1`, dual songs,
shared sample storage, and no serial console. Its display version is v0.3.

PIGEON has its own page registry, waveform reader, and sampler screens.
The inherited graph renderers (including synth editors, scope, piano roll,
sample-source editor, preset lists, and old project/song screens) and engine
picker are excluded from this profile. Holding or double-tapping EDIT cannot
open the old engine picker. The FX macro keyboard, scale/chord tools and duplicate
recording overlay have been removed. GLO mixer and SEQ tool overlays remain.
Their input path also excludes generic preset browsing and old page actions;
the PRESETS encoder selects chops, sequence steps or patterns in PIGEON.

The product menu omits KO MODE, USB SERIAL, HOME and SCOPE, along with
legacy palette/style/large-layout, acceleration, FX latch, scale LEDs,
step-preview, chord-entry and generic help options. Its navigation
uses visible rows, so removed settings cannot be selected by scrolling or
restoring an obsolete selection. Editor menu descriptors omit unsupported
serial and old home/scope settings; remaining setting IDs keep their meaning.

Keep the shared audio effects, modulation, sample decoders, sequencer,
recording, MIDI, panel calibration, USB audio, backup and safe update paths.
Keep serialized engine/track IDs and project/backup codecs: deleting their
reserved slots would reinterpret existing files. Compatibility source and
tests remain in the repository; their old screens and synth renderers are
not part of the PIGEON firmware image.

Every sampler build runs `tools/pigeon_profile_audit.py` against its target
disassembly. It fails if inherited graph renderers, the engine picker,
legacy slice-editor functions, serial console, or unused synth renderers
reappear, or if the PIGEON UI and sample renderers are missing.
The ELF data-symbol check also rejects live synth preset banks, FM6 factory
patches, the former uncompressed splash pixel array and keycap bitmap data.

The splash uses lossless row RLE over the existing palette indices: 18,295
bytes including palette and row offsets, versus 58,112 before. The generator
verifies decoding against every original indexed pixel. Drawing uses the
existing single-row canvas; the artwork, signature, text and three-second
splash duration are unchanged.

PIGEON does not boot, browse, load, save or migrate synth preset banks.
Backup objects 6–9 retain their IDs and serialized layouts, but are read from
flash on demand through the existing storage scratch buffer. Restore validates
their layouts and stores their bytes without activating a preset namespace.
An empty object 9 preserves object 8 when restoring an older archive.
The preset RAM mirrors and factory FM6 patch table are absent from the image.
Keep the small neutral FM6 initialization record and project patch codecs for
backward-compatible project serialization.

`tests/pigeon_ui_isolation_test.c` exercises stale state, page buttons, held
and double EDIT, menu visibility/wrapping, and retained quick layers. The
sampler test runner also covers sample playback, sequence timing, patterns,
arrangements, copy/move/swap and performance controls. Firmware artifacts
remain local until explicitly authorized for flashing or publication.
`tests/pigeon_legacy_backup_test.c` checks opaque preset-bank round trips,
empty objects, malformed layouts and preservation of the captured project.
