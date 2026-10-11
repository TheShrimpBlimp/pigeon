"""KO kit layout through the real encoder; pad order, exact zones, capacity and CLI output."""
import importlib.util
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

spec = importlib.util.spec_from_file_location("upload", Path(__file__).resolve().parents[1] / "tools/fm1_sample_upload.py")
upload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(upload)
with tempfile.TemporaryDirectory() as tmp:
    files = []
    for pad in range(16):
        path = Path(tmp) / f"reverse_name_{15-pad}.wav"
        with wave.open(str(path), "wb") as w:
            w.setparams((1, 2, 22050, 0, "NONE", "not compressed"))
            w.writeframes(struct.pack("<256h", *(int(14000 * math.sin(i * (pad+1) * 0.04)) for i in range(256))))
        files.append(str(path))
    hdr, data = upload.build_kit("CUSTOM", files)
    assert len(hdr) == upload.sio.SLOT_HDR_LEN and hdr[6] == 16 and len(data) == 16 * 128
    for pad in range(16):
        z = struct.unpack_from("<5I2h4B", hdr, 32 + pad * 28)
        assert z[0] == pad * 128 and z[1] == 256 and z[5] == (60 + pad) * 16
        assert z[8:10] == (60 + pad, 60 + pad) and z[10] == 0
    for wrong in ([], files + files[:1]):
        try:
            upload.build_kit("BAD", wrong)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid pad count accepted")
    prefix = Path(tmp) / "kit"
    subprocess.run([sys.executable, str(Path(upload.__file__)), "kit-build", "CUSTOM", str(prefix), *files],
                   check=True, capture_output=True)
    assert prefix.with_suffix(".hdr").read_bytes() == hdr
    assert prefix.with_suffix(".bin").read_bytes() == data
    song = Path(tmp) / "song.wav"
    def write_song(samples):
        with wave.open(str(song), "wb") as w:
            w.setparams((1, 2, 22050, 0, "NONE", "not compressed"))
            w.writeframes(struct.pack(f"<{samples}h", *(int(14000 * math.sin(i * 0.04)) for i in range(samples))))
    write_song(12 * 22050)
    chunks = upload.build_song("LONG", song)
    assert [c[0] for c in chunks] == [0, 2]
    assert len(chunks[0][2]) == upload.sio.SLOT_MAX_DATA - 4096
    assert sum(struct.unpack_from("<I", c[1], 36)[0] for c in chunks) == 12 * 22050
    assert chunks[0][1][7] == 0x4B and chunks[1][1][7] == 0x4C
    assert chunks[0][1][24:32] == chunks[1][1][24:32]
    for _, header, payload in chunks:
        assert struct.unpack_from("<I", header, 20)[0] == upload.zlib.crc32(payload)
    prefix = Path(tmp) / "long"
    subprocess.run([sys.executable, str(Path(upload.__file__)), "song-build", "LONG", str(prefix), str(song)],
                   check=True, capture_output=True)
    for slot, header, payload in chunks:
        assert Path(f"{prefix}.usr{slot + 1}.hdr").read_bytes() == header
        assert Path(f"{prefix}.usr{slot + 1}.bin").read_bytes() == payload
    write_song(256)
    short = upload.build_song("SHORT", song)
    assert len(short) == 1 and short[0][0] == 0 and short[0][1][7] == 0
    maximum = 2 * (2 * upload.sio.SLOT_MAX_DATA - 4096)
    write_song(maximum)
    full = upload.build_song("MAXIMUM", song)
    assert [len(c[2]) for c in full] == [upload.sio.SLOT_MAX_DATA - 4096, upload.sio.SLOT_MAX_DATA]
    write_song(maximum + 1)
    try:
        upload.build_song("TOO LONG", song)
    except ValueError:
        pass
    else:
        raise AssertionError("oversized song accepted")
    class FakeLink:
        def __init__(self, rc=0):
            self.calls = []; self.rc = rc
        def req(self, cmd, args, timeout=3):
            self.calls.append((cmd, args[0]))
            return [args[0], 0, 0, 0, 0] if cmd == 12 else [args[0], self.rc if cmd == 13 else 0]
    link = FakeLink()
    upload.upload_slot(link, *short[0])
    assert link.calls[0] == (11, 0) and link.calls[-1] == (13, 0)
    try:
        upload.upload_slot(FakeLink(3), *short[0])
    except ValueError:
        pass
    else:
        raise AssertionError("failed commit reported as success")
    class SongLink(FakeLink):
        def __init__(self, supported=True, fail_second=False):
            super().__init__(); self.supported = supported; self.fail_second = fail_second
        def req(self, cmd, args, timeout=3):
            if cmd == 15:
                self.calls.append((15, None))
                return [3, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0] + ([0x4B, 1] if self.supported else [])
            if cmd == 13 and args[0] == 2 and self.fail_second:
                self.calls.append((cmd, args[0])); return [2, 3]
            return super().req(cmd, args, timeout)
    paired = [(0, chunks[0][1], b"\x11"), (2, chunks[1][1], b"\x22")]
    link = SongLink()
    upload.upload_song(link, paired)
    assert link.calls == [(15, None), (11, 0), (11, 2), (12, 2), (13, 2), (11, 0), (12, 0), (13, 0)]
    for link in (SongLink(supported=False), SongLink(fail_second=True)):
        try:
            upload.upload_song(link, paired)
        except ValueError:
            pass
        else:
            raise AssertionError("unsafe paired upload accepted")
        assert all(slot != 1 for _, slot in link.calls)  # kit never touched
        if not link.supported:
            assert link.calls == [(15, None)]
        else:
            assert (13, 0) not in link.calls  # first half never committed after failed continuation
print("KO song/kit upload: PASSED")
