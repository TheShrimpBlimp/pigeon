# PIGEON sample storage

The current PIGEON profile uses two chopped songs and a 16-pad drum kit in a
shared bank. Synth engines are excluded from the sampler profile, while
serialized project and backup IDs remain compatible. For the user workflow,
start with [the tutorial](KO_MODE.md).

The published firmware and web tools retain the 408 KiB payload described
below. The local v0.3 candidate adds an 88 KiB extent at `0x60000–0x76000`,
bringing its payload to 496 KiB plus a 4 KiB header. Existing extents keep their
physical addresses; boot-marker sectors `0x76000–0x78000` stay reserved.
The candidate package declares the added extent with KOPOOL3 alongside KOPOOL2.
This expansion has passed simulated storage/recovery checks, not hardware tests.
Local CLI tools target the candidate layout and negotiate device capacity before
writing. Use the published web tool for banks matching published firmware.

## Published capacity and formats

The bank is 421,888 bytes (412 KiB). Its header sector takes 4 KiB, leaving
417,792 bytes (408 KiB) shared by encoded audio, waveform and MP3 seek indices.
Drums use mono IMA ADPCM at 22.05 kHz. The song uses either:

- Indexed mono MP3 at 22.05 kHz, 24–64 kbps; default 32 kbps. The importer disables
  the bit reservoir so chops can seek with bounded decode work. Ordinary MP3
  files cannot be copied directly to the device.
- Mono IMA ADPCM at 8, 11.025, 16 or 22.05 kHz, selected at import.

| Song format | With 3.2 seconds of drums total | With 9.6 seconds of drums total |
|---|---:|---:|
| MP3 24 kbps | about 119 seconds | about 96 seconds |
| MP3 32 kbps | about 90 seconds | about 73 seconds |
| MP3 48 kbps | about 61 seconds | about 49 seconds |
| ADPCM 11.025 kHz | about 68 seconds | about 55 seconds |
| ADPCM 22.05 kHz | about 34 seconds | about 28 seconds |

These estimates include the maximum 8 KiB waveform and MP3 frame index where
applicable. Encoder padding/alignment slightly reduce the limit. The importer
reports actual usage and rejects oversized banks before connecting to a device.
A tested 60-second, 32 kbps song plus sixteen 0.6-second drum hits uses 367,694
bytes, leaving 54,194 bytes free. A minute of 11.025 kHz ADPCM also fits with a
shorter kit. Neither MP3 nor lowered-rate ADPCM restores lost audio quality later.

## Build and import

```sh
PYTHON=.venv/bin/python ./build-sampler.sh
python3 -m pip install --target build/codec-python lameenc
python3 tools/ko_bank.py build beat.kob song.wav kick.wav snare.wav hat.wav
```

Use `--bitrate 24` for more MP3 duration, or `--codec adpcm --rate 11025` for the
lower CPU option. Up to sixteen drum WAV paths are assigned in the supplied pad
order. Host import computes waveform peaks and transient chop starts; all chop
controls remain editable. The existing KO loop/latch, pitch, zoom and FX controls
continue to apply. MP3 song playback requires KO mode on T1; SAMPLE/GRAIN
continue to use ADPCM sources. Compressed reverse playback is disabled.

After separately installing and validating this firmware on the device:

```sh
python3 tools/ko_bank.py info
python3 tools/ko_bank.py upload beat.kob
python3 tools/ko_bank.py export backup.kob
```

Upload replaces the entire song/kit bank. Back up the existing three FSMP slots
before migration using the existing Web editor full backup; the first new bank
overwrites their contents. The new uploader negotiates the exact bank layout
before any write. Its legacy counterpart is blocked while a KOB2 bank is active.
Export includes the separate 4 KiB chop/compressor metadata sector; uploading
that export restores those edits. The existing Web editor full backup does not
include this new bank, so use `ko_bank.py export` for its audio and edits. Audio hashes bind edits to the matching song
and kit. An interrupted upload leaves the bank unavailable, rather than exposing
partially uploaded audio. Re-upload a complete bank to recover.

