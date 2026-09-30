/*
 * Blueberry Tumble: the sounds, synthesised at start-up with the house synthesiser (games/common/src/house_audio.c:
 * sines with a pitch sweep and a soft decay, low-passed noise), mixed at the house loudness.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include "house_audio.h"
#include <string.h>

static int16_t snd_buf[HA_RATE / 2];       /* start-up scratch (state_audit.txt) */

/* store the first ms of the synthesis as a 16-kHz sample (half the memory: the sample-memory guideline is 64 KiB,
 * like the ARAM; every sound of the game is under 4 kHz), then clear the buffer (as ha_store) */
static void store_half(ha_synth *s, int slot, int ms)
{
    int n = ms * HA_RATE / 1000;
    if (n > s->len) n = s->len;
    int m = n / 2;
    for (int i = 0; i < m; i++) s->buf[i] = (int16_t)((s->buf[2 * i] + s->buf[2 * i + 1]) / 2);
    rs_sample_pcm16(slot, s->buf, m, HA_RATE / 2, -1);
    memset(s->buf, 0, (size_t)s->len * sizeof *s->buf);
}

void sfx_init(int sound_on)
{
    ha_synth s;
    const int R = HA_RATE;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0xb1eb3e77u);
    /* jump: a soft rising pop */
    ha_tone(&s, 0, 330, 620, 70, 46, 40);
    ha_hiss(&s, 0, 30, 6, 3);
    store_half(&s, SFX_JUMP, 80);
    /* land: a small soft thud */
    ha_tone(&s, 0, 160, 90, 60, 40, 30);
    ha_hiss(&s, 0, 40, 14, 3);
    store_half(&s, SFX_LAND, 70);
    /* squelch: a wet burst, a falling gulp and a splatter (the splat) */
    ha_hiss(&s, 0, 140, 60, 1);
    ha_tone(&s, 0, 420, 90, 180, 70, 70);
    ha_tone(&s, R * 60 / 1000, 900, 300, 90, 26, 30);
    ha_hiss(&s, R * 120 / 1000, 160, 26, 2);
    store_half(&s, SFX_SQUELCH, 280);
    /* boing: a mushroom pad, a springy sweep up with a wobble */
    ha_tone(&s, 0, 180, 520, 140, 60, 90);
    ha_tone(&s, R * 110 / 1000, 520, 700, 120, 34, 60);
    ha_tone(&s, R * 170 / 1000, 700, 640, 75, 18, 40);
    store_half(&s, SFX_BOING, 250);
    /* chime: a dew drop, two high bell partials */
    ha_tone(&s, 0, 1976, 1976, 210, 36, 70);
    ha_tone(&s, 0, 2960, 2960, 200, 16, 40);
    ha_tone(&s, R * 40 / 1000, 3951, 3951, 120, 8, 20);
    store_half(&s, SFX_CHIME, 220);
    /* whoosh: the gust catches the leaf, filtered noise swelling in */
    for (int i = 0; i < 6; i++) ha_hiss(&s, R * i * 30 / 1000, 230 - i * 30, 6 + i * 4, 3);
    ha_tone(&s, 0, 200, 340, 230, 10, 0);
    store_half(&s, SFX_WHOOSH, 240);
    /* crunch: the snowberry smashes something small, three noisy bites */
    for (int i = 0; i < 3; i++) {
        ha_hiss(&s, R * i * 28 / 1000, 40, 70 - i * 15, 0);
        ha_tone(&s, R * i * 28 / 1000, 240 - i * 40, 90, 40, 30, 15);
    }
    store_half(&s, SFX_CRUNCH, 130);
    /* grow: packing snow, a soft muffled puff rising */
    ha_hiss(&s, 0, 170, 36, 4);
    ha_tone(&s, 0, 150, 300, 170, 26, 100);
    store_half(&s, SFX_GROW, 180);
    /* splash: the stream washes the snow off */
    ha_hiss(&s, 0, 160, 50, 1);
    ha_tone(&s, 0, 1200, 500, 110, 18, 40);
    ha_tone(&s, R * 70 / 1000, 900, 400, 90, 12, 30);
    store_half(&s, SFX_SPLASH, 170);
    /* coin: a golden blueberry, a bright quick arpeggio up (E6 G#6 B6 E7) */
    ha_tone(&s, 0, 1319, 1319, 70, 30, 30);
    ha_tone(&s, R * 50 / 1000, 1661, 1661, 70, 30, 30);
    ha_tone(&s, R * 100 / 1000, 1976, 1976, 70, 30, 30);
    ha_tone(&s, R * 150 / 1000, 2637, 2637, 145, 30, 60);
    store_half(&s, SFX_COIN, 300);
    /* skid: sliding on ice, a thin hiss */
    ha_hiss(&s, 0, 175, 22, 0);
    store_half(&s, SFX_SKID, 180);
    /* gate: a new biome, a wooden knock and a bright fifth */
    ha_tone(&s, 0, 220, 180, 50, 50, 20);
    ha_tone(&s, R * 60 / 1000, 784, 784, 175, 30, 90);
    ha_tone(&s, R * 60 / 1000, 1175, 1175, 175, 22, 90);
    store_half(&s, SFX_GATE, 240);
    ha_init(SFX_HOUSE);                     /* HA_CONFIRM, HA_PAUSE, HA_MEDAL, HA_GAMEOVER... */
    ha_sound_on(sound_on);
}

void sfx_play(int id, int x)
{
    /* the house loudness: the crash 110, the main action 88, feedback 70, the soft ones 50 */
    static const uint8_t vol[SFX_COUNT] = {HA_VOL_ACTION, 50, HA_VOL_CRASH, HA_VOL_ACTION, HA_VOL_FEEDBACK,
                                           HA_VOL_FEEDBACK, HA_VOL_ACTION, HA_VOL_FEEDBACK, HA_VOL_FEEDBACK,
                                           HA_VOL_FEEDBACK, 50, HA_VOL_UI};
    ha_play_slot(id, vol[id], x, id == SFX_CHIME || id == SFX_SPLASH || id == SFX_COIN);
}
