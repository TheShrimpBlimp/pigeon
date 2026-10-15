#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the standalone PIGEON product profile.
set -eu
cd "$(dirname "$0")"
if [ "$#" -ne 0 ]; then
    printf '%s\n' 'build-sampler.sh builds the PIGEON development profile without arguments.' >&2
    exit 2
fi
export FELUCCA_KO=1
export FELUCCA_KO_STORAGE=1
export FELUCCA_SAMPLER_ONLY=1
export FELUCCA_KO_DUAL=1
export FELUCCA_CDC=0
./build.sh
"${PYTHON:-python3}" tools/pigeon_profile_audit.py build/felucca.dis
cp build/felucca.fwsc build/pigeon.fwsc
cp build/felucca.bin build/pigeon.bin
printf '%s\n' 'PIGEON: build/pigeon.fwsc'
