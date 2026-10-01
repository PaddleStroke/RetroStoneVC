/*
 * Leady Squid: the test bot (--opt bot=1). It plays from the SCREEN STATE
 * only: the sprites in OAM left by the last frame (its squid, and the caps of
 * the obstacles on screen: a top cap's lower edge is the top of a gap, a bottom
 * cap's upper edge its bottom) plus its own flaps, from which it knows its
 * speed and tilt like a player who knows the feel of the game. It never reads
 * the world, the RNG or anything off-screen.
 *
 * Each frame it prefers a move (keep its body a little above the bottom of the
 * next gap, lower when the gap after is lower) and checks it with a memoised
 * depth-first search over flap / no flap for the next 112 frames against the
 * obstacles it can see (a 3-px margin, relaxed when needed); if no move is
 * safe it takes the one that lives longest.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/leadysquid/LICENSE.
 */
#include "ls.h"
#include "assets.h"
#include "house_ui.h"
#include <string.h>

#define HORIZON 112           /* frames: past the next obstacle and into the one after */
#define SEEN_MAX 12

typedef struct { int x, gt, gb; } seen_ob;

static seen_ob seen[SEEN_MAX];
static int nseen;
static int since_flap[MAX_PLAYERS] = {1000, 1000, 1000, 1000};
static int32_t est_y[MAX_PLAYERS];
static int est_ok[MAX_PLAYERS];
static int margin = 1;               /* px added above and below the hitbox in the search */
static uint8_t memo[HORIZON + 1][RS_SCREEN_H][64];   /* depth + 1 */
static uint16_t stamp[HORIZON + 1][RS_SCREEN_H][64], cur_stamp;   /* memo valid when == cur_stamp */

void bot_reset(void)
{
    for (int p = 0; p < MAX_PLAYERS; p++) since_flap[p] = 1000, est_ok[p] = 0;
}

void bot_notify_flap(int player) { since_flap[player] = 0; }

static int in(int tile, int id, int frames)
{
    int t0 = ls_spr[id].tile, n = ls_spr[id].w / 8 * (ls_spr[id].h / 8);
    return tile >= t0 && tile < t0 + n * frames;
}

/* reads OAM: the player's squid (x centre, y centre: the one in its colours) and the gaps */
static int look(int player, int *cx, int *cy, int *moving)
{
    int found = 0, pal = player ? hu_player_pal(player) : 0;
    nseen = 0;
    int ntop = 0, nbot = 0;
    seen_ob tops[SEEN_MAX], bots[SEEN_MAX];
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        int t = s->tile;
        if (s->pal == pal && s->w == 32 &&
            (in(t, SPR_SQUID_TILT, 6) || in(t, SPR_SQUID_FLAP, 3) || in(t, SPR_SQUID_IDLE, 2))) {
            *cx = s->x + 16;
            *cy = s->y + 16;
            *moving = !in(t, SPR_SQUID_IDLE, 2);
            found = 1;
        }
        for (int th = 0; th < THEMES; th++) {
            if (in(t, SPR_CAP_KELP + th * 2, 1) && ntop < SEEN_MAX) {
                tops[ntop].x = s->x;
                tops[ntop++].gt = s->y + s->h;
            } else if (in(t, SPR_CAP_KELP + th * 2 + 1, 1) && nbot < SEEN_MAX) {
                bots[nbot].x = s->x;
                bots[nbot++].gb = s->y;
            }
        }
    }
    for (int i = 0; i < ntop; i++)
        for (int j = 0; j < nbot; j++)
            if (tops[i].x == bots[j].x && nseen < SEEN_MAX) {
                seen[nseen].x = tops[i].x;
                seen[nseen].gt = tops[i].gt;
                seen[nseen].gb = bots[j].gb;
                nseen++;
            }
    return found;
}

/* would the squid collide at frame k (the world scrolled k px since the look)? The margin
 * (3 px, relaxed to 0 when needed) keeps the plans robust. */
static int hits(const squid *s, int k)
{
    box b = squid_box(s);
    b.y0 -= margin;
    b.y1 += margin;
    if (box_hits_seabed(b)) return 1;
    for (int i = 0; i < nseen; i++) {
        obstacle o;
        o.x = seen[i].x;
        o.gap_top = seen[i].gt;
        if (seen[i].gb - seen[i].gt != GAP) o.gap_top = seen[i].gb - GAP;
        o.index = o.theme = 0;
        if (box_hits_obstacle(b, &o, k * (SCROLL >> 16))) return 1;
    }
    return 0;
}

