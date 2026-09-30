/*
 * Blueberry Tumble: the validator (make blueberrytumble-check). It measures every pattern with the input-search
 * solver and proves the content fair (DESIGN.md "The validator"):
 *   1. every pattern, every parameter combination, every tier: completable, every window >= MIN_WINDOW frames
 *   2. every coin spot: collectable with windows >= MIN_WINDOW
 *   3. links: tails and heads (the committed table), every chainable pair with its bridge
 *   4. streams from the real director: thousands of seeds, end to end
 *   5. beat sync     6. the difficulty curve     7. the committed src/tables.inc is up to date
 *
 *   validate --check [--report FILE]      all of the above (exit 1 on a failure)
 *   validate --write-tables FILE          measure and write src/tables.inc
 *   validate --check-tables               only 7
 *   validate --difficulty --csv FILE      the difficulty table and the generated curves (tools/difficulty.py)
 *   validate --quick                      fewer streams (development)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "solver.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *rep;
static int fails;
static void say(const char *fmt, ...)
{
    va_list a;
    va_start(a, fmt);
    vprintf(fmt, a);
    va_end(a);
    if (rep) { va_start(a, fmt); vfprintf(rep, fmt, a); va_end(a); }
}
#define FAIL(...) do { fails++; say("  FAIL "); say(__VA_ARGS__); say("\n"); } while (0)

static bt_course C;
#define PAT_COL 8               /* the pattern under test starts at this column */
#define LEAD_OUT 32

/* ---- one course: flat, the pattern(s), flat ----------------------------------------------------------------------- */
static int32_t frame_at_x(const bt_course *c, int32_t xpx)
{
    int32_t f = 0;
    while ((course_x(c, f) >> 16) < xpx) f++;
    return f;
}

typedef struct inst { int pat, combo, coin; } inst;

/* builds [flat][a][bridge][b]...[flat]; returns the end column of the last pattern */
static int32_t build(int tier, const inst *seq, int n, int32_t *starts, int32_t *ends, int bridge_auto)
{
    course_reset(&C, tier);
    course_place_flat(&C, -START_COL + PAT_COL, PAT_START);
    int prev = -1;
    for (int i = 0; i < n; i++) {
        if (bridge_auto) {
            int br = bridge_cells(prev, seq[i].pat, tier);
            if (br) course_place_flat(&C, br, PAT_BRIDGE);
        }
        starts[i] = C.next_col;
        course_place(&C, seq[i].pat, tier, seq[i].combo, seq[i].coin, 0);
        ends[i] = C.next_col;
        prev = seq[i].pat;
    }
    int32_t e = C.next_col;
    course_place_flat(&C, LEAD_OUT, PAT_BRIDGE);
    C.end_col = C.next_col;
    return e;
}

static void query(sv_query *q, int32_t end_col, int need_coin)
{
    memset(q, 0, sizeof *q);
    q->c = &C;
    berry_reset(&q->start, 0);
    q->start_f = 0;
    q->end_f = frame_at_x(&C, (end_col + LEAD_OUT / 2) * CELL);
    q->need_coin = need_coin;
    q->boundary_x = -1;
}

/* ---- scoring (DESIGN.md "Measured difficulty") ---------------------------------------------------------------------- */
static double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static int popcount(int v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }

typedef struct measure {
    int ok, window, presses, skills, react, score, tail, head;
    double density;
} measure;

static int score_of(int window, double density, int skills, int react)
{
    double w = clamp01((double)(DIFF_WIN_EASY - window) / (DIFF_WIN_EASY - DIFF_WIN_HARD));
    double d = clamp01(density / DIFF_DENS_MAX);
    double s = clamp01((popcount(skills) - 1) / 3.0);
    double r = clamp01((double)(DIFF_REACT_EASY - react) / (DIFF_REACT_EASY - DIFF_REACT_HARD));
    int sc = (int)(DIFF_W_WINDOW * w + DIFF_W_DENSITY * d + DIFF_W_SKILLS * s + DIFF_W_REACT * r + 0.5);
    return sc < 0 ? 0 : sc > 100 ? 100 : sc;
}

