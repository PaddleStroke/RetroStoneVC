/*
 * Beaver Rush: the test bot (--opt bot=N plays players 1..N). It plays from the SCREEN STATE only, as left by the
 * last frame: the BG2 map (which rows of its tree hold branch tiles, on which side) and its own view's sprites in
 * OAM (its beaver: which side of the trunk it stands on, from the sprite's flip). It never reads the world, the
 * trunk's generator or anything off screen.
 *
 * It plays like a quick, careful human: 7 to 9 frames between two gnaws (6.7 to 8.6 per second), 3 more to
 * switch sides, and it never gnaws where a branch would hit it (the side of the head-high branch, the side of
 * the branch coming down). Its speed, not its judgement, is what the timer eventually beats.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 */
#include "br.h"
#include "draw.h"
#include "assets.h"

static int wait_t[MAX_PLAYERS], gap[MAX_PLAYERS];
static rs_rng bot_rng;

void bot_reset(uint32_t seed)
{
    rs_rng_seed(&bot_rng, seed ^ 0xb07b07u);
    for (int p = 0; p < MAX_PLAYERS; p++) wait_t[p] = 0, gap[p] = 30;
}

/* the branch side of segment k of player p's tree, read from the BG2 map */
static int seen_branch(int p, int k)
{
    int row = (TRUNK_MAP_Y0 - SEG_H * (k + 1)) / 8, c0 = p * TREE_COLS, mw = rs_bg_map_w(RS_BG2) - 1;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 5; c++) {
            if (rs_bg_get(RS_BG2, (c0 + LBRANCH_COL + c) & mw, row + r)) return SIDE_L;
            if (rs_bg_get(RS_BG2, (c0 + RBRANCH_COL + c) & mw, row + r)) return SIDE_R;
        }
    return SIDE_NONE;
}

/* its beaver in its view's OAM range (a beaver frame): 1 = found; *side from the flip */
static int seen_beaver(int p, int *side)
{
    int t0 = br_spr[SPR_BEAVER].tile, t1 = br_spr[SPR_BEAVER + 9].tile + 16, first, count;
    draw_view_oam(p, &first, &count);
    for (int i = first; i < first + count && i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        if (s->tile >= t0 && s->tile < t1) {
            *side = (s->flags & RS_SPR_HFLIP) ? SIDE_R : SIDE_L;
            return 1;
        }
    }
    return 0;
}

int bot_decide(int player)
{
    int side;
    if (!seen_beaver(player, &side)) return 0;
    if (++wait_t[player] < gap[player]) return 0;
    int s0 = seen_branch(player, 0), s1 = seen_branch(player, 1);
    /* safe: not into the branch at head height (s0), not under the one coming down (s1) */
    int stay_ok = s0 != side && s1 != side;
    int other = other_side(side), go_ok = s0 != other && s1 != other;
    int want = stay_ok ? side : go_ok ? other : side;
    if (want != side && wait_t[player] < gap[player] + 3) return 0;   /* a switch takes a moment longer */
    wait_t[player] = 0;
    gap[player] = 7 + rs_rng_range(&bot_rng, 3);
    return want;
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("bot." #v, &(v), sizeof(v))
void bot_state(void) { S(wait_t); S(gap); S(bot_rng); }
#undef S
