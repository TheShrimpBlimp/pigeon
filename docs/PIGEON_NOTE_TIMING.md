# Per-note timing — PIGEON v0.4

Open SEQ, press SEQ again for the piano roll, then tap **FX** for **Note / Length + Nudge**. Stop playback before editing. These controls are independent of the track’s DIV setting.

| Control | Action |
| --- | --- |
| K1 / PRESETS | Choose one note in the selected step |
| Chop key | Select that existing note without adding or deleting it |
| K2 | Length: track gate, or 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 or 64 steps |
| K3 | That note’s nudge, −8 to +7 sixteenths of a step |
| K4 | Select the step |
| FX | Return to the piano roll |
| HOME | Return to the waveform |

Long notes extend across subsequent steps. The roll draws their duration across columns and pages. Returning with FX preserves the selected note; it is highlighted and its pitch, length (steps or GATE) and individual nudge appear below the roll. Nudge shifts one note’s start; other chord/drum notes retain their own timing. A negative nudge starts during the preceding step. Before the first step after PLAY, an early note starts at time zero. Notes cannot start before playback begins.

These settings apply in **tempo timing**. For sample-end timing, return to the roll and turn K2 left to select tempo first. Editing custom note timing enables unquantized playback. It changes that step’s repeats to one hit if needed, with a message; per-note timing and ratchets are not combined in this version.

Length is a gate duration, not time stretching. A nonlooping sample can finish before its gate expires; use a held/looping chop to sustain longer. Explicit drum lengths can shorten a hit. Existing track-gate behavior stays the default for untouched notes.

Edits participate in sequence undo/redo. Deleting one note shifts the remaining notes’ timing with them. Patterns, arrangements and complete `.pigeon` sessions retain the settings. A `.kob` bank stores samples/chops, so use a session to move sequence timing.

Custom note timing shares a limit of 128 entries with automation and parameter locks. If storage is full, the editor reports it without overwriting another entry. Older projects load with default note timing; projects saved with v0.4 require v0.4 or newer.

A repeated pitch, choke group or full voice pool can replace a sustaining note, just as in ordinary playback.
