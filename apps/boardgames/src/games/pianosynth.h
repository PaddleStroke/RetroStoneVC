/*
 * The piano's voice, shared by Play by Ear and Falling Notes (no SDL: the tests use it): a note made by code (no
 * samples), 16-bit at PK_RATE. Additive: up to 14 partials of the string, a little sharp as they climb (a stiff string's
 * inharmonicity), each a pair of oscillators about a cent apart (two strings of one note beating slowly); weighted as
 * by a hammer striking near an eighth of the string (the 8th, 16th partials faint), the high ones softer for high
 * notes. A soft attack (3 ms) with the felt hammer's short knock, then a fast decay into a long, slow one, the higher
 * partials dying sooner, and low notes ringing longer than high ones. "dull": the same note damped at once, fewer
 * partials, with a felt thump (a key gone wrong).
 * Written once, here, as static inline functions (as puzrand.h): the build takes a game's own files only, so a shared
 * .c file would be left out of one game's build or the other's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef PIANOSYNTH_H
#define PIANOSYNTH_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#define PK_RATE 44100

static inline float pk_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline float pk_seconds(int pitch, int dull)
{
    if (dull) return 0.42f;
    float f = 440.0f * powf(2.0f, (pitch - 69) / 12.0f);
    return pk_clampf(2.6f * powf(261.6f / f, 0.35f), 1.6f, 3.2f);
}

static inline int pk_synth_len(int pitch, int dull) { return (int)(pk_seconds(pitch, dull) * PK_RATE); }

static inline void pk_synth(int pitch, int dull, int16_t *out, int n)
{
    static float acc[33 * PK_RATE / 10];
    const float two_pi = 6.28318531f, pi = 3.14159265f;
    if (n > (int)(sizeof acc / sizeof acc[0])) n = (int)(sizeof acc / sizeof acc[0]);
    if (n <= 0) return;
    memset(acc, 0, sizeof(float) * (size_t)n);
    float f0 = 440.0f * powf(2.0f, (pitch - 69) / 12.0f), rel = 261.6f / f0;
    float B = 0.00012f * powf(f0 / 261.6f, 0.6f);   /* inharmonicity: higher strings are stiffer for their length */
    int parts = dull ? 5 : 14;
    float norm = 0;
    for (int k = 1; k <= parts; k++) {
        float fk = k * f0 * sqrtf(1 + B * k * k);
        if (fk > 9000.0f) break;
        float strike = fabsf(sinf(pi * k / 8.3f)) * 0.8f + 0.2f;   /* the hammer at ~1/8 of the string */
        float a = strike / powf((float)k, 1.15f) * expf(-(k - 1) * f0 / (dull ? 900.0f : 2600.0f));
        float fast = (dull ? 0.05f : 0.30f) * powf(rel, 0.25f) / (1 + 0.35f * (k - 1));
        float slow = (dull ? 0.16f : 2.4f) * powf(rel, 0.45f) / (1 + 0.22f * (k - 1));
        float mix = dull ? 0.5f : 0.62f;          /* how much of it dies fast */
        norm += a;
        for (int s = 0; s < 2; s++) {             /* two strings, a cent or so apart */
            float det = 1 + (s ? 0.00055f : -0.00045f) * (1 + 0.08f * k);
            float w = two_pi * fk * det / PK_RATE, c = cosf(w), sn = sinf(w);
            float x = sinf(1.3f * k + s), y = cosf(1.3f * k + s);   /* each partial its own phase */
            float ef = expf(-1.0f / (fast * PK_RATE)), es = expf(-1.0f / (slow * PK_RATE));
            float gf = a * 0.5f * mix, gs = a * 0.5f * (1 - mix);
            for (int i = 0; i < n; i++) {
                acc[i] += x * (gf + gs);
                gf *= ef;
                gs *= es;
                float nx = x * c + y * sn;          /* the oscillator turned one sample on */
                y = y * c - x * sn;
                x = nx;
                if (gf + gs < 1e-6f) break;
            }
        }
    }
    /* the attack (3 ms, the felt), the hammer's knock (a short low-passed noise), the end faded */
    unsigned seed = 0x9e3779b9u ^ (unsigned)pitch * 2654435761u;
    float lp = 0, knock = dull ? 0.20f : 0.07f;
    for (int i = 0; i < n; i++) {
        float t = (float)i / PK_RATE;
        seed = seed * 1664525u + 1013904223u;
        lp += 0.12f * (((seed >> 9) / 4194304.0f - 1) - lp);
        float v = acc[i] / (norm > 0 ? norm : 1) * (1 - expf(-t / 0.003f));
        v += knock * lp * expf(-t / (dull ? 0.025f : 0.008f));
        float tail = (float)(n - i) / (0.08f * PK_RATE);
        if (tail < 1) v *= tail;
        acc[i] = v;
    }
    /* loudness: the high notes a little softer, as the ear hears them loud; a wrong key quieter */
    float gain = (dull ? 0.38f : 0.55f) * powf(rel, 0.12f);
    for (int i = 0; i < n; i++) {
        float v = acc[i] * gain;
        v = v > 0.98f ? 0.98f : v < -0.98f ? -0.98f : v;
        out[i] = (int16_t)(v * 32767);
    }
}

#endif
