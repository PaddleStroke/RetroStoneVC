/*
 * Blueberry Tumble: the course. A ring of 16-px columns (the ground and ROWS cells each), the cones, the pattern
 * instances (segments), and the frame clock that ties the scroll to the music's tempo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/blueberrytumble/LICENSE.
 */
#include "bt.h"
#include <string.h>
#include <limits.h>

static const int fpb_num[NTIERS] = TIER_FPB_NUM;
static const int fpb_den[NTIERS] = TIER_FPB_DEN;

int tier_fpb_num(int t) { return fpb_num[t]; }
int tier_fpb_den(int t) { return fpb_den[t]; }
int biome_frames(int t) { return BIOME_CELLS / BEAT_CELLS * fpb_num[t] / fpb_den[t]; }

/* 64 px per beat: 64 * den / num px per frame */
int32_t tier_speed(int t) { return (int32_t)(((int64_t)BEAT_PX * Q16_ONE * fpb_den[t]) / fpb_num[t]); }

static int64_t tier_dx(int t, int32_t frames)
{
    return ((int64_t)frames * BEAT_PX * Q16_ONE * fpb_den[t]) / fpb_num[t];
}

void course_reset(bt_course *c, int fixed_tier)
{
    memset(c, 0, sizeof *c);
    c->next_col = START_COL;
    c->fixed_tier = fixed_tier;
    c->end_col = INT32_MAX;
    c->dir.fixed_tier = fixed_tier;
    c->dir.last = -1;
}

static const bt_col flat_col;   /* ground, nothing on it */

const bt_col *course_col(const bt_course *c, int32_t col)
{
    if (col >= c->end_col || col < c->next_col - RING || col >= c->next_col || col < START_COL) return &flat_col;
    return &c->col[(uint32_t)(col - START_COL) & (RING - 1)];
}

bt_col *course_col_w(bt_course *c, int32_t col)
{
    return &c->col[(uint32_t)(col - START_COL) & (RING - 1)];
}

int course_biome_of_col(int32_t col) { return (col < 0 ? 0 : col / BIOME_CELLS) % NBIOMES; }
int course_loop_of_col(int32_t col) { return col < 0 ? 0 : col / (BIOME_CELLS * NBIOMES); }

int course_tier_of_col(const bt_course *c, int32_t col)
{
    if (c->fixed_tier >= 0) return c->fixed_tier;
    int b = col < 0 ? 0 : col / BIOME_CELLS;
    return b < NTIERS - 1 ? b : NTIERS - 1;
}

int64_t course_x(const bt_course *c, int32_t f)
{
    if (c->fixed_tier >= 0) return tier_dx(c->fixed_tier, f);
    int64_t x = 0;
    int b = 0;
    for (;;) {
        int t = b < NTIERS - 1 ? b : NTIERS - 1, n = biome_frames(t);
        if (f < n) return x + tier_dx(t, f);
        f -= n;
        x += (int64_t)BIOME_CELLS * CELL * Q16_ONE;
        b++;
    }
}

int32_t course_speed(const bt_course *c, int32_t f) { return (int32_t)(course_x(c, f + 1) - course_x(c, f)); }

int64_t cone_x(const bt_cone *k, int64_t x_ref)
{
    int64_t m = (int64_t)k->meet * Q16_ONE;
    return m + (((m - x_ref) * k->k256) >> 8);
}

const bt_seg *course_seg_at(const bt_course *c, int32_t col)
{
    for (int32_t i = c->nseg - 1; i >= 0 && i >= c->nseg - SEG_RING; i--) {
        const bt_seg *s = &c->seg[i % SEG_RING];
        if (col >= s->col0 && col < s->col0 + s->len) return s;
    }
    return NULL;
}

static void push_seg(bt_course *c, int32_t col0, int len, int pat, int tier, int combo, int score, int target)
{
    bt_seg *s = &c->seg[c->nseg % SEG_RING];
    s->col0 = col0;
    s->len = (int16_t)len;
    s->pat = (int8_t)pat;
    s->tier = (uint8_t)tier;
    s->combo = (uint16_t)combo;
    s->score = (uint8_t)(score < 0 ? 0 : score);
    s->target = (int16_t)target;
    c->nseg++;
}

void course_place_flat(bt_course *c, int cells, int kind)
{
    int32_t c0 = c->next_col;
    for (int i = 0; i < cells; i++) {
        bt_col *k = course_col_w(c, c->next_col);
        memset(k, 0, sizeof *k);
        k->seg = (uint16_t)c->nseg;
        c->next_col++;
    }
    push_seg(c, c0, cells, kind, course_tier_of_col(c, c0), 0, 0, 0);
}

static void place_build(bt_course *c, const pbuild *b, int pat, int tier, int combo, int score, int target)
{
    int32_t c0 = c->next_col;
    for (int i = 0; i < b->len; i++) {
        bt_col *k = course_col_w(c, c->next_col);
        *k = b->col[i];
        k->gone = 0;
        k->seg = (uint16_t)c->nseg;
        c->next_col++;
    }
    for (int i = 0; i < b->ncone; i++) {
        bt_cone *k = &c->cone[c->ncone % CONE_RING];
        *k = b->cone[i];
        k->meet += c0 * CELL;
        k->gone = 0;
        c->ncone++;
    }
    push_seg(c, c0, b->len, pat, tier, combo, score, target);
}

static pbuild place_scratch;       /* scratch: rebuilt for every placement (state_audit.txt) */

int32_t course_place(bt_course *c, int pat, int tier, int combo, int coin, int target)
{
    pbuild *bp = &place_scratch;
    int32_t c0 = c->next_col;
    if (pat == PAT_GATE) {
        pat_build_gate(bp, course_biome_of_col(c0));
        place_build(c, bp, PAT_GATE, tier, 0, 0, target);
    } else {
        pat_build(bp, pat, tier, combo, coin);
        place_build(c, bp, pat, tier, combo, table_score(pat, tier, combo), target);
    }
    return c0;
}