For a standard-firmware downgrade, export the bank and use
`python3 tools/ko_bank.py erase` **before** installing the standard firmware.
The updated loader rejects the downgrade while the KOB2 marker exists. Erase
invalidates the bank and legacy slot headers; it does not securely erase audio.
Older loaders and external full-flash tools do not know these protections.

## Published flash layout

| Physical range (end exclusive) | Bank allocation |
|---|---:|
| `0xA0000–0xB3000` | 76 KiB, including bank header |
| `0xB4000–0xDC000` | 160 KiB |
| `0x78000–0x92000` | 104 KiB reclaimed application space |
| `0xEA000–0xFC000` | 72 KiB package-declared USR region |
| `0xB3000–0xB4000` | Separate chop/compressor metadata |

Project, preset, settings, bootloader and Bluetooth configuration regions are
excluded. The linker and package maker enforce the smaller application boundary.
A checked `KOPOOL2` package descriptor tells the loader to preserve reclaimed
audio during updates. Record cleanup also preserves the top sample region, even
if audio bytes resemble an OTA record. No additional physical flash is assumed.

## Runtime and verification

MP3 uses a fixed-point libmad decoder with preallocated state and no heap. One
decoder runs in the main loop; the audio interrupt reads two 576-sample PCM
buffers per song voice plus a 192-sample bridge at each loop start. Samples remain in flash. Eight song voices are shared between Song 1 and Song 2; the kit has four additional voices. Seek jobs decode up to three frames to recover synthesis
history, or up to four for a bridge spanning two frames. Jobs yield between
frames so each service call decodes at most one MP3 frame; an incomplete buffer
is not published, and voice/bank changes cancel pending work. Cache misses output
silence and increment a counter. Initial attacks wait for current and prefetched
PCM. The bridge occupies spare engine memory and handles frame crossings at loop
boundaries. Waveform drawing never decodes audio.

Running-voice refills take priority over cold attacks and optional loop bridges.
Overlapping triggers reuse another voice's published PCM when the requested
source frame matches. Flash refill reads copy contiguous extents in bulk.
Most decoder modules use speed optimization; Layer III retains size optimization
to keep the strict ROM-address build check intact. These reduce refill work but
do not establish a hardware guarantee of eight simultaneous MP3 voices.

The profile reallocates the existing 432 KiB SRAM budget to 64 KiB static RAM
and 368 KiB pool; build checks require 4 KiB and 8 KiB minimum headroom,
respectively. Final usage is reported by the build, not inferred from file sizes.

Run `python3 tests/ko_storage_test.py` with the MP3 encoder dependency installed.
It tests a minute-long fixture, seeked versus sequential decode, real bank upload
handlers against simulated NOR, editable loops/latch, voice caches, persistence,
corruption and write boundaries, with ASan/UBSan. The codec's fixed-point wrap
arithmetic and existing firmware shift arithmetic are excluded from UBSan;
memory and array bounds remain checked. Expanded loader preservation tests run:

```sh
cc -O2 tests/ldr_test.c -o build/ko-host/ldr_test
build/ko-host/ldr_test build/host/old.fwsc build/felucca.fwsc ko-storage
python3 tests/target_budget.py build/felucca.dis tests/target_budget.txt
```

The ordinary full regression suite must be run after a standard build, because
its reference renders use factory audio. Static render instruction checks do
not bound MP3 decode time. FM1 listening, boot/update behavior and real audio
deadlines with eight pitched voices and stacked effects still require hardware
validation. The source/build/test work does not flash or access the device.

## Playback diagnostics

`tools/ko_bank.py diagnostics --reset` clears RAM counters; play a short passage,
then run `tools/ko_bank.py diagnostics` to read them. Command 97, schema 1,
returns status 0, version 1, then ten unsigned 32-bit values encoded as five
7-bit bytes each (least significant first). It never writes flash or stops playback.

Fields: missing sample reads (not audio blocks), decoder errors, reused buffers,
decode calls, total decode microseconds, maximum decode microseconds, maximum
refill-poll gap during active song voices, maximum draw microseconds, ready
attacks, and maximum trigger-to-ready microseconds. Timing on FM1 uses TIMER4
and includes interrupt time; it measures elapsed time, not isolated CPU cost.
The draw maximum includes idle screens. Reset immediately before each short
trial; 32-bit totals may wrap during long sessions. Host tests use a synthetic
clock and do not validate hardware timing. No logging occurs in the audio IRQ.

