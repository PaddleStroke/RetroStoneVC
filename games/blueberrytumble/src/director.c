/*
 * Blueberry Tumble: the director. It writes the endless course ahead of the berry: many small patterns (and rarer set
 * pieces) and their parameters, picked by their MEASURED difficulty (src/tables.inc) so the generated difficulty
 * follows a target curve that rises with distance in waves; breathers after each wave; what was met lately (patterns,
 * pairs, instances) is avoided; bridges where two instances need one; a gate at each biome; rare golden blueberries.
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

int table_tail(int pat, int tier, int combo)
{
    const pat_tier_info *t = table_info(pat, tier);
    if (!t) return 99;
    if (combo < 0 || combo >= t->ncombo) return t->tail;
    return bt_ttail[t->first + combo];
}

int table_head(int pat, int tier, int combo)
{
    const pat_tier_info *t = table_info(pat, tier);
    if (!t) return -99;
    if (combo < 0 || combo >= t->ncombo) return t->head;
    return bt_thead[t->first + combo];
}

int bridge_cells(int prev, int prev_combo, int next, int next_combo, int tier)
{
    if (prev < 0 || next < 0) return 0;
    const pat_tier_info *a = table_info(prev, tier), *b = table_info(next, tier);
    if (!a || !b) return BEAT_CELLS;                /* a stale table: one safe beat */
    int frames = table_tail(prev, tier, prev_combo) - table_head(next, tier, next_combo) + LINK_MARGIN, cells = 0;
    if (frames > 0) {
        int32_t px = (int32_t)(((int64_t)frames * tier_speed(tier) + Q16_ONE - 1) >> 16);
        cells = (px + CELL - 1) / CELL;
        cells = (cells + BEAT_CELLS - 1) / BEAT_CELLS * BEAT_CELLS;
    }
    /* the few pairs the validator found needing more (tables.inc) */
    for (int i = 0; i < BT_TABLE_NLINKS; i++)
        if (bt_tlinks[i][0] == tier && bt_tlinks[i][1] == prev && bt_tlinks[i][2] == next)
            cells += bt_tlinks[i][3] * BEAT_CELLS;
    return cells;
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

/* ---- the run's seed ---------------------------------------------------------------------------------------------------- *
 * The frame of the press that starts the run, mixed with what the save RAM remembers (the runs played): two sessions
 * never replay each other's courses, and every bit of each input moves every bit of the seed (xorshift's first outputs
 * keep a seed's high bits: a seed that differs only in its low bits must not open the same way). */
static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint32_t run_seed(uint32_t press_frame, uint32_t runs, uint32_t salt)
{
    uint32_t s = mix32(press_frame ^ 0x6a09e667u);
    s = mix32(s ^ runs * 0x9e3779b9u);
    s = mix32(s ^ salt);
    return s ? s : 1;
}

/* ---- choosing ---------------------------------------------------------------------------------------------------------- */
static int allowed_here(int pat, int32_t col, int tier)
{
    const pattern_def *d = &bt_patterns[pat];
    if (pat_ncombo(pat, tier) <= 0) return 0;
    if (course_loop_of_col(col) > 0) return 1;      /* the night loop takes every pattern */
    return (d->biomes >> course_biome_of_col(col)) & 1;
}

static int recent(const bt_director *d, int pat)
{
    for (int i = 0; i < DIRECTOR_MEMORY; i++)
        if (d->recent[i] == pat) return 1;
    return 0;
}

static int recent_pair(const bt_director *d, int a, int b)
{
    if (a < 0) return 0;
    uint16_t k = (uint16_t)(a * 256 + b);
    int n = d->npairs < PAIR_MEMORY ? d->npairs : PAIR_MEMORY;
    for (int i = 0; i < n; i++)
        if (d->pairs[i] == k) return 1;
    return 0;
}

static int recent_inst(const bt_director *d, int pat, int combo)
{
    uint32_t k = (uint32_t)pat << 16 | (uint32_t)combo;
    int n = d->ninsts < INST_MEMORY ? d->ninsts : INST_MEMORY;
    for (int i = 0; i < n; i++)
        if (d->insts[i] == k) return 1;
    return 0;
}

