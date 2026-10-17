/*
 * Play by Ear in RetroStone Board, for one player: a piano keyboard in a walnut case, ivory and ebony keys. A tune
 * plays, each key lighting up in gold as its note sounds; then the player plays its notes back in order (the rhythm
 * does not count). A right key sounds and glows; a wrong one sounds dull and flashes red, a mistake, and the player
 * goes on from the same place. The tools: listen again (the tune once more) and the lamp (the next key lit). Above
 * the keyboard a staff: a dot for each note still to find, each note found written on it at its pitch. Solved: a
 * wave of gold over the keys, then the shell's stars card, the tune's title (and its composer) above it.
 * The levels: 150 in six chapters (melody_levels.h, from public-domain tunes only: tools/melody_tunes.txt); the die
 * tab: a phrase of the library at random (short, medium, long), often in another key. Stars: the logic puzzles' rule
 * on (hints + listens past the first, mistakes). The keys light up whatever the sound: with the sound off (a small
 * crossed-out speaker on the staff), the game is one of the eyes.
 * The piano: melody_synth.c renders each note once, played here through an audio device of the game's own (the
 * shell's sounds are its built-in ones only): a small mixer, each note released as the next one starts.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/melody.h"
#include "games/puzzles.h"
#include "shell.h"
#include "text.h"

enum { T_LISTEN, T_HINT, TOOLS };

#define START_MS 450                            /* the tune begins this long after a level shows */
#define FLASH_MS 420                            /* a key played: its light */
#define WRONG_MS 520
#define HINT_MS 1500
#define WAVE_STEP 45
#define WAVE_LEN 520
#define PRESS_IN 45                             /* a key played: it goes down this fast, */
#define PRESS_OUT 150                           /* and comes back up after its note this slowly */

static mel_game G;

/* ---- the sound: the game's own little mixer -------------------------------------------------------------------- */
#define VOICES 72
#define FADE (MEL_RATE / 9)                     /* a note released: it fades in this many samples */
static SDL_AudioDeviceID adev;
static int audio;                               /* 0 not tried, 1 open, -1 failed, -2 none (the picture mode) */
static int16_t *pcm[2][128];
static int pcm_n[2][128];
static struct { const int16_t *s; int n, pos, rel; float gain; } V[VOICES];

static void mix(void *u, Uint8 *stream, int bytes)
{
    (void)u;
    int16_t *out = (int16_t *)stream;
    int n = bytes / 2, quiet = snd_muted;
    for (int i = 0; i < n; i++) {
        float v = 0;
        for (int k = 0; k < VOICES; k++) {
            if (!V[k].s) continue;
            if (V[k].pos < 0) { V[k].pos++; continue; }
            float a = V[k].s[V[k].pos] * V[k].gain;
            if (V[k].rel >= 0 && V[k].pos >= V[k].rel) {
                float f = 1 - (float)(V[k].pos - V[k].rel) / FADE;
                if (f <= 0) { V[k].s = NULL; continue; }
                a *= f * f;
            }
            v += a;
            if (++V[k].pos >= V[k].n) V[k].s = NULL;
        }
        v = quiet ? 0 : v > 32000 ? 32000 : v < -32000 ? -32000 : v;
        out[i] = (int16_t)v;
    }
}

static int audio_on(void)
{
    if (audio) return audio > 0;
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {         /* the picture mode starts no sound */
        audio = -2;
        return 0;
    }
    SDL_AudioSpec want = {0}, have;
    want.freq = MEL_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = mix;
    adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    audio = adev ? 1 : -1;
    if (adev) SDL_PauseAudioDevice(adev, 0);
    return audio > 0;
}

static const int16_t *note_pcm(int pitch, int dull, int *n)
{
    pitch &= 127;
    if (!pcm[dull][pitch]) {
        int len = mel_synth_len(pitch, dull);
        int16_t *b = malloc((size_t)len * sizeof *b);
        if (!b) return NULL;
        mel_synth(pitch, dull, b, len);
        pcm_n[dull][pitch] = len;
        pcm[dull][pitch] = b;
    }
    *n = pcm_n[dull][pitch];
    return pcm[dull][pitch];
}

static int sound_ready(void) { return !snd_muted && audio_on(); }

/* what sounds stops: the notes waiting dropped, the ones ringing released */
static void hush_locked(void)
{
    for (int k = 0; k < VOICES; k++) {
        if (!V[k].s) continue;
        if (V[k].pos < 0) V[k].s = NULL;
        else if (V[k].rel < 0 || V[k].rel > V[k].pos) V[k].rel = V[k].pos;
    }
}

static void voice_add(const int16_t *s, int n, int delay, int rel, float gain)
{
    int k = 0;
    for (int i = 0; i < VOICES; i++)
        if (!V[i].s) { k = i; break; } else if (V[i].pos > V[k].pos) k = i;
    V[k].s = s;
    V[k].n = n;
    V[k].pos = -delay;
    V[k].rel = rel;
    V[k].gain = gain;
}

static void hush(void)
{
    if (audio <= 0) return;
    SDL_LockAudioDevice(adev);
    hush_locked();
    SDL_UnlockAudioDevice(adev);
}

static void sound_note(int pitch, int dull)
{
    if (!sound_ready()) return;
    int n;
    const int16_t *s = note_pcm(pitch, dull, &n);
    if (!s) return;
    SDL_LockAudioDevice(adev);
    hush_locked();
    voice_add(s, n, 0, -1, 1);
    SDL_UnlockAudioDevice(adev);
}

