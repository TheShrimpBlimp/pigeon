# PIGEON v0.5 development

Unpublished development work. Published firmware remains on the v0.4.x release line.

## Separate chop playback point

On a song waveform, tap **LFO** to cycle K1 between **loop start**,
**playback point** and **direction**. Turn K1 in playback-point mode to choose where the selected
chop starts when triggered. K2 still edits its end, K3 scrolls, K4 zooms.
Higher zoom gives finer playback-point adjustment. Turn K1 fully left to reset
the playback point to the loop start. Tap LFO twice more for normal boundary editing.
This replaces the waveform's former LFO shortcut to compressor makeup; makeup
remains reachable with LFO from the compressor page.

The point is independent of the loop boundaries: the initial pass starts there,
then a held loop wraps back to its original start. Nonlooping chops play only the
remaining part. Queue and sequencer triggers use the same point. ADPCM reverse
playback starts at the point, goes back to the original start, then loops from
the original end; reverse supports both ADPCM and MP3 song banks.

The waveform marks the playback point in the accent colour. Screen-reader K1
announcements follow the selected editing mode. SEL ending audition and Scratch
keep their existing behavior and override this ordinary trigger point.

Each chop stores a 16-bit fraction of its loop span, so moving boundaries keeps
the point in range. Copy, swap, move, duplicate, undo/redo and alternate layouts
carry it with the chop. Split/join retain its relative position within the new
boundaries. Preferences save automatically after playback stops and voices are
idle. Existing v0.4 banks start at the original beginning.

Playback fractions use an audio-bound, CRC-protected KPT2 extension at metadata
sector offset 3648 (288 bytes, ending at 3936). FKO1 and KEX1 tables retain their
original byte layouts; decoder checkpoints are rebuilt outside the audio ISR.
No sample storage is taken. Old firmware ignores the new points; saving edits
with old firmware may discard them.

Focused native and sanitizer checks cover first-pass position, loop return,
editing, undo, persistence, audio replacement and old banks. Full release tests
and a physical hardware check are still required before publishing v0.5.

## Per-chop reverse

On the song waveform, tap **LFO twice** from boundary mode to select
**K1 DIRECTION**. Turn **K1 right** for reverse or **left** for forward.
The direction is shown beneath the waveform. Tap LFO again to return to editing
loop boundaries. PRESETS or a chop key selects the chop to edit as usual.

Direction belongs to each chop; neighboring chops retain their own settings.
It saves with sample metadata and follows copy/settings-copy, move, swap,
duplicate, alternate layouts and undo/redo. Existing banks default to forward.
No additional sample audio or destructive conversion is needed.

Without a separate playback point, reverse starts at the chop's end. With one,
it starts there and travels backwards toward the loop start. Held loops wrap
to the original end; one-shots finish at the original start. Sequencer, MIDI and
queue triggers use the chop's direction. SEL ending audition stays forward;
Scratch retains its independent motor direction.

MP3 reverse reads decoded PCM from the cooperative voice caches; decompression
stays out of the audio interrupt. Previous-frame prefetch and a loop-end bridge
support backwards frame crossings and loop wraps. Physical performance testing
is still needed before release, especially with many simultaneous reverse voices.

## Final pre-1.0 feature work

The [v0.5 guide](PIGEON_V05_GUIDE.md) is the complete control map for the local
build. Per-chop cents, nondestructive gain/normalization, loop crossfade,
loader previews, pattern-launch cancellation/timing controls and session-library
summaries/overwrite previews are implemented locally. Normalization is measured
in the loader; the FM1 reads the resulting per-chop gain.

FKO2 keeps each on-disk chop at 16 bytes. Its formerly derived `st` word stores
signed cents in bits 0–7, gain Q8 in bits 8–19 (0 means unity), and crossfade ms
in bits 20–23. KEX2 uses the same packing for alternate layouts. FKO1/KEX1 remain
readable with neutral defaults. KPT2 at offset 3648 still stores first-pass
positions and direction, audio-bound and CRC-protected. Projects remain FUNA
with 115 parameters; sample metadata is separate.

Command 98 is a read-only capability query returning `[75,5,63]`. The loader
requires it before writing a bank containing FKO2 records. Existing diagnostics
on command 97 are unchanged. Crossfade uses spare ADPCM engine memory and the
existing MP3 loop bridge; it does not allocate another per-voice PCM pool.

Focused tests cover forward/reverse edges, overlap skipping, tuning/gain,
metadata reload, old-bank imports, decoded-audio preservation, browser editing,
normalization, previews, library save/overwrite/load and pending-pattern controls.
No full release suite or physical acceptance pass is claimed for v0.5.

## Recoverable chop editing and boundary audition

On Chop Playback (EDIT), K4 right resets all chops in the current layout to the
source bank's original assignments.
Reset publishes metadata through the existing per-song undo/redo slot. It does
not change sample bytes, alternate layouts or the other song. K4 left toggles
last-edit undo/redo.