static void remember(bt_director *d, int pat, int combo, int score)
{
    memmove(d->recent + 1, d->recent, sizeof d->recent - sizeof d->recent[0]);
    d->recent[0] = pat;
    if (d->last >= 0) d->pairs[d->npairs++ % PAIR_MEMORY] = (uint16_t)(d->last * 256 + pat);
    d->insts[d->ninsts++ % INST_MEMORY] = (uint32_t)pat << 16 | (uint32_t)combo;
    d->last = pat;
    d->last_combo = combo;
    if (score >= 0) d->mean256 += ((score << 8) - d->mean256) >> DIFF_MEAN_K;
}

/* the weight of a combo scoring s, for the band [lo, hi] around the aim: the nearer the aim the likelier (above the aim
 * counts double: the band reaches further down); seen lately: rare */
static int combo_weight(const bt_director *d, int pat, int combo, int s, int lo, int hi, int aim)
{
    if (s < lo || s > hi) return 0;
    int dist = s > aim ? (s - aim) * 2 : aim - s;
    int w = 4 + (DIFF_BAND_LO - dist > 0 ? DIFF_BAND_LO - dist : 0);
    if (recent_inst(d, pat, combo)) w = (w + 7) / 8;
    return w;
}

/* the combos of pat in [lo, hi]: their total weight (and one of them drawn by weight when pick != NULL) */
static int combos_in_band(const bt_director *d, int pat, int tier, int lo, int hi, int aim, rs_rng *rng, int *pick)
{
    int n = pat_ncombo(pat, tier), tot = 0;
    for (int k = 0; k < n; k++) {
        int s = table_score(pat, tier, k);
        if (s < 0) s = aim < hi ? aim : hi;         /* a stale table: every combo fits */
        tot += combo_weight(d, pat, k, s, lo, hi, aim);
    }
    if (!pick || !tot) return tot;
    int want = rs_rng_range(rng, tot);
    for (int k = 0; k < n; k++) {
        int s = table_score(pat, tier, k);
        if (s < 0) s = aim < hi ? aim : hi;
        want -= combo_weight(d, pat, k, s, lo, hi, aim);
        if (want < 0) { *pick = k; break; }
    }
    return tot;
}

/* how well pat can meet the aim: its best combo's weight (4 + DIFF_BAND_LO: one right on the aim) */
static int best_fit(const bt_director *d, int pat, int tier, int lo, int hi, int aim)
{
    int n = pat_ncombo(pat, tier), best = 0;
    for (int k = 0; k < n; k++) {
        int s = table_score(pat, tier, k);
        if (s < 0) s = aim < hi ? aim : hi;
        int w = combo_weight(d, pat, k, s, lo, hi, aim);
        if (w > best) best = w;
    }
    return best;
}