/* the whole tune, from delay_ms on: each note released a moment after the next one starts */
static void sound_tune(int delay_ms)
{
    if (!sound_ready()) return;
    const int16_t *s[MEL_MAX];
    int len[MEL_MAX];
    for (int i = 0; i < G.n; i++)
        if (!(s[i] = note_pcm(G.p[i], 0, &len[i]))) return;
    SDL_LockAudioDevice(adev);
    hush_locked();
    for (int i = 0; i < G.n; i++) {
        int at = (delay_ms + mel_ms(&G, i)) * (MEL_RATE / 1000);
        int rel = i + 1 < G.n ? (mel_len_ms(&G, i) + 40) * (MEL_RATE / 1000) : -1;
        voice_add(s[i], len[i], at, rel, 0.9f);
    }
    SDL_UnlockAudioDevice(adev);
}

static int silent(void) { return snd_muted || audio == -1; }

/* ---- the state on the screen ------------------------------------------------------------------------------------ */
static struct { int on; Uint32 t0; } play;      /* the tune playing (its lights): from t0 */
static struct { int pitch, wrong; Uint32 t0; } flash[4];   /* the last keys played */
static int nflash;
static struct { int pitch; Uint32 t0; } hint = {-1, 0};
static struct { int on; Uint32 t0; } win;
static struct { int on, pitch, tool; } ptr;
static int play_ms, play_lit;                   /* the tune's note sounding: how long since it began, its light's length */

static void calm(void)
{
    play.on = 0;
    nflash = 0;
    hint.pitch = -1;
    win.on = 0;
    memset(&ptr, 0, sizeof ptr);
    hush();
}

static void listen(Uint32 delay)
{
    play.on = 1;
    play.t0 = ui_ms() + delay;
    sound_tune((int)delay);
    ui_animate();
}

static Uint32 play_end(void) { return play.t0 + (Uint32)(mel_ms(&G, G.n - 1) + mel_len_ms(&G, G.n - 1)); }

/* the note of the tune sounding now (-1: none), and how far into its light */
static int playing(Uint32 now, float *glow)
{
    if (!play.on || now < play.t0) return -1;
    int t = (int)(now - play.t0);
    for (int i = G.n - 1; i >= 0; i--) {
        int s = mel_ms(&G, i);
        if (t < s) continue;
        int lit = (int)clampf(0.85f * mel_len_ms(&G, i), 110, 650);
        if (t >= s + lit) return -1;
        play_ms = t - s;
        play_lit = lit;
        *glow = 1 - 0.35f * (t - s) / (float)lit;
        return i;
    }
    return -1;
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static box room;
static int W;                                   /* white keys shown */
static float kx, ky, kw, kh, bw, bh, lip, fr;   /* the keys' top left, a white key, a black one, the case's parts */
static float sx, sy, sw, sh, sfr;               /* the staff's plank (its wood inside its frame) */
static float lg, row_h, col_w;                  /* the staff: its lines' gap, a row's height, a note's column */
static int per_row, rows;
static int made_kw = -1, made_kh = -1, made_w = -1, made_s = -1;
static SDL_Texture *t_white[4], *t_black[4], *t_case, *t_staff, *t_head[2], *t_dot, *t_badge, *t_note_small,
                   *t_plate;
static int plate_w, plate_h;

static int white_of(int k) { static const int w[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6}; return w[k]; }

/* a key's rectangle on the screen */
static void key_rect(int pitch, float *x, float *y, float *w, float *h)
{
    int o = (pitch - MEL_LOW) / 12, k = (pitch - MEL_LOW) % 12, wi = o * 7 + white_of(k);
    if (!mel_black(pitch)) {
        *x = kx + wi * kw;
        *y = ky;
        *w = kw;
        *h = kh;
        return;
    }
    static const float off[12] = {0, -0.1f, 0, 0.1f, 0, 0, -0.12f, 0, 0, 0, 0.12f, 0};
    *x = kx + (wi + 1 + off[k]) * kw - bw / 2;
    *y = ky;
    *w = bw;
    *h = bh;
}

static int key_at(float x, float y)
{
    int top = mel_top(W);
    for (int p = MEL_LOW; p <= top; p++) {     /* the black keys first: they stand over the white ones */
        if (!mel_black(p)) continue;
        float rx, ry, rw, rh;
        key_rect(p, &rx, &ry, &rw, &rh);
        if (x >= rx - 0.04f * kw && x < rx + rw + 0.04f * kw && y >= ry && y < ry + rh) return p;
    }
    for (int p = MEL_LOW; p <= top; p++) {
        if (mel_black(p)) continue;
        float rx, ry, rw, rh;
        key_rect(p, &rx, &ry, &rw, &rh);
        if (x >= rx && x < rx + rw && y >= ry - lip && y < ry + rh + fr) return p;
    }
    return -1;
}

/* ---- the pictures --------------------------------------------------------------------------------------------- */
static const uint8_t IVORY[3] = {246, 238, 218}, IVORY_GOLD[3] = {252, 214, 122}, EBONY[3] = {30, 26, 26},
                     EBONY_GOLD[3] = {196, 146, 54}, GOLD[3] = {255, 204, 96}, INK[3] = {64, 42, 28};

static uint8_t px8(float v) { return (uint8_t)clampf(v, 0, 255); }

/* the corners at the bottom rounded: the pixel's alpha */
static float round_bottom(int x, int y, int w, int h, float r)
{
    float cx = x + 0.5f < r ? r : x + 0.5f > w - r ? w - r : x + 0.5f, cy = h - r;
    if (y + 0.5f < cy || (x + 0.5f >= r && x + 0.5f <= w - r)) return 1;
    return clampf(r + 0.5f - hypotf(x + 0.5f - cx, y + 0.5f - cy), 0, 1);
}

/* an ivory key, w x h: shaded under the case at its back, its front edge (the lip) showing below, its sides; down
 * (pressed): its front edge shorter, a little darker; gold: lit */
static void paint_white(int w, int h, int down, int gold, uint8_t *o)
{
    const uint8_t *c = gold ? IVORY_GOLD : IVORY;
    float lipf = down ? 0.02f : 0.05f, r = 0.12f * w;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float t = (y + 0.5f) / h, k = 0.9f + 0.1f * t;
            k *= 0.78f + 0.22f * clampf(t / 0.05f, 0, 1);   /* the shadow of the case's lip */
            if (t > 1 - lipf) k *= 0.8f - 0.25f * (t - (1 - lipf)) / lipf;   /* the front face */
            if (x < 1 || x >= w - 1) k *= 0.82f;
            else if (x < 0.1f * w) k *= 1.02f;
            if (down) k *= 0.92f;
            float g = 3 * art_noise(x, y * 3);
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = px8(c[i] * k + g);
            p[3] = (uint8_t)(255 * round_bottom(x, y, w, h, r));
        }
}

