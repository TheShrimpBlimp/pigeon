#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Load WAV files into a Felucca user sample slot (USR1..USR3) over USB-MIDI.

  fm1_sample_upload.py info
  fm1_sample_upload.py load SLOT NAME file.wav[:ROOT[:LO-HI]] ...   (SLOT 1..3)
  fm1_sample_upload.py erase SLOT
  fm1_sample_upload.py build NAME OUT_PREFIX file.wav[...] ...      (no device: writes OUT_PREFIX.hdr / .bin)
  fm1_sample_upload.py kit SLOT NAME pad1.wav ... pad16.wav         (KO kit: fixed notes 60..75)
  fm1_sample_upload.py kit-build NAME OUT_PREFIX pad1.wav ...       (1..16 files, pad order preserved)
  fm1_sample_upload.py song NAME file.wav                          (KO paired USR1 + USR3)
  fm1_sample_upload.py song-build NAME OUT_PREFIX file.wav          (local paired files)

Each file becomes one zone: mono, 22050 Hz, IMA ADPCM (sampleio.py, the same encoder
as the built-in sets and the web editor). ROOT is a MIDI note (default: from the file
name, C4 = 60, else 60); without LO-HI the zones split the keyboard between their roots.
A slot holds 80 KiB (about 7 s at 22050 Hz). Protocol: web/EDITOR_PROTOCOL.md, cmds 11..15.
Needs mido (and a backend such as python-rtmidi) for the device commands.
"""
import sys
import time
import struct
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sampleio as sio  # noqa: E402

HDR = [0x7D, 0x46, 0x4C]
RC = {1: "size", 2: "header", 3: "data CRC", 4: "flash", 5: "zones"}


def printable(b):
    """bytes from the device as text for the terminal: printable ASCII only (no escape sequences)"""
    return "".join(chr(c) if 32 <= c < 127 else "?" for c in b)


def slot_info(r):
    """the SMP_INFO reply -> one line per slot; ValueError when it is malformed"""
    try:
        n, i, out = r[0], 2, []
        for k in range(n):
            nz = r[i]
            j = r.index(0, i + 1)
            out.append(f"USR{k + 1}: {'empty' if not nz else f'{nz} zones, {printable(r[i + 1:j])}, {r[j + 1]} KiB'}")
            i = j + 2
        return out
    except (IndexError, ValueError):
        raise ValueError("malformed SMP_INFO reply from the device") from None


def pack7(b):
    """groups of up to 7 bytes, each preceded by their top bits"""
    out = []
    for i in range(0, len(b), 7):
        g = b[i:i + 7]
        out.append(sum(((x >> 7) & 1) << j for j, x in enumerate(g)))
        out += [x & 0x7F for x in g]
    return out


class Link:
    def __init__(self):
        try:
            import mido
        except ImportError:
            sys.exit("needs mido: pip install mido python-rtmidi")
        self.mido = mido
        outs = [n for n in mido.get_output_names() if any(name in n.lower() for name in ("felucca", "pigeon"))]
        ins = [n for n in mido.get_input_names() if any(name in n.lower() for name in ("felucca", "pigeon"))]
        if not outs or not ins:
            sys.exit("no PIGEON or Felucca MIDI port: connect the FM-1 by USB")
        self.o = mido.open_output(outs[0])
        self.i = mido.open_input(ins[0])
        for _ in range(3):                          # the first frame after opening the port can be lost
            try:
                self.req(1, [], 1.0)
                break
            except TimeoutError:
                pass

    def req(self, cmd, args, timeout=3.0):
        for _ in self.i.iter_pending():
            pass
        self.o.send(self.mido.Message("sysex", data=HDR + [cmd] + list(args)))
        t0 = time.time()
        while time.time() - t0 < timeout:
            for m in self.i.iter_pending():
                if m.type == "sysex" and list(m.data[:4]) == HDR + [cmd]:
                    return list(m.data[4:])
            time.sleep(0.002)
        raise TimeoutError(f"no reply to cmd {cmd}")


def build(name, specs):
    """specs: "file.wav[:ROOT[:LO-HI]]" -> (header, ADPCM data) for SMP_END / SMP_WRITE"""
    zones = []
    for spec in specs:
        parts = spec.split(":")
        path = Path(parts[0])
        root = int(parts[1]) if len(parts) > 1 and parts[1] else sio.note_from_name(path.stem)
        lo, hi = (int(v) for v in parts[2].split("-")) if len(parts) > 2 else (None, None)
        sr, x = sio.read_any_wav(path)
        x = sio.resample(x, sr, sio.SLOT_RATE)
        if not x:
            raise ValueError(f"{path}: no audio")
        zones.append((sio.to_int16(x), root, lo, hi))
    return sio.user_slot(name, zones)


def build_kit(name, files):
    """1..16 WAVs in pad order, one exact MIDI zone per pad; file names never reorder hits."""
    if not 1 <= len(files) <= 16:
        raise ValueError("a KO kit needs 1..16 files")
    zones = []
    for pad, filename in enumerate(files):
        sr, x = sio.read_any_wav(Path(filename))
        x = sio.resample(x, sr, sio.SLOT_RATE)
        if not x:
            raise ValueError(f"{filename}: no audio")
        note = 60 + pad
        zones.append((sio.to_int16(x), note, note, note))
    return sio.user_slot(name, zones)


def build_song(name, filename):
    """Reserve USR1's metadata sector; independently encode the continuation in USR3."""
    sr, audio = sio.read_any_wav(Path(filename))
    audio = sio.resample(audio, sr, sio.SLOT_RATE)
    first = 2 * (sio.SLOT_MAX_DATA - 4096)
    capacity = first + 2 * sio.SLOT_MAX_DATA
    if not audio or len(audio) > capacity:
        raise ValueError(f"song needs 1..{capacity} samples ({capacity / sio.SLOT_RATE:.2f} seconds)")
    audio = sio.to_int16(audio)  # one normalization across both chunks
    chunks = [sio.user_slot(name, [(audio[:first], 60, 0, 127)])]
    if len(audio) > first:
        chunks.append(sio.user_slot(name, [(audio[first:], 60, 0, 127)]))
        pair = zlib.crc32(struct.pack("<II", len(audio), first))
        for _, data in chunks:
            pair = zlib.crc32(data, pair)
        tagged = []
        for i, (hdr, data) in enumerate(chunks):
            hdr = bytearray(hdr)
            hdr[7] = 0x4B + i
            struct.pack_into("<II", hdr, 24, 0x31534F4B, pair)
            tagged.append((bytes(hdr), data))
        chunks = tagged
    return [(slot, hdr, data) for slot, (hdr, data) in zip((0, 2), chunks)]


