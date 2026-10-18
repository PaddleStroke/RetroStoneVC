/*
 * Defuse the Bomb's levels, made by chance and chosen: src/games/defuse_levels.h. Our own levels: no published set
 * is used. Band by band (the plan below), the rules' generator (df_make, defuse.h) grows circuits of the band's
 * size and parts, and keeps a circuit when it is a puzzle:
 *   - the start is safe and not won; a way to win exists (the solver: every state the safe moves reach), its par in
 *     the band's range;
 *   - something always blows (a first move lights DETONATE), but not every first move;
 *   - from the NOT traps on: a trap (a move of the best way, made first, blows; or the move that lights most LEDs at
 *     once is the one that blows, or leaves no way to win); with the cables, some first cuts leave no way at all;
 *   - every switch, cable and memory chip matters, no gate is stuck;
 *   - a clear drawing (Logic Gates' router) with few crossings;
 *   - no repeats (the same values in every state: the same puzzle).
 * From each band's candidates the levels are taken evenly over their difficulty (df_rate), then laid out from the
 * easiest; each one's drawing is searched at length (lg_route_best) and stored after its text.
 * The chapters, one idea each: 1 (40 levels: the free edition's) switches, AND and OR (its first ten drawn by hand
 * below, each visibly its own), then NOT and its traps (the order of the moves); 2 cuttable wires (for good); 3 the
 * battery and the bomb's power socket (cut the power, or light the LEDs); 4 XOR; 5 push buttons and memory chips; 6
 * everything, the biggest. One or two disarm LEDs up to chapter 3, three from chapter 4, four in chapter 6.
 *   gcc -O2 -DDF_NO_LEVELS -Iapps/boardgames/src -o /tmp/dfgen apps/boardgames/tools/defuse_gen.c \
 *       apps/boardgames/src/games/defuse_rules.c apps/boardgames/src/games/logicgates_rules.c -lm \
 *       && /tmp/dfgen > apps/boardgames/src/games/defuse_levels.h
 * (deterministic: fixed seeds; -DDF_NO_LEVELS: the rules built without the levels, made here)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/defuse.h"

#define K(x) (1u << LG_##x)

typedef struct {
    int n;                                      /* levels */
    df_spec sp;
} band;

/* chapter 1's first ten, drawn by hand, one idea each (the switches' places only: the router draws them): a lamp
 * and DETONATE; every disarm LED at once; OR (one input is enough); AND (both); a gate into a gate, a switch already
 * on; a switch to turn off first (the order); one switch for two lamps; power led round through an OR before a cut;
 * a diamond of ORs (one switch lights both); and a showpiece, mirrored: the middle switch off first, then both sides */
static const char *const hand[] = {
    "~ s@a1 s@b1 L@a2(a1) A@b2(b1)",
    "~ s@a1 s@b1 s@c1 L@a2(a1) A@b2(b1) L@c2(c1)",
    "~ s@a1 s@b1 s@c1 A@a3(a1) OR@b2(b1,c1) L@b3(b2)",
    "~ s@a1 s@b1 s@c1 s@d1 AND@a2(a1,b1) L@a3(a2) L@b3(c1) A@c3(d1)",
    "~ s@a1 s@b1 s@c1 S@d1 OR@a2(a1,b1) AND@a3(a2,c1) L@a4(a3) AND@b2(b1,d1) A@b4(b2)",
    "~ S@a1 s@b1 s@c1 AND@a2(a1,b1) OR@b2(a1,c1) A@a3(a2) L@b3(b2) L@c3(b1)",
    "~ s@a1 s@b1 S@c1 AND@a2(a1,b1) OR@b2(b1,c1) AND@c2(a1,c1) L@a3(a2) L@b3(b2) A@c3(c2)",
    "~ S@a1 s@b1 S@c1 s@d1 s@e1 OR@a2(c1,e1) AND@a3(a1,a2) AND@b2(b1,d1) AND@c2(c1,d1) L@a4(a3) L@b4(b2) A@c4(c2)",
    "~ s@a1 s@b1 s@c1 S@d1 OR@a2(a1,b1) OR@b2(b1,c1) AND@a3(a2,b2) AND@c2(a1,c1) AND@d2(b1,d1) OR@c3(c2,d2) "
    "L@a4(a3) A@c4(c3)",
    "~ s@a1 s@b1 S@c1 s@d1 s@e1 AND@a2(a1,b1) AND@b2(b1,c1) AND@c2(c1,d1) AND@d2(d1,e1) OR@b3(b2,c2) L@a4(a2) "
    "A@b4(b3) L@c4(d2)",
};
#define HAND ((int)(sizeof hand / sizeof hand[0]))

