/*
 * Hex Landscape's levels: src/games/hexland_levels.h. Our own levels: no published or commercial set is used. Each
 * level is a landscape painted by the rules' maker (hxl_make, from fixed seeds: blobs of soft terrains, rivers and rails
 * wandering through, cut into its tiles, shuffled and turned onto the stack, some swapped for strays, quest flags from
 * its regions, a reserve for the bonuses), and one to three objectives (hexland.h): a forest of n, a village of exactly
 * n, n perfect tiles, n quest flags, a score. The chapters bring in the terrains and the objectives one at a time
 * (forests, fields and perfect tiles; villages and exact regions; rivers; quest flags; rails), levels 1-3 one objective
 * each. For each level a few landscapes are made; a beam search (a strong player who knows the whole stack, steered by
 * the objectives) plays each, and the objectives' numbers are set from what it reached (a part of it early, all of it
 * later): its play, cut where the last objective is met, is kept as the proof (tests/hexland_lines.h). The naive
 * players (the greedy one, the hint's, random ones) are measured on the same objectives: from level 4 none meets them
 * all, and of the landscapes the one where they fall shortest is kept. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/hxlgen apps/boardgames/tools/hexland_gen.c -lm && \
 *       /tmp/hxlgen apps/boardgames/tests/hexland_lines.h > apps/boardgames/src/games/hexland_levels.h
 * The difficulty table goes to stderr.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define HXL_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hexland_rules.c"

#define PER (HXL_LEVELS / HXL_CHAPTERS)
#define TRIES 8
#define BEAM 20
#define KIDS 7

/* ---- the players --------------------------------------------------------------------------------------------- */
static hxl_state played;                        /* the last player's final state */

static void rand_play(const hxl_state *from, uint32_t seed)
{
    static int moves[HXL_MOVES];
    uint32_t r = pr_seed(seed);
    played = *from;
    while (!hxl_over(&played)) {
        int n = hxl_moves(&played, moves);
        if (!n) break;
        int m = moves[pr_below(&r, n)];
        if (!hxl_place(&played, m / 6, m % 6, NULL)) break;
    }
}

static void hint_play(const hxl_state *from)
{
    played = *from;
    int c, k;
    while (!hxl_over(&played) && hxl_hint(&played, &c, &k) && hxl_place(&played, c, k, NULL)) {}
}

static void greedy_play(const hxl_state *from)
{
    static int moves[HXL_MOVES];
    static hxl_state scratch;
    played = *from;
    while (!hxl_over(&played)) {
        int n = hxl_moves(&played, moves), best = -1, bv = -1000000000;
        for (int i = 0; i < n; i++) {
            int nb, v = 8 * worth(&played, moves[i] / 6, moves[i] % 6, &scratch, &nb) + nb;
            if (v > bv) { bv = v; best = moves[i]; }
        }
        if (best < 0 || !hxl_place(&played, best / 6, best % 6, NULL)) break;
    }
}

/* ---- the beam: the best few states at each tile, each grown by its best few placements ---------------------------- */
typedef struct { hxl_state s; int value, nl; uint32_t key; int16_t line[HXL_STACK]; } node;
static int16_t best_line[HXL_STACK];             /* the beam's best play: its placements (cell * 6 + turn) */
static int best_nl;
static hxl_state best_end;

static uint32_t key_of(const hxl_state *s)
{
    uint32_t h = 2166136261u;
    for (int c = 0; c < HXL_CELLS; c++)
        if (s->cell[c]) h = (h ^ (uint32_t)c ^ s->cell[c] << 12) * 16777619u;
    return h ^ (uint32_t)s->ns * 31u;
}

static int node_value(const hxl_state *s) { return 8 * s->score + 8 * obj_value(s); }

