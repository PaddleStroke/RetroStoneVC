/*
 * The piano kit, shared by Play by Ear (melody.c) and Falling Notes (pianofall.c): the keys' and the case's pictures
 * (ivory and ebony keys, the walnut case and its red felt, a key going down), a little keyboard and an eighth note
 * painted into a thumbnail, the "sound off" badge (tapped: the sound on again); and the piano's sound: each note
 * rendered once by pianosynth.h, then mixed by a small mixer of the kit's own into the app's one sound device
 * (sound.c's snd_set_extra_mixer: Android plays one device at a time, a second one opened beside it stayed silent).
 * One mixer for both games: the first one to sound leaves it on the window (SDL_SetWindowData), the other finds it
 * there, so both share their notes and voices.
 * Written once, here, as static inline functions (as puzrand.h): the build takes a game's own files only, so a shared
 * .c file would be left out of one game's build or the other's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PIANOKIT_H
#define PIANOKIT_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/pianosynth.h"
#include "shell.h"

#if PK_RATE != 44100
#error "the piano is mixed into sound.c's stream: 44100 Hz"
#endif

/* ---- the sound --------------------------------------------------------------------------------------------------- */
#define PK_VOICES 72
#define PK_FADE (PK_RATE / 9)                   /* a note released: it fades in this many samples */
typedef struct { const int16_t *s; int n, pos, rel; float gain; } pk_voice;
typedef struct {
    int hooked;                                 /* its mixer given to sound.c */
    int16_t *pcm[2][128];                       /* each note made once (2: dull) */
    int pcm_n[2][128];
    pk_voice v[PK_VOICES];
} pk_mixer;

static pk_mixer pk_own, *pk_mx;

static inline pk_mixer *pk_mixer_get(void)
{
    if (pk_mx) return pk_mx;
    SDL_Window *w = ui.r ? SDL_RenderGetWindow(ui.r) : NULL;
    pk_mixer *m = w ? (pk_mixer *)SDL_GetWindowData(w, "pianokit") : NULL;
    if (!m) {
        m = &pk_own;
        if (w) SDL_SetWindowData(w, "pianokit", m);
    }
    return pk_mx = m;
}

/* (the audio thread) the piano's voices added into sound.c's stream, frames x channels, at PK_RATE */
static inline void pk_mix(void *u, int16_t *out, int frames, int rate, int channels)
{
    pk_mixer *m = (pk_mixer *)u;
    int quiet = snd_muted;
    (void)rate;                                 /* always 44100 (sound.c's device; checked above) */
    for (int i = 0; i < frames; i++) {
        float v = 0;
        for (int k = 0; k < PK_VOICES; k++) {
            pk_voice *V = &m->v[k];
            if (!V->s) continue;
            if (V->pos < 0) { V->pos++; continue; }
            float a = V->s[V->pos] * V->gain;
            if (V->rel >= 0 && V->pos >= V->rel) {
                float f = 1 - (float)(V->pos - V->rel) / PK_FADE;
                if (f <= 0) { V->s = NULL; continue; }
                a *= f * f;
            }
            v += a;
            if (++V->pos >= V->n) V->s = NULL;
        }
        if (quiet || v == 0) continue;
        for (int c = 0; c < channels; c++) {
            float s = out[i * channels + c] + v;
            out[i * channels + c] = (int16_t)(s > 32000 ? 32000 : s < -32000 ? -32000 : s);
        }
    }
}

/* the sound there: the app's device open (its mixer given to it the first time); 0 in the picture mode (no sound) */
static inline int pk_audio_on(void)
{
    pk_mixer *m = pk_mixer_get();
    if (!SDL_WasInit(SDL_INIT_AUDIO)) return 0;
    if (!m->hooked) {
        snd_set_extra_mixer(pk_mix, m);
        m->hooked = 1;
    }
    return snd_ready();
}

