#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Host-test fixtures only: changed body with stable stub and stubless baseline.
Never install these synthetic packages on hardware.
"""
import os
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import fm1pkg_make as f
f.SDK = Path(os.environ.get('AC79_SDK', str(Path.home() / 'fw-AC79_AIoT_SDK')))
a = bytearray(Path('build/pigeon.bin').read_bytes())
assert f.has_stub(a)
ota = Path('build/loader/ota.bin').read_bytes()
out = Path('build/pigeon-tests/recovery')
out.mkdir(parents=True, exist_ok=True)
old = a[f.STUB_LEN:]
# Baseline tests model flash writes, not execution of the synthetic old app.
(out / 'stubless-baseline.fwsc').write_bytes(f.ufw(f.flash_image(old,f.KEY),ota,'FM-1_900'))
(out / 'stubless-ko-baseline.fwsc').write_bytes(f.ufw(f.flash_image(old,f.KEY,ko_storage=True),ota,'FM-1_900'))
a[0x2000] ^= 1
(out / 'changed-body.fwsc').write_bytes(f.ufw(f.flash_image(a,f.KEY,ko_storage=True),ota,'FM-1_900'))
