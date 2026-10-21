# PIGEON development flash layout

The bank is **504 KiB total / 500 KiB usable**, including a 4 KiB header.
This pass adds one 4 KiB sector. It appends to the logical bank, preserving
every address used by existing banks. End addresses below are exclusive.

| Logical order | Physical start | Physical end | Size |
| --- | --- | --- | --- |
| 1 | `0xA0000` | `0xB3000` | 76 KiB |
| 2 | `0xB4000` | `0xDC000` | 160 KiB |
| 3 | `0x78000` | `0x92000` | 104 KiB |
| 4 | `0xEA000` | `0xFC000` | 72 KiB |
| 5 | `0x60000` | `0x76000` | 88 KiB |
| 6 (new) | `0x5F000` | `0x60000` | 4 KiB |

The first logical sector contains the bank header. The firmware body begins
at `0x5000` and must end before `0x5F000`. The current 364,548-byte image leaves
7900 bytes of body growth room; the build requires at least 4096 bytes.
The boot stub is unchanged.

No other regions are reclaimed: boot/header, boot marker at `0x76000`,
compatibility/configuration sectors, recovery and project storage, legacy
data, bank metadata at `0xB3000`, old preset banks at `0xDC000..0xE0000`,
OTA/move journals, autosaves, stock VM backup, Bluetooth data, global settings
and the device key retain their existing addresses.

## Update compatibility

Python and browser uploaders negotiate device capacity before beginning an
erase. Earlier layouts remain supported at their advertised capacity. The
loader validates the incoming pool descriptor and checks that the stored
bank fits before erasing.

If the bank uses the new tail, OTA also checks the incoming descriptor
before staging: an older-layout package is rejected with result **-17**.
This is necessary because its older loader cannot protect the new sector.
Export the bank and replace it with a bank that fits the older layout before
downgrading; rejection never automatically deletes samples. OTA also refuses
while a bank upload, USB take or collection recovery operation is active.

## Validation and remaining work

Host tests cover full-capacity upload and CRC/readback, stable old addresses,
reboot scanning, protected sectors, downgrade rejection before erase, and
update/recovery paths. Real-device acceptance is listed in
[PIGEON_V05_HARDWARE_TEST.md](PIGEON_V05_HARDWARE_TEST.md). The candidate has
not been flashed. The matching emulator WASM has been rebuilt and its native
and Chrome browser acceptance checks pass locally.