/* an ebony key: its top face darker at the back, a sheen towards the front, bevelled sides, its front face lighter */
static void paint_black(int w, int h, int down, int gold, uint8_t *o)
{
    const uint8_t *c = gold ? EBONY_GOLD : EBONY;
    float lipf = down ? 0.05f : 0.1f, r = 0.1f * w, side = fmaxf(1, 0.13f * w);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float t = (y + 0.5f) / h, face = 1 - lipf, k = 0.85f + 0.45f * t, add = 0;
            if (t > face) {                     /* the front face, sloping */
                k = 1.9f - 0.6f * (t - face) / lipf;
                add = gold ? 10 : 22;
            } else if (t > 0.72f * face && t < 0.86f * face) {
                add = (gold ? 16 : 9) * sinf(ART_PI * (t / face - 0.72f) / 0.14f);   /* the sheen */
            }
            if (x < side) { k *= 1.35f; add += 6; }
            else if (x >= w - side) k *= 0.75f;
            if (down) k *= 0.9f;
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = px8(c[i] * k + add + 2 * art_noise(x * 2, y));
            p[3] = (uint8_t)(255 * round_bottom(x, y, w, h, r));
        }
}

/* the case: walnut, the fallboard's edge above the keys with a strip of red felt, the keys' bed (dark) */
static void paint_case(int iw, int ih, int f, int lp, uint8_t *o)
{
    int w = iw + 2 * f, h = ih + 2 * f;
    art_plank(iw, ih, f, o);
    for (int y = f; y < f + ih; y++)
        for (int x = f; x < f + iw; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            int yy = y - f;
            if (yy < lp - lp / 5) {             /* the fallboard's edge: dark walnut, lit along its top */
                float k = 0.42f + 0.18f * (yy < 2 ? 1 : 0) - 0.1f * yy / lp;
                for (int i = 0; i < 3; i++) p[i] = px8(p[i] * k);
            } else if (yy < lp) {               /* the felt */
                float n = 6 * art_noise(x * 3, y * 3);
                p[0] = px8(122 + n);
                p[1] = px8(26 + n / 3);
                p[2] = px8(30 + n / 3);
            } else {                            /* the bed under the keys */
                p[0] = 22;
                p[1] = 16;
                p[2] = 12;
            }
        }
    (void)h;
}

/* a note's head, an oval tilted, ebony or gold, d across */
static void paint_head(int d, int gold, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    art_fig *f = art_fig_new();
    float xy[48];
    for (int i = 0; i < 24; i++) {
        float a = 2 * ART_PI * i / 24, ex = 0.46f * cosf(a), ey = 0.31f * sinf(a), r = -0.36f;
        xy[2 * i] = 0.5f + ex * cosf(r) - ey * sinf(r);
        xy[2 * i + 1] = 0.5f + ex * sinf(r) + ey * cosf(r);
    }
    art_poly(f, ART_PART, xy, 24);
    art_fig_paint(o, d, f, gold ? GOLD : (const uint8_t[3]){34, 28, 26}, gold ? (const uint8_t[3]){150, 96, 30}
                                                                             : (const uint8_t[3]){12, 10, 10},
                  0.05f, 0.5f);
}

/* an eighth note (its head, its stem, its flag) painted into a picture w x h, in the square (x, y, s) */
static void paint_glyph(uint8_t *o, int w, int h, float x, float y, float s, const uint8_t rgb[3], const uint8_t ink[3])
{
    int q = (int)s;
    if (q < 4) return;
    uint8_t *b = calloc((size_t)q * q, 4);
    if (!b) return;
    art_fig *f = art_fig_new();
    float xy[48];
    for (int i = 0; i < 24; i++) {
        float a = 2 * ART_PI * i / 24, ex = 0.2f * cosf(a), ey = 0.135f * sinf(a), r = -0.38f;
        xy[2 * i] = 0.36f + ex * cosf(r) - ey * sinf(r);
        xy[2 * i + 1] = 0.8f + ex * sinf(r) + ey * cosf(r);
    }
    art_poly(f, ART_PART, xy, 24);
    art_rect(f, ART_PART, 0.505f, 0.1f, 0.55f, 0.8f);
    const float flag[] = {0.505f, 0.08f, 0.55f, 0.08f, 0.59f, 0.16f, 0.66f, 0.23f, 0.74f, 0.31f, 0.79f, 0.41f,
                          0.79f, 0.52f, 0.74f, 0.64f, 0.72f, 0.62f, 0.745f, 0.52f, 0.73f, 0.43f, 0.67f, 0.36f,
                          0.59f, 0.31f, 0.505f, 0.28f};
    art_poly(f, ART_PART, flag, 14);
    art_fig_paint(b, q, f, rgb, ink, 0.03f, 0.4f);
    art_blend(o, w, h, b, q, q, (int)x, (int)y);
    free(b);
}