static void beam(const hxl_state *from)
{
    static node cur[BEAM], next[BEAM * (KIDS + 1)];
    static int moves[HXL_MOVES];
    static hxl_state scratch;
    int nc = 1, best = -1000000000;
    cur[0].s = *from;
    cur[0].value = node_value(from);
    cur[0].nl = 0;
    best_nl = 0;
    best_end = *from;
    while (nc) {
        int nn = 0;
        for (int i = 0; i < nc; i++) {
            hxl_state *s = &cur[i].s;
            if (hxl_over(s)) {
                if (cur[i].value > best) {
                    best = cur[i].value;
                    best_nl = cur[i].nl;
                    best_end = *s;
                    memcpy(best_line, cur[i].line, sizeof best_line);
                }
                continue;
            }
            int n = hxl_moves(s, moves), km[KIDS + 1], kv[KIDS + 1], nk = 0;
            for (int j = 0; j < n; j++) {
                int nb, v = 8 * worth(s, moves[j] / 6, moves[j] % 6, &scratch, &nb) + nb, p;
                if (nk < KIDS) p = nk++;
                else if (v <= kv[KIDS - 1]) continue;
                else p = KIDS - 1;
                while (p > 0 && kv[p - 1] < v) { km[p] = km[p - 1]; kv[p] = kv[p - 1]; p--; }
                km[p] = moves[j];
                kv[p] = v;
            }
            int hc, hk;                         /* the hint's placement too (it looks a tile ahead) */
            if (hxl_hint(s, &hc, &hk)) {
                int dup = 0;
                for (int j = 0; j < nk && !dup; j++) dup = km[j] == hc * 6 + hk;
                if (!dup) { km[nk] = hc * 6 + hk; kv[nk++] = 0; }
            }
            for (int j = 0; j < nk; j++) {
                node *x = &next[nn];
                x->s = *s;
                if (!hxl_place(&x->s, km[j] / 6, km[j] % 6, NULL)) continue;
                memcpy(x->line, cur[i].line, sizeof x->line);
                x->nl = cur[i].nl;
                x->line[x->nl++] = (int16_t)km[j];
                x->key = key_of(&x->s);
                int dup = 0;
                for (int q = 0; q < nn && !dup; q++) dup = next[q].key == x->key && next[q].s.score == x->s.score;
                if (dup) continue;
                x->value = node_value(&x->s);
                nn++;
            }
        }
        nc = 0;                                 /* the best BEAM of them */
        for (int k = 0; k < BEAM && k < nn; k++) {
            int b = k;
            for (int q = k + 1; q < nn; q++)
                if (next[q].value > next[b].value) b = q;
            if (b != k) { node t = next[k]; next[k] = next[b]; next[b] = t; }
            cur[nc++] = next[k];
        }
    }
}

/* ---- the levels ------------------------------------------------------------------------------------------------ */
/* each level's objectives (hexland.h's letters; the numbers come from the beam), the first the main one */
static const char *const SCHED[HXL_CHAPTERS][PER] = {
    {"f", "w", "p", "f p", "w f", "w p", "f w", "p w", "w f", "f w", "f w p", "w p f", "w f p", "p f w", "f p w",
     "w f s", "f p w", "p w f", "w f p", "f w p"},
    {"v", "=v", "v f", "=v w", "f v", "v p", "=f v", "w =v", "v f p", "=v f w", "v w p", "f =v p", "=w v f",
     "v p s", "=f w v", "v f w", "=v p f", "w v =f", "v =w p", "=v f p"},
    {"r", "r p", "r f", "f r", "r v", "=v r", "r w p", "w r f", "r =f", "v r p", "r f w", "=w r v", "r p f",
     "f r s", "r v p", "=v r w", "r f p", "w =f r", "r v =w", "r f p"},
    {"q", "f q", "q v", "r q", "q p", "v q f", "q =f", "w q r", "q v p", "=v q f", "q r w", "f q p", "q =w v",
     "r q f", "q v s", "=f q r", "q w p", "v q =f", "q r p", "q f =v"},
    {"t", "t r", "t f", "v t", "t p", "t q", "=f t", "t r w", "w t v", "t =v q", "r t f", "t f p", "q t =w",
     "t v r", "f t s", "t =f q", "w t p", "t r v", "=v t q", "t f p"},
};

static void spec_of(int level, hxl_spec *sp)
{
    int ch = level / PER, i = level % PER;
    static const int STARTS[8] = {0, 1, 4, 5, 2, 0, 3, 1};
    memset(sp, 0, sizeof *sp);
    sp->radius = i < 8 ? 2 : 3;
    sp->start = STARTS[(i + ch) % 8];
    sp->strays = i < 2 ? 0 : 6 + i;
    sp->reserve = 16;
    sp->terrains = ch == 0 ? 1u << HXL_MEADOW | 1u << HXL_FOREST | 1u << HXL_FIELD : 0x1e;
    if (ch >= 2) sp->rivers = 1 + (i >= 12);
    if (ch >= 3) { sp->quests = 2 + i / 7; sp->exact = 40; }
    if (ch >= 4) { sp->rails = 1; sp->river_quests = 1; sp->radius = i < 4 ? 2 : i < 16 ? 3 : 4; }
    if (strchr(SCHED[ch][i], 'p')) sp->start = 3;   /* a hole to fill perfectly */
    if (strchr(SCHED[ch][i], 'p') && level > 2 && sp->radius < 3) sp->radius = 3;   /* room for perfect tiles */
    if (level == 0) sp->start = 0;
    if (i == PER - 1) { sp->radius = ch < 4 ? 3 : 4; sp->start = 3; }   /* the showpiece */
}