/* the careful path from x = 0; then the late-edge path for the tail */
static void measure_one(int pat, int tier, int combo, measure *m)
{
    int32_t st, en;
    inst s = {pat, combo, 0};
    memset(m, 0, sizeof *m);
    int32_t end = build(tier, &s, 1, &st, &en, 0);
    sv_query q;
    query(&q, end, 0);
    q.boundary_x = en * CELL;
    static sv_result r;
    sv_reset();
    sv_careful(&q, &r);
    m->ok = r.ok;
    m->window = r.min_window > 99 ? 99 : r.min_window;
    m->presses = r.presses;
    m->skills = r.skills;
    m->react = r.react > 999 ? 999 : r.react;
    double secs = (double)(en - st) * CELL / (tier_speed(tier) / 65536.0) / 60.0;
    m->density = r.presses / secs;
    m->score = score_of(m->window, m->density, m->skills, m->react);
    m->tail = r.land_after < 0 ? 0 : r.land_after;
    if (r.ok) {
        q.late_last_press = 1;
        sv_careful(&q, &r);
        if (r.ok && r.land_after > m->tail) m->tail = r.land_after;
        if (!r.ok) m->tail = 99;                    /* the late edge fails: be safe, a long bridge */
    }
    /* the head: the latest landing (frames after the pattern's start) from which it is still fair */
    int32_t f0 = frame_at_x(&C, st * CELL);
    int lo = -40, hi = 40, best = -41;
    while (lo <= hi) {
        int k = (lo + hi) / 2;
        sv_query h = q;
        h.late_last_press = 0;
        h.boundary_x = -1;
        h.start_f = f0 + k;
        berry_reset(&h.start, course_x(&C, h.start_f));
        sv_careful(&h, &r);
        if (r.ok && r.min_window >= MIN_WINDOW) { best = k; lo = k + 1; }
        else hi = k - 1;
    }
    m->head = best;
}

/* ---- 1, 2, 7: every pattern -------------------------------------------------------------------------------------------- */
typedef struct pt_meas {
    int n;                      /* combos */
    uint8_t score[4096], win[4096], tl[4096];
    int8_t hd[4096];
    int tail, head, min_score, max_score, min_win, sum_score;
    int coin_min_win;
    int ok;
} pt_meas;
static pt_meas M[64][NTIERS];

