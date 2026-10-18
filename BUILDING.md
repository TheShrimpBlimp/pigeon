# Building PIGEON

## Prerequisites

- Python 3 with Pillow and fontTools. Pillow needs libraqm support for font
  rasterization. The repository `.venv` already provides these dependencies.
- The JieLi pi32v2 toolchain, installed with `tools/get_toolchain.sh` under
  `~/.jieli/toolchain`.
- On macOS, Docker Desktop with access to the repository folder. The toolchain
  runs in a Linux amd64 container. Linux x86-64 runs it natively.
- The AC79 SDK, whose packaging files are not bundled in this repository:

```sh
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
  https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override those locations. Node.js is needed for
browser checks; Emscripten is needed for the emulator build.

## Firmware

```sh
PYTHON=.venv/bin/python ./build-sampler.sh
```

Outputs:

| File | Purpose |
| --- | --- |
| `build/pigeon.fwsc` | Installable firmware package |
| `build/pigeon.bin` | Application binary |
| `build/loader/ota.bin` | Update loader |

The underlying build also emits `build/felucca.bin` and `build/felucca.fwsc` as
compatibility filenames. The PIGEON wrapper enables dual songs, shared sample
storage and the sampler-only profile, disables the serial console, and audits
the resulting image for inherited screens and unused synth renderers.

Version identity comes from `firmware/src/project_identity.h`. Do not use the
inherited `build.sh --release` path to package PIGEON. Plain `build.sh` remains
available for baseline regression work; it is not the PIGEON product build.
Factory sample generators and compatibility fixtures remain used by that
baseline. PIGEON starts with user-loaded samples and does not need those factory
instruments at runtime. See [the profile reference](docs/PIGEON_FIRMWARE_PROFILE.md).

## Emulator

Build the firmware once to generate tables and fonts, then activate Emscripten:

```sh
source build/emsdk/emsdk_env.sh
PYTHON=.venv/bin/python sh web/emu/build.sh
python3 -m http.server -d build/emu 8790
```

Open `http://localhost:8790/`. The emulator compiles the product firmware to
WebAssembly and keeps its simulated flash in browser storage. It starts empty
and accepts users' samples and sessions. Real USB audio, firmware installation
and TRS MIDI hardware are not emulated. Web MIDI/panel connections use the browser
controller bridge.

## Sample tool and local site

```sh
.venv/bin/python tools/pigeon_gui.py
```

Open `http://127.0.0.1:8765/pigeon/` in Chrome or Edge. See the
[sample-tool guide](web/pigeon/README.md) for editing, bank/session transfers,
backups and offline use.

`web/make_site.py` assembles a local website from a package and version label.
It uses the separate website checkout in `web/site/`. Building a local site does
not deploy it. Match the label to the actual embedded firmware version.

## Checks and UI renders

Use focused checks for the affected behavior during development. For the full
product and recovery suites:

```sh
PYTHON=.venv/bin/python sh tests/run_pigeon_tests.sh
PYTHON=.venv/bin/python sh tests/run_pigeon_recovery_tests.sh
```

Recovery tests create synthetic fixtures under `build/pigeon-tests/recovery/`;
those are not installable firmware. The broader inherited `tests/run_tests.sh`
suite supports baseline compatibility work. Do not regenerate golden audio or
CPU baselines merely to make a failing check pass.

`tools/render_pigeon_ui.c` renders the actual firmware against synthetic
fixtures. `tools/package_pigeon_ui.py` creates a gallery, contact sheets, PDF and
ZIP under `build/pigeon-ui-atlas/`. `tests/pigeon_screen_test.c` checks product
text layout. Emulator/browser check commands are documented in the
[sample-tool guide](web/pigeon/README.md); screen-reader checks are in
[v0.4 development notes](docs/PIGEON_V04_DEVELOPMENT.md).

## Prepare a package

```sh
.venv/bin/python tools/prepare_pigeon_beta.py --version 0.4 --verify-reproducible
```

The script builds firmware, checks target budgets, compares repeated builds when
requested, and copies packages, checksums, memory metrics and documentation to
`build/releases/pigeon-<label>/`. The default label follows the embedded PIGEON
version. `--version` overrides the output label; it does not set the firmware's
embedded version. Add `--run-tests` to run the product and recovery
suites. The manifest records whether they ran. Building a package does not create
a release or deploy it.

## Install and recover

Back up the connected instrument before installing. The
[published installer](https://theshrimpblimp.com/pigeon/webapp/installer/) serves
the released package. To install a local build, with `mido` and `python-rtmidi`:

```sh
.venv/bin/python tools/fm1_install.py --info
.venv/bin/python tools/fm1_install.py build/pigeon.fwsc
```

The ChoralRoot-derived boot stub is pinned by `firmware/stub/stub.lock`; its
marker is at flash sector `0x76000`, below the sample pool. Intended stub changes
require a version bump and `FM1_STUB_LOCK=update`. Follow the
[recovery guide](docs/PIGEON_RECOVERY.md) for interrupted updates, first-install
limits and dark-device recovery.

Current controls: [published control reference](docs/PIGEON_CONTROLS.md) and
[v0.4 development guide](docs/PIGEON_V04_GUIDE.md). Storage details:
[KO_STORAGE.md](docs/KO_STORAGE.md). Performance findings:
[PIGEON_PERFORMANCE_AUDIT.md](docs/PIGEON_PERFORMANCE_AUDIT.md).

## v0.4 release gate

The [v0.4 release checklist](docs/PIGEON_V04_RELEASE.md) identifies the exact
local candidate and short physical smoke test. Candidate generation does not
flash, publish, tag or push a release. Keep the generated manifest and checksums
with the package actually tested. Full host/recovery/reader and browser checks
are required for this release; see the final validation report under `build/`.