/* a schedule's objectives (letters without numbers: 1 each) */
static int sched_parse(const char *sched, hxl_obj *o)
{
    char buf[64];
    int k = 0;
    for (const char *s = sched; *s && k < 60; s++) {
        buf[k++] = *s;
        if (*s != ' ' && *s != '=' && (s[1] == ' ' || !s[1])) buf[k++] = '1';
    }
    buf[k] = 0;
    return hxl_objs_parse(buf, o, HXL_OBJS);
}

/* the objectives' guesses to steer the beam: big numbers (it grows the regions as far as it can) */
static int guesses(const char *sched, hxl_obj *o)
{
    hxl_obj t[HXL_OBJS];
    int n = sched_parse(sched, t);
    for (int i = 0; i < n; i++) {
        o[i] = t[i];
        o[i].n = t[i].kind == HXL_OB_PERFECT ? 5 : t[i].kind == HXL_OB_QUESTS ? 4 : t[i].kind == HXL_OB_SCORE ? 60000 : 40;
        if (o[i].kind == HXL_OB_EXACT) o[i].kind = HXL_OB_REGION;
    }
    return n;
}

/* the numbers from the beam's end: a part of what it reached early, all of it later; the exact ones a closed region
 * it made; the first three levels the numbers asked for when it reached them. 0: an objective it did not reach. */
static int objectives(int level, const char *sched, const hxl_state *end, hxl_obj *o, int full)
{
    static const int WANT[3] = {4, 5, 2};
    int n = sched_parse(sched, o);
    double k0 = 0.6 + 0.25 * level / (HXL_LEVELS - 1.0), k = level < 3 ? 1 : k0 + (1 - k0) * full / 8.0;
    for (int i = 0; i < n; i++) {
        int got;
        switch (o[i].kind) {
        case HXL_OB_REGION: got = hxl_largest(end, o[i].terr, NULL); break;
        case HXL_OB_EXACT: {                   /* a closed region it made, the largest of 3..12 */
            uint8_t mark[HXL_CELLS];
            memset(mark, 0, sizeof mark);
            got = 0;
            for (int c = 0; c < HXL_CELLS; c++) {
                if (!end->cell[c] || mark[c] || !hxl_has(end->cell[c], o[i].terr)) continue;
                int open, size = region_fill(end, c, o[i].terr, &open, mark);
                if (!open && size >= 3 && size <= 12 && size > got) got = size;
            }
            if (!got) { o[i].kind = HXL_OB_REGION; got = hxl_largest(end, o[i].terr, NULL); break; }   /* none closed: at least */
            o[i].n = (uint16_t)got;
            continue;
        }
        case HXL_OB_PERFECT: got = end->perfects; break;
        case HXL_OB_QUESTS: got = end->done; break;
        default: got = end->score; break;
        }
        int v = (int)(got * k);
        if (o[i].kind == HXL_OB_SCORE) v = v / 10 * 10;
        if (o[i].kind == HXL_OB_REGION && v < 3) v = 3;
        if (level < 3 && got >= WANT[level]) v = WANT[level];
        if (v < 1 || v > got) return 0;
        o[i].n = (uint16_t)v;
    }
    return n;
}

static int stars_of(const hxl_state *from, int who)
{
    if (who == 0) { blind = 1; greedy_play(from); blind = 0; }
    else if (who == 1) hint_play(from);
    else rand_play(from, 1000u + (uint32_t)who);
    return hxl_level_stars(&played, 0);
}