void director_start(bt_course *c, uint32_t seed)
{
    bt_director *d = &c->dir;
    rs_rng_seed(&d->rng, mix32(seed ^ 0x9e3779b9u) | 1);
    rs_rng_seed(&d->terrain, mix32(seed ^ 0x3c6ef372u) | 1);
    d->seed = seed;
    for (int i = 0; i < DIRECTOR_MEMORY; i++) d->recent[i] = -1;
    d->npairs = d->ninsts = 0;
    d->last = -1;
    d->last_combo = 0;
    d->breath_wave = 0;
    d->coins = 0;
    d->mean256 = difficulty_target(0) << 8;
    d->prof_col = START_COL;
    d->prof_d = d->prof_half = d->prof_step = 0;
    d->prof_feat = -1;
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
    /* steer: aim above the target when the course has lately been easier than it, below when harder */
    int steer = target - (d->mean256 >> 8);
    if (steer > DIFF_STEER) steer = DIFF_STEER;
    if (steer < -DIFF_STEER) steer = -DIFF_STEER;
    int aim = target + steer;
    int cap = target + DIFF_SPIKE;
    if (m < DIFF_EASY_M && cap > DIFF_EASY_MAX) cap = DIFF_EASY_MAX;
    if (aim > cap) aim = cap;
    int lo = aim - DIFF_BAND_LO, hi = aim + DIFF_BAND_HI;
    if (hi > cap) hi = cap;
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
    for (int pass = 0; pat < 0 && pass < 6; pass++) {
        int cand[64], wt[64], n = 0, tot = 0;
        for (int p = 0; p < bt_npatterns && n < 64; p++) {
            if (bt_patterns[p].breather || p == d->last || !allowed_here(p, col, tier)) continue;
            /* its weight, by how near the aim it can come (so the generated difficulty follows the target) */
            int fit = best_fit(d, p, tier, lo, hi, aim);
            int w = bt_patterns[p].weight * 64 * (fit + 2) / (DIFF_BAND_LO + 6);
            if (!fit) {
                /* all easier than the band: still in the mix now and then, as an interlude (the steering makes up
                 * for it); harder: never */
                if (pass > 0 || !combos_in_band(d, p, tier, 0, lo - 1, lo - 1, NULL, NULL)) continue;
                w /= DIFF_INTERLUDE;
            }
            if (recent(d, p)) w /= pass == 0 ? 64 : 4;  /* met lately: much rarer (unless little else fits) */
            if (recent_pair(d, d->last, p)) w /= 16;    /* this pair lately: rarer still */
            if (w < 1) w = 1;
            cand[n] = p;
            wt[n] = w;
            tot += w;
            n++;
        }
        if (n) {
            int want = rs_rng_range(&d->rng, tot), i = 0;
            while (want >= wt[i]) want -= wt[i++];
            pat = cand[i];
            if (!combos_in_band(d, pat, tier, lo, hi, aim, &d->rng, &combo))
                combos_in_band(d, pat, tier, 0, lo - 1, lo - 1, &d->rng, &combo);     /* an interlude: its hardest */
        } else {
            lo -= 8;                                /* nothing in the band: look easier (never harder) */
        }
    }
    if (pat < 0) {                                  /* nothing at all: a breather */
        for (int p = 0; p < bt_npatterns; p++)
            if (bt_patterns[p].breather) { pat = p; break; }
        combo = 0;
    }
    int br = bridge_cells(d->last, d->last_combo, pat, combo, tier);
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
    remember(d, pat, combo, bt_patterns[pat].breather ? -1 : table_score(pat, tier, combo));
}

/* ---- the slope's profile (drawing only: the physics, the solver and the validator never see it) ------------------ *
 * The playfield's shear draws the mean slope; the profile adds the landscape on top of it, column by column: steeper
 * and gentler stretches, rises and dips, a cliff edge dropping to a lower slope, a plateau. It only bends plain ground:
 * a column changes height only where it, and its neighbours, hold nothing (every obstacle sits on level ground, and
 * the berry's jumps over it read exactly as the physics has them). Steps (px, + = lower): 0 level; d +8; D +16; C +32
 * (the cliff: two plain columns on each side); u -8; h two columns of +4 (a gentle half-step); r two columns of -4 (a
 * plateau: it rises about as fast as the shear falls). Within PROF_WINDOW columns the profile never spans more than
 * PROF_SPAN px (the playfield map's rows), and it is drawn back toward level on the long run. */
static const char *const prof_features[] = {
    "0ddd0",            /* a steeper stretch */
    "h0h0h",            /* a gentle descent */
    "u0u",              /* a rise */
    "d00u",             /* a dip */
    "u00d",             /* a bump */
    "00C00",            /* a cliff edge: a drop to a lower slope */
    "rrrr",             /* a plateau */
    "D00",              /* a ledge: a step down */
    "rr0hh",            /* a hill */
    "0000",             /* straight on */
};
#define PROF_NFEAT ((int)(sizeof prof_features / sizeof prof_features[0]))

static int prof_plain(const bt_course *c, int32_t col)
{
    if (col < START_CELLS || col >= c->next_col) return 0;
    const bt_col *k = course_col(c, col);
    if (k->ground != G_GROUND || k->flags) return 0;
    for (int r = 0; r < ROWS; r++) if (k->cell[r]) return 0;
    return 1;
}
static int prof_free(const bt_course *c, int32_t col)
{
    return prof_plain(c, col - 1) && prof_plain(c, col) && prof_plain(c, col + 1);
}

