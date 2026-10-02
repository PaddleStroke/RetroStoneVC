/*
 * Pogo Mamie: the sounds and the music. The house synthesiser (games/common/src/house_audio.c) makes the house
 * set (confirm, pause, medal, the game-over sting, swish, ding, thud) and our own: the boing (played at a lower
 * pitch for a bigger bounce), the pigeon's coo, a tile's crack, glass, the umbrella's pop, the cat's meow, the
 * fall's whistle, the antenna's clang, a power-up, a splash and Mamie's grumbling. Ours are synthesised at the
 * house rate and stored at a third of it (10.7 kHz): the sample-memory guideline (64 KiB as ADPCM) holds with
 * the house set. The music: one musette waltz per district, another at night (tools/make_music.py).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pogomamie/LICENSE.
 */
#include "pm.h"
#include "house_audio.h"
#include <stdlib.h>
#include <string.h>

#define SLOT_OWN HA_COUNT       /* our samples come after the house set */
#define DECIM 3

static int music_on = 1, sound_on = 1, music_track = -1;

/* the slot of each effect: a house sound (< HA_COUNT) or ours */
enum { O_BOING = SLOT_OWN, O_COO, O_CRACK, O_GLASS, O_POP, O_MEOW, O_WHISTLE, O_CLANG, O_PICKUP, O_SPLASH, O_GRUMBLE };
static const struct { uint8_t slot, vol, echo; uint16_t pitch; } sounds[SFX_COUNT] = {
    [SFX_BOING] = {O_BOING, HA_VOL_ACTION, 0, 0x1000},
    [SFX_BOING_BIG] = {O_BOING, HA_VOL_ACTION + 8, 0, 0x0c40},     /* the same boing, lower: a bigger bounce */
    [SFX_SPRING] = {O_BOING, HA_VOL_ACTION + 12, 0, 0x0a00},
    [SFX_COO] = {O_COO, HA_VOL_FEEDBACK, 0, 0x1000},
    [SFX_CRACK] = {O_CRACK, HA_VOL_FEEDBACK + 8, 0, 0x1000},
    [SFX_GLASS] = {O_GLASS, HA_VOL_FEEDBACK + 8, 1, 0x1000},
    [SFX_POP] = {O_POP, HA_VOL_FEEDBACK, 0, 0x1000},
    [SFX_MEOW] = {O_MEOW, HA_VOL_FEEDBACK, 1, 0x1000},
    [SFX_WHISTLE] = {O_WHISTLE, HA_VOL_FEEDBACK, 0, 0x1000},
    [SFX_JINGLE] = {HA_GAMEOVER, 80, 0, 0x1000},
    [SFX_CLANG] = {O_CLANG, HA_VOL_FEEDBACK, 1, 0x1000},
    [SFX_PICKUP] = {O_PICKUP, HA_VOL_FEEDBACK, 1, 0x1000},
    [SFX_THUMP] = {HA_THUD, HA_VOL_CRASH, 0, 0x1000},
    [SFX_SPLASH] = {O_SPLASH, HA_VOL_CRASH - 20, 1, 0x1000},
    [SFX_GRUMBLE] = {O_GRUMBLE, HA_VOL_FEEDBACK, 0, 0x1000},
    [SFX_STUNT] = {HA_DING, HA_VOL_FEEDBACK, 1, 0x1000},
    [SFX_JOIN] = {HA_CONFIRM, 80, 0, 0x1000},
    [SFX_PAUSE] = {HA_PAUSE, HA_VOL_UI, 0, 0x1000},
    [SFX_SWISH] = {HA_SWISH, 50, 0, 0x1000},
    [SFX_MEDAL] = {HA_MEDAL, HA_VOL_UI, 1, 0x1000},
};

/* our own, into s's buffer at the house rate; stored at a third of it */
static void store_decimated(ha_synth *s, int slot, int ms)
{
    int n = ha_ms(ms) / DECIM;
    int16_t *d = malloc((size_t)n * sizeof *d);
    if (d) {
        for (int i = 0; i < n; i++)
            d[i] = (int16_t)((s->buf[i * DECIM] + s->buf[i * DECIM + 1] + s->buf[i * DECIM + 2]) / 3);
        rs_sample_pcm16(slot, d, n, HA_RATE / DECIM, -1);
        free(d);
    }
    memset(s->buf, 0, (size_t)s->len * sizeof *s->buf);
}

/* a tone with its 2nd and 3rd harmonics (a reedier voice) */
static void reed(ha_synth *s, int at, int f0, int f1, int ms, int vol, int decay)
{
    ha_tone(s, at, f0, f1, ms, vol, decay);
    ha_tone(s, at, f0 * 2, f1 * 2, ms, vol / 3, decay);
    ha_tone(s, at, f0 * 3, f1 * 3, ms, vol / 5, decay);
}

/* a sweep that wobbles (the pogo's spring): short segments alternately above and below the sweep */
static void wobble(ha_synth *s, int at, int f0, int f1, int ms, int vol, int decay, int depth)
{
    int n = 8;
    for (int i = 0; i < n; i++) {
        int a = f0 + (f1 - f0) * i / n, b = f0 + (f1 - f0) * (i + 1) / n;
        int d = depth * (n - i) / n * (i & 1 ? -1 : 1);
        reed(s, at + ha_ms(ms * i / n), a + d, b - d, ms / n + 4, vol * (n - i / 2) / n, decay);
    }
}

