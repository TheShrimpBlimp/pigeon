#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reject inherited screens and unused synth renderers in a PIGEON image."""
import re
import struct
import sys
from pathlib import Path


def audit(disassembly):
    symbols = set(re.findall(r"^([A-Za-z_][\w.]*):$", disassembly, re.M))
    forbidden = {
        "smp_sine", "draw_graph", "engine_columns", "layer_edit", "edit_load",
        "preset_step", "presets_turn", "select_engine", "eng_list_step",
        "up_boot", "upf_boot", "upf_migrate", "up_store", "up_load", "FM6_FACTORY",
        "slice_edit_begin", "slice_knob", "slice_env", "slice_act",
        "console_poll", "analog_render", "phase_render", "lofi_render",
        "formant_render", "trio_render", "wheel_render", "grain_render",
        "phys_render", "drum_render", "noise_render", "fm6_render", "digital_render",
    }
    leaked = sorted(s for s in symbols if s.split(".")[0] in forbidden or s.startswith("graph_"))
    missing = sorted({"ko_draw", "ui_input", "sample_render", "slice_render"} - symbols)
    if leaked or missing:
        raise ValueError(f"PIGEON profile audit failed: unwanted={leaked}, missing={missing}")
    return len(symbols)


def audit_assets(path):
    """Check allocated data too: disassembly alone omits zero-filled RAM."""
    data = path.read_bytes()
    if data[:5] != b"\x7fELF\x01" or data[5] not in (1, 2):
        raise ValueError("Expected a 32-bit target ELF")
    endian = "<" if data[5] == 1 else ">"
    header = struct.unpack_from(endian + "16sHHIIIIIHHHHHH", data)
    sections = [struct.unpack_from(endian + "IIIIIIIIII", data, header[6] + i*header[11])
                for i in range(header[12])]
    objects = {}
    for section in sections:
        if section[1] != 2:
            continue
        strings_section = sections[section[6]]
        strings = data[strings_section[4]:strings_section[4]+strings_section[5]]
        for offset in range(section[4], section[4]+section[5], section[9]):
            name, _, size, info, _, index = struct.unpack_from(endian + "IIIBBH", data, offset)
            if info & 15 == 1 and size and index:
                label = strings[name:].split(b"\0", 1)[0].decode("ascii", errors="replace")
                objects[label] = size
    leaked = sorted(set(objects) & {"up_bank", "upf", "FM6_FACTORY", "brand_splash_pixels", "KC_DATA"})
    if leaked:
        raise ValueError(f"PIGEON retains legacy/uncompressed assets: {leaked}")
    if not objects.get("brand_splash_rle") or not objects.get("brand_splash_row_offsets"):
        raise ValueError("Compressed splash asset is missing")
    return sum(objects.get(n, 0) for n in
               ("brand_splash_rle", "brand_splash_row_offsets", "brand_splash_palette"))


if __name__ == "__main__":
    try:
        count = audit(Path(sys.argv[1]).read_text())
        splash_bytes = audit_assets(Path(sys.argv[1]).with_suffix(".elf"))
    except (IndexError, OSError, ValueError) as error:
        sys.exit(str(error))
    print(f"PIGEON profile: {count} symbols checked; no inherited screens or unused synth renderers")
    print(f"PIGEON assets: {splash_bytes} B compressed splash; no synth preset RAM banks or factory patches")