static const char *only;
static void measure_all(int verbose)
{
    say("1. every pattern, every combination, every tier (windows >= %d frames)\n", MIN_WINDOW);
    for (int p = 0; p < bt_npatterns; p++) {
        int tot = 0, bad = 0, worst = 999;
        clock_t t0 = clock();
        if (only && strcmp(only, pat_name(p))) continue;
        char tiers[64] = "";
        for (int t = 0; t < NTIERS; t++) {
            pt_meas *pm = &M[p][t];
            memset(pm, 0, sizeof *pm);
            pm->n = pat_ncombo(p, t);
            pm->tail = 0;
            pm->head = 99;
            pm->min_score = 100;
            pm->min_win = 99;
            pm->coin_min_win = 99;
            pm->ok = 1;
            for (int k = 0; k < pm->n; k++) {
                measure m;
                measure_one(p, t, k, &m);
                tot++;
                if (!m.ok || m.window < MIN_WINDOW) {
                    bad++;
                    pm->ok = 0;
                    if (bad <= 400) {
                        int v[PAT_MAX_PARAMS] = {0};
                        pat_combo_values(p, t, k, v);
                        char vs[64] = "";
                        for (int i = 0; i < bt_patterns[p].nparams; i++)
                            snprintf(vs + strlen(vs), sizeof vs - strlen(vs), "%s%s=%d", i ? " " : "", bt_patterns[p].p[i].name, v[i]);
                        FAIL("%s tier %d (%s): %s, window %d", pat_name(p), t, vs, m.ok ? "tight" : "not completable",
                             m.window);
                    }
                }
                pm->score[k] = (uint8_t)m.score;
                pm->win[k] = (uint8_t)(m.window > 99 ? 99 : m.window);
                pm->tl[k] = (uint8_t)m.tail;
                pm->hd[k] = (int8_t)(m.head < -99 ? -99 : m.head);
                if (m.tail > pm->tail) pm->tail = m.tail;
                if (m.head < pm->head) pm->head = m.head;
                if (m.score < pm->min_score) pm->min_score = m.score;
                if (m.score > pm->max_score) pm->max_score = m.score;
                if (m.window < pm->min_win) pm->min_win = m.window;
                pm->sum_score += m.score;
                if (m.window < worst) worst = m.window;
                /* 2. the coin spot */
                if (bt_patterns[p].coin_spot) {
                    int32_t st, en;
                    inst s = {p, k, 1};
                    int32_t end = build(t, &s, 1, &st, &en, 0);
                    int coins = 0;
                    for (int32_t cc = st; cc < en; cc++)
                        for (int rr = 0; rr < ROWS; rr++) coins += course_col(&C, cc)->cell[rr] == K_COIN;
                    if (!coins) continue;           /* this combination has no coin spot */
                    sv_query q;
                    query(&q, end, 1);
                    static sv_result r;
                    sv_reset();
                    sv_careful(&q, &r);
                    int w = r.ok ? r.min_window : 0;
                    if (w < pm->coin_min_win) pm->coin_min_win = w;
                    if (!r.ok || r.min_window < MIN_WINDOW) {
                        bad++;
                        pm->ok = 0;
                        if (bad <= 400) FAIL("%s tier %d combo %d: the coin is %s (window %d)", pat_name(p), t, k,
                                           r.ok ? "too tight" : "not collectable", w);
                    }
                }
            }
            if (pm->n) snprintf(tiers + strlen(tiers), sizeof tiers - strlen(tiers), "%d", t);
        }
        say("  %-11s %4d instances (tiers %s): %s, tightest window %d frames\n", pat_name(p), tot, tiers,
            bad ? "FAILED" : "all fair", worst);
        if (verbose) printf("    (%.1f s)\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
        fflush(stdout);
    }
}

static void print_table(void)
{
    say("\n   pattern      tier:  combos  score min/avg/max  window  tail head   coin window\n");
    for (int p = 0; p < bt_npatterns; p++)
        for (int t = 0; t < NTIERS; t++) {
            pt_meas *pm = &M[p][t];
            if (!pm->n) continue;
            say("   %-11s  %d:   %5d   %3d / %3d / %3d   %4d    %3d  %3d   %s%d\n", t ? "" : pat_name(p), t, pm->n,
                pm->min_score, pm->sum_score / pm->n, pm->max_score, pm->min_win, pm->tail, pm->head,
                bt_patterns[p].coin_spot ? "" : "-", bt_patterns[p].coin_spot ? pm->coin_min_win : 0);
        }
}

static void write_tables(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(2); }
    fprintf(f, "/* Generated by tests/validate.c --write-tables (make blueberrytumble-tables). Do not edit.\n"
               " * Per pattern and tempo tier: the combos' measured difficulty (0-100) and tightest windows (frames), the\n"
               " * tail and head (frames) that size the bridges (DESIGN.md \"Measured difficulty\", \"Links\"). */\n");
    fprintf(f, "#define BT_TABLE_NPAT %d\n", bt_npatterns);
    fprintf(f, "static const char *const bt_tnames[BT_TABLE_NPAT] = {");
    for (int p = 0; p < bt_npatterns; p++) fprintf(f, "%s\"%s\"", p ? ", " : "", pat_name(p));
    fprintf(f, "};\nstatic const pat_tier_info bt_tinfo[BT_TABLE_NPAT][NTIERS] = {\n");
    int first = 0;
    for (int p = 0; p < bt_npatterns; p++) {
        fprintf(f, "    {");
        for (int t = 0; t < NTIERS; t++) {
            pt_meas *pm = &M[p][t];
            fprintf(f, "%s{%d, %d, %d, %d, %d, %d, %d}", t ? ", " : "", pm->n, first, pm->n ? pm->tail : 0,
                    pm->n ? (pm->head < -99 ? -99 : pm->head) : 0, pm->n ? pm->min_score : 0, pm->max_score,
                    pm->n ? pm->min_win : 0);
            first += pm->n;
        }
        fprintf(f, "}, /* %s */\n", pat_name(p));
    }
    fprintf(f, "};\n");
    for (int which = 0; which < 2; which++) {
        fprintf(f, "static const uint8_t %s[%d] = {", which ? "bt_twin" : "bt_tscore", first ? first : 1);
        int n = 0;
        for (int p = 0; p < bt_npatterns; p++)
            for (int t = 0; t < NTIERS; t++)
                for (int k = 0; k < M[p][t].n; k++)
                {
                    fprintf(f, "%s%s%d", n ? "," : "", n % 32 == 0 ? "\n    " : "",
                            which ? M[p][t].win[k] : M[p][t].score[k]);
                    n++;
                }
        fprintf(f, "};\n");
    }
    fclose(f);
    printf("wrote %s (%d patterns, %d instances)\n", path, bt_npatterns, first);
}

