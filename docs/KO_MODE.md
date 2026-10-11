# KO Mode v0.1

For the optional shared bank and minute-long songs, see [Expanded KO storage](KO_STORAGE.md).
The slot capacities below describe `FELUCCA_KO=1` without `FELUCCA_KO_STORAGE`.

An optional interface for one transient-chopped song sample and sixteen separate drum one-shots.
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
ALGORITHM selects SONG or DRUM while KO is enabled. Disable KO MODE to access all four tracks.

## Samples

- USR1: one mono song sample. Transient detection produces up to 32 initial chops. Sixteen white
  keys trigger the current bank; PRESETS selects a chop and switches banks as necessary.
- USR2: up to sixteen independent drum files, each on one exact note, 60..75. Empty pads are silent.
  Release does not cut a hit short. Eight voices are shared across tracks; stealing fades old hits.
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
| White keys | Trigger and select chops in current bank | Trigger pads 1..16 |
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
