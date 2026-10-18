/*
 * Falling Notes' rules (games/pianofall.h): the songs, a melody at random, the presses judged, the points, the stars,
 * the calibration.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/pianofall.h"
#include "games/pianofall_levels.h"
#include "games/puzrand.h"

_Static_assert(PFL_LEVELS == PFL_LEVEL_COUNT && PFL_CHAPTERS == PFL_CHAPTER_COUNT, "pianofall.h's counts");

int pfl_levels_count(void) { return PFL_LEVELS; }

const pfl_song *pfl_song_at(int level)
{
    return &pfl_songs[level < 0 ? 0 : level >= PFL_LEVELS ? PFL_LEVELS - 1 : level];
}

int pfl_black(int pitch)
{
    int k = pitch % 12;
    return k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
}

int pfl_whites(const pfl_game *g)
{
    int n = 0;
    for (int p = g->low; p <= g->high; p++) n += !pfl_black(p);
    return n;
}

int pfl_end_ms(const pfl_game *g)
{
    int e = 0;
    for (int i = 0; i < g->n; i++)
        if (g->note[i].t + g->note[i].len > e) e = g->note[i].t + g->note[i].len;
    return e;
}

int pfl_is_hold(const pfl_game *g, int i) { return i >= 0 && i < g->n && g->note[i].len >= PFL_HOLD_MS; }

static void fresh(pfl_game *g)
{
    memset(g->judge, 0, sizeof g->judge);
    memset(g->hold, 0, sizeof g->hold);
    memset(g->err, 0, sizeof g->err);
    g->scan = g->combo = g->best_combo = g->perfect = g->good = g->miss = g->stray = 0;
    g->err_sum = g->err_n = 0;
    g->now = -1000000;
}

void pfl_restart(pfl_game *g) { fresh(g); }

static int ms_of(int sixteenths, int bpm) { return sixteenths * 15000 / (bpm > 0 ? bpm : 100); }

void pfl_load(pfl_game *g, int level)
{
    const pfl_song *s = pfl_song_at(level);
    memset(g, 0, sizeof *g);
    g->level = level < 0 ? 0 : level >= PFL_LEVELS ? PFL_LEVELS - 1 : level;
    g->tune = s->tune;
    g->bpm = s->bpm;
    g->low = s->low;
    g->high = s->high;
    g->n = s->n > PFL_MAX ? PFL_MAX : s->n;
    for (int i = 0; i < g->n; i++) {
        const pfl_src *q = &pfl_notes[s->first + i];
        g->note[i].t = ms_of(q->t, s->bpm);
        g->note[i].len = ms_of(q->d, s->bpm);
        g->note[i].pitch = q->p;
    }
    fresh(g);
}

/* ---- a melody at random ------------------------------------------------------------------------------------------ */
static const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11}, MINOR[7] = {0, 2, 3, 5, 7, 8, 11};   /* (harmonic minor) */

typedef struct { int tonic; const int *scale; } rkey;

static int degree_pitch(rkey k, int d)       /* a degree of the scale (0 the tonic, 7 its octave, -1 below) */
{
    int o = d >= 0 ? d / 7 : -((-d + 6) / 7);
    return k.tonic + 12 * o + k.scale[d - 7 * o];
}

static int white_down(int p) { return pfl_black(p) ? p - 1 : p; }
static int white_up(int p) { return pfl_black(p) ? p + 1 : p; }

static int note_cmp(const void *a, const void *b)
{
    const pfl_note *x = a, *y = b;
    return x->t != y->t ? x->t - y->t : y->pitch - x->pitch;
}

/* the keyboard round the notes: an octave at least */
static void fit_keys(pfl_game *g)
{
    int lo = 127, hi = 0;
    for (int i = 0; i < g->n; i++) {
        if (g->note[i].pitch < lo) lo = g->note[i].pitch;
        if (g->note[i].pitch > hi) hi = g->note[i].pitch;
    }
    g->low = white_down(lo);
    g->high = white_up(hi);
    while (pfl_whites(g) < 8) {
        if (g->high < PFL_HIGHEST) g->high = white_up(g->high + 1);
        else g->low = white_down(g->low - 1);
    }
}

