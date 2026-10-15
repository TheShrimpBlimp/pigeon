# Historical sampler reference

This archived development reference includes earlier one-song and fixed-slot
profiles. Its capacities, menu paths and installation instructions are not the
current PIGEON workflow. Use [the current tutorial](KO_MODE.md) first.

# PIGEON sampler interface (v0.1)

For the optional shared bank and minute-long songs, see [Expanded KO storage](KO_STORAGE.md).
The slot capacities below describe `FELUCCA_KO=1` without `FELUCCA_KO_STORAGE`.

PIGEON uses this interface for one transient-chopped song sample and sixteen separate drum one-shots.
Build the standalone product with `./build-sampler.sh`; the inherited menu still calls this interface KO MODE.
The following also documents the legacy sampler build for compatibility.
Standard Felucca remains the default build; its engines, track count, parameter IDs, and FUNA project
format stay in place. The source now supports a longer song across two existing slots; flash partitions
and synth engines remain in place. Both firmware profiles build; KO host, encoder-capacity,
sanitizer and compiled DSP-budget checks pass. Hardware listening and deadline checks remain pending.

## Build and enable

Build with `FELUCCA_KO=1 ./build.sh` (use the project's configured Python/toolchain as usual).
Enable **MENU > SYSTEM > KO MODE** and return HOME. **OCT+ SET UP T1 + T2 SOUNDS** explicitly
configures T1 as SLICE / USR1 and T2 as SAMPLE / USR2 / KIT. It replaces those two sounds, keeps
their notes, and sets empty patterns to sixteen steps at 1/16. Existing nonempty pattern lengths
are preserved. It leaves T3/T4 intact; they can still play an existing project's patterns.
ALGORITHM selects SONG 1, DRUM KIT, or SONG 2 (tracks 1, 2, 3). The standalone build is sampler only: KO MODE stays on, synth engines are unavailable, and only three tracks render. The legacy Felucca build can disable KO MODE to access four tracks.

## Samples

- USR1: one mono song sample. Transient detection produces up to 32 initial chops. All 27 keyboard
  keys trigger song chops (the existing white keys retain chops 1..16, black keys add 17..27); PRESETS selects a chop and switches banks as necessary.
- USR2: up to sixteen independent drum files, each on one exact note, 60..75. Empty pads are silent.
  Release does not cut a hit short. The songs share eight voices, with four separate kit voices; stealing fades old hits.
- USR3: continuation of a longer song. Internal resampling is not implemented in v0.1.

Existing slots remain 80 KiB each, mono IMA ADPCM at the uploader's 22.05 kHz: about 7.38 seconds
per slot. All sixteen drum files share USR2's duration. Keep USR1's ADPCM payload at or below
77,312 bytes (about 7.01 seconds) to leave room for persistent chop metadata.

The new `song` command reserves USR1's metadata sector and uses USR3 when needed: up to 317,440
source samples, approximately **14.40 seconds**, with USR2 preserved. It normalizes the whole song
once and encodes the two chunks independently. Paired-capable firmware is required; the uploader
checks the capability before writing. This command overwrites USR1 and, for long songs, USR3.
A short replacement leaves unused USR3 data in place. The previously flashed firmware does not
support this command yet.

Upload a song after building, validating and installing the new firmware:

```sh
python3 tools/fm1_sample_upload.py song SONG song.wav
```

Upload a kit in pad order (file names and musical pitch do not reorder it):

```sh
python3 tools/fm1_sample_upload.py kit 2 MYKIT kick.wav snare.wav closed_hat.wav open_hat.wav
```

Supply up to sixteen WAV paths. `kit-build MYKIT output_prefix ...` creates the same header/data
locally without connecting to a device. `song-build SONG output_prefix song.wav` writes
`.usr1.hdr` / `.usr1.bin` and, when needed, `.usr3.hdr` / `.usr3.bin`. Existing commands remain
available; the `SMP_INFO` response gains a trailing `K, 1` capability marker in KO builds.

## Controls

| Control | Song HOME | Drum HOME |
|---|---|---|
| K1 | Selected chop start | Kit level |
| K2 | Selected chop end | Kit pitch |
| K3 | Scroll waveform without changing selected chop | Bit reduction |
| K4 | Horizontal zoom, 1x..64x | Low-pass tone |

KO knobs accelerate during sustained fast turns: continuous controls move up to
eight times farther per detent, while slow turns and direction changes retain
fine control. Zoom acceleration is capped at twice the normal movement. Loop,
latch, split/join and other small discrete choices stay unaccelerated. This is
enabled in KO regardless of the standard UI's KNOB ACCEL preference.
| Keyboard | White keys: chops 1..16, black keys: chops 17..27 | White keys: pads 1..16 |
| PRESETS | Select chop, including second bank | No preset replacement |
| OCT+ / OCT- | Selected chop pitch up/down one semitone, -24..+24 | Reserved |
| EDIT | Toggle waveform / chop options | Return to kit controls |

KO uses a green palette while enabled; disabling it restores the selected Felucca palette.
The waveform omits start/end millisecond readouts. Physical B3 and B4 are pads 4 and 11;
A-sharp keys are silent. Selecting a chop with its key or PRESETS recenters the waveform.

On the chop-options page, K1 turns held looping on/off, K2 turns latch on/off, and K3 splits
at the cursor when turned up or joins with the previous chop when turned down. Loop defaults
**on** and latch defaults **off** for every automatic chop and existing zero-flag record.
Turning a toggle clockwise enables it, counterclockwise disables it. With loop on, release
fades the chop; with loop off, it completes once. Latch ignores note-off; another trigger of
the same note releases it. Latch applies to keyboard, MIDI and sequencer note events. Transport
stop/all-notes-off releases latched voices. Flag and pitch changes apply to subsequent triggers.

Boundaries move in approximately 10 ms increments; endpoints are independent and may leave gaps or
overlap. Start order is retained. The minimum chop is 64 source samples. Pitch uses varispeed:
changing pitch changes duration. A voice snapshots pitch at its trigger; edited pitch affects the next
trigger. Existing boundaries may affect looping/reversing voices, as in SLICE's manual editor.

FX tap cycles FILTER / DELAY SEND / CHORUS SEND / REVERB SEND and DISTORTION / DELAY TIME /
FEEDBACK / LEVEL. Filter and distortion are per part; effect sends are per part into existing shared
buses. Delay time/feedback are global. Held FX retains the existing performance layer.

ENV opens the selected part's compressor: threshold, ratio, attack, release. LFO opens makeup gain.
Song and kit have independent settings and detector/gain state. Ratio 1 with makeup 0 bypasses exactly.
Compression uses a peak detector with control-block gain smoothing before pan and sends;
SPREAD's mid/side channels receive the same gain to retain stereo balance.
threshold references the internal pre-level signal. The existing master limiter remains downstream.
HOME returns to sample controls; EDIT on song HOME opens the chop-options page.

SEQ, REC, SAVE, and their existing layers remain available: live overdub, stopped step recording,
up to four notes in a step, swing, velocity, ratchets, and locks on existing track parameters. This
version uses the piano roll for the sample kit; it does not add a sixteen-lane drum grid. Independent
chop endpoints/pitch and compressor settings are not new automation parameter IDs.

## Persistence and compatibility

KO edits and the two compressors belong to sample slots, **not individual projects**. A project
keeps the existing source/engine references and track FX/sequencer values. Projects using the same
samples share the KO metadata.

The `FKO1` record resides at +256 in USR1's final sector, beside the existing `SLM1` record at +0.
Both are preserved by saves in this build. Slot CRCs bind metadata to the corresponding samples;
malformed records are ignored, and decoder checkpoints are rebuilt outside the audio ISR. Writes
wait until stopped, with no active voices, and idle for one second. A flash error retries; a full USR1 reports **KO EDITS
NOT SAVED** and keeps changes only in RAM. One copy means a power cut during saving can lose edits,
but cannot erase sample audio when the reserved-sector size limit is honored.

Paired chunks use version-1 sample headers with reserved-byte tags and a shared pair identifier.
Missing or mismatched continuations silence the paired song. Song metadata is bound to both chunks.
An interrupted upload can lose the old song; uploading both chunks again recovers it.

Older Felucca can read these projects and each sample header, but plays the chunks separately and does not provide KO pad mapping,
independent chops/pitch, or compression. Its ordinary USR1 slice saves may erase the KO record.
Existing sample backups cover the sample header/data, not this tail metadata; a complete KO metadata
archive is future work. No updater or flash partition changes are made.

## Validation

`tests/ko_test.c` exercises the actual UI/DSP, all sixteen pad mappings, transient initialization,
independent endpoints, checkpoints, pitch, split/join, release/end/steal behavior, exact compressor
bypass, separate gain state, stopped saves, CRC mismatch, torn saves, and FUNA serialization.
`tests/ko_upload_test.py` checks exact zones and input file ordering through the real WAV encoder.
Additional source test cases cover white-key positions, scroll/zoom/pitch separation, held loops,
latch release, one-shot completion, paired decoder boundaries, mismatched pair rejection, encoder
capacity and failed upload commits. These additions pass, including the exact maximum song
capacity and rejection of one source sample beyond it. KO also passes ASan/UBSan and the
compiled target DSP budget check.
Both run in `tests/run_tests.sh`. The ordinary build's existing tests must also pass.

Physical FM1 latency, worst-case CPU load with both compressors and reverse/FX/USB active, and
listening tests still require hardware. Host checks and compiled instruction budgets do not measure
the physical deadline.

## Independent second song

The standalone build enables `FELUCCA_KO_DUAL=1`. Track 3 uses SLICE / USR3 for
a separate imported song. Existing Song 1 and drum assignments remain tracks
1 and 2; track 4 is reserved and silent. Existing one-song banks load normally
with Song 2 empty. Select track 3 with ALGORITHM; if upgrading an old project,
OCT+ on its setup prompt initializes only Song 2, keeping the other two sounds
and patterns. Both song tracks use the same K1–K4 and OCT controls, with
independent chop tables, waveform positions, zoom, loop/latch flags, pitch and
compression. Sequencer patterns and locks use the existing third project slot.
The two songs share eight sounding voices; the kit has four dedicated voices. All three use the existing FX buses.

### Performance shortcuts and edit undo

Tap **ARP** twice on sampler HOME to reach the mute/solo page. K1/K2/K3 set mute for
Song 1 / Drum Kit / Song 2 (right mutes, left unmutes). K4 selects solo: 0 off,
1 Song 1, 2 Drum Kit, 3 Song 2. Further ARP taps cycle the send pages and return to the waveform. In the song's EDIT page,
K4 undoes/redoes the last chop edit. Black keys now play additional song chops;
they no longer perform mute, solo or undo.

Solo is temporary, preserves stored mutes, and clears on project load. A muted
track receives no new triggers and contributes no dry signal or new sends;
shared FX tails can ring out. Existing voices advance so unmuting resumes in time.

The song header shows the imported eight-character sample name and chop number,
with MUTE/SOLO status. The drum header uses the legacy kit name when available,
otherwise DRUM KIT. The small strip beneath the waveform follows the most recently
triggered voice on that song. Updating it does not redraw or decode the waveform.

Each song has its own single edit undo/redo slot covering chop bounds, pitch,
loop/latch, split and join. Scrolling, zoom and selecting pads do not consume undo.
Undo is RAM-only and clears when new audio replaces the song; edited results still
use the existing chop metadata persistence format.

### Sampler sequencer

Tap **SEQ** on sampler HOME while stopped to enable the selected track's sequencer.
The existing sequence length is preserved; K4 selects 1–64 steps. Tap SEQ again or HOME to return to samples;
playback timing remains active. Each of Song 1, Drum Kit and Song 2 can be enabled
independently. Existing sequencer pages and held-button layers remain available.

- K1: tempo (BPM).
- K2 left: TEMPO, using the existing division, swing, gate and lock behavior.
- K2 right: SAMPLE END. Each step plays its chops once to completion, at their
  pitches; multi-note steps wait for the longest voice. Loop/latch settings are
  bypassed for these sequenced triggers without changing the stored chop flags.
- K3: choose a step while stopped.
- K4: sequence length while stopped. EDIT: clear the selected step and advance.
- REC: arm/disarm. While stopped, sample keys write one chop/hit per step and
  advance, wrapping at the chosen sequence length. PLAY leaves stopped step entry, disarms the
  enabled sampler tracks, and plays the pattern. Press REC while playing to
  enable live recording again.

When armed, a white-key LED indicates the current recording step (1..16). During
unarmed playback, song keys light for sounding chops, including black keys; drum
keys light for sounding hits. Chops 28..32 are in the second bank, reached using
PRESETS; its first five white keys play those chops.

SAMPLE END follows actual voice completion, including decoder startup and pitch.
Rests, empty/chance-skipped/muted steps, and ties without a voice use one tempo
step. Ratchets/nudges are not applied in SAMPLE END; use TEMPO for those features.
Song-chain playback retains its existing tempo timing. Timing changes require the
transport stopped. Timing preferences save with the sample-bank metadata using
previously reserved bytes (record size stays 544 bytes); old records default to
TEMPO. Projects keep their existing pattern format. Loading a project disables
sampler sequencing mode until SEQ is tapped again, retaining the timing preference.

### Zoom-scaled waveform knobs

K1 moves the start by about 1/128 of the visible waveform per slow click;
K2 moves the end by about 1/256.
K3 scrolls by about 1/24 of the visible waveform per slow click, including at
full zoom-out (about five seconds on the 119-second Don't Cry song). Existing
fast-turn acceleration multiplies these movements. Zooming in reduces movement
for precise edits; scrolling never changes the selected chop. After a K1 or K2
edit, the scroll cursor jumps to that updated start or end, including an end
boundary exactly at the sample's length.

## PIGEON display and sequencer timing

Knob edits update changed text and waveform columns through the existing buffered
canvas, without clearing the page. The waveform follows the newest playing song
voice when it leaves the visible window; manual editing or scrolling pauses
following for one second. Following moves the view without changing the selected
chop or edit cursor. K1 start adjustments now move twice as far per detent and
retain zoom scaling, acceleration, and valid start/end limits.

On the 16-step sequencer page, K1 sets BPM and OCT−/OCT+ select the step
length: 1/4, 1/8, 1/16, 1/32, triplets, or longer divisions. These are the existing
project-compatible per-track divisions, with swing retained. SAMPLE END still
waits for the complete chop; its rests use the chosen division. OCT keys continue
to control pitch on the waveform/edit pages. The existing three-second photo
splash remains until the replacement PIGEON logo is supplied.

Each song chop has independent positions: K1 can cross neighboring chops, but
stops before its own end. Extend the end with K2 to move the start farther. Pad
assignments stay fixed while positions change; saving retains this order. Split
inserts a new pad after the selected one, and join merges the selected pad into
the previous pad using the earliest start and latest end.

## Patterns, arrangement and mixer

This workflow takes inspiration from Pocket Operator for Pixel's direct phrase
selection, duplication and track mixing. Four phrase slots **A–D** reuse the
existing saved project slots. A slot holds all three tracks' sequences, timing,
parameter locks and automation. Sample/chop data stays in the shared sample bank;
selecting a phrase keeps the current instruments, levels, mutes and FX.

Tap **SCL** to cycle **Patterns → Arrangement → Waveform**. In Patterns:

- The first four white keys select A–D. During playback a saved selection queues
  for the next 4/4 bar, with a blinking pending-pattern LED. The last selection wins.
- K1 or PRESETS selects the target slot. An empty target keeps the current working
  phrase, ready to save. Save edits before selecting another occupied slot.
- K4 left copies the current working phrase, including unsaved edits; right pastes
  the clipboard into the selected target and saves it. OCT−/OCT+ also copy/paste.
- K2 copies a saved source A–D into the clipboard without changing the working phrase.
- K3 right appends the selected pattern to the arrangement; left removes its last section.
- REC saves the working phrase and arrangement to the selected slot. PLAY loops its
  saved phrase. EDIT returns to the waveform.

In Arrangement, the first four white keys append A–D. K1/PRESETS selects a section;
K2 sets its pattern for all tracks; K3 sets 1–16 repeats; K4 right inserts a section
following the selection, and left deletes it. PLAY runs the sections in order and
stops after the final repeat. There are at most 16 sections. Existing arrangements
with different patterns per track remain playable; the display marks them LANES.
REC saves the arrangement with the selected pattern slot. The existing SAVE project
page also remains available.

Copy, paste, save, and arrangement edits require stopped transport. Saving/pasting
also requires releasing sounding pads to avoid a flash erase during audio. Live
launches read sources cached before PLAY and never load projects from flash in the
ISR. On stopping live pattern playback, the last playing phrase becomes editable.
Pattern launches and arrangements use TEMPO timing; standalone SAMPLE END sequencing
remains available on the 16-step page. Queued launches use a 4/4 bar even when a
phrase uses a different step division or length.

Tap **ARP** to cycle **Levels → Mute/Solo → Delay → Chorus → Reverb → Waveform**.
K1 controls Song 1, K2 the drum kit and K3 Song 2. K4 sets the selected track's pan
on levels/send pages, and selects solo on the mute page (0 off). These controls
work during playback and retain the existing buffered partial screen updates.

Workflow references: [Pocket Operator for Pixel announcement](https://teenage.engineering/_img/629b0b5bd9a5e7000404f5d0_original.pdf)
and [PO-33 pattern copy/chaining guide](https://teenage.engineering/guides/po-33/en).

### Sequence grid and length

The sampler sequencer supports **1–64 steps per track**, saved with the project.
Tap SEQ to open it without changing the existing length. K1 controls BPM,
K2 selects tempo or sample-end timing while stopped, K3 selects the step,
and **K4 sets sequence length while stopped**. OCT keys select the step division.
EDIT clears the selected step and its locks, then advances (use this for rests).
REC arms step entry; PLAY starts playback.

The grid shows 16 steps per page and follows the selected or playing step.
A dot marks a note, a bottom line marks a step outside the chosen length,
and the highlighted cell marks the current step. During recording the key LEDs
show the step within its 16-step page; during playback they show sounding chops.
Shortening a sequence preserves later notes so expanding it restores them.

### Direct beat editing and performance controls

On the stopped, unarmed sequencer grid, K3 selects a step and a sample key assigns
its chop without advancing or sounding. REC retains the existing preview and
advance workflow. EDIT clears the step and its locks and advances. Hold SAVE to
undo/redo the last assignment, clear, or step-detail gesture.

Each grid cell shows its step number above its chop/pad number. ENV switches
between the grid and step details: K1 velocity, K2 gate (a per-step parameter
lock), K3 probability, K4 repeats (1–4). PRESETS selects a step on the detail page.
LFO opens timing controls: K1 swing, K2 track gate, K3 BPM. Gate and repeats use
TEMPO playback; SAMPLE END continues to wait for complete samples. Kit one-shots
retain full playback by default; an explicit per-step gate lock cuts the hit with
a short fade. Per-pad decay offers independent envelope shaping.

On the drum waveform/home page, ENV opens pad settings. A white key selects and
plays a pad. K1 pitch (±24 semitones), K2 level (0–127), K3 decay (0 full length,
1–127 approximately 12 ms–1.5 s), K4 choke group (0 off, 1–4). Pads sharing a
nonzero group fade the previous pad when triggered. Settings save in the sample
metadata, bound to that kit's audio CRC; replacing a kit resets its pad settings.

On FX pages, hold LFO while adjusting a knob to make a temporary effect change;
releasing LFO restores its prior value. EDIT resets filter, distortion, delay,
chorus and reverb sends on the selected track. Compression remains separate.

On Patterns or Arrangement, ENV opens arrangement tools: K1 chooses NEXT BAR or
PATTERN END launch timing; K2 moves the selected section; K3 right duplicates it,
including repeats and per-track pattern assignments. ENV opens variations; SCL returns to Patterns;
REC saves the chain and launch preference with the selected project. Pattern-end
launches use the arrangement clock track's loop (Song 1 before a pattern launch);
tracks with different sequence lengths may therefore restart midway. Sources
remain cached before playback; switching does no flash writes or decoding.

### Local library and complete backups

Use `.venv/bin/python tools/pigeon_library.py`. All sample edits are prepared
locally; only `upload` or `restore` writes to the instrument. Stop playback and
release pads before device transfers.

```
# Capture patterns, current music, settings, presets, songs, kit and chop metadata.
.venv/bin/python tools/pigeon_library.py backup build/session.pigeon
# Replace just Song 2 in an exported .kob (Song 1 and kit retain encoded bytes).
.venv/bin/python tools/pigeon_library.py replace-song build/new.kob build/current.kob song.mp3 --slot 2 --start 0 --seconds 30 --bitrate 64
# Replace kit only; provide 1–16 files in white-key pad order.
.venv/bin/python tools/pigeon_library.py replace-kit build/new.kob build/current.kob kick.wav snare.wav hat.wav
# Preview the actual encoded song locally, before upload.
.venv/bin/python tools/pigeon_library.py preview build/new.kob build/preview.wav --slot 2 --play
.venv/bin/python tools/pigeon_library.py info build/new.kob
.venv/bin/python tools/pigeon_library.py upload build/new.kob
.venv/bin/python tools/pigeon_library.py restore build/session.pigeon
```

Imports accept formats supported by ffmpeg, convert to mono 22.05 kHz, and report
remaining shared capacity. Untouched songs, kit audio and metadata are retained;
replaced songs receive fresh transient chops. The tool refuses an oversized bank
before writing. Complete archives validate every object's CRC before restore and
save an automatic recovery archive first. Restore commits objects individually:
a disconnect can leave a partial restore; reconnect and restore again. Sample
uploads also make a recovery bank and verify the encoded payload after writing.

## Chop envelopes and operations

From song HOME, EDIT opens chop settings; ENV then opens the envelope page: K1 attack (0–155 ms),
K2 release (0–310 ms; 0 uses the track default), K3 hold/one-shot/latch,
and K4 loop on/off. ENV returns to chop settings; EDIT returns to waveform editing. Legacy chops keep their
existing held-loop behavior. Explicit one-shot always plays once.

LFO on the chop settings page opens chop operations: K1 selects a chop, K2 chooses a settings
source, K3 clockwise duplicates / counterclockwise splits, and K4 clockwise
copies settings / counterclockwise merges with the previous chop. Settings copy
preserves boundaries. To overwrite an existing key with the whole chop, choose the destination with K1 and the source with K2, then tap REC. This copies boundaries and all playback settings without inserting a chop or shifting any keys. Stop playback and release pads first. LFO returns to chop settings; EDIT returns to the waveform.

On the grid, LFO opens timing; LFO again opens recording preferences. K1 chooses
metronome OFF / REC / ON, K2 independently chooses count-in OFF / 1 bar / 2 bars,
K3 selects live quantization OFF / 1/16 / 1/8, and K4 toggles song choke.
Quantization affects live input during transport, not stored pattern events.
Choke fades earlier voices on the same song track. Stop cancels queued live notes.

From Arrangement, ENV opens tools; ENV again opens variations: K1 chooses destination A–D, K2 clears
selected-track notes while retaining locks, K3 clears locks separately, and K4
copies the current project to the chosen slot. Copy/save stops playback before
flash access. Existing project A/B storage commits retain the last valid project
after interrupted writes; sample/chop metadata remains a single-copy sector.

## Tempo, rolls and advanced sequencing

From SEQ: LFO opens timing, LFO again opens recording preferences, and ENV opens
tempo tools. EDIT taps tempo (40–240 BPM); a gap over two seconds starts a fresh
tap series. K1 adjusts BPM, K2 sets the selected chop's beat count (1–16), K3
applies its calculated BPM, and K4 selects the chop. Calculation uses the
unpitched audio length, not an automatic musical beat detector.

From the grid: ENV opens step details; ENV again opens advanced step editing.
K1 sets pitch ±24 semitones using an existing parameter lock, K2 sets microtiming
from −8 to +7 sixteenths of a step, K3 sets the condition, and K4 selects the step.
Microtiming enables unquantized step playback. As in the inherited sequencer,
nudges apply to single-repeat NOTE steps; repeated steps stay on their subdivision.
Conditions are ALWAYS/probability, EVERY SECOND PASS, EVERY FOURTH PASS, FIRST
PASS, and AFTER FIRST. They replace the probability choice on that step; the
probability knob restores a probability-based step. Every-second/fourth starts
on passes 2/4. Transport restart resets counts; stopped pattern selection also
resets them. Early step-zero nudges use the upcoming pass's condition.

From drum HOME: ENV opens pad settings; LFO opens roll controls. K1 selects
1/4, 1/8, 1/16, or 1/32, and K2 toggles roll OFF/ON. With roll enabled, held drum
pads repeat at that division, whether or not transport is running. Release,
Stop, or MIDI panic cancels repeats. Roll note-ons can be recorded with the
existing recording workflow. Up to eight held pad notes are retained.

## Chop groups, song gain and alternate layouts

From song HOME: EDIT opens chop settings, LFO opens operations, and FX opens
group/layout/gain controls. K1 sets the selected chop's choke group (0–3; 0 allows
overlap). Only chops in the same nonzero group cut each other off; song-wide
choke still cuts all earlier chops on that song track.

K2 switches A/B chop layouts; K3 adjusts non-destructive track gain (0–127);
K4 clockwise copies the current layout into the other layout. Stop and release
all pads before switching or copying layouts. B initially copies A. Each song
has two independent layouts of up to 32 chops using the same encoded audio, so
layout changes consume no sample payload. Layouts retain boundaries, pitch,
envelopes, loop/latch/one-shot settings and choke groups. Gain stays in the
existing project track-level field; saving a project/session retains it.

The GUI step editor also exposes pitch, nudge and conditions, and its chop
editor exposes group 0–3. Its chop edits affect the layout active in the loaded
bank. Full bank/session backups include the inactive layout too. Extended
conditions and transpose locks require this firmware; older firmware may reject
projects that use them. Existing projects keep their layout and defaults.

### Drum-loop tempo sync

Song 2 (and Song 1) now has optional speed-based tempo sync. From the sequencer,
press LFO for timing, LFO for recording preferences, ENV for tempo tools, then
FX for sync. K1 sets 1–64 beats, K2 turns sync off/on, K3 chooses selected-chop
or whole-song reference, and K4 selects the reference chop. Adjusting these
controls captures the reference length. Chop-boundary edits afterward do not
change that reference until you adjust a sync control again.

Enter the number of beats actually contained in the reference, rather than
its desired duration. The project BPM sets playback speed. For example, a
four-beat loop lasting two seconds is 120 BPM; at 90 BPM it plays at 75% speed.
Sync changes pitch with speed; OCT and step pitch remain additive. The sync
ratio is limited to 0.5–2.0, so tempos outside that range will not match exactly.
The whole-song setting changes the timing reference, not the chop boundaries:
use a chop spanning the complete loop for whole-loop playback, or sequence the
existing chops for sliced playback. Slice timing still comes from the sequencer.

Sync defaults off and is stored with the sample-bank metadata when edits are
saved. It is bound to the imported audio CRC, and a new sample resets it. The
new optional KSY1 record is at metadata offset 3456; existing sample capacity,
project format, and other metadata records are unchanged. Hardware playback
validation is required before calling tempo sync verified on the FM1.
