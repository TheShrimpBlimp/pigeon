#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Produce exactly one PIGEON package for fm1-build to attest and publish.
set -eu
cd "$(dirname "$0")/.."
[ "$#" -eq 1 ] || { echo 'Usage: sh tools/build_signed_release.sh VERSION' >&2; exit 2; }
PIGEON_RELEASE_VERSION=$1
export PIGEON_RELEASE_VERSION
"${PYTHON:-python3}" - <<'PY'
import os,re
from pathlib import Path
version=os.environ['PIGEON_RELEASE_VERSION']
if not re.fullmatch(r'(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:-(?:alpha|beta|rc)\.(?:0|[1-9]\d*))?',version):
    raise SystemExit('Release version must be X.Y.Z or X.Y.Z-beta.N (alpha/rc also supported), without v.')
source=Path('firmware/src/project_identity.h').read_text()
match=re.search(r'^#define FW_DISPLAY_VERSION "v([^"\n]+)"$',source,re.M)
if not match or match[1]!=version:
    raise SystemExit('Release tag must match the embedded PIGEON display version.')
output=Path('build/signed')
if output.exists() and any(p.name!=f'pigeon-{version}.fwsc' for p in output.glob('*.fwsc')):
    raise SystemExit('Refusing to mix packages from different signed releases.')
PY
# Keep the tested PIGEON profile and existing FM-1_900 protocol/update identity.
sh build-sampler.sh
mkdir -p build/signed
cp build/pigeon.fwsc "build/signed/pigeon-$PIGEON_RELEASE_VERSION.fwsc"
printf 'PIGEON release package: build/signed/pigeon-%s.fwsc\n' "$PIGEON_RELEASE_VERSION"
