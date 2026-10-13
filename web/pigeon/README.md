# PIGEON FM1 Sampler GUI

A standalone browser tool for PIGEON on the FM1. The same files run on macOS,
Windows, and Linux, locally or on an HTTPS website. There is no backend, account,
CDN, analytics, or audio upload. Chrome or Edge is recommended for USB MIDI/SysEx.
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

1. Open a `.kob` bank, open a `.pigeon` session, read the device, or import Song 1.
2. Choose audio, enter start/end times, choose 24–64 kbps, preview, and import.
   Song 2 requires Song 1. Capacity is checked before changing the working bank.
3. Select up to 16 one-shots in pad order, preview them, then Replace kit.
   Bulk selection replaces the entire kit. Individual pad selection updates that
   pad and preserves the other pads. Gaps use tiny silent hits. Clear kit removes all pads.
4. Save bank downloads a file. Send bank saves a full recovery session locally and
   initiates a recovery download before writing; it verifies audio and metadata
   by reading the device back. Back up full session includes all ten project and
   settings objects plus both songs, drums, and chop metadata.
5. Open a `.pigeon` session to enable Restore session. Restore validates every
   checksum before writes, saves a recovery session first, restores current
   music last, and verifies the resulting session. Object commits are separate:
   interruption can leave a partially restored device. Recovery stays available
   through Save recovery backup, including after reload.

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
page, styles, JavaScript, worker, encoder, licenses, and source archive. Every
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
No website deployment has been performed.

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
