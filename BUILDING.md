# Building PIGEON

The standalone PIGEON development profile is built with `./build-sampler.sh`.
It enables the sampler interface and expanded sample storage, producing
`build/pigeon.fwsc` and `build/pigeon.bin`. The underlying build also produces
the compatibility filenames below. `./build.sh` alone builds the inherited
Felucca baseline for regression work.

The underlying build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `felucca.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow and fontTools: `pip3 install Pillow fonttools` (the UI font and icons are
  rasterised at build time). Pillow must include libraqm support for OpenType font features; check with `python3 -c "from PIL import features; assert features.check('raqm')"`. The project `.venv` already provides this support
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).
- Emscripten (optional, `emcc`: the browser emulator, `web/emu/`).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build PIGEON

```sh
PYTHON=.venv/bin/python ./build-sampler.sh
```

This produces `build/pigeon.fwsc` and `build/pigeon.bin`.
`./build.sh` remains the baseline compatibility build.

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 1.0` makes a release build: the package identity becomes `FM-1_910`
and the version string `v1.0`; the package is `build/felucca-1.0.fwsc`, and
`build/release-1.0/` holds what a release ships: the package, the app
(`felucca-1.0-app.bin`), `SHA256SUMS`, the sample attribution, `LICENSE`, `LICENSING.md` and
`LICENSES/` (the package contains Apache-2.0 SDK files, so the licence texts travel with it).