/* 7. the committed table against the measures */
static void check_tables(void)
{
    say("7. the committed src/tables.inc matches the measures\n");
    int stale = 0;
    if (bt_table_npatterns != bt_npatterns) stale = 1;
    for (int p = 0; p < bt_npatterns && !stale; p++)
        for (int t = 0; t < NTIERS && !stale; t++) {
            const pat_tier_info *ti = table_info(p, t);
            pt_meas *pm = &M[p][t];
            if (!ti) { stale = 1; break; }
            if (!pm->n) continue;
            if (ti->tail != pm->tail || ti->head != (pm->head < -99 ? -99 : pm->head) || ti->min_window != pm->min_win) stale = 1;
            for (int k = 0; k < pm->n && !stale; k++)
                if (table_score(p, t, k) != pm->score[k] || table_window(p, t, k) != pm->win[k]) stale = 1;
        }
    if (stale) FAIL("src/tables.inc is stale: run make blueberrytumble-tables");
    else say("  up to date\n");
}

/* ---- 3. links -------------------------------------------------------------------------------------------------------------- */
static int both_allowed(int a, int b, int t)
{
    if (!pat_ncombo(a, t) || !pat_ncombo(b, t)) return 0;
    if (t == NTIERS - 1) return 1;                  /* the night loop: any biome */
    return ((bt_patterns[a].biomes & bt_patterns[b].biomes) >> t) & 1;
}

/* the combination with the longest tail, the one with the tightest head */
static int pick_tail(int p, int t)
{
    int best = 0;
    for (int k = 1; k < M[p][t].n; k++) if (M[p][t].tl[k] > M[p][t].tl[best]) best = k;
    return best;
}
static int pick_head(int p, int t)
{
    int best = 0;
    for (int k = 1; k < M[p][t].n; k++) if (M[p][t].hd[k] < M[p][t].hd[best]) best = k;
    return best;
}

static void check_links(uint32_t seed)
{
    say("3. links: every pair the director can chain, with its bridge\n");
    rs_rng rng;
    rs_rng_seed(&rng, seed);
    int pairs = 0, bad = 0, bridged = 0;
    for (int t = 0; t < NTIERS; t++)
        for (int a = 0; a < bt_npatterns; a++)
            for (int b = 0; b < bt_npatterns; b++) {
                if (a == b || !both_allowed(a, b, t)) continue;
                pairs++;
                if (bridge_cells(a, b, t)) bridged++;
                for (int v = 0; v < 3; v++) {
                    inst s[2] = {{a, v == 0 ? pick_tail(a, t) : rs_rng_range(&rng, M[a][t].n), 0},
                                 {b, v <= 1 ? pick_head(b, t) : rs_rng_range(&rng, M[b][t].n), 0}};
                    int32_t st[2], en[2];
                    int32_t end = build(t, s, 2, st, en, 1);
                    sv_query q;
                    query(&q, end, 0);
                    static sv_result r;
                    sv_reset();
                    sv_careful(&q, &r);
                    if (!r.ok || r.min_window < MIN_WINDOW) {
                        bad++;
                        if (bad <= 8) FAIL("tier %d: %s(%d) -> %s(%d), bridge %d: %s, window %d", t, pat_name(a), s[0].combo,
                                           pat_name(b), s[1].combo, bridge_cells(a, b, t), r.ok ? "tight" : "not completable",
                                           r.min_window);
                    }
                }
            }
    say("  %d pairs x 3 combinations: %s (%d pairs need a bridge)\n", pairs, bad ? "FAILED" : "all fair", bridged);
}

/* ---- 4, 5, 6: streams ---------------------------------------------------------------------------------------------------- */
static int freq[64];
typedef struct curve_pt { int32_t col; int score, target, pat; } curve_pt;
#define CURVE_MAX 4096
static curve_pt curve[CURVE_MAX];
static int ncurve;
static FILE *csv;

