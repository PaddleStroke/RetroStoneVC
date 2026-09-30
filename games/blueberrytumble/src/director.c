/*
 * Blueberry Tumble: the director. It writes the endless course ahead of the berry: patterns and their parameters
 * picked by their MEASURED difficulty (src/tables.inc) against a target curve that rises with distance in waves,
 * breathers after each wave, bridges where two patterns need one, a gate at each biome, rare golden blueberries.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include "tables.inc"
#include <string.h>

const int bt_table_npatterns = BT_TABLE_NPAT;

/* ---- the measured table --------------------------------------------------------------------------------------------- */
const pat_tier_info *table_info(int pat, int tier)
{
    if (pat < 0 || pat >= BT_TABLE_NPAT || pat >= bt_npatterns || tier < 0 || tier >= NTIERS) return NULL;
    if (strcmp(bt_tnames[pat], bt_patterns[pat].name)) return NULL;
    const pat_tier_info *t = &bt_tinfo[pat][tier];
    if (t->ncombo != pat_ncombo(pat, tier)) return NULL;
    return t;
}

int table_score(int pat, int tier, int combo)
{
    const pat_tier_info *t = table_info(pat, tier);
    if (!t || combo < 0 || combo >= t->ncombo) return -1;
    return bt_tscore[t->first + combo];
}

int table_window(int pat, int tier, int combo)
{
    const pat_tier_info *t = table_info(pat, tier);
    if (!t || combo < 0 || combo >= t->ncombo) return -1;
    return bt_twin[t->first + combo];
}

int bridge_cells(int prev, int next, int tier)
{
    if (prev < 0 || next < 0) return 0;
    const pat_tier_info *a = table_info(prev, tier), *b = table_info(next, tier);
    if (!a || !b) return BEAT_CELLS;                /* a stale table: one safe beat */
    int frames = a->tail - b->head + LINK_MARGIN;
    if (frames <= 0) return 0;
    int32_t px = (int32_t)(((int64_t)frames * tier_speed(tier) + Q16_ONE - 1) >> 16);
    int cells = (px + CELL - 1) / CELL;
    return (cells + BEAT_CELLS - 1) / BEAT_CELLS * BEAT_CELLS;
}

/* ---- the difficulty curve (tuning.h) ------------------------------------------------------------------------------ */
int difficulty_wave(int32_t m) { return m < 0 ? 0 : m / DIFF_WAVE_M; }

int difficulty_target(int32_t m)
{
    if (m < 0) m = 0;
    int ramp = DIFF_START + (int)((int64_t)(DIFF_MAX - DIFF_START) * m / (m + DIFF_HALF_M));
    int phase = m % DIFF_WAVE_M;
    int t = ramp + DIFF_WAVE_AMP * (2 * phase - DIFF_WAVE_M) / DIFF_WAVE_M;
    return t < 0 ? 0 : t;
}

/* ---- choosing ---------------------------------------------------------------------------------------------------------- */
static int allowed_here(int pat, int32_t col, int tier)
{
    const pattern_def *d = &bt_patterns[pat];
    if (pat_ncombo(pat, tier) <= 0) return 0;
    if (course_loop_of_col(col) > 0) return 1;      /* the night loop takes every pattern */
    return (d->biomes >> course_biome_of_col(col)) & 1;
}

/* how many combos of pat are in [lo, hi] (and one of them at random when pick != NULL) */
static int combos_in_band(int pat, int tier, int lo, int hi, rs_rng *rng, int *pick)
{
    int n = pat_ncombo(pat, tier), in = 0;
    for (int k = 0; k < n; k++) {
        int s = table_score(pat, tier, k);
        if (s < 0) s = (lo + hi) / 2;               /* a stale table: every combo fits */
        if (s >= lo && s <= hi) in++;
    }
    if (!pick || !in) return in;
    int want = rs_rng_range(rng, in);
    for (int k = 0; k < n; k++) {
        int s = table_score(pat, tier, k);
        if (s < 0) s = (lo + hi) / 2;
        if (s >= lo && s <= hi && want-- == 0) { *pick = k; break; }
    }
    return in;
}

static int recent(const bt_director *d, int pat)
{
    for (int i = 0; i < DIRECTOR_MEMORY; i++)
        if (d->recent[i] == pat) return 1;
    return 0;
}

