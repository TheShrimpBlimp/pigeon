# PIGEON

PIGEON is an open-source, audio-only performance sampler for the **M-VAVE FM1**.
Built on Felucca 1.5.1, it focuses on chopping samples, playing drums, sequencing
beats, and performing live. Its workflow draws inspiration from the Roland
SP-404, Teenage Engineering PO-33 KO, and Pocket Operator for Pixel.

Created and directed by **The Shrimp Blimp**. This checkout prepares **v0.4**;
the public installer remains on v0.3.1 until release approval. v0.4 includes
screen-reader support in the standard firmware. See the [v0.4 tutorial](docs/PIGEON_V04_GUIDE.md)
and [release checklist](docs/PIGEON_V04_RELEASE.md) before the final hardware test.
Both songs and drums share **496 KiB** of sample storage.

## What it does

- **Two independent song samples and a 16-pad drum kit**, sharing 496 KiB of
  flash-backed sample storage. Songs use indexed mono MP3 or IMA ADPCM;
  available duration depends on encoding quality and the other samples loaded.
- **Up to 32 editable chops per song**, with transient detection, independent
  boundaries, pitch, attack/release, one-shot, hold, looping, and latch. All 27 keys play chops; a second
  bank reaches chops 28–32.
- **A paged sequence grid with 1–64 steps per track**. Assign chops directly,
  record live or step by step, edit pitch/microtiming/conditions and
  velocity/gate/probability/repeats, and undo
  sequence edits. Choose tempo timing or complete-sample timing. Optional live
  quantization, grouped song choke, held drum rolls, tap/loop tempo, metronome,
  and count-in support recording. Each song has two alternate chop layouts.
- **Patterns A–D and a 16-section arrangement**, with section repeats,
  duplication, reordering, copy/paste, and queued bar-or-pattern-end launches.
- **A three-track mixer**, per-pad drum pitch/level/decay/choke groups, filter,
  delay, chorus, reverb, distortion, and separate compression. Temporary FX
  tweaks restore on release.
- **Local sample-management tools** for independent song/kit replacement,
  waveform and chop editors, drum settings, pattern editing, encoded previews,
  space reporting, firmware installation, and complete instrument backups.
- A green display with **Little Character** typography, buffered updates,
  waveform zoom, and playback following.

PIGEON gives both songs eight shared voices plus four dedicated drum
voices. Sustained 8+4 playback still needs physical testing. Synth engines are
unavailable in the PIGEON profile. Direct recording and resampling are outside the release scope. Scratch mode
provides forward/reverse playback; ordinary chop playback retains its existing behavior.

## New in v0.4

- FIFO chop queues, including repeated chops, with selection/removal of waiting entries.
- Scratch decks with waveform/record views, jog sensitivity, level cuts, reverse,
  brake/glide, cue return and cent tuning. Per-song settings are saved; cues are temporary.
- Individual length and nudge for all four notes of a step in the piano roll.
- Extended step conditions, held fills, Euclidean rhythms, DOUBLE/HALF and humanization.
- Per-track MIDI channels, optional incoming-note track selection and MIDI screen wake.
- Standard screen-reader protocol and optional emulator speech/live-region output.
- SEL plus a key auditions the ending, then loops from the beginning while held.
- Pattern copy/paste with destination-specific confirmation and preserved samples.

v0.4 uses 115-parameter projects. Back up a complete session before updating:
public v0.3.1 cannot load v0.4 project files. Banks and complete recovery archives
remain separate from sequence-only patterns. The sample editor accepts both
104–111-parameter older projects and the current v0.4 format.

## Main controls

| Control | Action |
| --- | --- |
| K1 / K2 on a song | Chop start / end |
| K3 / K4 on a song | Waveform scroll / horizontal zoom |
| OCT keys on a song | Chop pitch |
| SEQ | Sequence grid; K4 sets length while stopped |
| ENV on the grid | Step details |
| LFO on the grid | Swing and timing |
| SCL | Patterns / Arrangement |
| ARP | Mixer and FX sends |
| ENV on drum HOME | Per-pad settings |
| HOME | Return to waveform; from waveform, open Settings |
| SEL held + chop key | Audition the ending, then loop from the beginning while held |
| EDIT → FX | Scratch deck; EDIT switches performance/motor controls |
| EDIT → ARP | FIFO queue mode |
| FX on piano roll | Individual note length and nudge |

Read the [control guide](docs/KO_MODE.md) for recording, rest entry, undo,
chop banks, arrangement tools, and the library commands.

## Build and load

Follow [BUILDING.md](BUILDING.md), then:

```sh
PYTHON=.venv/bin/python ./build-sampler.sh
```

