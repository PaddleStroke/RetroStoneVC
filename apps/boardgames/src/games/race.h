/*
 * The race games' helpers, now the shell's and art.c's: the inlaid tile (art_inlay), the steady pseudo-random number
 * (art_hash), the eased slide and the hop (ui_ease, ui_hop), the inks and pi (ART_INK, ART_PALE, ART_PI). Kept, as
 * these names, for the games still written with them; new code uses the shell's and art.c's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef RACE_H
#define RACE_H

#include <stdint.h>

#include "shell.h"

#define RACE_PI ART_PI
#define RACE_INK ART_INK
#define RACE_PALE ART_PALE

static inline void race_inlay(uint8_t *o, int pw, int ph, float x0, float y0, float w, float h, float rad,
                              const float tone[3], float seam)
{
    art_inlay(o, pw, ph, x0, y0, w, h, rad, tone, seam);
}
static inline float race_hash(int a, int b) { return art_hash(a, b); }
static inline float race_ease(float f) { return ui_ease(f); }
static inline float race_lift(float f) { return ui_hop(f); }

#endif