static inline const int16_t *pk_note_pcm(int pitch, int dull, int *n)
{
    pk_mixer *m = pk_mixer_get();
    pitch &= 127;
    dull = dull ? 1 : 0;
    if (!m->pcm[dull][pitch]) {
        int len = pk_synth_len(pitch, dull);
        int16_t *b = (int16_t *)malloc((size_t)len * sizeof *b);
        if (!b) return NULL;
        pk_synth(pitch, dull, b, len);
        m->pcm_n[dull][pitch] = len;
        m->pcm[dull][pitch] = b;
    }
    *n = m->pcm_n[dull][pitch];
    return m->pcm[dull][pitch];
}

static inline int pk_sound_ready(void) { return !snd_muted && pk_audio_on(); }
/* the sound off: the app's sound muted (Settings), or no device (the picture mode has none and shows no badge) */
static inline int pk_silent(void) { return snd_muted || (SDL_WasInit(SDL_INIT_AUDIO) && !snd_ready()); }
static inline void pk_lock(void) { snd_lock(); }
static inline void pk_unlock(void) { snd_unlock(); }

/* the "sound off" badge, d across centred on (x, y), drawn while the sound is off */
static inline void pk_badge(SDL_Texture *t, float x, float y, float d)
{
    if (pk_silent()) ui_put_c(t, x, y, d, d);
}

/* a tap (px, py) on that badge: the sound on again (Settings' switch), or the device tried again; 1: taken */
static inline int pk_badge_tap(float px, float py, float x, float y, float d)
{
    if (!pk_silent() || hypotf(px - x, py - y) > fmaxf(0.75f * d, 0.4f * ui.unit)) return 0;
    if (snd_muted) shell_set_mute(0);
    if (!snd_ready()) snd_open(1);
    ui_sound(SND_TAP);
    ui_animate();
    return 1;
}

/* (locked) what sounds stops: the notes waiting dropped, the ones ringing released */
static inline void pk_hush_locked(void)
{
    pk_mixer *m = pk_mixer_get();
    for (int k = 0; k < PK_VOICES; k++) {
        pk_voice *V = &m->v[k];
        if (!V->s) continue;
        if (V->pos < 0) V->s = NULL;
        else if (V->rel < 0 || V->rel > V->pos) V->rel = V->pos;
    }
}

/* (locked) a note added: after delay samples, released at rel (-1: rings out), loud by gain; its voice (a free one,
 * else the one that has sounded longest) */
static inline int pk_voice_add(const int16_t *s, int n, int delay, int rel, float gain)
{
    pk_mixer *m = pk_mixer_get();
    int k = 0;
    for (int i = 0; i < PK_VOICES; i++)
        if (!m->v[i].s) { k = i; break; } else if (m->v[i].pos > m->v[k].pos) k = i;
    m->v[k].s = s;
    m->v[k].n = n;
    m->v[k].pos = -delay;
    m->v[k].rel = rel;
    m->v[k].gain = gain;
    return k;
}

static inline void pk_hush(void)
{
    if (!pk_mixer_get()->hooked) return;
    pk_lock();
    pk_hush_locked();
    pk_unlock();
}

/* a note alone: what sounded released, this one played */
static inline void pk_sound_note(int pitch, int dull)
{
    if (!pk_sound_ready()) return;
    int n;
    const int16_t *s = pk_note_pcm(pitch, dull, &n);
    if (!s) return;
    pk_lock();
    pk_hush_locked();
    pk_voice_add(s, n, 0, -1, 1);
    pk_unlock();
}

/* a note added to those ringing (a chord), released after rel_ms (-1: until pk_sound_off); its voice, or -1, and its
 * sound in *sample (to know the voice still sounds it) */
static inline int pk_sound_on(int pitch, int dull, float gain, int rel_ms, const int16_t **sample)
{
    *sample = NULL;
    if (!pk_sound_ready()) return -1;
    int n;
    const int16_t *s = pk_note_pcm(pitch, dull, &n);
    if (!s) return -1;
    pk_lock();
    int k = pk_voice_add(s, n, 0, rel_ms < 0 ? -1 : rel_ms * (PK_RATE / 1000), gain);
    pk_unlock();
    *sample = s;
    return k;
}

