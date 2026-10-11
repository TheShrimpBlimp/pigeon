#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Browser encoder vs Python bank validator and actual firmware fixed-point decoder.

Run after generating firmware headers:
  python3 tests/pigeon_gui_codec_test.py --node /path/to/node

Runs native and ASan/UBSan decoders for every GUI bitrate. Never contacts hardware.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import ko_bank
import ko_codec_host
import sampleio


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--node', default='node')
    args = parser.parse_args()
    out = ROOT / 'build/gui-deps'
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run([args.node, 'web/pigeon/test.mjs'], cwd=ROOT, check=True)
    vector = json.loads((out / 'codec-vector.json').read_text())
    assert list(sampleio.ima_encode(vector['samples'], 0)[0]) == vector['adpcm']
    assert ko_bank.onsets(vector['samples'], 22050) == vector['onsets']
    for file in out.glob('song-*.kob'):
        ko_bank.validate(file.read_bytes())
    ko_bank.validate((out / 'dual-kit.kob').read_bytes())
    browser_bank = out / 'browser.kob'
    if browser_bank.exists():
        ko_bank.validate(browser_bank.read_bytes())
    print('Python cross-check: bank validation, ADPCM bytes, and onset positions passed', flush=True)
    for sanitize in (False, True):
        _, objects, _ = ko_codec_host.objects(sanitize)
        exe = out / ('gui-decoder-san' if sanitize else 'gui-decoder')
        flags = ['-O1' if sanitize else '-O2', '-g', '-w', '-DFPM_64BIT']
        if sanitize:
            flags += ['-fsanitize=address,undefined',
                      '-fno-sanitize=shift,signed-integer-overflow,pointer-overflow',
                      '-fno-sanitize-recover=undefined']
        subprocess.run([os.environ.get('CC', 'cc'), *flags, 'tests/ko_mp3_test.c',
                        *objects, '-lm', '-o', str(exe)], cwd=ROOT, check=True)
        for file in sorted(out.glob('song-*.kob')):
            print(('Sanitized ' if sanitize else 'Native ') + file.name, flush=True)
            subprocess.run([str(exe), str(file)], cwd=ROOT, check=True,
                           env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    print('GUI codec compatibility PASSED')


if __name__ == '__main__':
    main()