/* a little keyboard painted into a picture: its case at (x, y), w wide, n white keys from C, the keys lit (bits:
 * pitches from C4) */
static void paint_keyboard(uint8_t *o, int pw, int ph, int x, int y, int w, int n, uint32_t lit)
{
    int q = (w - 2) / n, f = q > 12 ? q / 6 : 2, khh = (int)(3.6f * q), lp = q / 2 + 1;
    if (q < 3) return;
    int iw = n * q, ih = lp + khh, cw = iw + 2 * f, ch = ih + 2 * f;
    uint8_t *c = calloc((size_t)cw * ch, 4), *k = calloc((size_t)q * khh, 4);
    int bq = (int)(0.6f * q), bhh = (int)(0.62f * khh);
    uint8_t *b = calloc((size_t)(bq > 0 ? bq : 1) * (bhh > 0 ? bhh : 1), 4);
    if (c && k && b && bq > 1) {
        paint_case(iw, ih, f, lp, c);
        for (int i = 0; i < n; i++) {
            int pitch = MEL_LOW + i / 7 * 12 + (const int[7]){0, 2, 4, 5, 7, 9, 11}[i % 7];
            paint_white(q - 1, khh, 0, lit >> (pitch - MEL_LOW) & 1, k);
            for (int yy = 0; yy < khh; yy++)
                memcpy(c + 4 * ((size_t)(f + lp + yy) * cw + f + i * q), k + 4 * (size_t)yy * (q - 1), (size_t)(q - 1) * 4);
        }
        for (int i = 0; i + 1 < n; i++) {
            int m = i % 7;
            if (m == 2 || m == 6) continue;
            int pitch = MEL_LOW + i / 7 * 12 + (const int[7]){1, 3, 0, 6, 8, 10, 0}[m];
            paint_black(bq, bhh, 0, lit >> (pitch - MEL_LOW) & 1, b);
            art_blend(c, cw, ch, b, bq, bhh, f + (i + 1) * q - bq / 2, f + lp);
        }
        art_blend(o, pw, ph, c, cw, ch, x, y);
    }
    free(c);
    free(k);
    free(b);
}

static void paint_badge(int d, uint8_t *o)   /* sound off: a crossed-out speaker on a dark disc */
{
    art_disc(d, (const uint8_t[3]){28, 22, 18}, 0.85f, 0, o);
    int q = (int)(0.66f * d);
    uint8_t *ic = calloc((size_t)q * q, 4);
    if (!ic) return;
    art_icon(ICON_MUTE, q, ic);
    for (int i = 0; i < q * q; i++) {
        ic[4 * i] = 246;
        ic[4 * i + 1] = 238;
        ic[4 * i + 2] = 220;
    }
    art_blend(o, d, d, ic, q, q, (d - q) / 2, (d - q) / 2);
    free(ic);
}

static void token(int player, int size, uint8_t *rgba)   /* an ivory disc, a note on it */
{
    (void)player;
    art_counter(COUNTER_DISC, 0, size, rgba);
    paint_glyph(rgba, size, size, 0.2f * size, 0.18f * size, 0.62f * size, (const uint8_t[3]){34, 28, 26},
                (const uint8_t[3]){12, 10, 10});
}

static Uint32 tex_sig(int a, int b, int c) { return (Uint32)(a * 4099 + b * 131 + c); }

static void make_textures(void)
{
    int w = (int)kw - 1, h = (int)kh, b = (int)bw, bhh = (int)bh;
    if (made_kw != w || made_kh != h) {
        made_kw = w;
        made_kh = h;
        for (int s = 0; s < 4; s++) {
            ui_free(&t_white[s]);
            ui_free(&t_black[s]);
            paint_white(w, h, s & 1, s >> 1, ui_buf(w, h));
            t_white[s] = ui_tex(w, h, ui_buf(w, h));
            paint_black(b, bhh, s & 1, s >> 1, ui_buf(b, bhh));
            t_black[s] = ui_tex(b, bhh, ui_buf(b, bhh));
        }
        int d = (int)fmaxf(4, 0.62f * ui.unit);
        ui_free(&t_note_small);
        memset(ui_buf(d, d), 0, (size_t)d * d * 4);
        paint_glyph(ui_buf(d, d), d, d, 0, 0, (float)d, PUZ_CREAM, (const uint8_t[3]){120, 110, 96});
        t_note_small = ui_tex(d, d, ui_buf(d, d));
    }
    int iw = W * (int)kw, ih = (int)lip + h, f = (int)fr;
    if ((int)tex_sig(iw, ih, f) != made_w) {
        made_w = (int)tex_sig(iw, ih, f);
        ui_free(&t_case);
        paint_case(iw, ih, f, (int)lip, ui_buf(iw + 2 * f, ih + 2 * f));
        t_case = ui_tex(iw + 2 * f, ih + 2 * f, ui_buf(iw + 2 * f, ih + 2 * f));
    }
    int ssig = (int)tex_sig((int)sw, (int)sh, (int)(lg * 8));
    if (ssig != made_s) {
        made_s = ssig;
        ui_free(&t_staff);
        int f2 = (int)sfr;
        art_plank((int)sw, (int)sh, f2, ui_buf((int)sw + 2 * f2, (int)sh + 2 * f2));
        t_staff = ui_tex((int)sw + 2 * f2, (int)sh + 2 * f2, ui_buf((int)sw + 2 * f2, (int)sh + 2 * f2));
        int d = (int)fmaxf(4, 1.35f * lg);
        for (int g = 0; g < 2; g++) {
            ui_free(&t_head[g]);
            paint_head(d, g, ui_buf(d, d));
            t_head[g] = ui_tex(d, d, ui_buf(d, d));
        }
        int dd = (int)fmaxf(3, 0.7f * lg);
        ui_free(&t_dot);
        art_disc(dd, (const uint8_t[3]){70, 48, 32}, 0.75f, 0, ui_buf(dd, dd));
        t_dot = ui_tex(dd, dd, ui_buf(dd, dd));
        int bd = (int)fmaxf(8, 0.62f * ui.unit);
        ui_free(&t_badge);
        paint_badge(bd, ui_buf(bd, bd));
        t_badge = ui_tex(bd, bd, ui_buf(bd, bd));
    }
}