/* would the offset nd keep the last PROF_WINDOW columns within PROF_SPAN px? */
static int prof_fits(bt_course *c, int32_t col, int nd)
{
    int lo = nd, hi = nd;
    for (int i = 1; i <= PROF_WINDOW && col - i >= START_COL && col - i >= c->next_col - RING; i++) {
        int v = course_col_w(c, col - i)->dy1;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    return hi - lo <= PROF_SPAN && nd >= -PROF_MAX && nd <= PROF_MAX;
}

static int prof_net(int f)
{
    int n = 0;
    for (const char *s = prof_features[f]; *s; s++)
        n += *s == 'd' ? 8 : *s == 'D' ? 16 : *s == 'C' ? 32 : *s == 'u' ? -8 : *s == 'h' ? 8 : *s == 'r' ? -8 : 0;
    return n;
}

static void profile_col(bt_course *c, int32_t col)
{
    bt_director *d = &c->dir;
    int D = d->prof_d, step = 0;
    if (d->prof_half) {
        step = d->prof_half;                        /* the second half of a half-step (its column was checked) */
        d->prof_half = 0;
    } else if (prof_free(c, col)) {
        if (d->prof_feat < 0) {
            /* a new feature, drawn back toward level: the ones that move away from it are rarer the further off */
            int w[PROF_NFEAT], tot = 0;
            for (int f = 0; f < PROF_NFEAT; f++) {
                int n = prof_net(f), off = D + n;
                w[f] = off * off <= D * D ? 8 : off * off > 24 * 24 ? 1 : 4;
                if (off > PROF_MAX || off < -PROF_MAX) w[f] = 0;
                tot += w[f];
            }
            int want = rs_rng_range(&d->terrain, tot), f = 0;
            while (want >= w[f]) want -= w[f++];
            d->prof_feat = f;
            d->prof_step = 0;
        }
        char s = prof_features[d->prof_feat][d->prof_step];
        int wait = 0;
        switch (s) {
        case 'd': step = 8; break;
        case 'D': step = 16; break;
        case 'u': step = -8; break;
        case 'C':
            if (prof_plain(c, col - 2) && prof_plain(c, col + 2)) step = 32;
            else wait = 1;
            break;
        case 'h': case 'r':
            if (prof_free(c, col + 1)) { step = s == 'h' ? 4 : -4; d->prof_half = step; }
            else wait = 1;
            break;
        default: break;
        }
        if (!prof_fits(c, col, D + step + d->prof_half)) {
            step = 0;                               /* it would leave the window: the feature ends here */
            d->prof_half = 0;
            d->prof_feat = -1;
        } else if (!wait && ++d->prof_step >= (int)strlen(prof_features[d->prof_feat])) {
            d->prof_feat = -1;
        }
    }
    bt_col *k = course_col_w(c, col);
    k->dy0 = (int8_t)D;
    k->dy1 = (int8_t)(D + step);
    d->prof_d = D + step;
}

int course_profile(const bt_course *c, int32_t x)
{
    int32_t col = x >> 4;
    if (col < c->next_col - RING || col >= c->next_col) return 0;
    const bt_col *k = &c->col[(uint32_t)(col - START_COL) & (RING - 1)];
    /* exactly the ramp strips' line (make_art.py mt_ramp): dy0 + round(delta * (x + 0.5) / 16), rounding down at .5 */
    int num = (k->dy1 - k->dy0) * (2 * (int)(x & 15) + 1) + 16;
    return k->dy0 + (num >= 0 ? num / 32 : -((-num + 31) / 32));
}

void director_fill(bt_course *c, int32_t upto)
{
    while (c->next_col < upto) next_segment(c);
    /* the profile follows, a few columns behind (it looks at its neighbours) */
    while (c->dir.prof_col < c->next_col - 3) profile_col(c, c->dir.prof_col++);
}