Holding SEL and a chop key auditions edits immediately: K1 retriggers with the
current settings, K2 uses the existing ending-audition path. Only local audition
keys retrigger; ordinary editing, MIDI and sequencer voices do not start notes.

The loader previews validated draft fields before Apply, with an optional
180 ms debounced audition on editing. Reset uses the original header chop
assignments rather than re-detecting transients. Dedicated history buttons use
the existing workspace history. Preview does not mutate the bank or history.

## Browser chop collections

`chop-library.js` compacts the union of selected source regions, relocates chop
boundaries and retains playback fractions/settings. IndexedDB's existing files
store holds collections under `collected-chops`, independently of bank/session
replacement and workspace history. Export/import uses versioned `.pigeon-chops`
JSON with little-endian float PCM; imports validate audio, bounds and settings.

Song assembly deduplicates regions per collection, supports repeated ordered
assignments and re-encodes via the bundled worker. Drum assembly renders each
selected region as a one-shot and preserves choke groups. Both use existing
bank packing/capacity checks and local undo. No firmware, flash layout or device
protocol changes are needed. The service worker includes the new module.

## USB sampling candidate and release gate

The 0.5 product profile disables `FELUCCA_KO_SCRATCH` and enables
`FELUCCA_USB_SAMPLE`. Compatibility Scratch source/tests remain; per-chop reverse
is independent. The emulator disables Scratch too, but cannot emulate the target
USB audio peripheral and does not advertise live USB capture.

USB OUT EP4 adds a fixed-rate mono PCM16 22050 Hz streaming interface alongside
existing stereo USB capture and MIDI. A 2048-sample pool ring separates the USB
producer from main-loop ADPCM encoding. The recording writer pre-erases unused
logical pool sectors, writes bounded pages, and commits the bank header only on
explicit Save. Existing banks append a replacement for the selected song; an empty pool creates
Song 1. Waveform minima/maxima are accumulated every 2048 source samples.

Receiver tests cover descriptor lengths/collection/format, PCM ordering, wrap,
overrun, malformed packets and resets. Real NOR/DSP harness tests cover creating
and validating a bank, preserving Song 1, cancelled takes across a rescan,
replacing Song 2, odd sample counts, capacity stops and write errors. USB timing
on hardware remains a release blocker. In particular, verify that page-program
IRQ masking does not lose OUT packets and that RX overrun reporting works on the
actual USB controller. The header save is not power-loss atomic.

### USB recorder workflow

Ready takes can be auditioned with PLAY without committing a header. The IMA
preview reads the appended take through the normal mixer and master control,
duplicating 22.05 kHz samples at the 44.1 kHz mixer rate. Save, retry and discard
stop preview before writing or reclaiming data. SCL opens replacement confirmation;
a second SCL commits, while LFO backs out. ENV similarly confirms a retry.
Successful saves reopen the selected song waveform. Input peak is sampled at
10 Hz with a brief IRQ-masked exchange, and full-scale clipping is latched until
the next take. Idle/capture displays report remaining seconds from available
pool capacity, reserving waveform space.

