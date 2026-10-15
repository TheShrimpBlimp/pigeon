# PIGEON Sample Loader 2.0

A standalone browser tool for PIGEON on the FM1. The same files run on macOS,
Windows, and Linux, locally or on an HTTPS website. There is no backend, account,
CDN dependency or audio upload. Successful firmware installs may report an
anonymous completion count; samples and device identity are not included. Chrome or Edge is recommended for USB MIDI/SysEx.
Other browsers may support local editing while lacking device connectivity.

## Open locally

From the repository root:

```
python3 tools/pigeon_gui.py
```

On Windows use `py tools/pigeon_gui.py`. This opens
`http://127.0.0.1:8765/pigeon/`. The server binds only to loopback and serves only
`web/`. Stop it with Ctrl+C. Use `--no-open` or `--port 8766` if needed.
Opening index.html directly with `file://` does not support modules, Web MIDI,
or service workers correctly. No Python or download is needed on the hosted website.

## Workflow

1. Open a `.kob` bank or `.pigeon` session, or use **Add audio files** under Song 1.
   Each slot accepts several files; adding more appends them to that same song.
2. Drag to select audio. Backspace/Delete removes the selection; **Keep selection**
   removes everything outside it. Place the cursor and press S to split. Drag clip
   chips to rearrange. Copy/cut/paste and Ctrl/Cmd+Z work on draft edits.
3. Preview using Listen controls. Scroll to zoom; pan with Alt-drag or the overview.
   Expand **Clip settings · trim, gain & fades** for precise per-clip processing.
   Open **Export range & quality** for exact range and 24–64 kbps MP3 quality.
   **Apply to local Song 1/2** encodes and commits the draft to the working bank.
   Song 2 requires an applied Song 1. Whole clips occupy the first chop keys; automatic slices fill the remaining slots, up to 32 total.
4. Choose up to 16 drum one-shots in pad order. Bulk file selection applies the
   kit locally; a single pad file replaces only that pad. Clear kit removes all pads.
5. **Save bank** downloads `.kob`. **Try this bank in emulator** opens the applied
   bank or loaded session in the hosted emulator; switch it on to load the content.
   The published FM1 storage limit is **496 KiB shared** between both songs and kit.
   v0.3 uses the expanded pool. Older connected firmware is limited to its reported capacity before any transfer. Unlimited browser banks may not fit hardware.
6. **Connect / back up** opens FM1 ports and backup tools. Connect matching ports,
   back up the device separately, then **Send bank to FM1** and review the change.
   Sending samples does not automatically make a full-session backup. It verifies
   bank audio and metadata by reading back. Keep USB connected until finished.
7. **Back up full session** includes projects, settings, both songs, drums and chop
   metadata. Open `.pigeon` to enable **Restore session to FM1**. Restore validates
   checksums, saves a recovery session first, restores current music last and
   verifies the result. Object commits are separate, so interruption can leave a
   partial restore. **Save recovery backup** retains recovery access after reload.

For a complete first-song walkthrough and hardware controls, see
[the tutorial](../../docs/KO_MODE.md) in the firmware repository.

The working bank persists in IndexedDB. Audio-file previews and staged imports
are temporary. Download bank/session files to keep durable backups. Replacing
song audio resets its chop settings through the firmware's existing CRC check;
replacing kit audio resets per-pad settings. Untouched encoded components and
metadata are preserved byte for byte. Chop edits preserve encoded audio.

Use **Chop editor** to load either waveform, drag boundaries, adjust pitch and
attack/release and choke groups, choose hold/one-shot/latch and looping, or detect transients with
adjustable sensitivity. Duplicate, split, merge, and copy settings work locally.
Rename changes the eight-character device name; full imported filenames remain
in local browser storage and edited session archives. Pad names are browser/session
labels, since the device bank has no per-pad name field. Pad settings allow pitch,
level, decay, choke groups, reordering, and removal without shifting later keys.

Open a full session to use the pattern grid: choose current project or A–D, track,
1–64-step length, chop/pad, pitch, microtiming, condition, velocity, probability,
and repeats. Conditions replace probability for that step. Extended pitch locks
and conditions require the updated firmware. Save full session
exports the edits; Restore session applies them to the instrument. Local session
edits persist across reloads.

