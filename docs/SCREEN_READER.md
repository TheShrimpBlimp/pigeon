# PIGEON screen-reader support

Screen-reader support is included in standard v0.4 firmware. The published v0.3.1
release does not include it. A separate reader fork is no longer needed.

## Connect an FM1

1. Install a build with screen-reader support and connect the FM1 by USB.
2. Open [FM-1 Screen Reader](https://charlesvestal.github.io/fm1-reader/) in desktop
   Chrome or Edge and connect the matching MIDI port.
3. Choose browser speech or your own screen reader.
4. Use **WHERE** to read the current page and control values again.

For this local release candidate, load `web/words/84859cb5f4.json` into the
reader manually. The matching hosted dictionary will accompany the release.
Connecting the reader does not change samples, sessions or settings.

## Emulator

Enable **screen announcements**, then choose **My screen reader** or **Browser
voice**. Use **Read current screen** to repeat the page. Announcements start off.
The development emulator includes its dictionary; this support is not yet on the
published emulator.

## Announcements

- Navigation speaks the page title. Adjusting a control speaks its name and value.
- WHERE reads current controls and the selected step's chord. In the piano roll,
  it also includes the selected note's length and nudge.
- Recording arm state, transport changes, errors and confirmation choices are spoken.
- Queue WHERE reads the selection and the next eight chop numbers in order,
  including repeats. K3 announces the selected waiting position and chop.
- Scratch announcements cover the selected chop, controls, tuning and cue/reset
  confirmations. EDIT changes control sets; PRESETS tunes and SEL resets tuning.

Playback ticks, jog movement, queue draining and decorative animations stay
silent. Empty note slots are identified without reading unused values. Errors
remain immediate; routine changes settle before being announced.

For the complete button map, see the [v0.4 guide](PIGEON_V04_GUIDE.md).

## Credits

The integration uses [Charles Vestal's FM-1 Screen Reader protocol](https://github.com/charlesvestal/fm1-reader),
word generator and pronunciation module, under GPL-3.0-only. PIGEON supplies its
page descriptions and emulator interface. Build details and verification status
are in [development notes](PIGEON_V04_DEVELOPMENT.md).

Sequence tools announce the chosen action and its relevant controls. Expanded
step conditions are spoken by name. Holding ARP for FILL does not change pages
or produce continuous announcements.
