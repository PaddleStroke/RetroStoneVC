/*
 * Bomber Mole: placeholder sound effects, synthesised at start-up, and music.
 * All rights reserved, 8BCraft.
 */
#include "bm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 32000
#define MAXLEN (RATE * 3 / 4)

static int16_t buf[MAXLEN];
static uint32_t noise_s = 0x1234567u;
static int music_on = 1, sfx_on = 1;
static char current[24];

static int noise(void)
{
    noise_s ^= noise_s << 13;
    noise_s ^= noise_s >> 17;
    noise_s ^= noise_s << 5;
    return (int)(noise_s & 0xffff) - 32768;
}

/* integer sine approximation (a parabola per half wave), phase 0..65535 */
static int wave(int kind, uint32_t ph)
{
    int p = (int)(ph & 0xffff);
    switch (kind) {
    case 0: return p < 32768 ? 20000 : -20000;                       /* square */
    case 1: return p < 16384 ? 12000 : -12000;                       /* thin square */
    case 2: return p < 32768 ? (p - 16384) * 2 * 20000 / 32768 : (49152 - p) * 2 * 20000 / 32768; /* triangle */
    default: {
        int x = p < 32768 ? p : p - 32768;
        int v = (int)((int64_t)x * (32768 - x) / 16384) * 20000 / 16384;
        return p < 32768 ? v : -v;
    }
    }
}

typedef struct tone { int kind, f0, f1, ms, vol; } tone;

/* a sequence of tones (frequency sweeps) with a short release, plus optional noise */
static int synth(const tone *t, int n, int noise_ms, int noise_vol, int noise_lp)
{
    int len = 0;
    uint32_t ph = 0;
    memset(buf, 0, sizeof buf);
    for (int i = 0; i < n; i++) {
        int samples = t[i].ms * RATE / 1000;
        for (int k = 0; k < samples && len < MAXLEN; k++, len++) {
            int f = t[i].f0 + (t[i].f1 - t[i].f0) * k / (samples ? samples : 1);
            ph += (uint32_t)(f * 65536 / RATE);
            int env = k < 64 ? k * 256 / 64 : samples - k < 256 ? (samples - k) : 256;
            buf[len] = (int16_t)(wave(t[i].kind, ph) * t[i].vol / 100 * env / 256);
        }
    }
    if (noise_ms) {
        int samples = noise_ms * RATE / 1000, lp = 0;
        if (samples > MAXLEN) samples = MAXLEN;
        for (int k = 0; k < samples; k++) {
            int v = noise();
            lp += (v - lp) >> noise_lp;                               /* one-pole low-pass */
            int env = (samples - k) * 256 / samples;
            env = env * env / 256;
            int s = buf[k] + lp * noise_vol / 100 * env / 256;
            buf[k] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
        }
        if (samples > len) len = samples;
    }
    return len;
}

static void make(int id, const tone *t, int n, int nms, int nvol, int nlp)
{
    int len = synth(t, n, nms, nvol, nlp);
    rs_sample_pcm16(id, buf, len, RATE, -1);
}

