#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Validate the release wrapper and exact package staged for attestation."""
import os,re,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
os.chdir(root)
version=re.search(r'^#define FW_DISPLAY_VERSION "v([^"\n]+)"$',Path('firmware/src/project_identity.h').read_text(),re.M)[1]
package=Path('build/signed')/f'pigeon-{version}.fwsc'
assert package.read_bytes()==Path('build/pigeon.fwsc').read_bytes(), 'Staging must not alter package bytes'
assert list(Path('build/signed').glob('*.fwsc'))==[package], 'Attest only the intended release package'
mismatch=version.rsplit('.',1)[0]+'.'+str(int(version.rsplit('.',1)[1])+1)
for bad in ['',mismatch,'../'+version,version+'\n','v'+version]:
    result=subprocess.run(['sh','tools/build_signed_release.sh',bad],capture_output=True,text=True)
    assert result.returncode!=0, f'Invalid or mismatched version accepted: {bad!r}'
assert package.read_bytes()==Path('build/pigeon.fwsc').read_bytes()
print(f'Signed release staging: exact PIGEON {version} package and tag/version guards passed.')