int main(int argc, char **argv)
{
    const char *lines = argc > 1 ? argv[1] : NULL;
    static hxl_game g, keep, play;
    static char start[4096], stack[8192], reserve[4096], goals[64];
    static int16_t keep_line[HXL_STACK];
    FILE *lf = lines ? fopen(lines, "w") : NULL;
    if (lf)
        fprintf(lf, "/* Hex Landscape: a play of each level meeting all its objectives (the levels' tool's beam "
                    "search, tools/hexland_gen.c; do not edit) */\nstatic const char *const HXL_LINES[HXL_LEVELS] = {\n");
    printf("/* Hex Landscape's levels: made by tools/hexland_gen.c (our own landscapes, measured by its players; do "
           "not edit).\n * Each level: its first tiles (\"<q><r><tile>\"), its stack, its reserve (the bonuses' "
           "tiles), its objectives (hexland.h). */\n");
    printf("const hxl_level_t hxl_levels[HXL_LEVELS] = {\n");
    fprintf(stderr, "level ch tiles objectives          beam-tiles greedy* hint* random* (stars of 3)\n");
    double sum[HXL_CHAPTERS][5] = {{0}};
    int fails = 0;
    int upto = getenv("HXL_UPTO") ? atoi(getenv("HXL_UPTO")) : HXL_LEVELS;   /* (a quick try: the first few) */
    for (int L = 0; L < upto; L++) {
        hxl_spec sp;
        spec_of(L, &sp);
        const char *sched = SCHED[L / PER][L % PER];
        int intro = L < 3, keep_nl = 0, best = -1000, kn = 0, kst[3] = {0};
        hxl_obj ko[HXL_OBJS];
        for (int t = 0; t < TRIES || (best <= -1000 && t < 4 * TRIES); t++) {   /* (more when none fits) */
            hxl_make(&g, &sp, 7919u * (uint32_t)L + 104729u * (uint32_t)t + 31u);
            g.s.nobj = guesses(sched, g.s.obj);
            g.s.met = 0;
            beam(&g.s);
            for (int full = 0; full < 9; full++) {   /* the numbers raised towards all the beam reached */
            hxl_obj o[HXL_OBJS];
            int n = objectives(L, sched, &best_end, o, full);
            if (!n) continue;
            /* the beam's play on the real objectives, cut where they are all met */
            play = g;
            memcpy(play.s.obj, o, sizeof o);
            play.s.nobj = n;
            play.s.met = 0;
            objs_check(&play.s, -1, NULL);
            int nl = 0;
            while (nl < best_nl && !hxl_over(&play.s) && hxl_place(&play.s, best_line[nl] / 6, best_line[nl] % 6, NULL))
                nl++;
            if (hxl_level_stars(&play.s, 0) != 3) continue;
            int st[3];
            hxl_state at = g.s;                 /* the naive players on the same objectives */
            memcpy(at.obj, o, sizeof o);
            at.nobj = n;
            at.met = 0;
            objs_check(&at, -1, NULL);
            st[0] = stars_of(&at, 0);
            st[1] = stars_of(&at, 1);
            st[2] = stars_of(&at, 2);
            int naive = st[0];                  /* the greedy player blind to the objectives (the hint's player: a help) */
            /* intro levels: the easiest; the others: the naive players short of three stars, the shortest kept */
            int aim = L < 40 ? 2 : 1, score = intro ? 100 * naive - nl : naive >= 3 ? -60 - st[0] - st[1] - nl / 10 : 100 - 30 * abs(naive - aim) - nl - 3 * full;
            if (score > best) {
                best = score;
                keep = g;
                memcpy(keep.s.obj, o, sizeof o);
                keep.s.nobj = n;
                keep.s.met = 0;
                memcpy(ko, o, sizeof o);
                kn = n;
                memcpy(kst, st, sizeof kst);
                keep_nl = nl;
                memcpy(keep_line, best_line, sizeof keep_line);
            }
            if (intro || naive < 3) break;   /* (else the numbers go up) */
            }
        }
        if (best <= -1000) { fprintf(stderr, "level %d: no landscape fits\n", L + 1); fails++; }
        else if (best < 0) fprintf(stderr, "level %d: a naive player meets all its objectives\n", L + 1);
        hxl_objs_text(ko, kn, goals, (int)sizeof goals);
        fprintf(stderr, "%5d %2d %5d %-20s %6d %7d %5d %7d\n", L + 1, L / PER + 1, keep.s.ns, goals, keep_nl, kst[0],
                kst[1], kst[2]);
        double *S = sum[L / PER];
        S[0] += keep_nl, S[1] += kst[0], S[2] += kst[1], S[3] += kst[2], S[4] += 1;
        hxl_text(&keep, start, stack, reserve, (int)sizeof stack);
        printf("    {\"%s\",\n     \"%s\",\n     \"%s\", \"%s\", 0},\n", start, stack, reserve, goals);
        if (lf) {
            fprintf(lf, "    \"");
            for (int j = 0; j < keep_nl; j++) {
                int q, r;
                hxl_qr(keep_line[j] / 6, &q, &r);
                fprintf(lf, "%s%d,%d,%d", j ? " " : "", q, r, keep_line[j] % 6);
            }
            fprintf(lf, "\",\n");
        }
        fflush(stdout);
    }
    printf("};\n");
    if (lf) { fprintf(lf, "};\n"); fclose(lf); }
    fprintf(stderr, "chapter tiles-to-win greedy* hint* random* (mean stars)\n");
    for (int c = 0; c < HXL_CHAPTERS; c++)
        fprintf(stderr, "%7d %12.1f %7.2f %5.2f %7.2f\n", c + 1, sum[c][0] / sum[c][4], sum[c][1] / sum[c][4],
                sum[c][2] / sum[c][4], sum[c][3] / sum[c][4]);
    return fails;
}
