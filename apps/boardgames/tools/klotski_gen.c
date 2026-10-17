/*
 * Klotski's levels: src/games/klotski_levels.h. Our own levels: no published or traditional set is used. A tray:
 * blocks laid by chance (the great block, six, five or four blocks of 1 x 2 standing or lying, small ones until two
 * cells are left); its whole family of positions found by the rules (klt_family: every position the blocks can reach,
 * and the fewest moves out of each, searched back from the solved ones). A family met before, or the mirror of one
 * (read from the right), is left out; so is a family whose great block never gets out (most of the small ones). Few
 * families are both large and deep, so each one gives many levels: at every distance from the way out, the few
 * positions that look best set out by hand (taste_of: the blocks alike on both sides, as in the traditional trays;
 * the great block high and in the middle, the way out far; the two empty cells side by side), the great block in the
 * upper half when it can be. The difficulty: the fewest moves, a little more in a large family (more ways to go
 * wrong). Each chapter has its range of moves and keeps 25 candidates spread evenly over its difficulties, from the
 * easiest: at each step the best looking of a few close in difficulty, the families mixed (one met in the chapter
 * counts less); the chapter's last level, its showpiece, the best looking among its hardest tenth. Never two at one
 * distance from one family, never a position or its mirror twice. The trays: 400 laid by chance, then the ones met
 * before (KNOWN, the families a run of 6000 found). Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/kltgen apps/boardgames/tools/klotski_gen.c -lm && \
 *       /tmp/kltgen > apps/boardgames/src/games/klotski_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define KLT_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/klotski_rules.c"

#define PER (KLT_LEVELS / KLT_CHAPTERS)
#define LAYS 400
#define SAMPLES 3                               /* the positions kept a distance a family */
#define FAR 256
#define MAXC 60000

static const int LO[KLT_CHAPTERS] = {3, 10, 20, 35, 50, 70}, HI[KLT_CHAPTERS] = {9, 19, 34, 49, 69, 255};

/* trays met before (the families a longer run of 6000 trays laid by chance found worth playing), looked at again
 * after the few laid by chance */