Workflow references: [Skiff](https://github.com/illuminus5/skiff#controls) for
contextual action hints, and [Melodee](https://github.com/keremimo/melodee) for
track-focused operations and explicit replacement confirmation. These are design
references; this recorder implementation remains PIGEON-specific.

The splash uses a lossless row-delta codec (literal/repeat/previous-row runs).
It reuses the existing canvas, waits for LCD DMA before modifying each row,
and requires sequential decoding. The generator verifies all palette indices;
`tests/brand_splash_test.py` verifies the compiled decoder against every expected
RGB565 pixel with sanitizers.

### Trim before saving

K1/K2 adjust ready-take start/end in 22-sample steps (approximately 1 ms), with
K3/K4 using 2205 samples (100 ms). Bounds are end-exclusive, clamped to the take,
and retain at least 64 samples. Changes stop preview; preview reconstructs the
IMA state at the selected start with interrupts enabled and watchdog servicing.
Save confirmation is the boundary for destructive edits to the pending take.

Confirmed trim decodes and re-encodes the selected range at the same 22.05 kHz
rate, rebuilding waveform bins. The USB PCM ring becomes a 4 KiB input cache;
existing storage scratch plus the recorder page buffer holds an output sector.
The output never overtakes decoded input. Source sectors are cached before an
output erase can overlap them. Only uncommitted sectors beyond the current bank
are rewritten. Partial output and following waveform sectors are handled
separately, including audio ending exactly at a sector boundary. Header commit
remains last. Errors reject the take while preserving the previously committed
bank (subject to the existing header-commit power-loss limitation).

RGB565-identical palette entries in the splash are canonicalised before row
compression, preserving every displayed pixel and requiring no new canvas RAM.

## Memory reclamation

PIGEON's runtime pattern caches and clipboard hold only the three active tracks.
FUNA project storage still holds four tracks, preserving the old file format and
opaque legacy track data. The hidden lane returns silence during arrangement
playback. Imported automation for that lane is excluded from the playback cache.

MIDI ownership packs two notes into each byte: three bits for track ID and one
for sustain. All 16 channels and 128 notes remain represented, including the
fourth track in the inherited engine profile. Ownership uses 1024 bytes instead
of 2048; no USB receive or audio-cache capacity is reduced.

The 1040-byte screen text cache and 1584-byte legacy manual-slice tables use
pool RAM in the PIGEON profile. This relocation frees main RAM, rather than
reducing total memory. Legacy tables retain both copies for safe publication to
the audio renderer and for old-bank migration.

The measured development build uses 54,888 of 65,536 main-RAM bytes and 367,340
of 376,832 pool bytes: 10,648 and 9,492 bytes free respectively. Both the 4096-byte
main reserve and 8192-byte pool reserve remain enforced. Compared with the prior
successful development build, main usage is down 6512 bytes and combined RAM
usage is down 4588 bytes; that comparison also includes intervening feature edits.
The image is 364,068 bytes. These are software build results, not a hardware
acceptance result or a published release.

### Canvas, engine workspace and upload headers

A second pass reduces PIGEON's canvas from 240 x 124 to 240 x 122 pixels,
saving 960 pool bytes. Menu document bands and split limits use the canvas row
limit; graph height, dialog content and screen coverage are retained. The
screen regression checks every canvas request before the renderer's fallback
clamp, including document and information menus.

Per-track engine workspace is 12,448 rather than 12,888 bytes in PIGEON,
saving 1320 pool bytes across three tracks. The full eight-voice MP3 cache,
loop bridges, reverse windows and crossfade metadata remain unchanged and
have compile-time fit checks. Unavailable synth engines retain their original
workspace checks in the inherited engine build.

Bank uploads reuse the two inactive bank headers, saving 1336 object bytes.
Beginning an upload first invalidates the bank and clears all sample sources.
Background rescans preserve the draft until commit; the storage test deliberately
rescans between header packets. Validation and payload CRC still precede header
publication. A successful upload clears the upload flag and rescans from flash.
USB recording refuses to start during an upload.

That pass measured an image of 364,160 bytes. Main RAM uses 53,544 bytes (11,992
free); pool RAM uses 365,060 bytes (11,772 free). This pass recovers 3624 bytes
including target alignment. No hardware flash or release is implied.

### Kit workspace, scratch lifetimes and lossless assets

The two song tracks retain their full 12,448-byte engine workspaces. The kit
has its own aligned 1024-byte workspace, sufficient for all eight legacy SLICE
reverse windows; its normal SAMPLE engine needs no engine workspace. This
recovers 11,424 pool bytes. Reserved track 4 has no workspace. Compile-time
size checks and a sanitizer regression verify that clearing and filling every
kit reverse window cannot overwrite either song cache.

Collection source IDs reuse the idle USB encoder page. All IDs are consumed
before the encoder state is initialized, reclaiming another 32 static bytes.
The collection ranges, output table, audio page and waveform coexist during
encoding and therefore retain separate storage. Recovery journals also retain
their state while interrupted operations can still be exported.

Icon generation deduplicates identical raster cells while preserving every
icon ID, size, pixel and ink bound. The reference-atlas test compares all 150
available ID/size combinations. The generated atlas shrinks by 232 bytes;
compiler/layout changes make the complete image 144 bytes smaller. Fonts and
help text needed by supported screens are retained.

Current target measurements: image 364,016 bytes; main RAM 53,544 / 65,536
(11,992 free); pool RAM 353,604 / 376,832 (23,228 free). Full song voice counts,
MP3 cache sizes, delay memory and USB receive buffers are unchanged. This pass
adds no sample flash capacity and has not been flashed to hardware.

### Appended flash sector

The sample bank now appends physical sector `0x5F000..0x60000`, increasing
capacity by 4096 bytes to 504 KiB total, including its 4 KiB header: **500 KiB
usable**. Every previous logical byte keeps its physical address. See
[PIGEON_FLASH_LAYOUT.md](PIGEON_FLASH_LAYOUT.md) for the layout and downgrade
checks. RAM reclamation above did not itself expand the flash bank.

The resulting target image is 364,548 bytes. Its body leaves 7900 bytes below
the sample pool; the build enforces a minimum 4096-byte growth reserve. Main
RAM remains 53,544 / 65,536 and pool RAM 353,604 / 376,832.

Native storage/codec tests, full-capacity bank tests, update/recovery tests and
capacity negotiation tests pass. The emulator source capacity is synchronized,
but its WASM binary has not been rebuilt. Browser layout automation requires
the missing Playwright dependency. This candidate has not been flashed; the
new sector still requires the hardware checks below.
