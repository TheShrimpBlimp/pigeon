/* SPDX-License-Identifier: GPL-3.0-only */
/* Product identity only: storage, protocol and update IDs stay compatible. */
#if FELUCCA_SAMPLER_ONLY
#define FW_DISPLAY_NAME "PIGEON"
#define FW_DISPLAY_TITLE "PIGEON"
#define FW_DISPLAY_VERSION "v0.3"
#define FW_DISPLAY_DESCRIPTION "Performance sampler"
#else
#define FW_DISPLAY_NAME "Felucca"
#define FW_DISPLAY_TITLE "FELUCCA"
#define FW_DISPLAY_VERSION FELUCCA_VERSION
#define FW_DISPLAY_DESCRIPTION "Multi-engine synthesizer"
#endif