UI drawing in the KO storage build services sample refills between bounded LCD
transfers (at most 240 pixels per canvas transfer), checking the refill deadline
approximately every millisecond. The hook is enabled only around main-loop UI
drawing, never during boot or firmware update screens. It does not process UI
input or mutate drawing buffers. Full redraw elapsed time may increase because
refill work runs inside it; `poll_gap_max_us` and missing sample reads are the
important playback measurements. Repeated selection of the same chop at the same
waveform position no longer requests a complete waveform redraw.

## Two independent songs (bank version 3)

On published firmware, both songs and up to sixteen drum hits share the same 408 KiB payload pool.
The duration estimates above are combined song duration, not duration per
song; the second song also has its own seek index and waveform. The default
32 kbps encoding can hold roughly 90 seconds combined without drums, or
roughly 73 seconds with sixteen 0.6-second drum hits. Actual importer size
checks are authoritative. Encoding options apply to both songs in `build`.

```sh
.venv/bin/python tools/ko_bank.py build dual.kob song1.wav kick.wav snare.wav --song2 song2.wav
# Alternatively retain an exported one-song bank, drum kit and chop edits:
.venv/bin/python tools/ko_bank.py add-song dual.kob backup.kob song2.wav
.venv/bin/python tools/ko_bank.py upload dual.kob
```

Version 3 keeps the physical flash layout and first descriptor unchanged in
size; it adds a second 668-byte descriptor inside the existing header sector.
Two-song uploads require capability version 3 before any erase. Version 2
banks remain supported. The secondary descriptor is programmed before the
primary commit header; corrupt or overlapping secondary ranges fail closed.
Song 2 metadata uses offset 1024 in the existing metadata sector; the original
Song 1/kit record stays at offset 256. Exports include both records.
Older one-song firmware cannot play version 3 banks; retain a version 2 backup
if reverting firmware. `add-song` accepts version 2 bases only; rebuild a
version 3 bank from the source files to replace its audio.

### Preview shared storage before uploading

`tools/ko_bank.py preview BANK.kob` validates a bank locally and shows both songs'
names, durations and codecs, kit usage, combined consumption of the **408 KiB**
audio payload, and free space. The `upload` command prints the same preview before
opening MIDI or writing flash. Preview requires no connected synth.

In `web/editor.html`, connect a sampler firmware and open Samples. Choose a `.kob`
bank to see the combined storage bar and song/kit breakdown before clicking Upload
sample bank. Banks are created with the Python build/add-song commands; the browser
does not transcode audio. CRC, descriptor and frame checks run before upload, and
the firmware capability is checked again before erase. Two-song banks cannot upload
to one-song firmware. The browser retains the legacy slot editor for other firmware.

### Optional performance preferences

New chop flags retain the 544-byte FKO1 record and FUNA project format. Bits 2–6
store attack in 5 ms units; bits 7–11 store release in 10 ms units; bit 12 enables
nonlooping hold release; bit 13 selects explicit one-shot. Legacy bits 0/1 keep
loop/latch meaning. A 28-byte KPR1 record at metadata +2048 stores per-track
quantization and per-song choke, bound to audio CRCs with its own CRC32. No
sample flash allocation changes. Older firmware cannot interpret extended chop
flags; retain a recovery bank before rolling back.

### Alternate layouts

KEX1 at metadata +2304 stores two inactive 516-byte chop tables, per-song audio
CRCs, active A/B indices, roll mode/division, and a record CRC32. It fits within
the existing 4096-byte metadata sector. FKO1 continues to hold each active
layout, so no encoded audio moves when a layout changes. Chop flag bits 14–15
encode choke groups 0–3.

FUNA retains its 3840-byte layout and parameter IDs. Pitch uses P_TRANS locks
(id 28); nudges use the existing four encoded note high bits and P_SQNT (94).
Probability codes 102–105 encode conditional steps. These were previously
invalid values; old projects remain readable, but old firmware cannot fully
interpret projects using the new conditions or transpose locks. Conditions
and probability are alternative choices for a step.
