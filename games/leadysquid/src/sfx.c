/*
 * Leady Squid: sound effects synthesised at start-up, and the music.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 */
#include "ls.h"
#include "house_audio.h"
#include <stdlib.h>

static int music_on = 1, sound_on = 1, music_started;

/* The house synthesiser (games/common/src/house_audio.c): the house sounds (ding, thud, swish, sparkle = HA_MEDAL,
 * join = HA_CONFIRM, pause) and Leady Squid's own (bloop, clank), in the order that keeps the noise the same. */
void sfx_init(void)
{
    const int R = HA_RATE;
    int16_t *buf = malloc(HA_RATE / 2 * sizeof *buf);
    if (!buf) return;
    ha_synth sy, *s = &sy;
    ha_begin(s, buf, HA_RATE / 2, 0x5eed1234u);     /* the same sounds at every start (a core restarted in the same
                                                       process too: save states check the samples) */
    /* bloop: a quick rising sine and a bubbly burble */
    ha_tone(s, 0, 220, 520, 90, 70, 60);
    ha_tone(s, R * 30 / 1000, 700, 1100, 40, 18, 20);
    ha_tone(s, R * 55 / 1000, 900, 1400, 35, 12, 15);
    ha_hiss(s, 0, 70, 10, 3);
    ha_store(s, SFX_BLOOP, 130);
    ha_store(s, SFX_DING, ha_make(s, HA_DING));      /* a soft bell, two partials */
    ha_store(s, SFX_THUD, ha_make(s, HA_THUD));      /* a low falling sine and a little noise */
    /* clank: inharmonic metal partials, two hits */
    for (int hit = 0; hit < 2; hit++) {
        int at = hit * R * 110 / 1000, v = hit ? 55 : 80;
        ha_tone(s, at, 820, 800, 160, v * 45 / 100, 40);
        ha_tone(s, at, 1178, 1160, 140, v * 35 / 100, 30);
        ha_tone(s, at, 1597, 1590, 110, v * 28 / 100, 25);
        ha_tone(s, at, 2311, 2300, 80, v * 18 / 100, 15);
        ha_hiss(s, at, 25, v / 2, 0);
    }
    ha_store(s, SFX_CLANK, 300);
    ha_store(s, SFX_SWISH, ha_make(s, HA_SWISH));    /* filtered noise for the panel */
    ha_store(s, SFX_SPARKLE, ha_make(s, HA_MEDAL));  /* a quick high arpeggio */
    ha_store(s, SFX_JOIN, ha_make(s, HA_CONFIRM));   /* two soft notes up */
    ha_store(s, SFX_PAUSE, ha_make(s, HA_PAUSE));    /* a short low blip */
    free(buf);
    rs_echo(HA_ECHO_DELAY, HA_ECHO_FB, HA_ECHO_VOL);
}

void sfx_pan(int id, int x)
{
    if (!sound_on) return;
    int pan = clampi(24 + x * 80 / RS_SCREEN_W, 0, 127);
    static const uint8_t vol[SFX_COUNT] = {88, 70, 110, 96, 50, 70, 80, 70};
    int v = rs_sfx(id, vol[id], pan, RS_PITCH_1);
    if (v >= 0 && (id == SFX_DING || id == SFX_BLOOP || id == SFX_CLANK)) rs_voice_echo(v, 1);
}

void sfx(int id) { sfx_pan(id, RS_SCREEN_W / 2); }

void music_start(void)
{
    if (!music_on) { rs_music_stop(); music_started = 0; return; }
    if (music_started && rs_music_playing()) return;
    size_t n;
    const void *m = rs_asset("music/tune.mod", &n);
    if (m && !rs_music_play(m, n, 1)) {
        rs_music_volume(HA_MUSIC_VOL);
        music_started = 1;
    }
}

void audio_set(int music, int sound)
{
    music_on = music;
    sound_on = sound;
    if (!music_on) { rs_music_stop(); music_started = 0; }
}

/* ---- save states (main.c): the sound options (the SDK restarts the tune where it was) ---- */
void sfx_state(void)
{
    rs_state_var("sfx.music_on", &music_on, sizeof music_on);
    rs_state_var("sfx.sound_on", &sound_on, sizeof sound_on);
    rs_state_var("sfx.music_started", &music_started, sizeof music_started);
    ha_state();                 /* the house audio kit's objects (house_audio.*) */
}
