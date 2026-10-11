# Local MP3 encoder

`lame.js`, based on the readable `lame.all.js` from **lamejs 1.2.1**, Alex Zhukov's JavaScript port of LAME.
Source: https://github.com/zhuker/lamejs and https://registry.npmjs.org/lamejs/-/lamejs-1.2.1.tgz.
License: LGPL-3.0; see LGPL-3.0.txt and the original lamejs-NOTICE.txt.
The sole modification explicitly sets `gfp.out_samplerate = samplerate` in
`Mp3Encoder`, preventing LAME from choosing 16 kHz at low bitrates. This change
is distributed under LGPL-3.0 in the readable runtime. Upstream corresponding source is included in `lamejs-1.2.1-source.tgz` beside the runtime (JavaScript/Java source,
original notices, and build scripts; upstream example media omitted).
LAME: https://lame.sourceforge.net/ .

The upstream encoder already disables its bit reservoir and VBR/ID3 tags. Pigeon
uses mono 22,050 Hz at 24–64 kbps, validates every frame, adds the firmware index,
and uses LAME's 576-sample encoder delay plus the decoder's 529-sample delay.