The PIGEON script selects `FELUCCA_KO=1`, `FELUCCA_KO_STORAGE=1`,
`FELUCCA_SAMPLER_ONLY=1`, and `FELUCCA_KO_DUAL=1` for two songs and the drum kit.
The shared flash bank and MP3 decoder replace factory
sample audio and changes the application reservation; see
[Expanded KO storage](docs/KO_STORAGE.md) for migration and validation details.
It requires flash storage, the editor upload protocol and the SLICE engine.

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`, `core.h` and `icons.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_CDC_DEFAULT` | 1 | `0`: the console is built in but left out of USB from boot (as MENU > USB SERIAL OFF) |
| `FELUCCA_UAC` | 1 | USB audio input (the master output, stereo, 44.1 or 48 kHz) |
| `FELUCCA_UAC_48K` | 1 | `0`: the USB audio input at 44.1 kHz only, as up to 1.0.5.2 (`firmware/src/usb.c`) |
| `FELUCCA_UAC_AS_FIRST` | 1 | the audio control header lists the audio streaming interface before the MIDI one, so the USB audio driver of older macOS (up to 15) finds it; `0`: 1.0 to 1.5's order, byte for byte (`firmware/src/usb.c`) |
| `FELUCCA_UART` | 1 | TRS MIDI IN |
| `FELUCCA_SLICE` | 1 | the SLICE engine |
| `FELUCCA_ICONS` | 1 | parameter icons on the knob cards |
| `FELUCCA_FM4` | 0 | the retired DIGITAL engine (4-operator FM) instead of its FM6 conversion |

`FELUCCA_USB_LAYOUT` (`0` to `3`, default `0`) picks other USB descriptor layouts for testing; see
`firmware/src/usb.c`.

## Samples

The CC0 instrument samples that the SAMPLE engine uses are in `assets/samples-cc0/`
(Versilian Studios, see `ATTRIBUTION.txt` there). `tools/fetch_cc0.py` downloads them
again from the source repositories. Without that folder the build still works, with no
instrument sets in the SAMPLE engine and no PIANO in SLICE (the PIANO set's middle C): they play a plain sine
at the note's pitch instead, and the screen says NO SAMPLE.

## Browser emulator

```
./build.sh
sh web/emu/build.sh
python3 -m http.server -d build/emu 8790     # open http://localhost:8790/
```

`web/emu/felucca_web.c` builds the firmware's sources as the host tests do, against a simulated FM-1, and
Emscripten compiles it to WebAssembly (`build/emu/felucca.wasm`, with `index.html`, `worklet.js` and the font in `fonts/`). The page
keeps the flash's storage sectors (projects, user presets, settings) in the browser. Not emulated: USB (audio,
MIDI out, the editor, updates), TRS MIDI and the user sample slots. A build of it is on the site:
<https://hugelton.github.io/Felucca/webapp/try/>.

## Tests

```
tests/run_tests.sh
```

Runs the host tests and, with Node.js, the web page tests (and, with Emscripten too, the browser emulator's). Run it after `./build.sh`
(it uses `build/` and needs `AC79_SDK` set as for the build). The suites cover flash storage,
user presets, projects of every format, backup, the keys and knobs, MIDI (USB, TRS, clock,
control), USB audio, the update entry and loader, the command-line installer, the UI (the real
drawing code against stubs: every screen in every palette is rendered and checked for clipped or
overlapping text; PNGs land in `build/ui_new/`), every engine (DRUM, NOISE, PHYS, FM6, SLICE, the
DIGITAL conversion), the chord keys, the modulation matrix, the FX layer, the reverbs, the SLICER
and swing. With `DAISYSP` pointing at a DaisySP checkout, the PHYS models are also compared with
their floating-point originals; without it that test is skipped.

The regression suite (`tests/regress.c`) renders every engine and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). After an intended change of
the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites the hashes; `BUDGET_UPDATE=1`
does the same for the cost files.

## Install

Use the web installer in Chrome or Edge:
<https://hugelton.github.io/Felucca/webapp/installer/>. It installs the released package.

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/felucca.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

```
python3 web/make_site.py build/felucca.fwsc dev /tmp/felucca-site
cd /tmp/felucca-site && python3 -m http.server 8000
# open http://localhost:8000/webapp/installer/
```

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

The standalone profile sets `FELUCCA_SAMPLER_ONLY=1`: only SONG 1 (SLICE), DRUM KIT (SAMPLE),
and SONG 2 (SLICE) render. Synth engines, extra track processing, continuous
oscilloscope capture, and the USB serial console are excluded. USB audio, MIDI,
sample transfer, updates, sequencer and effects remain. Four serialized track
slots and original engine IDs are retained; unavailable tracks stay silent.
The ordinary `build.sh` Felucca profile remains available.

`build-sampler.sh` also sets `FELUCCA_KO_DUAL=1` for independent Song 2.

## PIGEON sample GUI

Run `python3 tools/pigeon_gui.py` (Windows: `py tools/pigeon_gui.py`) and open
<http://127.0.0.1:8765/pigeon/> in Chrome or Edge. The same GUI is included at
`webapp/pigeon/` by `web/make_site.py` for HTTPS hosting. Wait for **Offline ready**
before disconnecting the network. See [the GUI guide](web/pigeon/README.md) for
imports, device transfers, recovery backups, offline caching, and browser tests.

For a local PIGEON candidate with checksums, repeated-build comparison and host
validation, see [PIGEON beta preparation](docs/PIGEON_BETA.md). Control reference:
[PIGEON controls](docs/PIGEON_CONTROLS.md). Static allocation and audio/display
findings: [performance audit](docs/PIGEON_PERFORMANCE_AUDIT.md).

## PIGEON recovery candidate

PIGEON builds include the ChoralRoot-derived boot stub pinned by
`firmware/stub/stub.lock`. Its marker occupies flash sector `0x76000`, below
KOPOOL2; the existing sample layout remains intact. See
[recovery and first-install limits](docs/PIGEON_RECOVERY.md).

After building, run `PYTHON=.venv/bin/python tests/run_pigeon_recovery_tests.sh`
and the existing product checks. The recovery suite generates synthetic host
fixtures under `build/pigeon-tests/recovery/`; never install those on hardware.
Intended stub changes require a version bump and `FM1_STUB_LOCK=update`.

The PIGEON browser trial starts empty and accepts users’ own samples with hardware-limited or browser-only expanded storage. Verify it with
`node web/emu/pigeon_test.mjs` and `node web/emu/test-browser.mjs` (the latter
uses the repository-local Playwright setup described in web/pigeon/README.md).
Firmware controller lease tests: compile `tests/browser_controller_test.c` with
`-Ibuild/gen -Ifirmware/src -lm`. Full panel USB control requires the updated
PIGEON firmware; physical-device verification remains pending.