void sfx_init(void)
{
    static const tone drop[] = {{3, 140, 60, 90, 90}};
    static const tone fuse[] = {{1, 2400, 2000, 20, 30}};
    static const tone blast[] = {{3, 70, 35, 450, 80}};
    static const tone brk[] = {{0, 180, 90, 60, 30}};
    static const tone dig[] = {{2, 300, 200, 30, 20}};
    static const tone grub[] = {{1, 1047, 1047, 50, 50}, {1, 1319, 1319, 50, 50}, {1, 1568, 1568, 50, 50}, {1, 2093, 2093, 90, 50}};
    static const tone pow[] = {{0, 400, 1400, 220, 45}};
    static const tone hurt[] = {{0, 700, 200, 220, 55}};
    static const tone ko[] = {{2, 900, 700, 150, 80}, {2, 700, 500, 150, 80}, {2, 500, 120, 400, 80}};
    static const tone exit[] = {{1, 523, 523, 120, 50}, {1, 659, 659, 120, 50}, {1, 784, 784, 120, 50}, {1, 1047, 1047, 300, 50}};
    static const tone depth[] = {{3, 200, 500, 200, 25}, {3, 500, 150, 200, 25}};
    static const tone edown[] = {{0, 1000, 250, 160, 45}};
    static const tone pounce[] = {{1, 500, 850, 120, 40}, {1, 850, 400, 150, 40}};
    static const tone bhit[] = {{0, 220, 90, 300, 70}};
    static const tone mmove[] = {{1, 1000, 1000, 30, 35}};
    static const tone mok[] = {{1, 800, 800, 60, 45}, {1, 1200, 1200, 90, 45}};
    static const tone splash[] = {{3, 600, 200, 80, 20}};
    static const tone fizz[] = {{1, 3000, 2500, 20, 10}};
    static const tone sw[] = {{0, 1500, 1500, 15, 40}, {0, 900, 900, 25, 40}};
    static const tone steam[] = {{3, 90, 90, 100, 15}};
    static const tone woof[] = {{0, 320, 200, 90, 55}, {0, 0, 0, 60, 0}, {0, 320, 200, 90, 55}};
    static const tone splat[] = {{3, 200, 80, 120, 60}};
#define M(id, t, nms, nvol, nlp) make(id, t, sizeof t / sizeof t[0], nms, nvol, nlp)
    M(SFX_BOMB_DROP, drop, 40, 20, 3);
    M(SFX_FUSE, fuse, 25, 25, 0);
    M(SFX_BLAST, blast, 600, 110, 2);
    M(SFX_BREAK, brk, 180, 70, 1);
    M(SFX_DIG, dig, 70, 45, 1);
    M(SFX_GRUB, grub, 0, 0, 0);
    M(SFX_POWERUP, pow, 0, 0, 0);
    M(SFX_HURT, hurt, 60, 30, 1);
    M(SFX_KO, ko, 0, 0, 0);
    M(SFX_EXIT_OPEN, exit, 0, 0, 0);
    M(SFX_DEPTH, depth, 400, 50, 4);
    M(SFX_ENEMY_DOWN, edown, 120, 40, 1);
    M(SFX_POUNCE, pounce, 0, 0, 0);
    M(SFX_BOSS_HIT, bhit, 200, 60, 2);
    M(SFX_MENU_MOVE, mmove, 0, 0, 0);
    M(SFX_MENU_OK, mok, 0, 0, 0);
    M(SFX_SPLASH, splash, 260, 60, 0);
    M(SFX_FIZZLE, fizz, 350, 35, 0);
    M(SFX_SWITCH, sw, 0, 0, 0);
    M(SFX_STEAM, steam, 650, 45, 1);
    M(SFX_WOOF, woof, 50, 30, 2);
    M(SFX_SPLAT, splat, 150, 60, 2);
#undef M
    rs_echo(90, 30, 25);
}

void sfx_at(int id, int x)
{
    if (!sfx_on) return;
    int pan = clampi(20 + x * 88 / (GW * CELL), 0, 127);
    int v = rs_sfx(id, 110, pan, RS_PITCH_1);
    if (id == SFX_BLAST && v >= 0) rs_voice_echo(v, 1);
}

void sfx(int id) { sfx_at(id, GW * CELL / 2); }

void music_play(const char *name)
{
    static const char *const ext[] = {".xm", ".it", ".s3m", ".mod"};
    if (!music_on) { current[0] = 0; rs_music_stop(); return; }
    if (!strcmp(name, current) && rs_music_playing()) return;
    for (unsigned i = 0; i < 4; i++) {
        char path[64];
        size_t n;
        snprintf(path, sizeof path, "music/%s%s", name, ext[i]);
        const void *m = rs_asset(path, &n);
        if (m && !rs_music_play(m, n, 1)) {
            snprintf(current, sizeof current, "%s", name);
            rs_music_volume(80);
            return;
        }
    }
    rs_log("music %s not found", name);
}

void audio_options(int m, int s)
{
    music_on = m;
    sfx_on = s;
    if (!m) { rs_music_stop(); current[0] = 0; }
}