def upload_slot(link, slot, hdr, data):
    r = link.req(11, [slot])
    if len(r) < 2 or r[1]:
        raise ValueError(f"USR{slot + 1}: begin failed")
    for off in range(0, len(data), 256):
        a = sio.SLOT_DATA_OFF + off
        r = link.req(12, [slot, a & 0x7F, (a >> 7) & 0x7F, (a >> 14) & 0x7F] + pack7(data[off:off + 256]))
        if len(r) < 5 or r[4]:
            raise ValueError(f"USR{slot + 1}: write at {a:#x} failed")
    r = link.req(13, [slot] + pack7(hdr), 5)
    if len(r) < 2 or r[1]:
        raise ValueError(f"USR{slot + 1}: commit failed")


def upload_song(link, chunks):
    info = link.req(15, [])
    slot_info(info)
    if info[-2:] != [0x4B, 1]:
        raise ValueError("device firmware does not support KO paired song uploads")
    if len(chunks) == 2:
        # Invalidate the old first half before replacing its continuation.
        r = link.req(11, [0])
        if len(r) < 2 or r[1]:
            raise ValueError("cannot invalidate the old song")
    for slot, hdr, data in reversed(chunks):
        upload_slot(link, slot, hdr, data)


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("info", "load", "erase", "build", "kit", "kit-build", "song", "song-build"):
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd in ("song", "song-build"):
        if len(sys.argv) != (5 if cmd == "song-build" else 4):
            sys.exit(__doc__)
        try:
            chunks = build_song(sys.argv[2], sys.argv[-1])
            if cmd == "song-build":
                for slot, hdr, data in chunks:
                    prefix = f"{sys.argv[3]}.usr{slot + 1}"
                    Path(prefix + ".hdr").write_bytes(hdr)
                    Path(prefix + ".bin").write_bytes(data)
                return
            link = Link()
            upload_song(link, chunks)
            print(f"{sys.argv[2]}: song uploaded; USR2 kit preserved")
        except (OSError, ValueError) as e:
            sys.exit(f"cannot load song: {e}")
        return
    if cmd in ("load", "build", "kit", "kit-build"):
        if len(sys.argv) < 5:
            sys.exit(__doc__)
        try:
            builder = build_kit if cmd in ("kit", "kit-build") else build
            hdr, data = builder(sys.argv[3] if cmd in ("load", "kit") else sys.argv[2], sys.argv[4:])
        except (OSError, ValueError) as e:
            sys.exit(f"cannot build the slot: {e}")
        if cmd in ("build", "kit-build"):
            Path(sys.argv[3] + ".hdr").write_bytes(hdr)
            Path(sys.argv[3] + ".bin").write_bytes(data)
            print(f"{sys.argv[2]}: {len(data)} B ADPCM, {hdr[6]} zones")
            return
    if cmd != "info":
        if len(sys.argv) < 3 or sys.argv[2] not in ("1", "2", "3"):
            sys.exit("SLOT is 1, 2 or 3")
        slot = int(sys.argv[2]) - 1
    link = Link()
    if cmd == "info":
        try:
            print("\n".join(slot_info(link.req(15, []))))
        except ValueError as e:
            sys.exit(str(e))
        return
    if cmd == "erase":
        print("erase:", "ok" if link.req(14, [slot], 10)[1] == 0 else "FAILED")
        return
    print(f"USR{slot + 1} {sys.argv[3]}: {len(data)} B ADPCM, {hdr[6]} zones")
    if link.req(11, [slot])[1]:
        sys.exit("begin failed")
    t0 = time.time()
    for off in range(0, len(data), 256):
        a = sio.SLOT_DATA_OFF + off
        r = link.req(12, [slot, a & 0x7F, (a >> 7) & 0x7F, (a >> 14) & 0x7F] + pack7(data[off:off + 256]))
        if r[4]:
            sys.exit(f"write at {a:#x} failed ({r[4]})")
        print(f"\r  {min(off + 256, len(data))} / {len(data)}", end="", flush=True)
    r = link.req(13, [slot] + pack7(hdr), 5)
    print(f"\n  {time.time() - t0:.1f} s, end:", "ok" if r[1] == 0 else f"FAILED ({r[1]}, {RC.get(r[1], '?')})")


if __name__ == "__main__":
    try:
        main()
    except TimeoutError as e:
        sys.exit(f"{e}: is Felucca running, and no other app using its MIDI port?")
