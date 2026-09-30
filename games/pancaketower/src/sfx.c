/*
 * Pancake Tower: the sounds (synthesised at start-up with the house synthesiser, games/common/src/house_audio.c)
 * and the music (three MODs of tools/make_music.py: the kitchen, the sky, space), crossfaded when the scenery
 * changes. The house UI sounds (confirm, pause, medal, game over) come from the kit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#include "pt.h"
#include "house_audio.h"
#include <string.h>

#define SFX_HOUSE SFX_COUNT         /* the house set follows the game's own sounds */
#define FADE 40                     /* frames of a music crossfade (out, then in) */

static int16_t snd_buf[HA_RATE / 2];
static int16_t half_buf[HA_RATE / 4];
static int sound_on = 1, music_on = 1;
static int tune = -1, want = -1, fade;   /* the music playing, the one wanted, the fade position */

/* semitones -> pitch (4.12): 2^(n/12) */
static const uint16_t SEMI[25] = {4096, 4340, 4598, 4871, 5161, 5468, 5793, 6137, 6502, 6889, 7298, 7732, 8192,
                                  8679, 9195, 9742, 10321, 10935, 11585, 12274, 13004, 13777, 14596, 15464, 16384};

/* store the buffer at 16 kHz (the game's sounds are soft and low: half the sample memory, the SPC700's budget) */
static void store_half(ha_synth *s, int id, int ms)
{
    int n = ha_ms(ms);
    if (n > s->len) n = s->len;
    for (int i = 0; i < n / 2; i++) half_buf[i] = (int16_t)((s->buf[2 * i] + s->buf[2 * i + 1]) / 2);
    rs_sample_pcm16(id, half_buf, n / 2, HA_RATE / 2, -1);
    memset(s->buf, 0, sizeof(int16_t) * (size_t)s->len);
}

void sfx_init(void)
{
    ha_synth s;
    ha_begin(&s, snd_buf, HA_RATE / 2, 0x5eed1234u);    /* the same sounds at every start (save states check them) */
    /* flop: a soft low thump and a short "fwup" of air */
    ha_tone(&s, 0, 150, 70, 130, 80, 70);
    ha_tone(&s, 0, 300, 180, 50, 25, 20);
    ha_hiss(&s, 0, 70, 22, 4);
    store_half(&s, SFX_FLOP, 150);
    /* ding: a round bell (C6 with a fifth and an octave partial) */
    ha_tone(&s, 0, 1047, 1047, 420, 44, 90);
    ha_tone(&s, 0, 1568, 1568, 260, 14, 50);
    ha_tone(&s, 0, 2093, 2093, 140, 8, 25);
    store_half(&s, SFX_DING, 430);
    /* syrup squish: a sticky falling gurgle */
    ha_tone(&s, 0, 620, 240, 90, 40, 40);
    ha_tone(&s, ha_ms(70), 420, 180, 110, 34, 50);
    ha_hiss(&s, 0, 150, 18, 3);
    store_half(&s, SFX_SQUISH, 190);
    /* splat: the piece cut off */
    ha_hiss(&s, 0, 110, 55, 2);
    ha_tone(&s, 0, 210, 90, 120, 60, 45);
    store_half(&s, SFX_SPLAT, 130);
    /* the ceiling crash: a big noise burst, a low boom, clattering bits */
    ha_hiss(&s, 0, 420, 90, 1);
    ha_tone(&s, 0, 95, 38, 380, 100, 160);
    for (int k = 0; k < 5; k++) ha_tone(&s, ha_ms(90 + k * 55), 900 + k * 230, 820 + k * 200, 40, 26, 15);
    store_half(&s, SFX_CRASH, 460);
    /* the roof: tiles breaking and clinking */
    ha_hiss(&s, 0, 220, 60, 2);
    ha_tone(&s, 0, 140, 70, 200, 60, 80);
    for (int k = 0; k < 4; k++) ha_tone(&s, ha_ms(40 + k * 50), 1900 + k * 260, 1850 + k * 250, 50, 28, 18);
    store_half(&s, SFX_ROOF, 300);
    /* plop: a topping lands */
    ha_tone(&s, 0, 420, 250, 70, 60, 35);
    ha_hiss(&s, 0, 40, 14, 3);
    store_half(&s, SFX_PLOP, 90);
    /* pour: glug, glug, glug */
    for (int k = 0; k < 3; k++) ha_tone(&s, ha_ms(k * 110), 520 - k * 60, 300 - k * 40, 90, 40, 40);
    ha_hiss(&s, 0, 330, 10, 4);
    store_half(&s, SFX_POUR, 360);
    /* splash: the rival's syrup flies in (a whoosh) and splats */
    ha_hiss(&s, 0, 160, 22, 4);
    ha_tone(&s, ha_ms(150), 380, 150, 100, 55, 40);
    ha_hiss(&s, ha_ms(150), 90, 40, 2);
    store_half(&s, SFX_SPLASH, 270);
    /* whoosh: the missed pancake falls (a slide whistle down) */
    ha_tone(&s, 0, 760, 150, 480, 45, 0);
    store_half(&s, SFX_WHOOSH, 490);
    /* fanfare: a topping, a regrow (a jazzy major sixth arpeggio) */
    ha_tone(&s, 0, 523, 523, 90, 34, 50);
    ha_tone(&s, ha_ms(70), 659, 659, 90, 34, 50);
    ha_tone(&s, ha_ms(140), 784, 784, 90, 34, 50);
    ha_tone(&s, ha_ms(210), 880, 880, 240, 34, 90);
    store_half(&s, SFX_FANFARE, 460);
    ha_init(SFX_HOUSE);
    ha_sound_on(sound_on);
}