static const char *const KNOWN[] = {
    "S00 T01 W02 S10 T12 W20 B30 T32 T33", "T00 T01 T02 T03 S20 W21 B30 S32 T33",
    "S00 S01 T02 S03 W10 S13 B21 T23 T30 S41 S42", "W00 T02 T03 S10 S11 W22 T30 T31 B32",
    "W00 W02 W10 S12 S13 S20 W21 B30 S32 S42 S43", "W00 W02 S10 W11 T13 W21 S30 T31 B32",
    "W00 S03 T10 S11 W12 B21 T23 T30 S41 S43", "T00 W02 W11 T13 S20 W21 B30 S32 T33",
    "S00 T02 S03 T10 T11 W22 B30 T32 T33", "T00 W01 T11 S13 T20 S22 S23 T31 B32 S40",
    "W00 T03 S10 T11 T12 T23 B30 T32 S43", "S01 T02 T03 T10 T11 W22 B30 S32 T33",
    "W00 T02 T03 W10 W20 B30 W32 S42 S43", "S00 T01 T02 T03 S10 T21 W22 T30 B32",
    "W01 T03 S10 W11 T20 S21 W22 T31 B32", "W00 W02 S10 W11 S13 B20 W22 S32 S33 S40 S41",
    "S01 W02 S10 T12 T13 W20 T30 T31 B32", "W00 W02 W10 W12 S22 S23 T30 T31 B32",
    "T01 W02 T10 W12 S22 S23 T30 T31 B32", "W00 T02 S03 T10 T11 T23 B30 T32 S43",
    "T00 S01 T02 T03 W21 S23 B30 S32 T33 S42", "W00 S02 T03 W10 T20 W21 T31 B32 S40",
    "S00 S01 S02 T10 T11 T12 S23 B30 S32 T33 S42", "S00 W01 S03 W10 W12 S20 S21 B22 W30 S40 S42",
    "W00 W02 S10 W12 S20 T22 S23 B30 T33 S42", "T00 T01 T12 T13 B20 W32 S40 S41 W42",
    "W00 W02 S10 W11 B20 S22 W32 S40 S41 S42 S43", "S00 S01 W02 W10 T12 T13 T21 T30 B32",
    "W00 W02 S10 W12 B20 S22 W32 S40 S41 S42 S43", "T01 W02 T10 S12 S13 T21 S22 T30 B32 S41",
    "S01 T02 S03 T10 T13 W21 T30 T31 B32", "T00 T01 W02 W12 B20 S22 T32 T33 S40",
    "W01 T03 W10 T12 S21 S23 B30 T32 T33", "T00 S01 W02 T12 W20 S23 T30 T31 B32",
    "W00 W02 W11 T13 B20 T32 T33 S40 S41", "W00 T02 T03 W20 B22 T30 T31 S42 S43",
    "T00 T01 W02 T12 B20 T32 T33 S40 S41", "S00 W01 T03 T10 W11 B22 T30 T31 S42",
    "S00 S02 T03 T10 T11 W22 T30 T31 B32", "W00 S02 S03 T10 B11 T13 T30 S32 T33 S42",
    "S00 T01 S02 S03 T10 B12 T30 T31 S32 S42 S43", "W00 W02 T10 S11 B12 T21 T30 S32 S33 S41",
    "S00 S01 S02 S03 W10 B12 W20 W30 W32 S42 S43", "T00 S02 T03 T11 S12 T20 W22 T31 B32",
    "W00 W02 B10 W12 W22 S30 T31 S32 S40 S43", "S00 W01 S03 W10 B12 S21 S30 W31 S33 W40 S43",
    "W00 W02 W10 B12 S20 S21 S30 T32 S33 S41 S43", "T00 T01 T02 S03 T13 B21 S30 S33 S40 W42",
    "S00 S01 W02 T10 T11 B12 W30 S32 W41 S43", "S00 S01 W02 W10 B12 T20 T21 S33 W40 S43",
    "W00 B02 W10 W20 S23 W30 S32 W40 W42", "W00 B02 W10 W20 S22 S23 W31 W40 W42",
    "T00 S01 S02 T03 B11 T20 T23 S31 W40 S43", "B00 W02 S12 S13 W20 W22 S31 W32 S41 S42 S43",
    "T00 T01 S03 T12 W20 S23 T30 T31 B32", "W00 B02 T10 T11 S22 S23 T32 T33 S40 S41",
    "S01 B02 T10 S11 S21 T22 T23 T30 S31 S41 S43", "W00 B02 W10 W20 W22 S31 T32 S33 S40 S43",
    "W00 B02 W10 S20 S22 S23 W30 S32 W40 W42", "T00 S01 W02 B11 S13 T20 S31 S32 T33 W40",
    "T00 T01 S02 S03 B12 T20 T21 W32 S42 S43", "B00 S02 T03 S20 S21 S22 T23 S30 T31 T32 S43",
    "W00 S02 T03 B11 S20 T23 T30 S32 S41 W42", "B00 W02 W12 W21 T23 W30 T32 S41 S43",
    "W00 W02 W10 B12 S20 S30 W31 W40 W42", "W00 B02 T11 T20 S22 S31 T32 T33 S40 S41",
    "W00 B02 W10 S20 S21 T23 W30 T32 W40", "W00 S02 T03 B10 S12 T22 S23 W30 W41 S43",
    "B00 S02 S03 W12 W20 S22 S23 W30 S32 S40 W41", "B00 W12 S20 S21 W22 S30 S31 S32 T33 W40 S42",
    "S00 W01 S10 T11 B12 S20 T30 S31 W32 W42", "S01 S02 T03 B10 T12 S23 W30 T32 T33 S41",
    "S00 S01 B02 W10 S21 W22 S30 W31 S33 S41 W42", "B00 S03 T12 S13 S20 S21 S30 S31 W32 W40 W42",
    "W00 S03 T10 S11 B12 S30 S31 T32 T33 W40", "S00 S01 W02 B10 W12 S22 S23 W30 W32 W40",
    "W01 S03 W11 S13 W20 W22 W30 B32 W40", "W00 W02 B11 S13 S20 W30 S32 S33 W40 W42",
    "S00 W01 S03 S10 T11 B12 S30 T32 T33 W40", "W00 W02 S10 B11 S13 S20 S30 S31 W32 S40 W42",
    "S00 S01 W02 W10 B12 W20 W32 W40 W42", "W01 S03 B10 T12 T13 T30 T32 S33 S41 S43",
    "T00 S02 S03 B11 T13 S30 T31 T32 S33 S40 S43", "W00 W02 S10 T11 B12 S20 W31 S40 W41 S43",
    "S00 T01 W02 S10 B12 S20 S21 S30 W31 W41 S43", "W00 B02 T10 S11 T21 W22 S32 S33 W41 S43",
    "W01 S03 S10 S11 B12 S20 S30 S31 W32 W40 W42", "S00 W01 S11 B12 W20 W30 W32 W40 W42",
    "S00 B01 T03 S20 T21 W22 T30 S32 T33 S41", "T00 S01 S02 T03 B11 S20 S30 T31 T32 S33 S43",
    "B00 S02 S03 W12 T20 S21 T22 T31 T33 S40", "B00 W02 S12 W20 S22 T23 W30 T32 W40",
    "W00 B02 S10 S11 T20 T22 S31 T33 S40 W41", "S00 W01 S03 B10 S12 S13 W22 W31 S33 S40 W41",
    "B00 S02 S03 S12 S13 T20 W22 W31 S40 W41 S43", "B00 S02 S03 W12 W20 S22 T23 W31 W40 S42",
    "T01 B02 T10 S21 S22 S23 S31 T32 T33 W40", "W00 S10 S11 B12 W20 W30 W32 S40 W41 S43",
    "S00 B01 T03 S10 W20 S22 T23 W30 S32 W42", "S00 S01 W02 B10 T12 S30 W31 T33 W40 S42",
    "B00 W02 T13 W20 W30 T32 S33 W40 S43", "W00 S02 S10 B11 S13 S23 W30 W32 S40 W41 S43",
    "S01 S02 S10 S11 B12 W20 W30 W32 S40 W41 S43", "W00 B02 S10 S11 W20 W22 S30 W31 W41 S43",
    "W00 B02 T10 S11 S21 W22 W30 S33 W40 S43", "B00 S02 S03 S12 S13 W20 W22 W31 S40 W41 S43",
    "S01 B02 S10 S11 W20 T22 S23 W30 W40 S42 S43", "W01 T10 S11 B12 S21 S30 W31 S33 W40 W42",
    "B00 S03 S12 S13 W21 S23 W30 W32 S40 W41 S43", "S00 S01 S02 S03 B10 W12 W22 T30 W31 S33 S43",
    "B00 W02 T12 S13 S20 S21 S30 W31 W40 W42", "B00 T02 T03 T20 T22 S23 S33 S40 S41 W42",
    "S00 B01 T03 S10 S20 W21 T23 T30 T32 S41", "S00 S01 W02 B10 T12 T13 W30 W32 W41",
    "S00 B01 S03 T13 W20 S22 S31 W32 S40 W41 S43", "S00 S01 W02 S10 B11 T20 T23 S31 W40 W42",
    "W00 B02 T10 S11 S21 W22 T31 T33 S40 S42", "B00 W02 S12 S20 S21 W22 W30 T33 W40 S42",
    "T00 B01 S03 T13 W20 S30 T31 S32 T33 S42", "W00 T02 T03 B10 S30 S31 W32 W40 W42",
    "B00 W02 T13 S20 W21 S30 W31 T33 W40", "W01 T03 B10 T12 S30 S31 W32 W40 W42",
    "T00 T01 B02 S20 S21 S22 W30 T32 T33 S40", "B00 S02 T03 T20 W21 T23 T31 S32 S40 S43",
    "B02 W10 T20 W21 S23 S31 W32 S40 S41 W42", "T00 S02 S03 B12 W20 S30 S31 W32 W40 W42",
    "T00 T01 B02 W20 T23 S30 S31 T32 S40 S43", "B00 S02 T03 S12 W20 S22 T23 T30 W31 S43",
    "B00 W02 S12 T13 S20 W21 S30 T31 T32 S43", "T00 B01 S03 S13 W20 W22 W31 S40 S41 W42",
    "S00 B01 S10 T13 W20 W30 W32 S40 S41 W42", "T00 B01 T03 T20 W21 T23 S40 S41 S42 S43",
    "S00 B01 T03 T10 W22 S30 T31 S32 T33 S40", "T00 B01 S03 T13 W20 T22 T30 S33 S42 S43",
    "T00 S01 B02 S11 W20 S22 T23 W30 S32 W41", "T00 B01 T03 W20 S22 S23 W30 T32 S33 S41",
    "T00 S01 T03 B11 S20 S30 W31 S33 W40 W42", "S00 T01 B02 S10 T22 S23 W30 T33 W40 S42",
    "B00 T02 T03 S20 W21 W30 T33 S40 S41 S42", "T01 S02 T10 B12 S21 S30 W31 T33 S40 W41",
    "B00 S02 T03 S20 W21 S23 T30 W32 W41 S43", "S00 B02 T10 S11 T21 W22 T30 S32 S33 W42",
    "T00 B02 S11 S20 S21 W22 W30 T32 W40 S43", "S00 S01 T02 T03 B10 S23 W30 T33 S40 W41",
    "T00 S01 B02 T11 S20 S23 T30 S31 W32 W42", "B00 S02 S13 S20 W21 S23 T30 W31 T33 W41",
    "B00 S03 S12 T13 W20 S22 S31 T32 T33 W40", "S00 B01 S03 T13 S20 W21 S30 T31 W32 W42",
    "S00 S02 T10 S11 B12 T21 T30 W32 W41 S43", "T00 S01 B02 W20 T22 S23 W30 W40 S42 S43",
};
#define NKNOWN ((int)(sizeof KNOWN / sizeof *KNOWN))

