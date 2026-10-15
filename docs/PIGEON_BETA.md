# Preparing a PIGEON beta candidate

Use the pinned target compiler provisioned by the existing build workflow and
Python from the project virtual environment. From the firmware repository:

```
.venv/bin/python tools/prepare_pigeon_beta.py --verify-reproducible --run-tests
```

Node.js must be on PATH for full web/codec checks. The script builds the sampler
profile, checks target CPU budgets, optionally repeats the build and compares
both binary and package SHA-256 hashes, and runs `tests/run_pigeon_tests.sh`: product controls, compatibility, recovery,
malformed data, 92 inherited golden audio renders, sanitizers, dual-song storage
and GUI codec checks. With `--run-tests`, it also runs
`tests/run_pigeon_recovery_tests.sh` for the pinned boot stub, commit ordering,
interrupted writes, and installer compatibility refusals. It copies firmware, checksums, target memory metrics, test logs and control
documentation into `build/releases/pigeon-0.3-beta-candidate/`. It never contacts
hardware, flashes, creates a release/tag, or publishes a website.

Without `--run-tests`, the manifest explicitly says the full host suite was not
run by this invocation. Independently collected logs can accompany a candidate;
do not change that flag to imply they were run by the script. Reproducibility is
checked for two builds in the same checkout/toolchain, not across operating
systems. A dirty checkout is recorded and is unsuitable for an official release.

Before a release, verify physical USB bank upload/readback, interrupted transfer
recovery, two-song/drum playback with FX, sustained retriggers, tempo sync, LEDs,
recording and save/reboot. Browser-only tests cannot establish those outcomes.
Keep the previous flashed package and a full project/sample recovery archive.

The local v0.3 candidate expands the sample payload to 496 KiB by reclaiming
legacy asset space. The published firmware and web tools remain at 408 KiB.
Existing bank extents and boot-marker sectors retain their addresses; the new
extent is appended logically. Serialized FUNA parameter IDs/layout remain unchanged.
Extended conditions and pitch locks need current PIGEON firmware; older firmware
may reject such projects. Imported audio and private recovery archives are not
release assets. Label this build a candidate until hardware checks pass.