/* that voice released now, if it still sounds that note (not taken since) and is not released yet */
static inline void pk_sound_off(int k, const int16_t *sample)
{
    pk_mixer *m = pk_mixer_get();
    if (k < 0 || k >= PK_VOICES || !m->hooked) return;
    pk_lock();
    pk_voice *V = &m->v[k];
    if (V->s == sample && V->pos >= 0 && (V->rel < 0 || V->rel > V->pos)) V->rel = V->pos;
    pk_unlock();
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static const uint8_t PK_IVORY[3] = {246, 238, 218}, PK_IVORY_GOLD[3] = {252, 214, 122}, PK_EBONY[3] = {30, 26, 26},
                     PK_EBONY_GOLD[3] = {196, 146, 54}, PK_GOLD[3] = {255, 204, 96}, PK_INK[3] = {64, 42, 28};

static inline uint8_t pk_px8(float v) { return (uint8_t)clampf(v, 0, 255); }

/* a white key's place among the white keys of its octave (0..6), for a pitch class */
static inline int pk_white_of(int k) { static const int w[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6}; return w[k]; }

static inline int pk_black(int pitch)
{
    int k = pitch % 12;
    return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
}

/* the corners at the bottom rounded: the pixel's alpha */
static inline float pk_round_bottom(int x, int y, int w, int h, float r)
{
    float cx = x + 0.5f < r ? r : x + 0.5f > w - r ? w - r : x + 0.5f, cy = h - r;
    if (y + 0.5f < cy || (x + 0.5f >= r && x + 0.5f <= w - r)) return 1;
    return clampf(r + 0.5f - hypotf(x + 0.5f - cx, y + 0.5f - cy), 0, 1);
}

/* an ivory key, w x h: shaded under the case at its back, its front edge (the lip) showing below, its sides; down
 * (pressed): its front edge shorter, a little darker; gold: lit */
static inline void pk_paint_white(int w, int h, int down, int gold, uint8_t *o)
{
    const uint8_t *c = gold ? PK_IVORY_GOLD : PK_IVORY;
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
            for (int i = 0; i < 3; i++) p[i] = pk_px8(c[i] * k + g);
            p[3] = (uint8_t)(255 * pk_round_bottom(x, y, w, h, r));
        }
}

/* an ebony key: its top face darker at the back, a sheen towards the front, bevelled sides, its front face lighter */
static inline void pk_paint_black(int w, int h, int down, int gold, uint8_t *o)
{
    const uint8_t *c = gold ? PK_EBONY_GOLD : PK_EBONY;
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
            for (int i = 0; i < 3; i++) p[i] = pk_px8(c[i] * k + add + 2 * art_noise(x * 2, y));
            p[3] = (uint8_t)(255 * pk_round_bottom(x, y, w, h, r));
        }
}

/* the case: walnut, the fallboard's edge above the keys with a strip of red felt, the keys' bed (dark); iw x ih inside
 * a frame f wide, the fallboard and felt lp high */
static inline void pk_paint_case(int iw, int ih, int f, int lp, uint8_t *o)
{
    int w = iw + 2 * f;
    art_plank(iw, ih, f, o);
    for (int y = f; y < f + ih; y++)
        for (int x = f; x < f + iw; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            int yy = y - f;
            if (yy < lp - lp / 5) {             /* the fallboard's edge: dark walnut, lit along its top */
                float k = 0.42f + 0.18f * (yy < 2 ? 1 : 0) - 0.1f * yy / lp;
                for (int i = 0; i < 3; i++) p[i] = pk_px8(p[i] * k);
            } else if (yy < lp) {               /* the felt */
                float n = 6 * art_noise(x * 3, y * 3);
                p[0] = pk_px8(122 + n);
                p[1] = pk_px8(26 + n / 3);
                p[2] = pk_px8(30 + n / 3);
            } else {                            /* the bed under the keys */
                p[0] = 22;
                p[1] = 16;
                p[2] = 12;
            }
        }
}

