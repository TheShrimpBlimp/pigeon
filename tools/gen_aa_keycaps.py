#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Generate control labels and legacy knob arc masks. Key hints use drawn geometry and lit.chr."""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
import aa_raster as ar  # noqa: E402

FONT = str(Path(__file__).resolve().parents[1] / "assets" / "fonts" / "LittleCharacterRegular.ttf")
KC_PX, KC_WGHT = 9, 600          # the label: Inter Tight 9 px / 600 (caps 7 rows)
KC_H, KC_R, KC_PAD = 13, 4, 3    # the pill: 13 px tall, radius 4, 3 px each side of the label's ink
KC_BASE = 10                     # baseline row: the caps on rows 3 .. 9

# (C name, label): the controls the hints name (panel.c B_NAME / E_NAME, short), then the state badges
LABELS = [("PRESETS", "PRESETS"), ("SELECT", "SELECT"), ("ALGO", "ALGO"), ("OCTDN", "OCT-"), ("OCTUP", "OCT+"),
          ("PLAY", "PLAY"), ("REC", "REC"), ("SAVE", "SAVE"), ("EDIT", "EDIT"), ("SEQ", "SEQ"), ("GLO", "GLO"),
          ("HOME", "HOME"), ("ARP", "ARP"), ("KEYS", "KEYS"), ("K1", "K1"), ("K2", "K2"), ("K3", "K3"), ("K4", "K4"),
          ("MUTE", "MUTE"), ("ARM", "ARM"), ("HOLD", "HOLD"), ("ON", "ON"), ("OFF", "OFF"),
          ("FX", "FX"), ("K14", "K1-4"), ("SCL", "SCL"), ("OCTPM", "OCT-/+"),
          ("LFO", "LFO"), ("ENV", "ENV")]   # (1.5: HELP's hints, src/ui_help.c)

KNOBS = [("BIG", 17), ("SMALL", 10)]   # outer radius; the ring is 2 px


def knob_quadrant(r):
    """(coverage nibbles, angle bytes) of the top-right quadrant, row j = dy (0 nearest the centre)"""
    cov, ang = [], []
    r0 = r - 2.0
    for j in range(r):
        for i in range(r):
            n = 0
            for sy in range(4):
                for sx in range(4):
                    dx, dy = i + (sx + 0.5) / 4, j + (sy + 0.5) / 4
                    d = math.hypot(dx, dy)
                    n += r0 <= d <= r
            cov.append(min(15, int(round(n * 15 / 16))))
            a = math.atan2(i + 0.5, j + 0.5)          # from 12 o'clock, clockwise: dx over dy (up)
            ang.append(min(255, int(a / (math.pi / 2) * 256)))
    return cov, ang


def main():
    out = Path(sys.argv[1])
    # Labels only: the firmware draws rectangles and its existing lit.chr font.
    lines = ["/* generated control labels; no keycap bitmap assets */", "#pragma once", "#include <stdint.h>",
             "#define KC_H 13", "enum {"]
    lines += [f"    KC_{c}," for c,l in LABELS]
    lines += ["    KC_COUNT", "};", "static const kc_t KC[KC_COUNT] = {"]
    lines += [f'    {{0, 0, "{l}"}},' for c,l in LABELS]
    lines += ["};", ""]
    knob_bytes = 0
    for name, r in KNOBS:
        cov, ang = knob_quadrant(r)
        pc = ar.pack(cov)
        knob_bytes += len(pc) + len(ang)
        lines += [f"#define KNOB_{name}_R {r}",
                  ar.c_array(f"KNOB_{name}_COV", "uint8_t", list(pc), fmt="0x{:02x}"),
                  ar.c_array(f"KNOB_{name}_ANG", "uint8_t", ang), ""]
    out.write_text("\n".join(lines))
    print(f"control labels: {len(LABELS)} text labels; 0 B keycap bitmaps; knob arcs {knob_bytes} B -> {out}")



if __name__ == "__main__":
    main()