static void collect_segments(const bt_course *c, int32_t from, uint32_t seed, int *nseen)
{
    for (int32_t i = *nseen; i < c->nseg; i++) {
        const bt_seg *s = &c->seg[i % SEG_RING];
        if (s->col0 < from) continue;
        if (s->pat >= 0) freq[s->pat]++;
        if (ncurve < CURVE_MAX) {
            curve[ncurve].col = s->col0;
            curve[ncurve].score = s->pat >= 0 ? s->score : -1;
            curve[ncurve].target = s->target;
            curve[ncurve].pat = s->pat;
            ncurve++;
        }
        if (csv) fprintf(csv, "%u,%d,%d,%d,%d,%s\n", seed, (int)s->col0, s->len, s->pat >= 0 ? s->score : -1, s->target,
                         pat_name(s->pat));
    }
    *nseen = c->nseg;
}

static int check_stream(uint32_t seed, int metres, int solve, double *roll, int nroll)
{
    course_reset(&C, -1);
    director_start(&C, seed);
    int nseen = 0, bad = 0;
    ncurve = 0;
    int32_t end_col = metres;
    sv_query q;
    memset(&q, 0, sizeof q);
    q.c = &C;
    berry_reset(&q.start, 0);
    q.end_f = 0;
    while ((course_x(&C, q.end_f) >> 16) < end_col * CELL) q.end_f++;
    q.horizon = 420;
    q.fill_to = end_col + 64;
    q.boundary_x = -1;
    if (solve) {
        static sv_result r;
        sv_reset();
        sv_careful(&q, &r);
        /* the segments seen by the solver */
        if (!r.ok || r.min_window < MIN_WINDOW) {
            int32_t x = course_x(&C, r.ok ? r.min_window_frame : r.fail_frame) >> 16;
            const bt_seg *s = course_seg_at(&C, x / CELL);
            FAIL("seed %u: %s at %d m (in %s, tier %d), window %d", seed, r.ok ? "tight" : "stuck", x / CELL,
                 s ? pat_name(s->pat) : "?", course_tier_of_col(&C, x / CELL), r.min_window);
            bad = 1;
        }
    } else {
        for (int32_t col = 0; col < end_col; col += 32) director_fill(&C, col + 64);
    }
    /* the segments: the ring only keeps the last SEG_RING, so collect as we go (regenerate with the same seed) */
    course_reset(&C, -1);
    director_start(&C, seed);
    for (int32_t col = 0; col < end_col; col += 16) {
        director_fill(&C, col + 32);
        collect_segments(&C, -1000, seed, &nseen);
    }
    /* 5. beat sync and 6. the curve on this stream */
    for (int i = 0; i < ncurve; i++) {
        if (curve[i].col % BEAT_CELLS) { FAIL("seed %u: a segment at %d is off the beat", seed, curve[i].col); bad = 1; }
        if (curve[i].pat == PAT_GATE && curve[i].col % BIOME_CELLS) { FAIL("seed %u: a gate off its bar", seed); bad = 1; }
        if (curve[i].pat < 0) continue;
        int tgt = curve[i].target;                  /* the target the director aimed at when it chose it */
        if (curve[i].score > tgt + DIFF_SPIKE) {
            FAIL("seed %u: %s at %d m scores %d > target %d + %d", seed, pat_name(curve[i].pat), curve[i].col,
                 curve[i].score, tgt, DIFF_SPIKE);
            bad = 1;
        }
        if (curve[i].col < DIFF_EASY_M && curve[i].score > DIFF_EASY_MAX) {
            FAIL("seed %u: %s at %d m (the first 30 s) scores %d", seed, pat_name(curve[i].pat), curve[i].col, curve[i].score);
            bad = 1;
        }
    }
    /* rolling means (per 100 m, over 300 m ~ 30 s): accumulated by the caller */
    for (int w = 0; w < nroll; w++) {
        int32_t c0 = w * 100, c1 = c0 + 300;
        double sum = 0, cells = 0;
        for (int i = 0; i < ncurve; i++) {
            if (curve[i].pat < 0 || curve[i].pat == PAT_GATE) continue;
            if (curve[i].col < c0 || curve[i].col >= c1) continue;
            sum += curve[i].score;
            cells += 1;
        }
        if (cells > 0) roll[w] += sum / cells;
    }
    return bad;
}

