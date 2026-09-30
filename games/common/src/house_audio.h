/*
 * The 8BCraft house audio kit (games/common): the sound-effect synthesiser of Leady Squid, the house UI
 * sounds, and the loudness targets every game mixes at (docs/art-direction.md "Audio").
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Sounds are synthesised at start-up (sines with a pitch sweep and a soft decay, low-passed noise) into
 * 32 kHz PCM samples: no sound files. Music is a 4-channel MOD written by the game's tools/make_music.py
 * with games/common/tools/house_music.py and played by the SDK (rs_music_play).
 */
#ifndef HOUSE_AUDIO_H
#define HOUSE_AUDIO_H

#include <stdint.h>
#include "rs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- loudness targets (all games mix at the same level) ---------------------------------------------------------- */
#define HA_MUSIC_VOL   56       /* rs_music_volume() */
#define HA_ECHO_DELAY  110      /* rs_echo(): a small room, echo on the "wet" sounds only */
#define HA_ECHO_FB     35
#define HA_ECHO_VOL    30
/* rs_sfx() volumes: the loudest (a crash) 110, the main action 88, feedback 70, UI 50-80 */
#define HA_VOL_CRASH   110
#define HA_VOL_ACTION  88
#define HA_VOL_FEEDBACK 70
#define HA_VOL_UI      70

/* ---- the synthesiser ---------------------------------------------------------------------------------------------- */
#define HA_RATE 32000
typedef struct ha_synth {
    int16_t *buf;               /* the caller's buffer (zeroed by ha_begin and after each ha_store) */
    int len;                    /* in samples */
    uint32_t noise;             /* the noise generator's state */
} ha_synth;
void ha_begin(ha_synth *s, int16_t *buf, int len, uint32_t noise_seed);
/* a sine sweep f0 -> f1 (Hz) from sample `at`, `ms` long, volume 0..100, a hyperbolic decay (decay_ms: the time
 * to half volume, 0 = none), a 3-ms attack and a 12-ms release: mixed into the buffer */
void ha_tone(ha_synth *s, int at, int f0, int f1, int ms, int vol, int decay_ms);
/* low-passed white noise (lp_shift 0 = bright .. 4 = dull), a squared linear fade */
void ha_hiss(ha_synth *s, int at, int ms, int vol, int lp_shift);
/* store the first `ms` of the buffer as sample `slot` (one-shot), then clear the buffer */
void ha_store(ha_synth *s, int slot, int ms);
int  ha_ms(int ms);             /* ms -> samples */

/* ---- the house sounds -------------------------------------------------------------------------------------------------- */
enum {
    HA_CONFIRM,     /* two soft notes up (C5, G5): start, join, menu confirm (Leady Squid's "join") */
    HA_PAUSE,       /* a short low blip (A4 -> E4) */
    HA_MEDAL,       /* a quick high arpeggio (C7 E7 G7): a medal, a new best */
    HA_GAMEOVER,    /* the sting: a swish and three falling notes (G4 Eb4 C4) */
    HA_SWISH,       /* filtered noise: a panel sliding in */
    HA_DING,        /* the score bell (G6 with two partials) */
    HA_SELECT,      /* a tiny tick: a cursor moving */
    HA_THUD,        /* a low falling thud: a hit */
    HA_COUNT
};
/* synthesise house sound `id` into s's buffer; returns its length in ms (then ha_store it) */
int  ha_make(ha_synth *s, int id);
/* synthesise the whole house set into sample slots first..first+HA_COUNT-1 (uses a temporary buffer) and set
 * the house echo */
void ha_init(int first_slot);
/* play house sound id (at the house volume, centred) or a game slot panned by screen x (0..319) */
int  ha_play(int id);
int  ha_play_slot(int slot, int vol, int x, int echo);
void ha_sound_on(int on);
/* start the game's music (an asset of its pack, e.g. "music/tune.mod") at the house volume, once; music_on 0 stops it */
void ha_music(const char *asset, int music_on);
/* save states */
void ha_state(void);

#ifdef __cplusplus
}
#endif
#endif
