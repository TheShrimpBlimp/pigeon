# Pigeon licensing

The owner-supplied splash photograph in `assets/branding/splash.png` is not an
inherited Felucca asset. Its provenance and redistribution status are documented
in `assets/branding/README.md`; the software license does not establish a license
for that photograph.

Pigeon is a derivative of Felucca 1.5.1, developed as an independent FM1
performance sampler. Original copyright and license notices remain intact.

Pigeon is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). That covers Pigeon source contributions and inherited GPL code. Creative assets
retain their documented licenses; the supplied photograph is addressed separately
above. A few
bundled or ported parts keep their own licences; they are listed below, and their licence
texts are in `LICENSES/`.

The Pigeon sampler profile bundles libmad under
GPL-2.0-or-later. Its original notices and license are retained in
`firmware/vendor/libmad/COPYRIGHT` and `firmware/vendor/libmad/COPYING`; provenance
and the local bounds fix are documented in `firmware/vendor/libmad/FELUCCA_ORIGIN.md`.
The combined firmware remains GPL-3.0-only.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

## Pigeon authorship and AI-use disclosure

Pigeon is directed and maintained by its human project creator, The Shrimp Blimp,
and human contributors. Original Felucca and third-party authors retain their
copyrights. Pigeon-specific contributions are distributed under GPL-3.0-only;
this does not transfer ownership of inherited work or separately licensed assets.

**AI was used for menial coding. All assets, direction, conceptualization, and
thought are human.** This is the project creator's authorship statement. Creative
assets are human-created or human-sourced and credited to their respective
creators; AI assistance was used for routine code implementation, conversion,
testing, and maintenance. This disclosure adds no terms to the GPL or the
third-party licenses.

## What is GPL-3.0-only

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header:

- the firmware: `firmware/` (app, HAL, update loader)
- the build script and tools: `build.sh`, `tools/`
- the web pages (installer, editor, the browser emulator `web/emu/`) and their tests: `web/` (not the Fukiai and DotGothic16 fonts, below)
- the host tests: `tests/`

Three source files are ports and keep the licence of their originals:
`firmware/src/phys_dsp.c` (DaisySP, MIT), `firmware/src/phys_symp.c` (Rings, MIT) and
`firmware/src/fm6_core.c` (msfa, Apache-2.0). The firmware built with them is GPL-3.0-only as
a whole.

You may use, study, change and share Pigeon under the GPL. If you distribute Pigeon, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified Pigeon inside.

## Inherited Hügelton Instruments work

The following foundation material remains credited to Hügelton Instruments (Leo Kuroshita):