/* the staff's rows: the fewest that hold the notes with room for each head */
static void fit_staff(void)
{
    for (rows = 1; rows <= 4; rows++) {
        lg = fminf(fminf(sh / (rows * 9.5f), 0.5f * kw), 0.05f * fminf(ui.w, ui.h));
        per_row = (int)((sw - 2 * lg) / (2.0f * lg));
        if (rows * per_row >= G.n) break;
    }
    if (rows > 4) rows = 4;
    per_row = (G.n + rows - 1) / rows;
    col_w = fminf(2.6f * lg, (sw - 2 * lg) / (per_row > 0 ? per_row : 1));
    row_h = sh / rows;
}

static void layout(box b)
{
    room = b;
    W = G.whites ? G.whites : 15;
    float S = fminf(b.w, b.h), m = floorf(0.025f * S);
    int portrait = ui.w <= ui.h;
    fr = floorf(fmaxf(3, 0.022f * S));
    kw = floorf(fminf((b.w - 2 * m - 2 * fr) / W, 0.16f * fminf(ui.w, ui.h)));
    kh = floorf(fminf(portrait ? 8.0f * kw : 5.2f * kw, (portrait ? 0.5f : 0.56f) * b.h));
    lip = floorf(fmaxf(4, 0.3f * kw));
    bw = floorf(0.58f * kw);
    bh = floorf(0.62f * kh);
    float cw = W * kw + 2 * fr, ch = lip + kh + 2 * fr;
    float cx = floorf(b.x + (b.w - cw) / 2), cy = floorf(b.y + b.h - m - ch);
    kx = cx + fr;
    ky = cy + fr + lip;
    ui_grid_fit((box){cx, cy, cw, ch}, 1, 1, 0);   /* the board for the shell: its stars card goes over it */
    sfr = floorf(fmaxf(3, 0.016f * S));
    float avail = cy - b.y - 2 * m;
    sw = fminf(cw, b.w - 2 * m) - 2 * sfr;
    sh = floorf(fminf(avail - 2 * sfr, 0.8f * cw));
    if (sh < 0.3f * ui.unit) sh = 0.3f * ui.unit;
    sx = floorf(b.x + (b.w - sw) / 2);
    sy = floorf(b.y + m + sfr + fmaxf(0, (avail - sh - 2 * sfr) / 2));
    fit_staff();
    float need = floorf(rows * 10.5f * lg);     /* no more wood than the staff's rows want */
    if (sh > need) {
        sy = floorf(sy + (sh - need) / 2);
        sh = need;
        row_h = sh / rows;
    }
    make_textures();
}

static void relayout(void)
{
    if (room.w > 0) layout(room);
}

/* ---- the staff --------------------------------------------------------------------------------------------------- */
/* a pitch's place on the staff: steps above its bottom line (E4) */
static int staff_step(int pitch)
{
    int o = pitch / 12 - 5, k = pitch % 12;
    return o * 7 + white_of(k) - 2;
}

static void note_xy(int i, float *x, float *y)  /* note i's column, and its row's bottom line */
{
    int r = i / per_row, c = i % per_row, inrow = r < rows - 1 ? per_row : G.n - r * per_row;
    float w = inrow * col_w;
    *x = sx + (sw - w) / 2 + (c + 0.5f) * col_w;
    *y = sy + r * row_h + row_h / 2 + 2 * lg;
}