static void breathers_easier(uint32_t seeds)
{
    int n = 0, easier = 0;
    for (uint32_t s = 1; s <= seeds; s++) {
        course_reset(&C, -1);
        director_start(&C, s * 7919u);
        int nseen = 0;
        ncurve = 0;
        for (int32_t col = 0; col < 2500; col += 16) {
            director_fill(&C, col + 32);
            collect_segments(&C, -1000, s, &nseen);
        }
        for (int i = 2; i < ncurve; i++) {
            if (curve[i].pat < 0 || !bt_patterns[curve[i].pat].breather) continue;
            int prev[2], k = 0;
            for (int j = i - 1; j >= 0 && k < 2; j--)
                if (curve[j].pat >= 0 && !bt_patterns[curve[j].pat].breather) prev[k++] = curve[j].score;
            if (k < 2) continue;
            n++;
            if (curve[i].score < (prev[0] + prev[1]) / 2) easier++;
        }
    }
    say("  breathers: %d of %d are easier than the two patterns before them\n", easier, n);
    if (n == 0 || easier * 100 < n * 90) FAIL("breathers are not clearly easier (%d / %d)", easier, n);
}

static void check_streams(int nshort, int short_m, int nlong, int long_m)
{
    say("4. streams from the director, solved end to end (the careful path, a %d-frame horizon)\n", 420);
    memset(freq, 0, sizeof freq);
    double roll[40] = {0};
    int bad = 0, nroll = long_m / 100 - 3;
    if (nroll > 40) nroll = 40;
    for (int i = 0; i < nshort; i++) bad += check_stream(1000 + i * 7u, short_m, 1, roll, 0);
    say("  %d seeds x %d m: %s\n", nshort, short_m, bad ? "FAILED" : "all completed, every window >= 3 frames");
    int bad2 = 0;
    for (int i = 0; i < nlong; i++) bad2 += check_stream(50000 + i * 13u, long_m, 1, roll, nroll);
    say("  %d seeds x %d m (every tier, the night loop): %s\n", nlong, long_m, bad2 ? "FAILED" : "all completed");
    say("  pattern frequencies over the streams:\n   ");
    int tot = 0;
    for (int p = 0; p < bt_npatterns; p++) tot += freq[p];
    for (int p = 0; p < bt_npatterns; p++) say(" %s %.1f%%%s", pat_name(p), tot ? 100.0 * freq[p] / tot : 0, p % 7 == 6 ? "\n   " : "");
    say("\n");
    for (int p = 0; p < bt_npatterns; p++)
        if (!freq[p]) FAIL("the director never used %s", pat_name(p));
    say("5. beat sync: segments on beats, gates on bars (checked on every stream); tiers: speed x frames/beat = 64 px\n");
    say("6. the difficulty curve: rolling 30-s (300 m) mean over %d seeds:\n   ", nlong);
    int mono = 1;
    for (int w = 0; w < nroll; w++) {
        say(" %d:%.1f", w * 100, roll[w] / (nlong ? nlong : 1));
        if (w > 0 && roll[w] / nlong < roll[w - 1] / nlong - 2.0) mono = 0;
    }
    say("\n");
    if (!mono) FAIL("the rolling mean falls by more than 2 points somewhere");
    else say("  rises on average (never falls by more than 2 points from one window to the next)\n");
    breathers_easier(40);
}

/* ---- the difficulty report ----------------------------------------------------------------------------------------------- */
static void difficulty_report(const char *csv_path)
{
    print_table();
    if (csv_path) {
        csv = fopen(csv_path, "w");
        if (!csv) { perror(csv_path); exit(2); }
        fprintf(csv, "seed,col,len,score,target,pattern\n");
        double roll[40] = {0};
        for (uint32_t s = 1; s <= 5; s++) check_stream(s, 3200, 0, roll, 0);
        fclose(csv);
        csv = NULL;
        printf("wrote %s\n", csv_path);
    }
}