void pfl_random(pfl_game *g, int variant, uint32_t seed)
{
    static const int BARS[3][8][5] = {          /* a bar's rhythms (sixteenths), 0 ends it */
        {{4, 4, 4, 4}, {4, 4, 8}, {8, 4, 4}, {8, 8}, {4, 4, 4, 4}, {4, 8, 4}, {4, 4, 8}, {4, 4, 4, 4}},
        {{4, 4, 4, 4}, {2, 2, 4, 4, 4}, {4, 2, 2, 8}, {6, 2, 4, 4}, {4, 4, 8}, {8, 4, 4}, {4, 2, 2, 4, 4}, {6, 2, 8}},
        {{2, 2, 2, 2, 8}, {4, 2, 2, 4, 4}, {6, 2, 6, 2}, {3, 1, 4, 4, 4}, {2, 2, 4, 2, 6}, {4, 4, 2, 2, 4},
         {8, 2, 2, 4}, {2, 2, 2, 2, 8}},
    };
    static const int BPM[3] = {84, 104, 120}, PHRASES[3] = {3, 4, 4}, SPAN[3] = {7, 9, 11};
    static const int CLOSE[2][5] = {{4, 4, 8}, {8, 8}};   /* a phrase's last bar: a long note at its end */
    static const int TONIC_M[4] = {67, 65, 62, 69}, TONIC_H[6] = {62, 58, 63, 57, 64, 62};
    int v = variant < 0 || variant >= PFL_VARIANTS ? 0 : variant;
    uint32_t s = pr_seed(seed);
    rkey k = {60, MAJOR};
    if (v == PFL_MEDIUM) {                      /* G, F, D major, A minor */
        int c = pr_below(&s, 4);
        k.tonic = TONIC_M[c];
        k.scale = c == 3 ? MINOR : MAJOR;
    } else if (v == PFL_HARD) {                 /* D, Bb, Eb, A major, E, D minor */
        int c = pr_below(&s, 6);
        k.tonic = TONIC_H[c];
        k.scale = c >= 4 ? MINOR : MAJOR;
    }
    memset(g, 0, sizeof *g);
    g->level = -1;
    g->tune = -1;
    g->bpm = BPM[v];
    int lo = 0, hi = SPAN[v];                   /* the degrees the tune moves in, from the tonic */
    int t = 0, d = pr_below(&s, 3) * 2;         /* the first note: the tonic, its third or its fifth */
    pfl_note mel[PFL_MAX];
    int nm = 0;
    for (int ph = 0; ph < PHRASES[v]; ph++)
        for (int bar = 0; bar < 4; bar++) {
            const int *r = BARS[v][pr_below(&s, 8)];
            int last = ph == PHRASES[v] - 1 && bar == 3;
            if (bar == 3) r = CLOSE[last ? 0 : pr_below(&s, 2)];
            for (int j = 0; j < 5 && r[j] && nm < PFL_MAX / 2; j++) {
                int end = bar == 3 && (j == 4 || !r[j + 1]);
                if (nm) {                       /* a step, sometimes a leap; a phrase ends on the tonic or the fifth */
                    int step = (int)pr_below(&s, 7) - 3;
                    if (step == 3 || step == -3) step = v == PFL_EASY ? step / 3 : step + (step > 0 ? 1 : -1) * (int)pr_below(&s, 2);
                    if (!step) step = pr_below(&s, 2) ? 1 : -1;
                    d += step;
                    if (end) d = last || pr_below(&s, 2) ? (d > 3 ? 7 : 0) : 4;
                }
                d = d < lo ? lo + (lo - d) % 2 : d > hi ? hi - (d - hi) % 2 : d;
                mel[nm].t = t;
                mel[nm].len = r[j];
                mel[nm].pitch = degree_pitch(k, d);
                nm++;
                t += r[j];
            }
        }
    /* the second voice (hard): a third below on the strong beats, else a sixth */
    pfl_note all[PFL_MAX];
    int n = 0, top = 0;
    for (int i = 0; i < nm; i++) {
        all[n++] = mel[i];
        if (mel[i].pitch > top) top = mel[i].pitch;
    }
    int lowest = PFL_LOWEST, w = 0;              /* the second voice's lowest: the keyboard 15 white keys at most */
    for (int p = white_down(lowest); p <= white_up(top); p++) w += !pfl_black(p);
    for (; w > PFL_WHITES_MAX; lowest++)
        if (!pfl_black(lowest)) w--;
    while (pfl_black(lowest)) lowest++;
    if (v == PFL_HARD)
        for (int i = 0; i < nm && n < PFL_MAX; i++) {
            if (mel[i].t % 8 || mel[i].len < 4) continue;
            for (int c = 0; c < 2; c++) {
                int want = mel[i].pitch, q = -1;
                for (int dd = -14; dd <= 14; dd++)   /* the melody note's degree: then two (or five) below */
                    if (degree_pitch(k, dd) == want) q = degree_pitch(k, dd - (c ? 5 : 2));
                if (q < lowest) continue;
                int clash = 0;
                for (int o = 0; o < n; o++)
                    if (all[o].pitch == q && all[o].t < mel[i].t + mel[i].len + 2 && mel[i].t < all[o].t + all[o].len + 2)
                        clash = 1;
                if (clash) continue;
                all[n].t = mel[i].t;
                all[n].len = mel[i].len;
                all[n].pitch = q;
                n++;
                break;
            }
        }
    qsort(all, (size_t)n, sizeof all[0], note_cmp);
    g->n = n;
    for (int i = 0; i < n; i++) {
        g->note[i].t = ms_of(all[i].t, g->bpm);
        g->note[i].len = ms_of(all[i].len, g->bpm);
        g->note[i].pitch = all[i].pitch;
    }
    fit_keys(g);
    fresh(g);
}