typedef struct { int np; klt_piece p[KLT_PIECES]; } tray;

typedef struct {
    char text[KLT_PIECES * 4 + 1];
    uint64_t key;                               /* the position's, its mirror's alike */
    int dist, size, fam;
    double diff, taste;
} cand;

static cand cands[MAXC];
static int ncand;
static uint64_t *sigs;
static int nsigs;
static uint32_t rng;

/* the family being looked at: its size, its farthest position, and at each distance the few positions that look
 * best (two lists: the great block high, and any) */
typedef struct {
    uint64_t sig;
    int size, far;
    int seen[FAR][2];
    tray pick[FAR][2][SAMPLES];
    double taste[FAR][2][SAMPLES];
} look;

/* how well a tray looks set out by hand (higher: better): its blocks alike on both sides (a tray read from the right
 * the same, as the traditional ones are), the great block high and in the middle (the way out looks far), the two
 * empty cells side by side */
static double taste_of(const klt_piece *p, int np)
{
    int8_t kind[KLT_W * KLT_H];
    memset(kind, -1, sizeof kind);
    int bx = 0, by = 0;
    for (int i = 0; i < np; i++) {
        int t = p[i].type;
        if (t == KLT_BIG) bx = p[i].x, by = p[i].y;
        for (int dy = 0; dy < PH[t]; dy++)
            for (int dx = 0; dx < PW[t]; dx++) kind[p[i].x + dx + (p[i].y + dy) * KLT_W] = (int8_t)t;
    }
    int same = 0, gap[2], ng = 0;
    for (int y = 0; y < KLT_H; y++)
        for (int x = 0; x < KLT_W / 2; x++) same += kind[x + y * KLT_W] == kind[KLT_W - 1 - x + y * KLT_W];
    for (int c = 0; c < KLT_W * KLT_H && ng < 2; c++)
        if (kind[c] < 0) gap[ng++] = c;
    int side = ng == 2 && ((gap[1] - gap[0] == 1 && gap[0] % KLT_W != KLT_W - 1) || gap[1] - gap[0] == KLT_W);
    return 2.5 * same / (KLT_W / 2 * KLT_H) + (by == 0 ? 1.0 : by == 1 ? 0.5 : 0) + (bx == 1 ? 0.8 : 0) +
           (side ? 0.3 : 0);
}

