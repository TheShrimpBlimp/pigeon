# PIGEON v0.3 UI

The local firmware uses one visual system throughout its sampler pages and
support screens: Little Character, PIGEON green, a dark background, rectangular
geometry and explicit button names. The splash artwork remains unchanged.

The top strip shows the selected track, phrase, transport/record state, tempo,
BPM lock and battery. Temporary messages use that strip. The next line identifies
the sample and selected chop, drum pad or sequence step where relevant.

Parameter pages use four cards, arranged in knob order: K1 top left, K2 top
right, K3 bottom left, K4 bottom right. Each card names its control and shows its
value. Units stay small; long values use a smaller face rather than clipping.
A short bar represents bounded continuous amounts. Actions and choices have no
amount bar. An unused knob has no card.

Page names sit above the controls. The bottom two lines explain the secondary
action and navigation. FX explicitly labels EDIT as RESET; HOME returns to the
sample. Existing knob assignments, shortcuts and sound parameters are preserved.

The waveform retains its detailed peak display and moving playhead. The
sequencer retains its sixteen-cell window and live record/play status. Patterns
uses four large A–D tiles with saved/empty, selected, playing and queued states.
Arrangement and track mixers use the same parameter cards as sample editing.
GLO and SEQ shortcuts use compact knob cards above a framed text map of keys.
Settings, confirmations, project naming, calibration and count-in share the
same typography and rectangular controls.

## Review and checks

`tools/render_pigeon_ui.c` renders the actual firmware with synthetic fixtures.
`tools/package_pigeon_ui.py` creates the gallery, native PNGs, contact sheets,
PDF and ZIP under `build/pigeon-ui-atlas/`. These are local review artifacts.

`tests/pigeon_screen_test.c` checks text clipping across all sampler views,
maximum FX values, held shortcuts and visible settings. UI tests also cover
cached redraws, queued-pattern boundaries and live sync-speed updates.

Building or rendering does not flash hardware or publish website firmware.
