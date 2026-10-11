# Pigeon splash

`splash.png` is the photograph supplied by the project owner for the firmware
splash screen. It is kept separate from inherited Felucca assets and notices.
No third-party redistribution license for this photograph has been established.

`tools/gen_brand_splash.py` preserves the full image, fits it within 208 x 192
pixels and encodes a 256-color RGB565 palette plus pixel indices. The current
portrait occupies 20,288 bytes of flash and uses the existing display canvas.
This asset is included only in sampler-profile firmware.