/* blocks laid by chance: the great block, m of 1 x 2 (standing or lying), small ones until two cells are left */
static int lay(klt_piece *p, uint32_t *s)
{
    static const int SETS[3][2] = {{6, 2}, {5, 4}, {4, 6}};
    const int *set = SETS[pr_below(s, 3)];
    int np = 0;
    int8_t at[KLT_W * KLT_H];
    memset(at, -1, sizeof at);
    for (int kind = 0; kind < 3; kind++) {
        int count = kind == 0 ? 1 : kind == 1 ? set[0] : set[1];
        for (int k = 0; k < count; k++) {
            int t = kind == 0 ? KLT_BIG : kind == 2 ? KLT_SMALL : pr_below(s, 2) ? KLT_TALL : KLT_WIDE, placed = 0;
            for (int tries = 0; tries < 60 && !placed; tries++) {
                if (kind == 1 && tries == 30) t = t == KLT_TALL ? KLT_WIDE : KLT_TALL;
                p[np].type = (int8_t)t;
                p[np].x = (int8_t)pr_below(s, KLT_W - PW[t] + 1);
                p[np].y = (int8_t)pr_below(s, KLT_H - PH[t] + 1);
                if (!fits(at, p, np, p[np].x, p[np].y)) continue;
                for (int dy = 0; dy < PH[t]; dy++)
                    for (int dx = 0; dx < PW[t]; dx++) at[p[np].x + dx + (p[np].y + dy) * KLT_W] = (int8_t)np;
                np++;
                placed = 1;
            }
            if (!placed) return 0;
        }
    }
    return valid(p, np) ? np : 0;
}

