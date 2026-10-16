# PIGEON v0.3.1

Release date: October 16, 2026. Includes the piano roll, four-note sequencer entry and REC indicator fix. Flashed to an FM1: reboot and firmware identity verified, with the complete sample bank and all session objects unchanged. Hands-on checks of the new controls remain pending.

## Piano roll

- Press SEQ to open the existing step grid. Press SEQ again to switch to the piano roll; further presses alternate between the two views.
- HOME returns to the waveform. Both views edit the same sequence; switching views does not clear or copy notes.
- The sequence length is **1–64 steps**, controlled by K4 while stopped. Sixteen visible columns are a window, not a sixteen-step limit. K3 selects the step and moves to later pages automatically; playback follows the playing page.
- Piano-roll rows show MIDI note/chop-key assignments, with a keyboard at the left. PRESETS scrolls through the note rows. Notes outside the current vertical window remain unchanged and can be viewed by scrolling.
- K1 sets BPM, K2 chooses tempo/sample-end timing, OCT changes step duration. These controls match the grid.
- While stopped and unarmed, chop keys toggle individual notes on the selected step, up to four notes. The grid shows a chord as `2N`, `3N` or `4N`; the roll draws each note. REC arms step entry: hold up to four keys together, then release all keys to advance once; during playback, armed keys record live. EDIT clears the selected step and advances. ENV opens step details, and LFO opens timing.
- The roll shows stored chord notes and legacy drum-lane hits. Pitch automation remains in advanced step details; the roll’s vertical axis identifies the triggered note/chop key, not the transposition of the audio inside that chop.
- REC stays lit whenever any track is armed, including inverted LED mode.
- View selection and row scrolling are temporary UI state; the project and sample-bank formats are unchanged.

Polyphony validation is recorded locally in `build/poly-test.log`, `build/poly-san.log` and `build/poly-product-tests.log`; firmware and browser builds are in `build/poly-firmware-build.log` and `build/poly-wasm-build.log`. The polyphony and REC-light changes still need physical control checks.

The functional, sanitizer, storage and codec checks pass. The target static instruction estimate currently flags `fm1_alnk0_irq` at 42,683 against a 37,502 baseline (+14%, above its +10% threshold); host DSP regression reports no CPU entries over budget. This static estimate remains under investigation; it is not a measured hardware CPU result. Sustained hardware polyphony and the new controls still need hands-on testing.