void sfx_init(void)
{
    ha_init(0);
    int len = HA_RATE * 11 / 10;
    int16_t *buf = malloc((size_t)len * sizeof *buf);
    if (!buf) return;
    ha_synth s;
    ha_begin(&s, buf, len, 0x9090a1u);
    /* boing: a rising sproing */
    wobble(&s, 0, 190, 520, 210, 70, 140, 40);
    store_decimated(&s, O_BOING, 230);
    /* the pigeon's coo: croo-croo */
    for (int k = 0; k < 2; k++) {
        reed(&s, ha_ms(k * 170), 420, 360, 140, 50, 90);
        ha_hiss(&s, ha_ms(k * 170), 60, 6, 4);
    }
    store_decimated(&s, O_COO, 330);
    /* a tile cracks */
    ha_hiss(&s, 0, 50, 70, 1);
    ha_hiss(&s, ha_ms(60), 80, 50, 1);
    ha_tone(&s, 0, 180, 90, 110, 50, 60);
    store_decimated(&s, O_CRACK, 180);
    /* glass: high inharmonic partials and a tinkle */
    ha_tone(&s, 0, 2600, 2550, 260, 30, 80);
    ha_tone(&s, 0, 3710, 3690, 200, 22, 60);
    ha_tone(&s, ha_ms(30), 4700, 4680, 160, 14, 50);
    ha_hiss(&s, 0, 140, 36, 0);
    ha_tone(&s, ha_ms(150), 3100, 3100, 120, 14, 40);
    store_decimated(&s, O_GLASS, 290);
    /* the umbrella pops open */
    ha_hiss(&s, 0, 45, 60, 2);
    ha_tone(&s, ha_ms(20), 300, 900, 110, 40, 60);
    store_decimated(&s, O_POP, 140);
    /* meow: up, then down */
    reed(&s, 0, 480, 820, 140, 45, 0);
    reed(&s, ha_ms(140), 820, 430, 240, 45, 160);
    store_decimated(&s, O_MEOW, 390);
    /* the fall's whistle: a long descending sine */
    ha_tone(&s, 0, 1800, 420, 900, 45, 0);
    store_decimated(&s, O_WHISTLE, 900);
    /* the antenna: a metal clang */
    ha_tone(&s, 0, 900, 880, 280, 35, 70);
    ha_tone(&s, 0, 1370, 1350, 230, 25, 50);
    ha_tone(&s, 0, 2130, 2100, 160, 16, 30);
    ha_hiss(&s, 0, 20, 40, 0);
    store_decimated(&s, O_CLANG, 290);
    /* a power-up: a quick arpeggio up */
    reed(&s, 0, 784, 784, 60, 32, 30);
    reed(&s, ha_ms(55), 988, 988, 60, 32, 30);
    reed(&s, ha_ms(110), 1175, 1175, 60, 32, 30);
    reed(&s, ha_ms(165), 1568, 1568, 100, 32, 60);
    store_decimated(&s, O_PICKUP, 270);
    /* a splash */
    ha_hiss(&s, 0, 380, 70, 2);
    ha_tone(&s, 0, 300, 120, 180, 30, 80);
    store_decimated(&s, O_SPLASH, 390);
    /* Mamie's grumbling: six muttered syllables */
    {
        static const int f[6] = {190, 240, 170, 260, 210, 150};
        for (int i = 0; i < 6; i++) reed(&s, ha_ms(i * 95), f[i], f[i] - 30, 80, 38, 50);
    }
    store_decimated(&s, O_GRUMBLE, 580);
    free(buf);
}

/* effect `id` panned by screen x; pitch 0 = its own (0x1000 = the sample's rate) */
void sfx_at(int id, int x, int pitch)
{
    if (!sound_on || id < 0 || id >= SFX_COUNT) return;
    ha_play_pitched(sounds[id].slot, sounds[id].vol, x,
                    pitch ? pitch : sounds[id].pitch, sounds[id].echo);
}

void sfx(int id) { sfx_at(id, RS_SCREEN_W / 2, 0); }

void music_district(int district, int night)
{
    static const char *const names[5] = {"music/tune_montmartre.mod", "music/tune_seine.mod",
                                         "music/tune_haussmann.mod", "music/tune_eiffel.mod", "music/tune_night.mod"};
    int track = night ? 4 : district;
    if (!music_on) { music_stop(); return; }
    if (track == music_track && rs_music_playing()) return;
    size_t n;
    const void *m = rs_asset(names[track], &n);
    if (m && !rs_music_play(m, n, 1)) {
        rs_music_volume(HA_MUSIC_VOL);
        music_track = track;
    }
}

void music_stop(void)
{
    rs_music_stop();
    music_track = -1;
}

void audio_set(int music, int sound)
{
    music_on = music;
    sound_on = sound;
    ha_sound_on(sound);
    if (!music_on) music_stop();
}

/* ---- save states (main.c): the sound options and the tune (the SDK restarts it where it was) ---- */
void sfx_state(void)
{
    rs_state_var("sfx.music_on", &music_on, sizeof music_on);
    rs_state_var("sfx.sound_on", &sound_on, sizeof sound_on);
    rs_state_var("sfx.music_track", &music_track, sizeof music_track);
    ha_state();
}