/* the keys' pictures for the screen: white w x h, black b x bh; [s]: s & 1 down, s & 2 lit in gold */
static inline void pk_key_textures(int w, int h, int b, int bh, SDL_Texture *white[4], SDL_Texture *black[4])
{
    for (int s = 0; s < 4; s++) {
        ui_free(&white[s]);
        ui_free(&black[s]);
        pk_paint_white(w, h, s & 1, s >> 1, ui_buf(w, h));
        white[s] = ui_tex(w, h, ui_buf(w, h));
        pk_paint_black(b, bh, s & 1, s >> 1, ui_buf(b, bh));
        black[s] = ui_tex(b, bh, ui_buf(b, bh));
    }
}

/* an eighth note (its head, its stem, its flag) painted into a picture w x h, in the square (x, y, s) */
static inline void pk_paint_glyph(uint8_t *o, int w, int h, float x, float y, float s, const uint8_t rgb[3],
                                  const uint8_t ink[3])
{
    int q = (int)s;
    if (q < 4) return;
    uint8_t *b = (uint8_t *)calloc((size_t)q * q, 4);
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

/* a little keyboard painted into a picture: its case at (x, y), w wide, n white keys from a C, each len times as long
 * as wide (3.6: a piano's), the keys lit (bit i: the key i semitones above that C) */
static inline void pk_paint_keyboard(uint8_t *o, int pw, int ph, int x, int y, int w, int n, float len, uint32_t lit)
{
    int q = (w - 2) / n, f = q > 12 ? q / 6 : 2, khh = (int)(len * q), lp = q / 2 + 1;
    if (q < 3) return;
    int iw = n * q, ih = lp + khh, cw = iw + 2 * f, ch = ih + 2 * f;
    uint8_t *c = (uint8_t *)calloc((size_t)cw * ch, 4), *k = (uint8_t *)calloc((size_t)q * khh, 4);
    int bq = (int)(0.6f * q), bhh = (int)(0.62f * khh);
    uint8_t *b = (uint8_t *)calloc((size_t)(bq > 0 ? bq : 1) * (bhh > 0 ? bhh : 1), 4);
    if (c && k && b && bq > 1) {
        pk_paint_case(iw, ih, f, lp, c);
        for (int i = 0; i < n; i++) {
            int key = i / 7 * 12 + (const int[7]){0, 2, 4, 5, 7, 9, 11}[i % 7];
            pk_paint_white(q - 1, khh, 0, lit >> key & 1, k);
            for (int yy = 0; yy < khh; yy++)
                memcpy(c + 4 * ((size_t)(f + lp + yy) * cw + f + i * q), k + 4 * (size_t)yy * (q - 1), (size_t)(q - 1) * 4);
        }
        for (int i = 0; i + 1 < n; i++) {
            int m = i % 7;
            if (m == 2 || m == 6) continue;
            int key = i / 7 * 12 + (const int[7]){1, 3, 0, 6, 8, 10, 0}[m];
            pk_paint_black(bq, bhh, 0, lit >> key & 1, b);
            art_blend(c, cw, ch, b, bq, bhh, f + (i + 1) * q - bq / 2, f + lp);
        }
        art_blend(o, pw, ph, c, cw, ch, x, y);
    }
    free(c);
    free(k);
    free(b);
}

static inline void pk_paint_badge(int d, uint8_t *o)   /* sound off: a crossed-out speaker on a dark disc */
{
    art_disc(d, (const uint8_t[3]){28, 22, 18}, 0.85f, 0, o);
    int q = (int)(0.66f * d);
    uint8_t *ic = (uint8_t *)calloc((size_t)q * q, 4);
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

/* a key's picture, dp of the way down (0 up, 1 down): the down picture over the up one, a little lower and darker
 * (kh: a white key's height) */
static inline void pk_put_key(SDL_Texture *up, SDL_Texture *dn, float x, float y, float w, float h, float dp, float kh)
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

#endif
