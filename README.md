# PIGEON — sampler firmware for the M-VAVE FM1

PIGEON is free, open-source sampler firmware for the **M-VAVE FM1 (FM-1)**,
with **500 KiB of usable shared sample storage** across two song tracks and
16 drum pads. The [official PIGEON website](https://theshrimpblimp.com/pigeon/)
hosts the browser emulator, sample loader and firmware installer. Created and
directed by **The Shrimp Blimp**, built on Felucca 1.5.1 and shared under GPL-3.0-only.
Chop, sequence, perform and record samples over USB.

This branch contains **0.5.0-beta.1 — THE FINAL BETA**. Software checks pass;
the maintainer reports the hardware checklist complete. Start with the [0.5 tutorial](docs/PIGEON_V05_GUIDE.md),
[quick controls](docs/PIGEON_CONTROLS.md) and [hardware checklist](docs/PIGEON_V05_HARDWARE_TEST.md).

## What it does

- Two independent songs with up to **32 chops each**, plus **16 drum pads**,
  sharing **500 KiB usable sample storage**. Songs use indexed mono MP3 or IMA
  ADPCM; the drum kit and USB takes use ADPCM. Duration depends on encoding and
  the other samples loaded. Older connected firmware retains its advertised limit.
- Per-chop boundaries, playback point, forward/reverse direction, semitone and
  cent tuning, gain, envelopes, trigger modes and 0–8 ms loop crossfade. The
  loader offers nondestructive normalization. Two alternate layouts share audio.
- **USB audio recording** into the selected song or drum pad: input meter,
  threshold start, seconds/bar limits, audition, trim and confirmed saving.
  The host sends mono 16-bit PCM at 22.05 kHz. Scratch is excluded from 0.5;
  per-chop reverse remains. Live input monitoring and internal resampling are
  not implemented.
- **Keep selected chops** on the FM1, trim unused source regions, or collect
  chops from multiple songs in the browser library. Space reclamation removes
  obsolete payloads; keep downloaded backups before replacing audio.
- A step grid and piano roll with **1–64 steps per track and four notes per
  step**: live/step entry, note length/nudge, conditions, probability, repeats,
  Euclidean tools, fills, quantization, metronome and count-in.
- Patterns **A–D**, queued bar/pattern-end changes, and a **16-section arrangement**
  with per-track pattern choices, repeats and captured mutes.
- A three-track mixer, drum choke groups, FIFO chop queues, tempo tools, filter,
  delay, chorus, reverb, distortion and compression.
- Browser tools for editing, previewing, full-session backup/restore and firmware
  installation. Record the FM1 master output into a computer over USB audio, or
  capture the emulator output as **WAV and MP3**.
- Screen-reader support in the firmware and emulator, with optional speech or
  live-region output. Announcements start off until connected/enabled.

The songs share eight playback voices; drums have four additional voices.
Host tests do not establish worst-case FM1 playback or flash-write timing.
Synth engines are unavailable in the PIGEON product profile.

## Main controls

| Control | Action |
| --- | --- |
| K1 / K2 on a song | Chop start / end |
| LFO on a song | Cycle K1: start, playback point, direction, cents, gain, crossfade |
| K3 / K4 on a song | Waveform scroll / zoom |
| OCT on a song | Semitone pitch |
| SEQ | Step grid; press again for piano roll |
| ENV / LFO on the grid | Step details / timing and recording preferences |
| SEL | Patterns, then Arrangement |
| ARP | Mixer and FX sends |
| EDIT → FX on a song | USB sampler |
| ENV → FX on drums | USB sampler for the selected pad |
| EDIT → LFO → SEL on a song | Keep selected chops |
| EDIT → ARP | FIFO chop queue |
| SEL held + chop key | Audition the ending, then loop while held |
| HOME | Waveform; Settings when already on the waveform |

## Build, back up and install

Follow [BUILDING.md](BUILDING.md), then:

```sh
PYTHON=.venv/bin/python ./build-sampler.sh
```

Outputs are `build/pigeon.fwsc` and `build/pigeon.bin`. Building does not flash
the FM1. Save a complete `.pigeon` recovery archive before updating. A `.kob`
contains samples and chop settings; it does not contain all patterns/settings.

Use the sample loader’s firmware panel for a backed-up PIGEON update, or the
[CLI installer](tools/fm1_install.py) for supported installation/recovery paths.
Updates use **M-UPGRADE over USB MIDI SysEx**. When moving from another custom
firmware, follow [TransitFM1](https://quixotic7.github.io/TransitFM1/) and the
[recovery guide](docs/PIGEON_RECOVERY.md); do not bypass compatibility checks.
Keep USB power connected until installation and restart finish.

0.5 retains the 115-parameter project layout. Public 0.3.1 cannot read these
projects. Modern chop metadata requires 0.5; uploaders check capabilities before
writing. A bank using the expanded flash tail must be reduced before an older
layout can be installed. See [flash layout](docs/PIGEON_FLASH_LAYOUT.md).

## Guides

- [Complete feature list](docs/PIGEON_V05_FEATURES.md)
- [0.5 start-to-finish tutorial](docs/PIGEON_V05_GUIDE.md)
- [Quick controls](docs/PIGEON_CONTROLS.md) and [sample workflow](docs/KO_MODE.md)
- [Sample loader guide](web/pigeon/README.md)
- [Storage and transfers](docs/KO_STORAGE.md)
- [Screen-reader setup](docs/SCREEN_READER.md)
- [Acceptance record](docs/PIGEON_V05_ACCEPTANCE.md) and [hardware tests](docs/PIGEON_V05_HARDWARE_TEST.md)
- [Update safeguards](docs/PIGEON_RECOVERY.md) and [release signing](docs/RELEASE_SIGNING.md)
- Historical [0.4 tutorial](docs/PIGEON_V04_GUIDE.md) and [release notes](docs/PIGEON_V04_RELEASE.md)
- [Contributing](CONTRIBUTING.md) and [licensing](LICENSING.md)

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

The [browser emulator](https://theshrimpblimp.com/pigeon/webapp/emu/) starts empty.
Load your own songs and drums, or open a `.kob` bank or `.pigeon` session.
The 0.5 candidate supports the 500 KiB FM1 pool or unlimited browser storage;
unlimited banks must be reduced before transfer to hardware. The public
emulator remains on its deployed version until release.

Connect a MIDI controller to play the selected track. **Record output** captures
browser audio; stop and download WAV or MP3. This records emulator playback,
whereas the FM1 USB sampler records audio sent from the computer to the device.
Audio processing stays on your computer. The emulator credits Charles Vestal’s
FM1 X0X code and the Felucca adaptation by Leo Kuroshita and Hügelton Instruments.

### Screen-reader support

Standard firmware includes the FM-1 Screen Reader protocol. The emulator offers
browser speech and live-region announcements; both start disabled. See
[setup and controls](docs/SCREEN_READER.md).
