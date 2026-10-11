Source: https://github.com/sezero/libmad
Revision: 486f902c6c686eafced3450851849527e29bc7f6
License: GPL-2.0-or-later (COPYING and source notices).
Felucca uses the synchronous fixed-point decoder only.

Local change: synth.c keeps reversed synthesis-window pointers within their
32-element coefficient array; indices are rebased by 15 with identical values.
This avoids undefined pointer formation detected by UBSan.