**Firmware update** accepts a local PIGEON `.fwsc`, validates package checksums
and identity, reads the connected version, saves a full recovery session before
installing, and verifies the returning identity. Keep USB connected during the
update. This path uses the existing firmware update protocol; physical-device
verification of the new GUI remains pending. Updating needs a responsive PIGEON
device so the mandatory backup can complete. For a device already in loader
mode, use the CLI installer recovery procedure.

## Offline and hosting

Wait until **Offline ready** before disconnecting. The service worker caches the
page, styles, JavaScript, shared footer, worker, encoder and bundled licences. Every
feature—including new imports and USB transfers—runs locally after caching.
The open page needs no server connection. Its cached URL can reopen offline
while site data is retained. The browser can clear/evict storage; first load or
an incomplete cache still needs connectivity. An offline page cannot recover a
missing dependency. Updates wait until the old app is closed, avoiding replacement
of code during a device transfer.

Host `web/pigeon/` and its sibling modules `web/ko_bank.js`,
`web/fm1backup.js`, `web/fm1pkg.js`, and `web/fm1ota.js` on the same HTTPS origin, preserving their relative paths.
`web/make_site.py` includes it at **webapp/pigeon/** in the generated site.
Set a MIME type of `text/javascript` for `.js` files and allow Web MIDI with
`Permissions-Policy: midi=(self)` if configuring that header. A same-origin page
is preferable to an iframe; an embedded page also needs MIDI permission delegation.
The hosted site also serves shared `footer.css` and `footer.js` at the site root.
Keep those assets available when hosting; the loader caches them for offline use.

## Audio and verification

Web Audio decodes locally supported input formats and mixes/resamples to mono
22.05 kHz. PCM WAV and MP3 are tested; availability of other formats depends on
the browser. Samples normalize to peak 30000, matching the CLI policy. Importing
very large files can exhaust browser memory; imports are limited to 256 MiB and
10 minutes, with firmware sample-pool limits applied to the trimmed result.

MP3 encoding happens in a dedicated worker using vendored **lamejs 1.2.1**.
The output sample rate is explicitly fixed to 22.05 kHz at every bitrate. Every
frame is checked for firmware-compatible MPEG-2 mono, size, and zero reservoir;
then an index and 1105-sample combined encoder/decoder skip are added.
Drums use the same IMA ADPCM encoder and bank format as the Python tools.
Transient detection generates up to 32 initial chops. Chop envelopes and explicit
trigger modes require the corresponding updated firmware; older builds do not
understand the new reserved flag bits.

Tests:

```
node web/pigeon/test.mjs
# Editing tests compile the native firmware importer (requires cc and build/gen):
node web/pigeon/test-editing.mjs
python3 tests/pigeon_gui_codec_test.py --node node
# Browser test requires Playwright + Chromium. Repository-local setup:
npm install --prefix build/gui-deps/testing playwright
PLAYWRIGHT_BROWSERS_PATH=build/gui-deps/browsers node build/gui-deps/testing/node_modules/playwright/cli.js install chromium
PLAYWRIGHT_BROWSERS_PATH=build/gui-deps/browsers node web/pigeon/test-browser.mjs
```

The core test covers all six bitrates, independent replacement, kit preservation,
archive validation before writes, protocol replies, readback, and restore order.
The browser test imports and encodes stereo WAV, then disables network access
and imports Song 2, imports drums, previews, exports, reloads from cache, checks
persistence, and checks mobile layout. It also edits chops, drum settings, patterns, and
validates a firmware package offline. The editing test checks the exact FUNA
bytes through the actual firmware importer. Device protocol tests use a simulated FM1;
real USB transfers and Windows/Linux hardware testing remain necessary.

The tool is GPL-3.0-only. The separately bundled LAME/lamejs encoder is LGPL-3.0;
its notices, readable modified runtime, original corresponding source, and
license texts are in `vendor/`.

The GUI uses Little Character Regular by S M Astrid Bin, under SIL OFL 1.1.
The unmodified font and original license are bundled in `fonts/` and cached offline.

## Emulator banks

The emulator can save its applied sample bank as `.kob`, including current chop and playback metadata. Open that file here, or use **Open session in sample loader** in the hosted emulator to carry it here locally. Connect the FM1 and use **Send bank to FM1** to review and upload it. This replaces the device sample bank; it does not install firmware. A `.kob` contains samples and chop settings. Use **Save session (.pigeon)** or **Open session in sample loader** to include the current sequence, saved A–D patterns, settings and user presets. In the loader, **Restore session to FM1** restores all of these together and replaces the device projects, settings and sample bank; its existing recovery backup and verification steps apply. Unlimited banks must fit the hardware storage limit before export.

## Joining samples into a song

Use **Add audio files** for either song to select one or several files. More files
append to the same slot. Split the timeline and drag clip chips into order;
expand **Clip settings · trim, gain & fades** for per-clip adjustments and move
buttons. **Rebuild song from clips** refreshes a preview after detailed changes;
**Apply** commits encoded audio into the local bank. Each clip becomes a chop
(up to 32); cropping the combined waveform adjusts those boundaries. Encoding
and capacity checks happen before the bank is replaced.

Clip gain ranges from −24 to +12 dB. Fade in/out defaults to 3 ms and can be set from 0 to 100 ms; processing happens when rebuilding. Gain is applied per clip and clipped to the valid audio range and its level is preserved during encoding. No chop naming is required.

To copy a chop onto any existing key in the same song, select the destination in Chop, choose Copy from, and click Copy whole chop here. It overwrites boundaries and all playback settings, preserving chop count and every other key's assignment. Copy settings remains available separately.


## Rearranging keys, undo and saved sessions

Select a chop and choose the other key under Copy from. **Swap selected with Copy from** exchanges the two assignments; **Move selected to Copy from key** shifts intervening assignments without discarding chops. Existing pattern note numbers remain unchanged, so their triggered audio follows the new assignments.

In Drum pad settings, select the source pad and a Copy to pad destination, then **Copy pad to key**. Audio and playback settings are copied together; unused intermediate keys become silent pads. Send the edited bank to use it on your physical FM1.

**Undo** and **Redo** retain 20 committed workspace snapshots while this page is open, covering bank, chop, pad, label and pattern/session edits. Ctrl+Z / Cmd+Z undo; Ctrl+Shift+Z / Cmd+Shift+Z and Ctrl+Y redo. Text inputs keep their normal text undo. Draft audio has its own undo history while editing; uncommitted fields, device transfers and library deletions are outside workspace history. Reloading clears undo history but retains the current workspace.

The **Saved session library** stores named banks and full sessions in this browser. Save, load, rename, delete or download entries. Loading is a local edit and can be undone. Download copies to preserve them outside browser storage. A bank contains samples and settings; a full session also carries patterns and supported device settings.


## Sample loader 2.0 waveform controls

Click the waveform to place its cursor; the white cursor head can be dragged. Drag the green S/E flags to trim, drag the song waveform to select a region (Shift-drag also selects); in the chop editor, drag inside its selection to move the complete region. Scroll over the waveform to zoom around the pointer, or use the optional Zoom slider. Drag the overview below the waveform to pan; Alt-drag or Shift-wheel also pans. Double-click resets the view. Arrow keys move the cursor; Home/End jump to the file edges. The cursor follows section and chop previews. Precise numeric start/end fields remain available.


## Timeline editing

The waveform uses the firmware’s 220-column envelope. Drag to select audio; Backspace/Delete removes it, Keep selection discards everything else, and S splits at the cursor. Drag clip chips to rearrange them or use their move buttons. Copy, cut and paste work at the cursor. Ctrl/Cmd+Z and redo also cover draft audio edits. Adding another file appends it to the current song. Apply commits the edited audio to the bank.

Use **Try this bank in emulator** to open the current bank or full session directly in the hosted emulator. Switch the emulator on to load it. **Load bank / session** also accepts saved `.kob` banks and `.pigeon` sessions; sessions retain their patterns. Banks must fit the connected device’s reported capacity before transfer.
