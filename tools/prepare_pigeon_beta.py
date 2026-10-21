#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a local PIGEON candidate with checksums; never flash or publish it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from ko_bank import SIZE, HEAD

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    identity = (ROOT / 'firmware/src/project_identity.h').read_text()
    version = re.search(r'^#define FW_DISPLAY_VERSION "v([^"\n]+)"$', identity, re.MULTILINE)
    if not version:
        parser.error('PIGEON display version is missing from project_identity.h')
    parser.add_argument('--version', default=version.group(1), help='output label (defaults to the embedded PIGEON version)')
    parser.add_argument('--verify-reproducible', action='store_true')
    parser.add_argument('--run-tests', action='store_true')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', args.version):
        parser.error('version must be a filename-safe label')
    out = ROOT / 'build/releases' / ('pigeon-' + args.version)
    out.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, 'PYTHON': sys.executable, 'FELUCCA_SAMPLER_ONLY': '1', 'FELUCCA_KO_DUAL': '1'}
    env['PATH'] = str(Path(sys.executable).parent) + os.pathsep + env.get('PATH', '')

    def run(command, name):
        with (out / name).open('w') as log:
            subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)

    run(['sh', 'build-sampler.sh'], 'build.log')
    names = ('pigeon.bin', 'pigeon.fwsc')
    first = {name: sha(ROOT / 'build' / name) for name in names}
    if args.verify_reproducible:
        run(['sh', 'build-sampler.sh'], 'rebuild.log')
        second = {name: sha(ROOT / 'build' / name) for name in names}
        if first != second:
            raise SystemExit('Rebuild differs: candidate not prepared')
    run([sys.executable, 'tests/target_budget.py'], 'target-budget.log')
    if args.run_tests:
        node = shutil.which('node', path=env['PATH'])
        if not node:
            raise SystemExit('Node.js required for --run-tests codec verification')
        env['NODE'] = node
        run(['sh', 'tests/run_pigeon_tests.sh'], 'host-tests.log')
        run(['sh', 'tests/run_pigeon_recovery_tests.sh'], 'recovery-tests.log')

    metrics = re.search(r'image (\d+) B; RAM .data\+\.bss (\d+) B of (\d+); pool (\d+) B of (\d+)', (out / 'build.log').read_text())
    if not metrics:
        raise SystemExit('Missing target memory validation: candidate not prepared')
    artifacts = {}
    for name in names:
        dest = out / name.replace('pigeon.', 'pigeon-' + args.version + '.')
        shutil.copyfile(ROOT / 'build' / name, dest)
        artifacts[dest.name] = {'bytes': dest.stat().st_size, 'sha256': sha(dest)}
    for name in ('KO_MODE.md', 'PIGEON_CONTROLS.md', 'PIGEON_PERFORMANCE_AUDIT.md', 'PIGEON_RECOVERY.md', 'PIGEON_V03_RELEASE.md', 'PIGEON_V04_RELEASE.md', 'PIGEON_V04_GUIDE.md', 'PIGEON_V05_GUIDE.md', 'PIGEON_V05_ACCEPTANCE.md', 'PIGEON_V05_HARDWARE_TEST.md', 'PIGEON_V05_TEST_RUN_20261021.md', 'PIGEON_FLASH_LAYOUT.md', 'RELEASE_SIGNING.md', 'PIGEON_NOTE_TIMING.md', 'SCREEN_READER.md'):
        source = ROOT / 'docs' / name
        if source.exists():
            shutil.copyfile(source, out / name)
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True).strip())
    source_paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    source_hashes = {name: sha(ROOT / name) for name in sorted(set(source_paths)) if (ROOT / name).is_file()}
    source_hash_path = out / 'source-sha256.json'
    source_hash_path.write_text(json.dumps(source_hashes, indent=2) + '\n')
    values = list(map(int, metrics.groups()))
    manifest = {'product': 'PIGEON', 'candidate': args.version, 'source_commit': revision,
                'source_has_uncommitted_changes': dirty, 'source_file_hashes_sha256': sha(source_hash_path), 'flashed': False, 'hardware_verified': False,
                'identical_rebuild': args.verify_reproducible, 'host_suite_run': args.run_tests,
                'target_budget_passed': True, 'sample_payload_bytes': SIZE - HEAD,
                'shared_song_voice_limit': 8, 'dedicated_drum_voice_limit': 4, 'app_bytes': values[0],
                'ram_bytes': values[1], 'ram_limit': values[2],
                'pool_bytes': values[3], 'pool_limit': values[4], 'artifacts': artifacts}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(out)
    print('Candidate prepared locally. Hardware validation remains pending.')


if __name__ == '__main__':
    main()