Outputs: `build/pigeon.fwsc` and `build/pigeon.bin`. Install this project's
package with `tools/fm1_install.py`; upstream installers distribute upstream
Felucca. Before installing from another custom firmware, back up, revert to
official M-VAVE V15 using that firmware’s documented procedure, then install
PIGEON. See the release notes for completed installation checks and remaining physical verification.

- [v0.4 start-to-finish tutorial](docs/PIGEON_V04_GUIDE.md)
- [v0.4 release notes and hardware checklist](docs/PIGEON_V04_RELEASE.md)
- [Sample/chop workflow](docs/KO_MODE.md)
- [Quick control reference](docs/PIGEON_CONTROLS.md)
- [v0.3.1 release notes and validation limits](docs/PIGEON_V031_RELEASE.md)
- [v0.3 release history](docs/PIGEON_V03_RELEASE.md)
- [Sample storage and transfers](docs/KO_STORAGE.md)
- [Sample library and complete archives](tools/pigeon_library.py)
- [Contributing](CONTRIBUTING.md)
- [Update safeguards and recovery](docs/PIGEON_RECOVERY.md)
- [Licensing and attribution](LICENSING.md)

## Human authorship and AI assistance

**AI was used for menial coding. All assets, direction, conceptualization, and
thought are human.** Creative assets are human-created or human-sourced and
credited to their creators. AI assistance supported routine implementation,
conversion, testing, and maintenance under human direction.

## Foundation and licensing

PIGEON is its own project, built on the work of
[Felucca](https://github.com/hugelton/Felucca), Leo Kuroshita, Hügelton
Instruments, and the credited contributors and library authors.

PIGEON remains **GPL-3.0-only**. The [GPL text](LICENSE), inherited copyright
notices, and third-party licenses are preserved. The text splash is described in
[assets/branding/README.md](assets/branding/README.md).
The software license does not grant rights to imported commercial recordings.

FUNA project layouts, engine/parameter IDs, and USB/update protocol identities
remain for compatibility. Legacy synth project data can be retained but its
engines do not sound in PIGEON; projects using replaced factory samples can
sound different. The baseline build and regression fixtures remain available
for compatibility checks. Internal `FELUCCA_*` names are implementation details.

### Browser sampler tool

Run `python3 tools/pigeon_gui.py` to open the local GUI on macOS/Linux, or
`py tools/pigeon_gui.py` on Windows. It manages both songs, 16 drum pads, previews,
storage, device transfers, and complete session backups. It also runs directly
from an HTTPS website without installing anything and works offline after
**Offline ready** appears. Audio processing stays on your computer.
See [the GUI guide](web/pigeon/README.md).

The hosted PIGEON site reports anonymous successful-install totals. Browser
installers count only after the FM1 restart identity is verified; repeated
installations count again, and offline reports wait in local browser storage.
The counter stores a random event ID, firmware checksum, source, and day, with
no sample data or device identity. These are client-reported events, not unique
devices. Tracking begins with this release; earlier installs are not counted.

## Project repositories

- Firmware: https://github.com/TheShrimpBlimp/pigeon
- Sample tool: https://github.com/TheShrimpBlimp/pigeon-tool
- Website: https://github.com/TheShrimpBlimp/pigeon-website

The firmware repository is public and welcomes contributions. The sample tool
and website have separate repositories. The website checkout lives at
`web/site/`; clone it there when preparing a hosted build from this firmware
checkout. The standalone sample tool checkout lives at `repositories/pigeon-tool/`.
These independent checkouts are excluded from the firmware repository.

### Try PIGEON in the browser

The [browser emulator](https://theshrimpblimp.com/pigeon/webapp/emu/) starts
empty. Load your own Song 1, Song 2 and drum samples; audio stays on your
computer. Choose the real 496 KiB FM1 sample pool or unlimited browser storage.
Connect a MIDI controller (including the FM1) to play its keys; use the on-screen
knobs and buttons to edit. The emulator credits Charles Vestal’s FM1 X0X browser
code and the Felucca adaptation by Leo Kuroshita and Hügelton Instruments.

Press SEQ again to alternate the step grid and piano roll. Both support four notes per step: unarmed keys toggle notes; stopped REC entry advances when all chord keys are released. REC remains lit when armed in every LED mode.

### Screen-reader support

Standard v0.4 builds include FM-1 Screen Reader support for hardware and the
emulator, including the piano roll, chords, queue and scratch decks. It starts
silent until connected/enabled. [Setup and dictionary](docs/SCREEN_READER.md).

The [v0.4 guide](docs/PIGEON_V04_GUIDE.md) is the current control map. Earlier
v0.4 development firmware was flashed during development; the final candidate
still needs its own short hardware smoke test before deployment approval.