| What | Licence | Where |
| --- | --- | --- |
| Felucca: firmware, tools, web pages, tests | GPL-3.0-only | `LICENSE` |
| The PHASE engine's waveforms: a C port of the oscillator of CrispyZebra (<https://github.com/hugelton/CrispyZebra>) | GPL-3.0 | `firmware/src/eng_phase.c` |
| The DRUM voices and kit | GPL-3.0-only | `firmware/src/drum_voice.c`, `firmware/src/eng_drum.c` |
| The Hügelton Sample Pack: Felucca's drum sounds, made by `tools/gen_waves.py`, from which the SLICE engine's BREAK is built (not CC0) | GPL-3.0-only | `tools/gen_waves.py` |
| The controls picture | GPL-3.0-only | `docs/controls.jpg` |
| The Fukiai icon font (<https://github.com/hugelton/Fukiai>): the firmware's icons (rasterised at build time by `tools/gen_aa_icons.py`) and the web editor's | MIT | `web/fukiai.ttf`, `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Inter Tight font by The Inter Project Authors: retained source asset used by the legacy panel-diagram generator (`tools/gen_aa_font.py`; the generated tables are not offered as a font, and the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/InterTight[wght].ttf`, `LICENSES/OFL-InterTight.txt` (also `assets/fonts/OFL.txt`) |
| Instrument samples (Versilian Studios VSCO-2 Community Edition, VCSL): the SAMPLE sets, also the SLICE engine's PIANO (the PIANO set's middle C) | CC0 1.0 | `assets/samples-cc0/`, provenance in `ATTRIBUTION.txt` there |
| DaisySP by Electrosmith, Corp and Emilie Gillet (<https://github.com/electro-smith/DaisySP>): the PHYS engine's modal and string models and the resonator, ported to fixed point | MIT | `firmware/src/phys_dsp.c`, `LICENSES/MIT-DaisySP.txt` |
| Rings by Emilie Gillet (<https://github.com/pichenettes/eurorack>): the PHYS engine's sympathetic strings, ported to fixed point | MIT | `firmware/src/phys_symp.c`, `LICENSES/MIT-Rings.txt` |
| msfa by Google Inc. and Pascal Gauthier, from Dexed (<https://github.com/asb2m10/dexed>): the FM6 engine's synthesis, ported to integer C (Dexed itself is GPL-3.0; only msfa is used; the FM6 factory patches are Felucca's own) | Apache-2.0 | `firmware/src/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| X0X by charlesvestal (<https://github.com/charlesvestal/fm1-x0x>), a Felucca fork: the design of the browser emulator (the worklet, the device clock driven by its audio, the exports); its files credit it in their headers | GPL-3.0 | `web/emu/` |
| DotGothic16 font by The DotGothic16 Project Authors (<https://github.com/fontworks-fonts/DotGothic16>): the browser emulator's knob labels, served with its page (cut to printable ASCII; the font declares no Reserved Font Name) | SIL OFL 1.1 | `web/emu/fonts/DotGothic16-subset.woff`, `LICENSES/OFL-DotGothic16.txt` (also `web/emu/fonts/OFL.txt`) |
| JieLi AC79 SDK by JieLi Technology: three of its files go into every `.fwsc` package (below); none are in this tree | Apache-2.0 | `LICENSES/Apache-2.0.txt` |

On the device, HOME held > ABOUT opens the information screen; turning PRESETS scrolls on into
CREDITS, a short list of these authors, licences and source URLs.

## JieLi SDK files in the packages (Apache-2.0)

A `.fwsc` package made by `tools/build.py` (with `tools/fm1pkg_make.py`; this is the package the
web installer installs) holds three unmodified files from the JieLi AC79 SDK
(<https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>, `cpu/wl82/tools/`). They are read from your SDK
checkout at build time (see BUILDING.md); no SDK files are in this tree.

| File in the package | What it is |
| --- | --- |
| `uboot.boot` | the first-stage boot loader (SPL) |
| `cfg_tool.bin` | the chip configuration block |
| `eq_cfg_hw.bin` | the default hardware EQ table |

They are licensed under the Apache License, Version 2.0 (`LICENSES/Apache-2.0.txt`), not under
the GPL, and their copyright stays with JieLi Technology. The SDK has no NOTICE file. Apache-2.0
files may be distributed together with GPL-3.0 code; Felucca itself stays GPL-3.0-only.

Every distribution of a package carries the licence texts with it: the release folder
(`tools/build.py --release`) and the site (`web/make_site.py`, next to the package in
`firmware/` and linked from the installer page).

## No vendor material

No M-VAVE material is part of Felucca. The firmware links no vendor code and contains no vendor
data, and the packages hold only Felucca and the three SDK files above. The installer's
"Return to official V15" holds only the official file's size and SHA-256: you select the
official firmware file you downloaded yourself, and it is checked and installed in your browser,
never uploaded or redistributed.

## Contributions

Contributions are welcome under GPL-3.0-only.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.

"M-VAVE" and "FM-1" are trademarks of their respective owners. Felucca is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners.

## Radio

Felucca never enables the Bluetooth / Wi-Fi radio of the hardware.

## Little Character display font

Pigeon uses Little Character Regular by S M Astrid Bin, copyright May 2025,
under the SIL Open Font License 1.1. The unmodified font is vendored at
`assets/fonts/LittleCharacterRegular.ttf`; its original license is in
`assets/fonts/LittleCharacter-OFL.txt` and `LICENSES/LittleCharacter-OFL.txt`.
Source: https://github.com/disastrid/LittleCharacter . The firmware embeds
rasterized glyphs generated from this font.

## Browser sample encoder

The Pigeon sample GUI bundles lamejs 1.2.1, Alex Zhukov's JavaScript port of
LAME, under LGPL-3.0. Its readable runtime at `web/pigeon/vendor/lame.js` is
modified only to set the output sample rate explicitly. Original corresponding
source, attribution, and LGPL/GPL texts are shipped in that directory and with
the generated website. See [encoder provenance](web/pigeon/vendor/README.md).
This browser dependency is separate from the firmware image.
