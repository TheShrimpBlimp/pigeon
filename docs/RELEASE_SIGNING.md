# PIGEON release signing

PIGEON 0.4.1 retains the v0.4 sampler, data formats, flash layout and FM-1_900
update/protocol identity. It changes the displayed version and adds the release
provenance workflow. Development features for v0.5 are not included.

The tagged source is built by the Firmware Hub's reusable workflow:
`zvenson/fm1-build/.github/workflows/build.yml@v1`. GitHub attests the resulting
`.fwsc` using its short-lived signing identity, then attaches that exact file and
SHA256SUMS to the tag's release. No local signing key is needed or committed.
Product, recovery, codec and package checks must pass before attestation/release.

## Publish

1. Update `FW_DISPLAY_VERSION` in `firmware/src/project_identity.h` to the exact
   release version, including `v`. Review and merge that source first.
2. Tag the reviewed commit: `git tag v0.4.1`, then `git push origin v0.4.1`.
3. Wait for **Signed firmware release** to succeed in GitHub Actions. Use its
   `pigeon-0.4.1.fwsc` release asset. Do not replace it with a local rebuild.

`tools/build_signed_release.sh` validates the tag/version before building,
uses the PIGEON sampler profile and stages one package under `build/signed/`.
Its package bytes are unchanged when copied into the release directory. Running
it locally produces a test package; only the GitHub workflow produces the
provenance attestation.

## Verify

Download the release asset, then run:

```sh
gh attestation verify pigeon-0.4.1.fwsc --repo TheShrimpBlimp/pigeon \
  --signer-workflow zvenson/fm1-build/.github/workflows/build.yml
```

The attestation establishes which repository, commit and workflow produced the
file. It does not prove the firmware is bug-free or add on-device secure boot.
The Firmware Hub verifies provenance when it syncs the release; its badge may
therefore appear later than the GitHub release.

Protocol and rationale:
- https://thefirmwarehub.com/kb/#sign
- https://thefirmwarehub.com/kb/#provenance
