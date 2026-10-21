#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Remove disposable render output; preview by default, use --apply to delete."""
import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
# These directories contain generated screenshots, review galleries or demo audio.
# Keep releases, toolchains, generated build inputs, device backups and user audio.
DIRECTORIES = {
    'pigeon-ui-atlas', 'v04-announcement-render', 'v04-whimsy',
    'announcement-v03', 'announcement-waveform', 'before-dual-song',
    'beta-preview', 'little-character-screens', 'workflow-screens',
    'pigeon-screens', 'seq-length-screens', 'seq-screens', 'arrange-screens',
    'ko-screens', 'ko_preview', 'pigeon-fx-preview', 'whimsy-preview',
    'no-aa-preview', 'ui_fx', 'ui_name', 'ui_roll', 'ui_layers',
    'ui_fm4_slot', 'ui_slot', 'piano_hd',
    'perform_demo', 'click_demo', 'mod_demo', 'sound15_demo', 'noise_demo',
    'throw_demo', 'sample_demo', 'sound14_demo', 'fm6_demo', 'insert_demo',
    'fx_demo', 'slice_demo', 'slicer_demo', 'fm4_demo', 'tracks_demo',
    'phys_demo', 'drum_demo', 'reverb_demo',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true', help='delete the listed output')
    args = parser.parse_args()
    build = ROOT / 'build'
    total = 0
    count = 0
    for name in sorted(DIRECTORIES):
        directory = build / name
        if directory.is_symlink() or not directory.is_dir():
            continue
        files = list(directory.rglob('*'))
        # Refuse symlinks rather than follow a path outside generated output.
        if any(p.is_symlink() for p in files):
            print(f'Skipped {directory.relative_to(ROOT)}: contains symlinks')
            continue
        size = sum(p.stat().st_size for p in files if p.is_file())
        print(f'{"Removing" if args.apply else "Would remove"} {directory.relative_to(ROOT)} ({size / 1048576:.1f} MiB)')
        if args.apply:
            shutil.rmtree(directory)
        total += size
        count += 1
    print(f'{count} directories, {total / 1048576:.1f} MiB {"removed" if args.apply else "available to clean"}.')


if __name__ == '__main__':
    main()