/* how many frames the squid can live from frame k (HORIZON = safe): depth-first, memoised */
static int depth(const squid *s, int k)
{
    if (k >= HORIZON) return HORIZON;
    int y = clampi((int)(s->y >> 16), 0, RS_SCREEN_H - 1), f = s->flap_t > 63 ? 63 : s->flap_t;
    if (stamp[k][y][f] == cur_stamp) return memo[k][y][f] - 1;
    int best = k;
    for (int flap = 0; flap < 2 && best < HORIZON; flap++) {
        squid n = *s;
        squid_swim_step(&n, flap);
        int d = hits(&n, k + 1) ? k : depth(&n, k + 1);
        if (d > best) best = d;
    }
    memo[k][y][f] = (uint8_t)(best + 1);
    stamp[k][y][f] = cur_stamp;
    return best;
}

int bot_decide(int player)
{
    int cx = 0, cy = 0, moving = 0;
    since_flap[player]++;
    if (!look(player, &cx, &cy, &moving)) { est_ok[player] = 0; return 0; }
    if (!moving) {                                          /* get ready: start the run */
        est_ok[player] = 0;
        return since_flap[player] > 40 && (rs_frame_count() % 64) == 20;
    }
    /* the squid as the player knows it: its position on screen, its speed and tilt from its last
     * flap, and the sub-pixel position followed from frame to frame (resynchronised whenever the
     * pixel shown disagrees) */
    squid s;
    squid_reset(&s, cx);
    s.state = SQ_SWIM;
    s.y = (int32_t)cy << 16;
    if (since_flap[player] < 1000) {
        s.vy = FLAP_VY;
        s.tilt = TILT_FLAP;
        for (int i = 1; i < since_flap[player] && i < 200; i++) {
            s.vy = min32(s.vy + GRAVITY, MAX_FALL);
            s.tilt = max32(s.tilt - TILT_RATE, TILT_MIN);
        }
        s.flap_t = since_flap[player];
    }
    if (est_ok[player] && (int)((est_y[player] + Q16_ONE / 2) >> 16) == cy) s.y = est_y[player];
    else s.y = ((int32_t)cy << 16) - Q16_ONE / 2 + (s.vy > 0 ? Q16_ONE / 2 : 0);
    /* the preferred move: keep the body a little above the bottom of the next gap (the arc of a
     * flap then stays inside the gap); the search vetoes moves with no safe future */
    const seen_ob *next = NULL, *after = NULL;
    for (int i = 0; i < nseen; i++)
        if (seen[i].x + OBST_W > cx - 8 && (!next || seen[i].x < next->x)) next = &seen[i];
    for (int i = 0; next && i < nseen; i++)
        if (seen[i].x > next->x && (!after || seen[i].x < after->x)) after = &seen[i];
    int target = next ? next->gb - 16 : PLAY_H / 2;
    if (next && after && cx + 8 > next->x && after->gb > next->gb) target = next->gb - 10;  /* hug the bottom */
    int want = cy > target && s.vy >= 0;
    squid mv[2] = {s, s};
    squid_swim_step(&mv[0], 0);
    squid_swim_step(&mv[1], 1);
    /* safe with a comfortable margin first, then tighter ones; doomed: the longest life */
    int d = -1, dd[2] = {0, 0};
    for (margin = 3; margin >= 0 && d < 0; margin--) {
        for (int o = 0; o < 2; o++) {
            int m = o ? !want : want;
            if (++cur_stamp == 0) { memset(stamp, 0, sizeof stamp); cur_stamp = 1; }
            dd[m] = hits(&mv[m], 1) ? 0 : depth(&mv[m], 1);
            if (dd[m] >= HORIZON) { d = m; break; }
        }
    }
    if (d < 0) d = (dd[1] > dd[0] ? 1 : dd[0] > dd[1] ? 0 : want) + 2;
    est_y[player] = mv[d & 1].y;
    est_ok[player] = 1;
    if (rs_option_int("botlog", 0))
        rs_log("bot f%u: y=%d (%.2f) vy=%d since=%d seen=%d next=(%d,%d,%d) margin=%d -> %d", rs_frame_count(), cy,
               s.y / 65536.0, s.vy, since_flap[player], nseen, next ? next->x : 0, next ? next->gt : 0,
               next ? next->gb : 0, margin + 1, d);
    return d & 1;
}

/* ---- save states (main.c). The search's memo (memo, stamp: 5 MB) is a cache and is not saved; after a load its
   entries belong to another timeline, and one stamped with the next cur_stamp would be taken as valid: clear
   them (a fresh process starts cleared too) ---- */
#define S(v) rs_state_var("bot." #v, &(v), sizeof(v))
void bot_state(void)
{
    S(seen); S(nseen); S(since_flap); S(est_y); S(est_ok); S(margin); S(cur_stamp);
}
#undef S

void bot_state_loaded(void) { memset(stamp, 0, sizeof stamp); }