/* ---- playing ----------------------------------------------------------------------------------------------------- */
static void rescan(pfl_game *g)
{
    while (g->scan < g->n && g->judge[g->scan] != PFL_PENDING) g->scan++;
}

void pfl_advance(pfl_game *g, int t)
{
    if (t > g->now) g->now = t;
    for (int i = g->scan; i < g->n && g->note[i].t + PFL_GOOD_MS < g->now; i++)
        if (g->judge[i] == PFL_PENDING) {
            g->judge[i] = PFL_MISS;
            g->miss++;
            g->combo = 0;
        }
    for (int i = 0; i < g->n; i++)
        if (g->hold[i] == PFL_HOLDING && g->now >= g->note[i].t + g->note[i].len) g->hold[i] = PFL_HELD;
    rescan(g);
}

int pfl_press(pfl_game *g, int pitch, int t)
{
    pfl_advance(g, t);
    int best = -1, bd = PFL_GOOD_MS + 1;
    for (int i = g->scan; i < g->n && g->note[i].t <= t + PFL_GOOD_MS; i++) {
        if (g->judge[i] != PFL_PENDING || g->note[i].pitch != pitch) continue;
        int dt = abs(t - g->note[i].t);
        if (dt < bd) {
            bd = dt;
            best = i;
        }
    }
    if (best < 0) {
        g->stray++;
        g->combo = 0;
        return -1;
    }
    int e = t - g->note[best].t;
    g->judge[best] = bd <= PFL_PERFECT_MS ? PFL_PERFECT_HIT : PFL_GOOD_HIT;
    if (g->judge[best] == PFL_PERFECT_HIT) g->perfect++;
    else g->good++;
    g->err[best] = (int16_t)e;
    g->err_sum += e;
    g->err_n++;
    g->hold[best] = pfl_is_hold(g, best) ? PFL_HOLDING : PFL_TAP;
    if (++g->combo > g->best_combo) g->best_combo = g->combo;
    rescan(g);
    return best;
}

void pfl_release(pfl_game *g, int i, int t)
{
    if (i < 0 || i >= g->n || g->hold[i] != PFL_HOLDING) return;
    g->hold[i] = t >= g->note[i].t + g->note[i].len - PFL_HOLD_SLACK ? PFL_HELD : PFL_BROKEN;
}