static band plan[] = {
    /* chapter 1 after the ten by hand: switches, AND, OR; then NOT */
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 2, .gates_hi = 3, .kinds = K(AND) | K(OR), .leds_lo = 1, .leds_hi = 2,
          .par_lo = 2, .par_hi = 4, .depth_hi = 3, .cross = 1}},
    {20, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 2, .gates_hi = 4, .kinds = K(AND) | K(OR) | K(NOT), .need = K(NOT),
          .leds_lo = 1, .leds_hi = 2, .par_lo = 2, .par_hi = 4, .trap = 1, .depth_hi = 3, .cross = 1}},
    /* chapter 2: cuttable wires */
    {10, {.sw_lo = 2, .sw_hi = 3, .gates_lo = 2, .gates_hi = 4, .kinds = K(AND) | K(OR) | K(NOT), .leds_lo = 1,
          .leds_hi = 2, .fuses_lo = 1, .fuses_hi = 2, .par_lo = 2, .par_hi = 4, .trap = 1, .depth_hi = 3, .cross = 1}},
    {12, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = K(AND) | K(OR) | K(NOT), .leds_lo = 2,
          .leds_hi = 2, .fuses_lo = 2, .fuses_hi = 3, .par_lo = 3, .par_hi = 5, .trap = 1, .dead = 1, .depth_hi = 4,
          .cross = 2}},
    /* chapter 3: the battery and the power socket */
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 2, .gates_hi = 4, .kinds = K(AND) | K(OR) | K(NOT), .leds_lo = 1,
          .leds_hi = 2, .fuses_lo = 1, .fuses_hi = 2, .battery = 1, .plug = 1, .par_lo = 2, .par_hi = 4, .trap = 1,
          .depth_hi = 3, .cross = 2}},
    {12, {.sw_lo = 4, .sw_hi = 5, .gates_lo = 3, .gates_hi = 5, .kinds = K(AND) | K(OR) | K(NOT), .leds_lo = 2,
          .leds_hi = 2, .fuses_lo = 1, .fuses_hi = 3, .battery = 1, .plug = 1, .par_lo = 2, .par_hi = 5, .trap = 1,
          .depth_hi = 4, .cross = 2}},
    /* chapter 4: XOR */
    {10, {.sw_lo = 3, .sw_hi = 4, .gates_lo = 3, .gates_hi = 5, .kinds = K(AND) | K(OR) | K(NOT) | K(XOR),
          .need = K(XOR), .leds_lo = 2, .leds_hi = 3, .fuses_hi = 1, .par_lo = 3, .par_hi = 5, .trap = 1,
          .depth_hi = 4, .cross = 2}},
    {12, {.sw_lo = 4, .sw_hi = 5, .gates_lo = 4, .gates_hi = 6, .kinds = K(AND) | K(OR) | K(NOT) | K(XOR),
          .need = K(XOR), .leds_lo = 3, .leds_hi = 3, .fuses_lo = 1, .fuses_hi = 2, .battery = 1, .par_lo = 4,
          .par_hi = 6, .trap = 1, .depth_hi = 4, .cross = 2}},
    /* chapter 5: push buttons and memory chips */
    {10, {.sw_lo = 2, .sw_hi = 3, .btn = 1, .latches = 1, .gates_lo = 2, .gates_hi = 4,
          .kinds = K(AND) | K(OR) | K(NOT), .leds_lo = 2, .leds_hi = 3, .fuses_hi = 1, .par_lo = 3, .par_hi = 5,
          .trap = 1, .depth_hi = 4, .cross = 2}},
    {12, {.sw_lo = 2, .sw_hi = 4, .btn = 2, .latches = 2, .gates_lo = 3, .gates_hi = 6,
          .kinds = K(AND) | K(OR) | K(NOT) | K(XOR), .leds_lo = 3, .leds_hi = 3, .fuses_hi = 2, .par_lo = 4,
          .par_hi = 7, .trap = 1, .depth_hi = 4, .cross = 2}},
    /* chapter 6: everything */
    {10, {.sw_lo = 3, .sw_hi = 5, .btn = 1, .latches = 1, .gates_lo = 4, .gates_hi = 7,
          .kinds = K(AND) | K(OR) | K(NOT) | K(XOR) | K(NAND) | K(NOR), .leds_lo = 3, .leds_hi = 3, .fuses_lo = 1,
          .fuses_hi = 3, .battery = 1, .plug = 1, .par_lo = 3, .par_hi = 7, .trap = 1, .depth_hi = 5, .cross = 3,
          .secs = 90}},
    {12, {.sw_lo = 4, .sw_hi = 5, .gates_lo = 5, .gates_hi = 8, .kinds = K(AND) | K(OR) | K(NOT) | K(XOR) | K(NAND) | K(NOR),
          .leds_lo = 4, .leds_hi = 4, .fuses_lo = 1, .fuses_hi = 3, .battery = 1, .par_lo = 5, .par_hi = 8, .trap = 1,
          .depth_hi = 5, .cross = 3, .secs = 90}},
};
#define BANDS ((int)(sizeof plan / sizeof plan[0]))
#define CAND 8                                  /* candidates per level kept */

