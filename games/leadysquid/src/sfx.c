/*
 * Leady Squid: sound effects synthesised at start-up, and the music.
 * All rights reserved, 8BCraft.
 */
#include "ls.h"
#include <string.h>

#define RATE 32000
#define MAXLEN (RATE / 2)

static int16_t buf[MAXLEN];
static uint32_t noise_s = 0x5eed1234u;
static int music_on = 1, sound_on = 1, music_started;

static int noise(void)
{
    noise_s ^= noise_s << 13;
    noise_s ^= noise_s >> 17;
    noise_s ^= noise_s << 5;
    return (int)(noise_s & 0xffff) - 32768;
}

/* integer sine, phase 0..65535 -> -16384..16384 (a parabola per half wave) */
static int isin(uint32_t ph)
{
    int p = (int)(ph & 0xffff);
    int x = p < 32768 ? p : p - 32768;
    int v = (int)((int64_t)x * (32768 - x) / 16384);
    return p < 32768 ? v : -v;
}

static void put(int i, int v)
{
    v += buf[i];
    buf[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

/* a sine sweep f0 -> f1 (Hz) from sample `at`, `ms` long, volume 0..100, exponential-ish decay */
static void tone(int at, int f0, int f1, int ms, int vol, int decay_ms)
{
    int n = ms * RATE / 1000;
    uint32_t ph = 0;
    for (int k = 0; k < n && at + k < MAXLEN; k++) {
        int f = f0 + (f1 - f0) * k / (n ? n : 1);
        ph += (uint32_t)(f * 65536 / RATE);
        int att = k < 96 ? k * 256 / 96 : 256;
        int dec = decay_ms ? 256 * decay_ms * RATE / 1000 / (decay_ms * RATE / 1000 + k) : 256;
        int rel = n - k < 400 ? (n - k) * 256 / 400 : 256;
        put(at + k, isin(ph) * vol / 100 * att / 256 * dec / 256 * rel / 256 * 2);
    }
}

static void hiss(int at, int ms, int vol, int lp_shift)
{
    int n = ms * RATE / 1000, lp = 0;
    for (int k = 0; k < n && at + k < MAXLEN; k++) {
        lp += (noise() - lp) >> lp_shift;
        int env = (n - k) * 256 / n;
        put(at + k, lp * vol / 100 * env / 256 * env / 256);
    }
}

static void store(int id, int ms)
{
    int n = ms * RATE / 1000;
    if (n > MAXLEN) n = MAXLEN;
    rs_sample_pcm16(id, buf, n, RATE, -1);
    memset(buf, 0, sizeof buf);
}

void sfx_init(void)
{
    memset(buf, 0, sizeof buf);
    /* bloop: a quick rising sine and a bubbly burble */
    tone(0, 220, 520, 90, 70, 60);
    tone(RATE * 30 / 1000, 700, 1100, 40, 18, 20);
    tone(RATE * 55 / 1000, 900, 1400, 35, 12, 15);
    hiss(0, 70, 10, 3);
    store(SFX_BLOOP, 130);
    /* ding: a soft bell, two partials */
    tone(0, 1568, 1568, 420, 42, 90);
    tone(0, 2349, 2349, 300, 18, 50);
    tone(0, 3951, 3951, 120, 6, 20);
    store(SFX_DING, 430);
    /* thud: a low falling sine and a little noise */
    tone(0, 120, 45, 240, 95, 120);
    hiss(0, 90, 40, 2);
    store(SFX_THUD, 250);
    /* clank: inharmonic metal partials, two hits */
    for (int hit = 0; hit < 2; hit++) {
        int at = hit * RATE * 110 / 1000, v = hit ? 55 : 80;
        tone(at, 820, 800, 160, v * 45 / 100, 40);
        tone(at, 1178, 1160, 140, v * 35 / 100, 30);
        tone(at, 1597, 1590, 110, v * 28 / 100, 25);
        tone(at, 2311, 2300, 80, v * 18 / 100, 15);
        hiss(at, 25, v / 2, 0);
    }
    store(SFX_CLANK, 300);
    /* swish: filtered noise for the panel */
    hiss(0, 180, 30, 4);
    store(SFX_SWISH, 180);
    /* sparkle: a quick high arpeggio */
    tone(0, 2093, 2093, 70, 25, 30);
    tone(RATE * 60 / 1000, 2637, 2637, 70, 25, 30);
    tone(RATE * 120 / 1000, 3136, 3136, 160, 25, 50);
    store(SFX_SPARKLE, 300);
    /* join: two soft notes up */
    tone(0, 523, 523, 110, 40, 60);
    tone(RATE * 100 / 1000, 784, 784, 200, 40, 80);
    store(SFX_JOIN, 300);
    /* pause: a short low blip */
    tone(0, 440, 330, 80, 40, 40);
    store(SFX_PAUSE, 90);
    rs_echo(110, 35, 30);
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
        rs_music_volume(56);
        music_started = 1;
    }
}

void audio_set(int music, int sound)
{
    music_on = music;
    sound_on = sound;
    if (!music_on) { rs_music_stop(); music_started = 0; }
}