static void draw_staff(Uint32 now, int sounding, float glow)
{
    int f2 = (int)sfr;
    ui_put(t_staff, sx - f2, sy - f2, sw + 2 * f2, sh + 2 * f2);
    float t = fmaxf(1, floorf(0.09f * lg));
    for (int r = 0; r < rows; r++) {
        float x, base;
        note_xy(r * per_row, &x, &base);
        for (int l = 0; l < 5; l++)
            ui_fill(sx + 0.6f * lg, floorf(base - l * lg) - t / 2, sw - 1.2f * lg, t, INK[0], INK[1], INK[2], 170);
    }
    int solved = mel_solved(&G);
    float hd = 1.35f * lg;
    for (int i = 0; i < G.n; i++) {
        float x, base;
        note_xy(i, &x, &base);
        if (i == sounding) ui_mark_gold_a(x, base - 2 * lg, 2.8f * lg, glow);
        if (i < G.pos) {                         /* found: written at its pitch */
            int st = staff_step(G.p[i]);
            float y = base - st * lg / 2;
            for (int l = -2; l >= st; l -= 2)  /* ledger lines below the staff (C4), above it (A5 and higher) */
                ui_fill(x - 0.95f * lg, floorf(base - l * lg / 2) - t / 2, 1.9f * lg, t, INK[0], INK[1], INK[2], 200);
            for (int l = 10; l <= st; l += 2)
                ui_fill(x - 0.95f * lg, floorf(base - l * lg / 2) - t / 2, 1.9f * lg, t, INK[0], INK[1], INK[2], 200);
            int up = st < 4;                    /* the stem: up below the middle line, down above it */
            ui_fill(up ? x + 0.56f * lg : x - 0.62f * lg, up ? y - 3.2f * lg : y, fmaxf(1, 0.1f * lg), 3.2f * lg,
                    solved ? 150 : 24, solved ? 100 : 20, solved ? 30 : 18, 230);
            if (mel_black(G.p[i])) {            /* a sharp before it */
                float sxx = x - 1.35f * lg, a = fmaxf(1, 0.1f * lg);
                ui_fill(sxx - 0.2f * lg, y - 0.7f * lg, a, 1.4f * lg, 24, 20, 18, 230);
                ui_fill(sxx + 0.15f * lg, y - 0.8f * lg, a, 1.4f * lg, 24, 20, 18, 230);
                ui_fill(sxx - 0.42f * lg, y - 0.32f * lg, 0.84f * lg, a * 1.3f, 24, 20, 18, 230);
                ui_fill(sxx - 0.42f * lg, y + 0.18f * lg, 0.84f * lg, a * 1.3f, 24, 20, 18, 230);
            }
            int lit = solved || (nflash && i == G.pos - 1 && !flash[nflash - 1].wrong &&
                                 now - flash[nflash - 1].t0 < FLASH_MS);
            if (lit && !solved) ui_mark_gold_a(x, y, 2.4f * lg, 0.8f);
            ui_put_c(t_head[solved ? 1 : 0], x, y, hd, hd);
        } else if (i == G.pos) {                /* the next one: a dot in gold light (still: no redraw while idle) */
            ui_mark_gold_a(x, base - 2 * lg, 2.0f * lg, 0.6f);
            ui_put_c(t_dot, x, base - 2 * lg, 0.85f * lg, 0.85f * lg);
        } else {
            SDL_SetTextureAlphaMod(t_dot, 150);
            ui_put_c(t_dot, x, base - 2 * lg, 0.62f * lg, 0.62f * lg);
            SDL_SetTextureAlphaMod(t_dot, 255);
        }
    }
    if (silent()) {                             /* the sound off: the lights alone */
        float d = fminf(0.62f * ui.unit, 0.5f * sh);
        ui_put_c(t_badge, sx + sw - 0.7f * d, sy + 0.7f * d, d, d);
    }
}

/* the tune's name over the staff, once it is played back: its title, its composer under it in gold */
static void draw_title(void)
{
    int tw, th, cw2 = 0, ch2 = 0;
    float px = fminf(0.5f * ui.unit, 0.2f * sh);
    SDL_Texture *t = ui_word("melody_titles", 2 * G.tune, px, sw - 1.2f * px, PUZ_CREAM, &tw, &th), *c = NULL;
    char who[96];
    if (text_word("melody_titles", 2 * G.tune + 1, who, sizeof who) && who[0])
        c = ui_word("melody_titles", 2 * G.tune + 1, 0.72f * px, sw - 1.2f * px, GOLD, &cw2, &ch2);
    if (!t) return;
    int w = (int)fmaxf(tw, cw2) + (int)(1.4f * px), h = th + ch2 + (int)(0.5f * px);
    if (!t_plate || plate_w != w || plate_h != h) {
        static const uint8_t col[4] = {28, 34, 32, 236};
        ui_free(&t_plate);
        plate_w = w;
        plate_h = h;
        art_roundrect(w, h, 0.3f * px, col, ui_buf(w, h));
        t_plate = ui_tex(w, h, ui_buf(w, h));
    }
    float cx = sx + sw / 2, cy = sy + sh / 2, y = floorf(cy - h / 2.0f);
    ui_put(t_plate, floorf(cx - w / 2.0f), y, (float)w, (float)h);
    ui_put(t, floorf(cx - tw / 2.0f), y + floorf(0.25f * px), (float)tw, (float)th);
    if (c) ui_put(c, floorf(cx - cw2 / 2.0f), y + floorf(0.25f * px) + th, (float)cw2, (float)ch2);
}

/* ---- the keyboard ------------------------------------------------------------------------------------------------ */
static float key_light(int pitch, Uint32 now, int sounding, float glow, int *wrong)
{
    float a = 0;
    *wrong = 0;
    if (sounding >= 0 && G.p[sounding] == pitch) a = glow;
    for (int i = 0; i < nflash; i++)
        if (flash[i].pitch == pitch) {
            Uint32 len = flash[i].wrong ? WRONG_MS : FLASH_MS, dt = now - flash[i].t0;
            if (dt >= len) continue;
            float f = 1 - (float)dt / len;
            if (flash[i].wrong) *wrong = (int)(255 * f);
            else a = fmaxf(a, f);
        }
    if (hint.pitch == pitch) {
        float h = puz_pulse((float)(now - hint.t0), 0, HINT_MS);
        if (h > 0) a = fmaxf(a, h);
    }
    if (win.on && now >= win.t0) {
        float x, y, w, h;
        key_rect(pitch, &x, &y, &w, &h);
        float wv = puz_pulse((float)(now - win.t0), (x - kx) / kw * WAVE_STEP, WAVE_LEN);
        a = fmaxf(a, wv);
    }
    return a;
}