/* a sound panned by screen x, pitched by semitones (0..24); echo on the wet ones (the ding, the syrup) */
void sfx_play(int id, int x, int pitch_steps)
{
    static const uint8_t vol[SFX_COUNT] = {HA_VOL_ACTION, HA_VOL_FEEDBACK, HA_VOL_FEEDBACK, HA_VOL_ACTION,
                                           HA_VOL_CRASH, HA_VOL_CRASH - 10, HA_VOL_FEEDBACK, 60, HA_VOL_ACTION,
                                           HA_VOL_ACTION, HA_VOL_FEEDBACK};
    if (!sound_on || id < 0 || id >= SFX_COUNT) return;
    int pan = clampi(24 + x * 80 / RS_SCREEN_W, 0, 127);
    int v = rs_sfx(id, vol[id], pan, SEMI[clampi(pitch_steps, 0, 24)]);
    if (v >= 0 && (id == SFX_DING || id == SFX_SQUISH || id == SFX_POUR)) rs_voice_echo(v, 1);
}

void sfx(int id) { sfx_play(id, RS_SCREEN_W / 2, 0); }

/* the music of a segment: the kitchen, the sky (and the clouds), space (and the stratosphere) */
void music_update(int segment, int playing)
{
    static const char *const names[3] = {"music/kitchen.mod", "music/sky.mod", "music/space.mod"};
    if (!music_on) {
        if (tune >= 0) rs_music_stop();
        tune = -1;
        return;
    }
    int w = segment <= SEG_KITCHEN ? 0 : segment <= SEG_CLOUDS ? 1 : 2;
    (void)playing;
    if (tune < 0 || !rs_music_playing()) {         /* the first start: at once */
        size_t n;
        const void *m = rs_asset(names[w], &n);
        if (m && !rs_music_play(m, n, 1)) {
            tune = want = w;
            fade = 0;
            rs_music_volume(HA_MUSIC_VOL);
        }
        return;
    }
    if (w != want) { want = w; }
    if (want != tune) {                            /* fade out, switch, fade in */
        if (fade < FADE) fade++;
        rs_music_volume(HA_MUSIC_VOL * (FADE - fade) / FADE);
        if (fade >= FADE) {
            size_t n;
            const void *m = rs_asset(names[want], &n);
            if (m && !rs_music_play(m, n, 1)) tune = want;
            fade = -FADE;
        }
    } else if (fade < 0) {
        fade++;
        rs_music_volume(HA_MUSIC_VOL * (FADE + fade) / FADE);
    } else if (fade > 0) {                         /* came back to the same tune mid-fade */
        fade--;
        rs_music_volume(HA_MUSIC_VOL * (FADE - fade) / FADE);
    }
}

void audio_set(int music, int sound)
{
    music_on = music;
    sound_on = sound;
    ha_sound_on(sound);
    if (!music_on) { rs_music_stop(); tune = -1; }
}

/* ---- save states (main.c): the options and the crossfade (the SDK restarts the tune where it was) ---- */
void sfx_state(void)
{
    rs_state_var("sfx.sound_on", &sound_on, sizeof sound_on);
    rs_state_var("sfx.music_on", &music_on, sizeof music_on);
    rs_state_var("sfx.tune", &tune, sizeof tune);
    rs_state_var("sfx.want", &want, sizeof want);
    rs_state_var("sfx.fade", &fade, sizeof fade);
    ha_state();
}
