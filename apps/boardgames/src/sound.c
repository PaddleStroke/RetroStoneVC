/*
 * The board games' sounds, made by code at start (no sound files): wooden clicks, a stone set down, a disc dropped
 * in a frame, a chime for check, a short tune for the end of a game. A small mixer plays several at once.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

#define RATE 44100
#define VOICES 6

int snd_muted;
static SDL_AudioDeviceID dev;
static float *clip[SNDS];
static int len[SNDS];
static struct { int s, pos; float vol; } voice[VOICES];

/* a sine at f Hz from t0 (s), its amplitude decaying by e every `decay` seconds */
static void tone(float *b, int n, float t0, float f, float amp, float decay)
{
    for (int i = (int)(t0 * RATE); i < n; i++) {
        float t = (float)i / RATE - t0;
        b[i] += amp * expf(-t / decay) * sinf(6.2831853f * f * t) * fminf(1, t * RATE / 24);
    }
}

/* a burst of noise, low-passed (k: 0..1, the smaller the duller) */
static void noise(float *b, int n, float t0, float amp, float decay, float k)
{
    unsigned seed = 12345;
    float lp = 0;
    for (int i = (int)(t0 * RATE); i < n; i++) {
        float t = (float)i / RATE - t0;
        seed = seed * 1664525u + 1013904223u;
        float w = (seed >> 9) / 4194304.0f - 1;
        lp += k * (w - lp);
        b[i] += amp * expf(-t / decay) * lp;
    }
}

static float *make(int s, float seconds)
{
    len[s] = (int)(seconds * RATE);
    clip[s] = calloc((size_t)len[s], sizeof(float));
    return clip[s];
}

static void synth(void)
{
    float *b;
    b = make(SND_MOVE, 0.12f);                  /* a wooden piece set on wood */
    tone(b, len[SND_MOVE], 0, 1150, 0.30f, 0.018f);
    tone(b, len[SND_MOVE], 0, 520, 0.26f, 0.035f);
    noise(b, len[SND_MOVE], 0, 0.30f, 0.004f, 0.5f);
    b = make(SND_CAPTURE, 0.20f);               /* two knocks: the piece taken, the piece landing */
    tone(b, len[SND_CAPTURE], 0, 950, 0.26f, 0.02f);
    noise(b, len[SND_CAPTURE], 0, 0.30f, 0.004f, 0.5f);
    tone(b, len[SND_CAPTURE], 0.055f, 820, 0.32f, 0.025f);
    tone(b, len[SND_CAPTURE], 0.055f, 380, 0.30f, 0.05f);
    noise(b, len[SND_CAPTURE], 0.055f, 0.35f, 0.005f, 0.45f);
    b = make(SND_PLACE, 0.12f);                 /* a stone: sharper */
    tone(b, len[SND_PLACE], 0, 2300, 0.24f, 0.012f);
    tone(b, len[SND_PLACE], 0, 900, 0.24f, 0.025f);
    noise(b, len[SND_PLACE], 0, 0.40f, 0.003f, 0.7f);
    b = make(SND_DROP, 0.22f);                  /* a disc dropping into its slot */
    tone(b, len[SND_DROP], 0, 610, 0.30f, 0.06f);
    tone(b, len[SND_DROP], 0, 1320, 0.16f, 0.03f);
    noise(b, len[SND_DROP], 0, 0.25f, 0.006f, 0.35f);
    b = make(SND_CHECK, 0.45f);                 /* a chime */
    tone(b, len[SND_CHECK], 0, 880, 0.20f, 0.18f);
    tone(b, len[SND_CHECK], 0, 1320, 0.14f, 0.14f);
    b = make(SND_END, 1.0f);                    /* a rising arpeggio */
    tone(b, len[SND_END], 0.00f, 523.3f, 0.18f, 0.30f);
    tone(b, len[SND_END], 0.12f, 659.3f, 0.18f, 0.30f);
    tone(b, len[SND_END], 0.24f, 784.0f, 0.18f, 0.30f);
    tone(b, len[SND_END], 0.36f, 1046.5f, 0.20f, 0.35f);
    b = make(SND_TAP, 0.04f);                   /* a button */
    tone(b, len[SND_TAP], 0, 1600, 0.16f, 0.008f);
    b = make(SND_DENY, 0.16f);                  /* not allowed: a low buzz */
    tone(b, len[SND_DENY], 0, 196, 0.22f, 0.07f);
    tone(b, len[SND_DENY], 0, 207, 0.22f, 0.07f);
    b = make(SND_DICE, 0.55f);                  /* dice rattling, then landing: knocks closer and closer */
    static const float knock[] = {0.00f, 0.07f, 0.13f, 0.20f, 0.25f, 0.31f, 0.35f, 0.40f, 0.43f};
    for (unsigned i = 0; i < sizeof knock / sizeof knock[0]; i++) {
        float k = 1 - i * 0.07f;
        tone(b, len[SND_DICE], knock[i], 1700 + 230 * (i % 3), 0.16f * k, 0.010f);
        noise(b, len[SND_DICE], knock[i], 0.26f * k, 0.004f, 0.6f);
    }
}

static void mix(void *u, Uint8 *stream, int bytes)
{
    (void)u;
    int16_t *out = (int16_t *)stream;
    int n = bytes / 2;
    for (int i = 0; i < n; i++) {
        float v = 0;
        for (int k = 0; k < VOICES; k++) {
            if (voice[k].s < 0) continue;
            if (voice[k].pos < 0) { voice[k].pos++; continue; }   /* a sound waiting for its moment */
            v += clip[voice[k].s][voice[k].pos++] * voice[k].vol;
            if (voice[k].pos >= len[voice[k].s]) voice[k].s = -1;
        }
        v = v > 1 ? 1 : v < -1 ? -1 : v;
        out[i] = (int16_t)(v * 30000);
    }
}

/* the device: closed while an Arcade tab's game plays on its own (arcade_play.c: Android's audio drivers, OpenSL ES
 * and AAudio, play one device at a time; a second one opened beside this one was silent), opened again after */
void snd_open(int on)
{
    if (!on) {
        if (dev) SDL_CloseAudioDevice(dev);
        dev = 0;
        for (int k = 0; k < VOICES; k++) voice[k].s = -1;
        return;
    }
    if (dev || !SDL_WasInit(SDL_INIT_AUDIO)) return;
    SDL_AudioSpec want = {0}, have;
    want.freq = RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = mix;
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
}

void snd_init(void)
{
    for (int k = 0; k < VOICES; k++) voice[k].s = -1;
    synth();
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) return;
    snd_open(1);
}

void snd_play(int s) { snd_play_vol(s, 1); }

void snd_play_vol(int s, float volume) { snd_play_at(s, volume, 0); }

void snd_play_at(int s, float volume, int delay_ms)
{
    if (!dev || snd_muted || s < 0 || s >= SNDS || volume <= 0) return;
    SDL_LockAudioDevice(dev);
    int k = 0;
    for (int i = 0; i < VOICES; i++)
        if (voice[i].s < 0 || voice[i].pos > voice[k].pos) { k = i; if (voice[i].s < 0) break; }
    voice[k].s = s;
    voice[k].pos = delay_ms > 0 ? -(delay_ms * RATE / 1000) : 0;
    voice[k].vol = volume > 1 ? 1 : volume;
    SDL_UnlockAudioDevice(dev);
}