typedef struct { char txt[DF_TEXT]; df_rating r; } cand;

static int by_score(const void *a, const void *b)
{
    float x = ((const cand *)a)->r.score, y = ((const cand *)b)->r.score;
    return x < y ? -1 : x > y;
}

static uint64_t keys[DF_LEVELS * CAND * 2];
static int nkeys;

static int known(uint64_t k)
{
    for (int i = 0; i < nkeys; i++)
        if (keys[i] == k) return 1;
    return 0;
}

/* the circuit's shape: its text without the switches' start nor which lamp is which (S as s, A as L); 64 bits */
static uint64_t shape_of(const char *txt)
{
    uint64_t h = 0xcbf29ce484222325ull;
    const char *s = strchr(txt, '~');
    for (s = s ? s : txt; *s && *s != '|'; s++) {
        char c = *s == 'S' ? 's' : *s == 'A' && s[1] == '@' ? 'L' : *s;
        h = (h ^ (uint8_t)c) * 0x100000001b3ull;
    }
    return h;
}
static uint64_t shapes[DF_LEVELS * CAND * 2];
static int nshapes;

static int shape_count(uint64_t k)
{
    int n = 0;
    for (int i = 0; i < nshapes; i++) n += shapes[i] == k;
    return n;
}

int main(void)
{
    static cand C[DF_LEVELS * CAND], chosen[DF_LEVELS];
    static df_level D;
    int total = HAND;
    for (int b = 0; b < BANDS; b++) total += plan[b].n;
    if (total != DF_LEVELS) {
        fprintf(stderr, "the plan has %d levels, not %d\n", total, DF_LEVELS);
        return 1;
    }
    int out = 0, bad = 0;
    for (int i = 0; i < HAND; i++) {          /* the ten by hand first, as written (no generated repeat of them) */
        cand *c = &chosen[out++];
        char err[96];
        snprintf(c->txt, sizeof c->txt, "%s", hand[i]);
        if (!df_parse(c->txt, &D, err, sizeof err)) { fprintf(stderr, "hand %d: %s\n", i + 1, err); return 1; }
        uint32_t st = 0;
        for (int k = 0; k < D.nsw; k++)
            if (D.L.n[D.sw[k]].init) st |= df_bit_switch(&D, k);
        if (df_rate(&D, st, &c->r) <= 0) { fprintf(stderr, "hand %d: no puzzle\n", i + 1); return 1; }
        shapes[nshapes++] = shape_of(c->txt);
        keys[nkeys++] = c->r.key;
    }
    for (int b = 0; b < BANDS; b++) {
        int want = plan[b].n * CAND, nc = 0;
        uint32_t seed = 1000003u * (uint32_t)(b + 3);   /* the seeds the bands had before the ten by hand */
        for (int t = 0; t < 20000 && nc < want; t++) {
            cand *c = &C[nc];
            if (!df_make(seed + (uint32_t)t, &plan[b].sp, 60, c->txt, sizeof c->txt, &c->r)) continue;
            if (known(c->r.key)) continue;
            uint64_t sh = shape_of(c->txt);
            if (shape_count(sh) >= 1) continue;  /* one puzzle a circuit */
            shapes[nshapes++] = sh;
            keys[nkeys++] = c->r.key;
            nc++;
        }
        if (nc < plan[b].n) {
            fprintf(stderr, "band %d: %d candidates only\n", b, nc);
            bad = 1;
            if (!nc) continue;
        }
        qsort(C, (size_t)nc, sizeof C[0], by_score);
        for (int i = 0; i < plan[b].n; i++) {   /* evenly over the difficulty */
            int k = plan[b].n > 1 ? i * (nc - 1) / (plan[b].n - 1) : 0;
            if (k >= nc) k = nc - 1;
            chosen[out++] = C[k];
        }
        fprintf(stderr, "band %d: %d candidates, scores %.1f..%.1f\n", b, nc, C[0].r.score, C[nc - 1].r.score);
    }
    if (bad) return 1;
    /* each chapter from its easiest (the bands kept in order), then its drawings searched */
    printf("/* Defuse the Bomb's levels: made by tools/defuse_gen.c (do not edit). Each one: the roles, then Logic\n"
           " * Gates' text of the circuit with its stored drawing (defuse.h); then the pars. */\n");
    printf("const char *const df_levels[DF_LEVELS] = {\n");
    static uint8_t pars[DF_LEVELS];
    for (int i = 0; i < DF_LEVELS; i++) {
        char full[DF_TEXT + LG_LAYOUT], err[96];
        if (!df_parse(chosen[i].txt, &D, err, sizeof err)) { fprintf(stderr, "level %d: %s\n", i + 1, err); return 1; }
        lg_route_best(&D.L, 300);
        if (!df_text_with_layout(chosen[i].txt, &D, full, sizeof full) || !df_parse(full, &D, err, sizeof err)) {
            fprintf(stderr, "level %d: its drawing: %s\n", i + 1, err);
            return 1;
        }
        df_rating r;
        df_values v;
        df_eval(&D, 0, -1, &v);
        uint32_t st = 0;
        for (int k = 0; k < D.nsw; k++)
            if (D.L.n[D.sw[k]].init) st |= df_bit_switch(&D, k);
        if (df_rate(&D, st, &r) <= 0 || r.par != chosen[i].r.par) { fprintf(stderr, "level %d: rated anew\n", i + 1); return 1; }
        pars[i] = (uint8_t)r.par;
        printf("    \"%s\",\n", full);
        fprintf(stderr, "%3d ch%d par %d vars %d gates %d leds %d lose %d/%d dead %d order %d greedy %d sol %d x%d %.1f\n",
                i + 1, df_chapter(i) + 1, r.par, r.vars, r.gates, D.nled, r.lose_first, r.moves_first,
                r.dead_first, r.order_trap, r.greedy_trap, r.nsol, D.L.crossings, r.score);
    }
    printf("};\nconst uint8_t df_pars[DF_LEVELS] = {");
    for (int i = 0; i < DF_LEVELS; i++) printf("%s%d", !i ? "\n    " : i % 30 ? ", " : ",\n    ", pars[i]);
    printf("\n};\n");
    return 0;
}
