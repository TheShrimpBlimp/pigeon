# PIGEON v0.4 development notes

The published release remains v0.3.1. Use the [v0.4 guide](PIGEON_V04_GUIDE.md)
for controls and the [note-timing guide](PIGEON_NOTE_TIMING.md) for editing limits.

## Current status

The previously flashed development FM1 runs v0.4-dev with queue mode, SEL end audition, REC fixes,
scratch controls, cent tuning, saved deck preferences, per-note timing and screen
reader support. Boot and data preservation were verified after the October 16,
2026 flash. The bank and stored settings/patterns matched the backup; the live
project matched after migration from 111 to 115 parameters.

The record-centre pigeon and illustrated empty-song page were added after that
flash. They are implemented locally but are not yet on the FM1. No v0.4 build
has been published.

Focused checks cover playback, queue order/removal, timing, persistence, text
layout and reader announcements. Remaining checks include hardware listening,
recording behavior, real screen-reader use and physical FM1 panel control of the
emulator. These checks are distinct from successful builds and USB verification.

## Storage and audio

- Projects remain 3840 bytes. Four per-note parameter IDs bring the count to 115.
  Timing uses existing motion records, shared with automation and locks.
- Scratch preferences use an optional CRC-checked SCR1 record at sample-metadata
  offset 3584, bound independently to each song's audio CRC. Cue, motor state,
  running voices and fader level are not serialized.
- Audio is decoded in the main loop. ADPCM scratch decks use two 1024-sample
  windows each; MP3 uses the existing per-voice caches. A cache miss holds the
  position and fades output. Fast MP3 scratching still needs listening checks.
- Pigeons are drawn with native geometry. Queue redraws are cached between
  queue/palette changes and half-second wing-animation ticks. Decorative motion
  produces no reader announcements.

## Screen-reader builds

Standard hardware and emulator builds generate the pronunciation header from
`firmware/words.tsv`. Native checks can generate it directly:

```sh
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h
cc -O1 -w -Ibuild/gen -Ifirmware/src tests/pigeon_reader_host.c -lm -o build/pigeon-reader-host
build/pigeon-reader-host tests/scenarios/reader-main.sr
build/pigeon-reader-host tests/scenarios/reader-v04.sr
node tests/reader-browser-test.mjs
```

After building the emulator, `tests/reader-wasm-test.mjs` checks the compiled
protocol and `tests/reader-page-test.mjs` checks Chromium AudioWorklet/live-region
behavior. `tests/run_reader_tests.sh` runs the broader reader audit.

Announcements run in the main loop; the USB transmitter never waits for queue
space. WHERE retries one chunk per UI frame. Protocol and pronunciation modules
are pinned to upstream commit `541ea332ebecd84aeea6a45b52a5fe99801c8570`.

`tools/gen_words.py` produces `web/words/<fingerprint>.json` and
`build/gen/ann_words.h`. Released fingerprints must be retained. Publish the
matching dictionary with the firmware release; the current development URL is
`https://theshrimpblimp.com/pigeon/words/84859cb5f4.json`. Before that release,
load the local JSON manually into the official reader. The emulator bundles it.

## Credits

Screen-reader protocol, word generator and pronunciation module: Charles Vestal,
GPL-3.0-only. Notices remain in `firmware/src/announce.c`, `tools/gen_words.py`
and `web/emu/reader-words.js`.

Record geometry reference: [VoltViz VinylSendspin](https://github.com/sanderdw/voltviz/blob/79ba2267362db3f7c29a311a6892c6dffd90b553/src/visualizers/impl/VinylSendspin.ts),
MIT, © 2026 VoltViz Contributors. The firmware draws its own fixed-point geometry;
no cover artwork or image assets are imported. See [the license notice](../LICENSES/MIT-VoltViz.txt).


## Pattern, queue and cue editing

Local additions after the latest flash: Patterns FX copies and LFO pastes, with destination-specific load/paste prompts. K3 selects arbitrary waiting queue entries and ENV removes the selection while preserving FIFO note/velocity order. Scratch cues are retained per song/chop in RAM, guarded by audio generation and chop bounds; they are not serialized. These changes have not been flashed or published.

## Selective Felucca 1.6 / 1.6.1 backports

Local v0.4 now includes stable same-step automation row ordering, dark LED rows
before flash erase, and exact firmware-area size checks in both update entry
and loader before erasing anything. An update rejected before staging no longer
erases the staging area as a side effect.

The MIDI and sequencer additions are described in the [v0.4 guide](PIGEON_V04_GUIDE.md):
custom channels for PIGEON's three tracks, MIDI wake and optional auto-selection,
expanded conditions/FILL, Euclidean generation, DOUBLE/HALF and velocity/timing
humanization. This is a selective adaptation, not a rebase; the upstream synth
engines and removed overlays remain excluded. Source references:
[Felucca 1.6](https://github.com/hugelton/Felucca/releases/tag/v1.6) and
[1.6.1](https://github.com/hugelton/Felucca/releases/tag/v1.6.1).

Project slots remain 3840 bytes with 115 track parameters. PIGEON preserves
condition byte values 102–105 and appends new conditions at 106–116; these are
not interchangeable with upstream Felucca's condition numbering. Header bytes
65 and 67 store three 5-bit channel offsets and a TRACK flag, leaving the full
128-event automation/note-timing pool intact. Older PIGEON files have zero in
those header bytes and restore default routing. New conditions are rejected by
older PIGEON releases; keep a session backup before downgrading. Earlier v0.4 builds with the same 115-parameter layout ignore the new routing
header and use its stored CH1-4 fallback. Public v0.3.1 cannot read the current
115-parameter project format.

Second-core rendering was assessed but is not enabled or imported. Upstream
splits per-track rendering and makes modulation/envelope state private to each
core. PIGEON also shares a sampler voice budget, decoder scheduling and MP3
cache publication (`ko_pcm.c`), plus scratch cache requests (`eng_slice.c`).
Those require a separate synchronization design and hardware audio validation;
turning on upstream CPU2 directly would not establish that those accesses are
safe. It is a separate performance project, not a requirement for these backports.

Release preparation now runs the full product/recovery suites, target budgets,
reader audit, WebAssembly and browser checks against the v0.4 candidate. See the
[v0.4 release gate](PIGEON_V04_RELEASE.md) and local validation report for results.
The final candidate has not been flashed or published.

## Release-candidate validation

The final local candidate identifies itself as PIGEON v0.4. Full product and
recovery suites, all five browser suites, reader protocol/audit (zero errors and
warnings), WebAssembly and panel tests pass. Two target builds are byte-identical.
The target keeps the original slice/reverse budgets and separately checks new
scratch render/cache functions. The 115-parameter sample-tool failure is fixed,
with 104/111-parameter editing and all 15 conditions covered by compatibility
tests. The stock V15 fixture matches its pinned checksum; stock-return and
modified-stock rejection tests pass.

The exact final candidate has not been flashed. Hardware verification and explicit
deployment approval remain the final release gates. See the candidate manifest
and generated release report for hashes and detailed logs.
