# PIGEON update safeguards and recovery

Adapted from ChoralRoot FM-1 by Quixotic7 and the ChoralRoot contributors:
https://github.com/Quixotic7/ChoralRootFM1 (GPL-3.0-only).
Reference revision: aa5875af20e3046ee161f988c5d91d3ae66d0c74.

## Before installing

Charge the FM1 fully. Use a USB data cable directly to the computer, without a
hub; keep the computer awake. When coming from another custom firmware, revert
to official M-VAVE V15 using its documented stock-return procedure before
installing PIGEON. Back up your instrument and keep it connected
until the installer verifies the restarted identity.

Browser and command-line installers read the running firmware's INFO text,
because FM-1_9xx identities are shared by forks. Stock V1–V99, PIGEON, Felucca
1.0 or later, Melodee, and ChoralRoot are recognized update sources. Sloop,
FM-1_000 rescue mode, Felucca 0.x/beta builds, missing INFO on a 9xx identity,
and unknown firmware are refused before the update command. Follow the source
firmware's official-return procedure first. CLI --force overrides this refusal
and is explicitly unsafe. A checksum-verified official V15 return remains possible.

## Local v0.3 candidate: recovery stub version 5

The local candidate uses ChoralRoot’s boot-stub design, adapted to PIGEON’s
sample storage boundaries. This describes unpublished candidate code. Flash 0x4120–0x4fff contains the pinned stub;
the app starts at 0x5000. Flash sector 0x76000 holds the commit marker at
0x76fe0. The app body must end at or before 0x60000; the proposed extra sample extent
occupies 0x60000–0x76000. The KOPOOL2 boundary stays 0x78000. Existing sample
extents, metadata, projects and settings retain their physical addresses. The
new extent is appended logically, with a KOPOOL3 descriptor alongside KOPOOL2.
Stub version 5 restores the plaintext window before reading the unchanged
boot marker after the running application has exposed the expanded sample pool.
Changing from an older stub rewrites sector 0x4000 and renews the first-sector
protection gap described below. Do not infer physical recovery safety from
simulated interrupted-write tests.

The loader checks the complete package before erasing, invalidates the commit
marker before changing any other sector, and writes its marker sector last.
The marker contains body length, body CRC-32, and marker CRC-32. The application
directory CRC-16 is compensated to a fixed value so the first sector stays
identical between builds with the same stub. Valid staged loaders remain stored
at 0xe0000 after a successful update.

If the body is incomplete, the stub hands off to that staged loader on the next
power-on. Reconnect and run the installer to finish the update. Hold OCT- alone
while switching on to request recovery; holding both OCT keys retains calibration.
Three short LED flashes indicate loader handoff. A missing/invalid loader or a
repeated failed handoff falls back to ROM USB boot (one long flash).
For a dark instrument exposing WL82 UBOOT1.00, see Quixotic7's recovery project:
https://github.com/Quixotic7/MvaveFM1Unbricker.

The first installation of this stub, or a change to its bytes, rewrites sector
0x4000 and has an unprotected window. Fully charge the instrument and keep the
cable connected. Installing older firmware or stock removes this protection.
This is recovery support, not a guarantee against every cause of an unbootable
instrument. Device power-cycle recovery and the OCT- gesture require physical
FM1 verification; host tests and target compilation cannot establish those results.

The stub is pinned in firmware/stub/stub.lock. Intended stub changes require a
version bump and FM1_STUB_LOCK=update when building, with the renewed first-sector
window documented in release notes. No hardware is flashed by the build or tests.