int pfl_over(const pfl_game *g)
{
    if (g->n <= 0) return 0;
    for (int i = 0; i < g->n; i++)
        if (g->judge[i] == PFL_PENDING || g->hold[i] == PFL_HOLDING) return 0;
    return 1;
}

int pfl_points(const pfl_game *g)
{
    int p = 0;
    for (int i = 0; i < g->n; i++) {
        int q = g->judge[i] == PFL_PERFECT_HIT ? 100 : g->judge[i] == PFL_GOOD_HIT ? 60 : 0;
        p += g->hold[i] == PFL_BROKEN ? q / 2 : q;
    }
    return p;
}

int pfl_accuracy(const pfl_game *g) { return g->n > 0 ? pfl_points(g) / g->n : 0; }

int pfl_stars(int accuracy) { return accuracy >= 95 ? 3 : accuracy >= 80 ? 2 : accuracy >= 50 ? 1 : 0; }

int pfl_calibrate(const pfl_game *g, int offset)
{
    if (g->err_n < 8) return offset;
    int o = offset + g->err_sum / g->err_n / 2;
    return o < -PFL_OFFSET_MAX ? -PFL_OFFSET_MAX : o > PFL_OFFSET_MAX ? PFL_OFFSET_MAX : o;
}

void pfl_autoplay(pfl_game *g, int t, int late)
{
    for (;;) {
        int at = t + 1, which = -1, up = 0;     /* the next press or let-go, in time */
        for (int i = g->scan; i < g->n; i++) {
            if (g->note[i].t + late >= at) break;
            if (g->judge[i] == PFL_PENDING) {
                at = g->note[i].t + late;
                which = i;
                break;
            }
        }
        for (int i = 0; i < g->n; i++)
            if (g->hold[i] == PFL_HOLDING && g->note[i].t + g->note[i].len < at) {
                at = g->note[i].t + g->note[i].len;
                which = i;
                up = 1;
            }
        if (which < 0) break;
        if (up) pfl_release(g, which, at);
        else pfl_press(g, g->note[which].pitch, at);
    }
    pfl_advance(g, t);
}

int pfl_script(pfl_game *g, const char *moves)
{
    const char *s = moves;
    while (*s) {
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (!strncmp(s, "end", 3)) {
            pfl_autoplay(g, pfl_end_ms(g) + 1, 0);
            s += 3;
            continue;
        }
        char c = *s++;
        if ((c != 'p' && c != 'g' && c != 'm') || *s < '0' || *s > '9') return 0;
        int t = 0;
        while (*s >= '0' && *s <= '9') t = t * 10 + (*s++ - '0');
        if (c == 'm') pfl_advance(g, t);
        else pfl_autoplay(g, t, c == 'g' ? 100 : 0);
    }
    return 1;
}

int pfl_check(const pfl_game *g)
{
    if (g->n <= 0 || g->n > PFL_MAX) return 1;
    if (pfl_black(g->low) || pfl_black(g->high) || g->low < PFL_LOWEST - 1 || g->high > PFL_HIGHEST) return 2;
    if (pfl_whites(g) > PFL_WHITES_MAX || pfl_whites(g) < 8) return 3;
    for (int i = 0; i < g->n; i++) {
        const pfl_note *a = &g->note[i];
        if (a->len <= 0) return 4;
        if (a->pitch < g->low || a->pitch > g->high || a->pitch < PFL_LOWEST || a->pitch > PFL_HIGHEST) return 5;
        if (i && a->t < g->note[i - 1].t) return 6;
        int at_once = 0;
        for (int j = i + 1; j < g->n; j++) {
            const pfl_note *b = &g->note[j];
            if (b->t == a->t) at_once++;
            if (b->pitch != a->pitch) continue;
            if (b->t < a->t + a->len || b->t - a->t < PFL_GAP_MIN) return 7;
            break;
        }
        if (at_once > 1) return 8;
    }
    return 0;
}