static void remember(bt_director *d, int pat)
{
    memmove(d->recent + 1, d->recent, sizeof d->recent - sizeof d->recent[0]);
    d->recent[0] = pat;
    d->last = pat;
}

void director_start(bt_course *c, uint32_t seed)
{
    bt_director *d = &c->dir;
    uint32_t s = seed * 2654435761u ^ 0x9e3779b9u;     /* mixed: nearby seeds give unrelated courses */
    s ^= s >> 15;
    s *= 0x2c1b3c6du;
    s ^= s >> 12;
    rs_rng_seed(&d->rng, s ? s : 1);
    d->seed = seed;
    for (int i = 0; i < DIRECTOR_MEMORY; i++) d->recent[i] = -1;
    d->last = -1;
    d->breath_wave = 0;
    d->coins = 0;
    course_place_flat(c, -START_COL + START_CELLS, PAT_START);
}

static pbuild len_scratch;         /* scratch (state_audit.txt) */

static int pat_len(int pat, int tier, int combo)
{
    pbuild *b = &len_scratch;
    pat_build(b, pat, tier, combo, 0);
    return b->len;
}

static void next_segment(bt_course *c)
{
    bt_director *d = &c->dir;
    int32_t col = c->next_col;
    int32_t boundary = (col / BIOME_CELLS + 1) * BIOME_CELLS;
    int tier = course_tier_of_col(c, col);
    if (col > 0 && col % BIOME_CELLS == 0 && c->fixed_tier < 0) {
        course_place(c, PAT_GATE, tier, 0, 0, 0);
        d->last = -1;
        return;
    }
    int32_t m = col;
    int target = difficulty_target(m), wave = difficulty_wave(m);
    int lo = target - DIFF_BAND_LO, hi = target + (DIFF_BAND_HI < DIFF_SPIKE ? DIFF_BAND_HI : DIFF_SPIKE);
    if (m < DIFF_EASY_M && hi > DIFF_EASY_MAX) hi = DIFF_EASY_MAX;
    if (lo > hi) lo = hi;
    int pat = -1, combo = 0;
    if (wave > d->breath_wave) {
        /* the wave is over: a breather first */
        d->breath_wave = wave;
        int nb = 0, cand[8];
        for (int p = 0; p < bt_npatterns && nb < 8; p++)
            if (bt_patterns[p].breather && allowed_here(p, col, tier)) cand[nb++] = p;
        if (nb) {
            pat = cand[rs_rng_range(&d->rng, nb)];
            combo = rs_rng_range(&d->rng, pat_ncombo(pat, tier));
        }
    }
    for (int pass = 0; pat < 0 && pass < 4; pass++) {
        int cand[64], n = 0;
        for (int p = 0; p < bt_npatterns; p++) {
            if (bt_patterns[p].breather || p == d->last || !allowed_here(p, col, tier)) continue;
            if (pass == 0 && recent(d, p)) continue;
            if (combos_in_band(p, tier, lo, hi, NULL, NULL) > 0 && n < 64) cand[n++] = p;
        }
        if (n) {
            pat = cand[rs_rng_range(&d->rng, n)];
            combos_in_band(pat, tier, lo, hi, &d->rng, &combo);
        } else if (pass >= 1) {
            lo -= 12;                               /* nothing in the band: look easier (never harder) */
        }
    }
    if (pat < 0) {                                  /* nothing at all: a breather */
        for (int p = 0; p < bt_npatterns; p++)
            if (bt_patterns[p].breather) { pat = p; break; }
        combo = 0;
    }
    int br = bridge_cells(d->last, pat, tier);
    int len = pat_len(pat, tier, combo);
    if (col + br + len > boundary) {
        course_place_flat(c, (int)(boundary - col), PAT_BRIDGE);
        d->last = -1;
        return;
    }
    if (br) course_place_flat(c, br, PAT_BRIDGE);
    int coin_on = bt_patterns[pat].coin_spot && m >= COIN_MIN_M && rs_rng_range(&d->rng, 256) < COIN_CHANCE;
    if (coin_on) d->coins++;
    course_place(c, pat, tier, combo, coin_on, target);
    remember(d, pat);
}

void director_fill(bt_course *c, int32_t upto)
{
    while (c->next_col < upto) next_segment(c);
}
