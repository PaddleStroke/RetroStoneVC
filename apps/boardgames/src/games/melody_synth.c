/*
 * Play by Ear's piano, made by code (no samples): each note rendered once, when first played, into 16-bit sound. The
 * voice itself is shared with Falling Notes: games/pianosynth.h (additive partials, two strings a little apart, the
 * hammer's knock, a fast then slow decay; "dull" a key gone wrong). No SDL: games/melody.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/melody.h"
#include "games/pianosynth.h"

_Static_assert(MEL_RATE == PK_RATE, "one rate");

int mel_synth_len(int pitch, int dull) { return pk_synth_len(pitch, dull); }

void mel_synth(int pitch, int dull, int16_t *out, int n) { pk_synth(pitch, dull, out, n); }