static uint64_t mirror_code(const klt_piece *src, int np)
{
    klt_piece p[KLT_PIECES];
    memcpy(p, src, sizeof p);
    for (int i = 0; i < np; i++) p[i].x = (int8_t)(KLT_W - PW[p[i].type] - p[i].x);
    return code_of(p, np);
}

static uint64_t key_of(const klt_piece *p, int np)
{
    uint64_t a = code_of(p, np), b = mirror_code(p, np);
    return a < b ? a : b;
}

static void first_pass(const klt_game *pos, int dist, void *ctx)
{
    look *L = ctx;
    uint64_t m = key_of(pos->p, pos->np);
    if (!L->sig || m < L->sig) L->sig = m;
    if (dist > L->far) L->far = dist;
    L->size++;
}

static int big_y(const klt_piece *p, int np)
{
    for (int i = 0; i < np; i++)
        if (p[i].type == KLT_BIG) return p[i].y;
    return 0;
}

static void second_pass(const klt_game *pos, int dist, void *ctx)
{
    look *L = ctx;
    if (dist < LO[0] || dist >= FAR) return;
    int high = big_y(pos->p, pos->np) <= 1, n = ++L->seen[dist][high], slot = n - 1;
    double t = taste_of(pos->p, pos->np);
    if (slot >= SAMPLES) {                      /* the one that looks worst makes way */
        slot = 0;
        for (int s = 1; s < SAMPLES; s++)
            if (L->taste[dist][high][s] < L->taste[dist][high][slot]) slot = s;
        if (t <= L->taste[dist][high][slot]) return;
    }
    L->pick[dist][high][slot].np = pos->np;
    memcpy(L->pick[dist][high][slot].p, pos->p, sizeof pos->p);
    L->taste[dist][high][slot] = t;
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static look L;
    static klt_game g;
    int failed = 0, stuck = 0, far_hist[20] = {0};
    rng = pr_seed(4000);
    sigs = malloc((LAYS + NKNOWN) * sizeof *sigs);
    if (!sigs) return 1;
    for (int lay_no = 0; lay_no < LAYS + NKNOWN; lay_no++) {
        memset(&g, 0, sizeof g);
        if (lay_no >= LAYS) klt_parse(&g, KNOWN[lay_no - LAYS]);
        else g.np = lay(g.p, &rng);
        if (!g.np) { failed++; continue; }
        memset(&L, 0, sizeof L);
        if (!klt_family(&g, first_pass, &L)) { failed++; continue; }
        int met = 0;
        for (int i = 0; i < nsigs && !met; i++) met = sigs[i] == L.sig;
        if (met) continue;
        sigs[nsigs++] = L.sig;
        far_hist[L.far / 10 < 19 ? L.far / 10 : 19]++;
        if (L.far < LO[0]) { stuck++; continue; }
        klt_family(&g, second_pass, &L);
        for (int d = LO[0]; d < FAR && d <= L.far; d++) {
            int high = L.seen[d][1] > 0, n = L.seen[d][high] < SAMPLES ? L.seen[d][high] : SAMPLES;
            for (int s = 0; s < n && ncand < MAXC; s++) {
                tray *t = &L.pick[d][high][s];
                klt_game pos;
                pos.np = t->np;
                memcpy(pos.p, t->p, sizeof pos.p);
                cand *c = &cands[ncand++];
                klt_text(&pos, c->text);
                c->key = key_of(t->p, t->np);
                c->dist = d;
                c->size = L.size;
                c->fam = nsigs - 1;
                c->diff = d + 0.8 * log2((double)L.size) + (high ? 0 : 0.5) + 0.01 * s;
                c->taste = L.taste[d][high][s];
            }
        }
        if (lay_no % 100 == 0) fprintf(stderr, "%d trays, %d families, %d candidates\n", lay_no, nsigs, ncand);
    }
    fprintf(stderr, "%d families, %d trays not laid or too large, %d families stuck; the farthest:", nsigs, failed,
            stuck);
    for (int i = 0; i < 20; i++) fprintf(stderr, " %d+:%d", 10 * i, far_hist[i]);
    fprintf(stderr, "\n");
    qsort(cands, (size_t)ncand, sizeof *cands, by_diff);
    static cand out[KLT_LEVELS], pool[MAXC];
    int nout = 0;
    for (int c = 0; c < KLT_CHAPTERS; c++) {
        int npool = 0;
        for (int i = 0; i < ncand; i++)
            if (cands[i].dist >= LO[c] && cands[i].dist <= HI[c]) pool[npool++] = cands[i];
        fprintf(stderr, "chapter %d: %d candidates, %d .. %d moves\n", c + 1, npool, npool ? pool[0].dist : 0,
                npool ? pool[npool - 1].dist : 0);
        int used_fam[64], nfam = 0;
        for (int i = 0, last = -1; i < PER && npool; i++) {
            double q = c == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)(q * (npool - 1) + 0.5), span = npool / (3 * PER);
            if (at <= last) at = last + 1;
            if (i == PER - 1 && at < npool * 9 / 10) at = npool * 9 / 10;   /* the showpiece: among the hardest */
            int to = i == PER - 1 ? npool - 1 : at + span, best = -1;
            double best_score = -1e9;
            if (to > npool - 1) to = npool - 1;
            if (c == 0 && i == 0) to = at;
            for (int k = at; k <= to || (best < 0 && k < npool); k++) {
                int bad = 0;                    /* not one distance of a family twice, nor a position */
                for (int o = 0; o < nout && !bad; o++)
                    bad = out[o].key == pool[k].key || (out[o].fam == pool[k].fam && out[o].dist == pool[k].dist);
                if (bad) continue;
                double score = pool[k].taste;   /* the families mixed: one met in the chapter already counts less */
                for (int f = 0; f < nfam; f++) score -= used_fam[f] == pool[k].fam ? 0.7 : 0;
                if (score > best_score) { best_score = score; best = k; }
            }
            if (best < 0) best = at < npool ? at : npool - 1;
            last = best;
            if (nfam < 64) used_fam[nfam++] = pool[best].fam;
            out[nout++] = pool[best];
        }
    }
    printf("/*\n"
           " * Klotski's levels: our own, made by tools/klotski_gen.c (blocks laid by chance, every position of their family\n"
           " * and its fewest moves found by the rules, six chapters of 25 from the easiest). Each: the blocks (B the great\n"
           " * block, T standing, W lying, S small; their top left cell, row then column), then (in the comment) its fewest\n"
           " * moves and its family's size; the fewest moves again in klt_level_par (the tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const klt_levels[KLT_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: %d moves, a family of %d */\n", out[i].text, i + 1, out[i].dist, out[i].size);
    printf("};\n\nconst uint8_t klt_level_par[KLT_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].dist);
    printf("\n};\n\nconst uint32_t klt_level_family[KLT_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 15 ? " " : "\n    ", out[i].size);
    printf("\n};\n");
    free(sigs);
    for (int i = 0; i < nout; i++)
        for (int k = 0; k < i; k++)
            if (out[i].key == out[k].key) return 1;
    return nout == KLT_LEVELS ? 0 : 1;
}