/* how far down a key is (0 up, 1 down): held under the finger; played (by the player, or the tune), a short push */
static float key_depth(int pitch, Uint32 now, int sounding)
{
    if (ptr.on && ptr.pitch == pitch) return 1;
    float d = 0;
    if (sounding >= 0 && G.p[sounding] == pitch) {
        float out = fminf(PRESS_OUT, 0.5f * play_lit);
        d = clampf(fminf(play_ms / (float)PRESS_IN, (play_lit - play_ms) / out), 0, 1);
    }
    for (int i = 0; i < nflash; i++)
        if (flash[i].pitch == pitch) {
            float dt = (float)(now - flash[i].t0), end = PRESS_IN + 90 + PRESS_OUT;
            if (dt < end) d = fmaxf(d, clampf(fminf(dt / PRESS_IN, (end - dt) / PRESS_OUT), 0, 1));
        }
    return d;
}

/* a key's picture, dp of the way down: the down picture over the up one, a little lower and darker */
static void put_key(SDL_Texture *up, SDL_Texture *dn, float x, float y, float w, float h, float dp)
{
    float sink = floorf(dp * fmaxf(1, 0.014f * kh) + 0.5f);   /* its back sinks under the case: the key shorter */
    y += sink;
    h -= sink;
    Uint8 c = (Uint8)(255 - 26 * dp);
    SDL_SetTextureColorMod(up, c, c, c);
    ui_put(up, x, y, w, h);
    SDL_SetTextureColorMod(up, 255, 255, 255);
    if (dp > 0) {
        SDL_SetTextureColorMod(dn, c, c, c);
        SDL_SetTextureAlphaMod(dn, (Uint8)(255 * dp));
        ui_put(dn, x, y, w, h);
        SDL_SetTextureAlphaMod(dn, 255);
        SDL_SetTextureColorMod(dn, 255, 255, 255);
    }
}

static void draw_keys(Uint32 now, int sounding, float glow)
{
    ui_put(t_case, kx - fr, ky - lip - fr, W * kw + 2 * fr, lip + kh + 2 * fr);
    int top = mel_top(W);
    for (int pass = 0; pass < 2; pass++)
        for (int p = MEL_LOW; p <= top; p++) {
            if (mel_black(p) != pass) continue;
            float x, y, w, h;
            int wrong;
            key_rect(p, &x, &y, &w, &h);
            float a = key_light(p, now, sounding, glow, &wrong), dp = key_depth(p, now, sounding);
            if (a > 0 || wrong || (dp > 0 && dp < 1)) ui_animate();
            int s = a > 0.15f ? 2 : 0;
            if (pass) {                          /* its shadow on the ivory, shorter as it goes down */
                float k = 1 - 0.7f * dp;
                ui_fill(x + 0.12f * k * w, y, w, h + 0.015f * k * kh, 0, 0, 0, (Uint8)(38 - 14 * dp));
                ui_fill(x + 0.06f * k * w, y, w, h + 0.008f * k * kh, 0, 0, 0, (Uint8)(38 - 14 * dp));
                put_key(t_black[s], t_black[s | 1], x, y, w, h, dp);
            } else {
                put_key(t_white[s], t_white[s | 1], x, y, w - 1, h, dp);
            }
            y += floorf(dp * fmaxf(1, 0.014f * kh) + 0.5f);
            if (wrong) {
                ui_fill(x + 1, y, w - 2, h * (pass ? 0.92f : 0.95f), 210, 52, 40, (Uint8)(0.55f * wrong));
                ui_mark_glow(x + w / 2, y + 0.82f * h, 1.4f * w);
            }
            if (a > 0) ui_mark_gold_a(x + w / 2, y + (pass ? 0.75f : 0.8f) * h, (pass ? 1.9f : 1.7f) * w, a);
        }
}

static void draw(void)
{
    if (!t_case) layout(room);
    Uint32 now = ui_ms();
    float glow = 0;
    int sounding = playing(now, &glow);
    if (play.on) {
        if (now >= play_end()) play.on = 0;
        else ui_animate();
    }
    if (win.on) {
        if (now - win.t0 < (Uint32)(W * WAVE_STEP + WAVE_LEN)) ui_animate();
        else win.on = 0;
    }
    draw_staff(now, sounding, glow);
    draw_keys(now, sounding, glow);
    if (mel_solved(&G) && !win.on) draw_title();
    int solved = mel_solved(&G);
    ui_tool(T_LISTEN, ICON_SOUND, solved ? UI_DIM : play.on ? UI_ON : 0);
    ui_tool(T_HINT, ICON_HINT, solved ? UI_DIM : 0);
}

/* ---- playing ---------------------------------------------------------------------------------------------------- */
static void flash_key(int pitch, int wrong)
{
    if (nflash == 4) memmove(flash, flash + 1, 3 * sizeof flash[0]), nflash = 3;
    flash[nflash].pitch = pitch;
    flash[nflash].wrong = wrong;
    flash[nflash].t0 = ui_ms();
    nflash++;
}

static void key_down(int pitch)
{
    play.on = 0;                                /* the tune stops: the player's turn */
    int r = mel_press(&G, pitch);
    if (r == MEL_OVER) { ui_sound(SND_DENY); return; }
    sound_note(pitch, r == MEL_WRONG);
    flash_key(pitch, r == MEL_WRONG);
    if (hint.pitch >= 0 && r != MEL_WRONG) hint.pitch = -1;
    if (r == MEL_DONE) {
        win.on = 1;
        win.t0 = ui_ms() + 250;
    }
    ui_animate();
}

static void use_hint(void)
{
    int p = mel_hint(&G);
    if (p < 0) { ui_sound(SND_DENY); return; }
    play.on = 0;
    hint.pitch = p;
    hint.t0 = ui_ms();
    sound_note(p, 0);
    ui_animate();
}

