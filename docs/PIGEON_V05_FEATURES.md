# PIGEON 0.5 — THE FINAL BETA

New and expanded features in the 0.5 release line:

- **USB sampling into the FM1:** record to the selected song or drum pad, with input metering, clipping indication, sound-triggered start, and seconds or bars limits.
- **Review before saving:** audition an unsaved take, trim its start/end, retry or discard, and confirm before replacing the destination.
- **Keep selected chops:** retain useful regions, trim away the rest, and build up either song. On-device retention preserves key numbers; browser trimming renumbers them.
- **Reclaim sample storage:** compact live audio and remove obsolete payloads. The shared sample bank now has **500 KiB usable**.
- **More control per chop:** independent playback start, reverse, fine tuning, nondestructive gain, and loop crossfade. The loader adds normalization and matching previews.
- **Safer chop editing:** reset to source assignments, undo/redo, and immediate boundary audition while holding SEL and a chop key.
- **Reusable chop collections:** collect sounds from multiple sources in the browser, export/import collections, and assemble song banks or drum kits.
- **Pattern launch controls:** choose next-bar or pattern-end switching and cancel a pending launch.
- **Clearer session management:** bank/session summaries and overwrite previews help distinguish sample-only transfers from full-session restores.
- **Emulator output recording:** download performances as WAV or MP3.
- **Accessibility and interface polish:** screen-reader support, shared site navigation, corrected SEL button hints, and fixes for Settings and recorder text flicker.
- **Update safeguards:** expanded-bank compatibility checks prevent an older updater from overwriting audio in the new flash region; recovery documentation and complete-session backups accompany the beta.

Scratch mode is excluded to make room for USB sampling. Ordinary per-chop reverse remains available.

The maintainer reports the hardware checklist complete on 2026-10-21. The final SEL label-only rebuild has automated software checks; it does not change recording or storage behavior. See [validation](PIGEON_V05_ACCEPTANCE.md) and the [tutorial](PIGEON_V05_GUIDE.md) for evidence, controls and limits.