int main(int argc, char **argv)
{
    const char *write = NULL, *report = NULL, *csv_path = NULL;
    int check = 0, check_tab = 0, diff = 0, quick = 0, verbose = 0, trace_tier = 0, trace_combo = 0, trace_coin = 0;
    const char *trace = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--check-tables")) check_tab = 1;
        else if (!strcmp(argv[i], "--write-tables") && i + 1 < argc) write = argv[++i];
        else if (!strcmp(argv[i], "--report") && i + 1 < argc) report = argv[++i];
        else if (!strcmp(argv[i], "--difficulty")) diff = 1;
        else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
        else if (!strcmp(argv[i], "--quick")) quick = 1;
        else if (!strcmp(argv[i], "--only") && i + 1 < argc) only = argv[++i];
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--trace") && i + 3 < argc) {
            trace = argv[++i];
            trace_tier = atoi(argv[++i]);
            trace_combo = atoi(argv[++i]);
            if (i + 1 < argc && !strcmp(argv[i + 1], "coin")) { trace_coin = 1; i++; }
        }
        else { fprintf(stderr, "usage: validate --check | --write-tables FILE | --check-tables | --difficulty [--csv F]\n"); return 2; }
    }
    if (report) rep = fopen(report, "w");
    sv_init();
    if (trace) {
        /* --trace PAT TIER COMBO [coin]: the careful path, toggle by toggle */
        /* or --trace A:ca+B:cb TIER 0: a pair with its bridge */
        inst pair[2];
        int npair = 0;
        char buf[128];
        snprintf(buf, sizeof buf, "%s", trace);
        for (char *tok = strtok(buf, "+"); tok && npair < 2; tok = strtok(NULL, "+")) {
            char *colon = strchr(tok, ':');
            int cb = trace_combo;
            if (colon) { *colon = 0; cb = atoi(colon + 1); }
            int q2 = 0;
            while (q2 < bt_npatterns && strcmp(pat_name(q2), tok)) q2++;
            if (q2 == bt_npatterns) { fprintf(stderr, "no pattern %s\n", tok); return 2; }
            pair[npair].pat = q2;
            pair[npair].combo = cb;
            pair[npair].coin = trace_coin;
            npair++;
        }
        int p = pair[0].pat;
        int32_t st2[2], en2[2];
        int32_t end = build(trace_tier, pair, npair, st2, en2, 1);
        int32_t st = st2[0], en = en2[npair - 1];
        sv_query q;
        query(&q, end, trace_coin);
        q.boundary_x = en * CELL;
        static sv_result r;
        sv_reset();
        sv_careful(&q, &r);
        printf("%s tier %d combo %d%s: cells %d..%d, %s, %d presses, min window %d, react %d, tail %d\n", pat_name(p),
               trace_tier, trace_combo, trace_coin ? " (coin)" : "", st, en, r.ok ? "ok" : "FAILED", r.presses,
               r.min_window, r.react, r.land_after);
        if (!r.ok) printf("  stuck at frame %d (x %.1f)\n", r.fail_frame, course_x(&C, r.fail_frame) / 65536.0);
        for (int i = 0; i < r.ntog; i++)
            printf("  %s at frame %d (x %.1f px = cell %.2f), window %d..%d (%d frames)\n", r.tog[i].press ? "press  " : "release",
                   r.tog[i].frame, course_x(&C, r.tog[i].frame) / 65536.0, course_x(&C, r.tog[i].frame) / 65536.0 / CELL - st,
                   r.tog[i].win_lo, r.tog[i].win_hi, r.tog[i].win_hi - r.tog[i].win_lo + 1);
        for (int32_t c = st; c < en; c++) {
            const bt_col *k = course_col(&C, c);
            printf("  cell %2d: g%d f%d |", c - st, k->ground, k->flags);
            for (int rr = 0; rr < 8; rr++) printf("%c", ".BLThpPOC"[k->cell[rr]]);
            printf("\n");
        }
        return 0;
    }
    measure_all(verbose);
    if (write) { write_tables(write); return fails ? 1 : 0; }
    check_tables();
    if (diff) difficulty_report(csv_path);
    if (check) {
        print_table();
        check_links(2026);
        if (quick) check_streams(40, 800, 6, 2400);
        else check_streams(2000, 700, 120, 3000);
    }
    say("%s\n", fails ? "VALIDATOR FAILED" : "validator: all passed");
    if (rep) fclose(rep);
    (void)check_tab;
    return fails ? 1 : 0;
}