static void press(float x, float y)
{
    memset(&ptr, 0, sizeof ptr);
    int t = ui_tool_hit(x, y);
    if (t >= 0) {
        if (mel_solved(&G)) { ui_sound(SND_DENY); return; }
        if (t == T_LISTEN) {
            mel_listen(&G);
            hint.pitch = -1;
            listen(120);
        } else {
            use_hint();
        }
        return;
    }
    if (mel_solved(&G)) return;
    int p = key_at(x, y);
    if (p < 0) return;
    ptr.on = 1;
    ptr.pitch = p;
    key_down(p);
}

static void motion(float x, float y)           /* a finger sliding on plays nothing more: a tap is one note */
{
    (void)x;
    (void)y;
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    ptr.on = 0;
    ui_animate();
}

static void begin(void)
{
    calm();
    made_w = made_s = -1;
    relayout();
    listen(START_MS);                           /* the tune, first of all */
}

static void start(int level)
{
    mel_load(&G, level);
    begin();
}

static void start_random(int variant)
{
    mel_random(&G, variant < 0 || variant >= MEL_VARIANTS ? 0 : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int undo(void)
{
    if (!mel_undo(&G)) return 0;
    calm();
    return 1;
}

/* the moves at once (mel_script's notation), the last one shown lit: a key (gold, or red when wrong), a hint */
static int script(const char *moves)
{
    calm();
    char buf[512];
    snprintf(buf, sizeof buf, "%s", moves);
    char *e = buf + strlen(buf);
    while (e > buf && e[-1] == ' ') *--e = 0;
    char *s = e;
    while (s > buf && s[-1] != ' ') s--;
    char last[16];                              /* (copied by hand: no format-truncation warning) */
    size_t ln = strlen(s) < sizeof last - 1 ? strlen(s) : sizeof last - 1;
    memcpy(last, s, ln);
    last[ln] = 0;
    *s = 0;
    if (!mel_script(&G, buf)) return 0;
    if (!strcmp(last, "h") || !strcmp(last, "l")) {
        mel_script(&G, last);
        if (last[0] == 'h' && !mel_solved(&G)) {
            hint.pitch = mel_next(&G);
            hint.t0 = ui_ms() - HINT_MS / 2;
        }
        return 1;
    }
    if (!last[0]) return 1;
    int used, p = mel_parse_note(last, &used);
    if (p < 0) return 0;
    int r = mel_press(&G, p);
    if (r != MEL_OVER) flash_key(p, r == MEL_WRONG);
    if (r == MEL_DONE) {
        win.on = 1;
        win.t0 = ui_ms();
    }
    return 1;
}

static int status(void) { return mel_solved(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void)
{
    Uint32 now = ui_ms();
    return (play.on && now < play_end()) || (win.on && now - win.t0 < (Uint32)(W * WAVE_STEP + WAVE_LEN));
}
static int level_stars(void) { return ui_logic_stars(mel_help(&G), G.mistakes); }

static void bar(int player, box b)              /* the notes still to find */
{
    (void)player;
    ui_bar_number(b, t_note_small, G.n - G.pos);
}

/* ---- the menu's card, the picker's, the chapters' tabs ----------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    if (variant < 0) {                          /* the menu: a keyboard, three keys lit, a note above */
        int w = (int)(0.94f * size);
        paint_keyboard(rgba, size, size, (size - w) / 2, (int)(0.42f * size), w, 8,
                       1u << 4 | 1u << 7 | 1u << 11);
        paint_glyph(rgba, size, size, 0.3f * size, 0.02f * size, 0.42f * size, GOLD, (const uint8_t[3]){150, 96, 30});
        return;
    }
    /* random play: a keyboard, as many notes above it as the phrases are long */
    int w = (int)(0.9f * size), n = variant == MEL_SHORT ? 2 : variant == MEL_MEDIUM ? 3 : 5;
    paint_keyboard(rgba, size, size, (size - w) / 2, (int)(0.46f * size), w, 8, variant == MEL_SHORT ? 1u << 4 :
                   variant == MEL_MEDIUM ? 1u << 4 | 1u << 9 : 1u << 0 | 1u << 4 | 1u << 7 | 1u << 9 | 1u << 12);
    float s = (n > 3 ? 0.24f : 0.3f) * size, step = (0.86f * size - s) / (n > 1 ? n - 1 : 1);
    for (int i = 0; i < n; i++)
        paint_glyph(rgba, size, size, 0.05f * size + i * step, (0.06f + 0.08f * (i % 2)) * size, s, GOLD,
                    (const uint8_t[3]){150, 96, 30});
}

static void chapter_icon(int chapter, int size, uint8_t *rgba)   /* a keyboard, a key lit more a chapter */
{
    memset(rgba, 0, (size_t)size * size * 4);
    static const uint32_t LIT[6] = {1u << 4, 1u << 2 | 1u << 5, 1u << 0 | 1u << 4 | 1u << 7,
                                    1u << 0 | 1u << 3 | 1u << 5 | 1u << 7, 1u << 0 | 1u << 1 | 1u << 4 | 1u << 6 | 1u << 7,
                                    1u << 1 | 1u << 2 | 1u << 3 | 1u << 5 | 1u << 6 | 1u << 7};
    int w = (int)(0.96f * size), c = chapter < 0 ? 0 : chapter > 5 ? 5 : chapter;
    paint_keyboard(rgba, size, size, (size - w) / 2, (int)(0.1f * size), w, 5, LIT[c]);
}

static const game melody_game = {
    .id = "melody", .order = 449, .variants = MEL_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .fill = 1, .tools = TOOLS,
    .levels = MEL_LEVEL_COUNT, .chapters = MEL_CHAPTER_COUNT, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_stars = level_stars,
};
REGISTER_GAME(melody_game)
