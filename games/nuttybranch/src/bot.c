/*
 * Nutty Branch: the test bot (--opt bot=N). It plays from the SCREEN only, like a player:
 *   - the sprites in OAM left by the last frame: its squirrel (in its colours: the body's lean frame and its feet),
 *     its shadow in the air, the nuts, the traps (their size gives their distance, their place on the branch their
 *     angle), the stump splinters of a fork's dead branch;
 *   - the branch layer's per-line registers of the last frame (the HDMA tables: where the branch centre is, its
 *     scale, the texture row): the branch's width and position on each line, the gaps (lines with no branch between
 *     lines with branch), the bark rows rushing by (its speed);
 * and its own inputs. It never reads the world, the course or the RNG.
 * Each frame it picks a target angle (clear of what is ahead, toward nuts when it is safe, near the top), steers to
 * it with a PD controller, and jumps (a tap, or held for knots and gaps) when something it cannot dodge arrives.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/nuttybranch/LICENSE.
 */
#include "game.h"
#include "assets.h"
#include "house_ui.h"
#include <string.h>

#define SEEN_MAX 48
#define DEG(x) ((x) * 4096)

typedef struct seen { int kind; int32_t z, ang, half, h; } seen;   /* z Q8 units, ang Q12 deg */
enum { K_NUT, K_TWIG, K_KNOT, K_WEB, K_SAP, K_HOLE, K_CATER, K_CONE, K_OWL, K_STUMP, K_GAP, K_BEES, K_SWING, K_ARCH };

static seen sn[SEEN_MAX];
static int nsn;
static int32_t prev_ang[MAX_PLAYERS], angv_est[MAX_PLAYERS];
static int have_prev[MAX_PLAYERS], hold_left[MAX_PLAYERS], cool[MAX_PLAYERS];
static int16_t prev_row[RS_SCREEN_H];
static int32_t speed_est = Q8(5);
static int dead_side;                         /* a fork's dead branch seen ahead: -1, 1, 0 */
static int windy;                             /* leaves streaming across the screen: a gust */

void bot_reset(void)
{
    memset(have_prev, 0, sizeof have_prev);
    memset(hold_left, 0, sizeof hold_left);
    memset(cool, 0, sizeof cool);
    for (int y = 0; y < RS_SCREEN_H; y++) prev_row[y] = ROW_NONE;
    speed_est = Q8(5);
}

static int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }

/* arcsin of a Q14 ratio, in Q12 degrees (a search in the game's own sine) */
static int32_t asin_q12(int32_t r)
{
    int neg = r < 0;
    if (neg) r = -r;
    if (r >= 16384) return neg ? -DEG(90) : DEG(90);
    int32_t lo = 0, hi = DEG(90);
    while (hi - lo > 64) {
        int32_t mid = (lo + hi) / 2;
        if (sin_q14(mid) < r) lo = mid; else hi = mid;
    }
    return neg ? -lo : lo;
}

/* the branch on screen line y: the centre (px) nearest x and the half width (Q8 px), the line's z (Q8 units) */
static int line_branch(int y, int x, int *xc, int32_t *hw, int32_t *z)
{
    *z = 0;
    if (y < 0 || y >= RS_SCREEN_H) return 0;
    int row = draw_line_row[y], a = draw_line_a[y];
    if (!line_is_bark(y) || a <= 0) return 0;
    *z = (int32_t)a * FOCAL;
    *hw = BRANCH_R * 256 * 256 / a;
    *xc = draw_line_x[y];
    if (row >= ROW_FORK) {                     /* two branches: centres at +- S/2 texels from the midpoint */
        int j = (row - ROW_FORK) / 8;
        int half = FORK_STEP * (j + 1) / 2 * 256 / a;
        *xc = x < draw_line_x[y] ? draw_line_x[y] - half : draw_line_x[y] + half;
    }
    return 1;
}

static int32_t angle_at(int y, int x, int32_t *z)
{
    int xc;
    int32_t hw;
    if (!line_branch(y, x, &xc, &hw, z)) return 0;
    int32_t r = (int32_t)((int64_t)(x - xc) * 256 * 16384 / hw);
    if (r > 16384) r = 16384;
    if (r < -16384) r = -16384;
    return asin_q12(r);
}


static void add_seen(int kind, int x, int y, int32_t half, int32_t h, int has_ang, int32_t ang_given)
{
    if (nsn >= SEEN_MAX) return;
    int32_t z;
    int32_t a = angle_at(y, x, &z);
    if (z == 0) return;
    if (has_ang) a = ang_given;
    for (int i = 0; i < nsn; i++)                 /* several pieces or sprites of one thing: once */
        if (sn[i].kind == kind && iabs32(sn[i].z - z) < 256 * 3 && iabs32(sn[i].ang - a) < DEG(3)) return;
    sn[nsn++] = (seen){kind, z, a, half, h};
}

/* the scale (Q8 px per unit) of a sprite's pre-drawn size: what its size says about its distance */
static const int lod_k[8] = {410, 294, 212, 154, 110, 79, 56, 41};

static int in_set(int f, int id, int n) { return f >= id && f < id + n; }

/* reads the screen: the player's squirrel (its feet, its lean) and the things on the branch */
static int look(int player, int *fx, int *fy, int *air)
{
    int found = 0, pal = hu_player_pal(player);
    nsn = 0;
    dead_side = 0;
    windy = 0;
    *air = 0;
    *fx = *fy = 0;
    int shx[12], shy[12], nsh = 0, stump_y = -1;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        int f, x, y;
        if (!draw_sprite_id(s, &f, &x, &y)) continue;
        int flip = (s->flags & RS_SPR_HFLIP) != 0;
        if (s->pal == pal && (in_set(f, SPR_SQ_RUN, NFR_SQ_RUN) || in_set(f, SPR_SQ_JUMP, NFR_SQ_JUMP))) {
            *fx = x;
            *fy = y;
            if (in_set(f, SPR_SQ_JUMP, NFR_SQ_JUMP)) *air = 1;
            found = 1;
            continue;
        }
        if (in_set(f, SPR_LEAF, NFR_LEAF) || in_set(f, SPR_RAIN, NFR_RAIN)) { windy = 1; continue; }
        if (in_set(f, SPR_SHADOW, NFR_SHADOW)) {            /* a falling thing's, or a squirrel's own */
            if (nsh < 12) { shx[nsh] = x; shy[nsh++] = y; }
            continue;
        }
        if (in_set(f, SPR_BROKEN, NFR_BROKEN)) {             /* a broken end: off the centre line, a dead branch */
            if (y >= 0 && y < RS_SCREEN_H && y > stump_y && iabs32(x - draw_line_x[y]) > 20) {
                stump_y = y;
                dead_side = x < draw_line_x[y] ? -1 : 1;
            }
            continue;
        }
        if (in_set(f, SPR_ACORN, NFR_ACORN) || in_set(f, SPR_HAZEL, NFR_HAZEL) || in_set(f, SPR_WALNUT, NFR_WALNUT))
            add_seen(K_NUT, x, y, 0, 0, 0, 0);
        else if (in_set(f, SPR_LIMB, NFR_LIMB)) {          /* a branch across: low (jump it) or an arch (run under) */
            int arch = f - SPR_LIMB >= LODS - 1, side = flip ? -1 : 1;
            if (arch) add_seen(K_ARCH, x, y, 0, 0, 1, side);
            else add_seen(K_TWIG, x, y, TWIG_HALF, TWIG_H, 1, side * DEG(21));
        } else if (in_set(f, SPR_KNOT, NFR_KNOT)) add_seen(K_KNOT, x, y, KNOT_HALF, KNOT_H, 0, 0);
        else if (in_set(f, SPR_WEB, NFR_WEB)) add_seen(K_WEB, x, y, WEB_HALF, WEB_H, 0, 0);
        else if (in_set(f, SPR_SNOW, NFR_SNOW)) add_seen(K_KNOT, x, y, SNOW_HALF, SNOW_H, 0, 0);
        else if (in_set(f, SPR_SAP, NFR_SAP)) add_seen(K_SAP, x, y, SAP_HALF, 0, 0, 0);
        else if (in_set(f, SPR_HOLE, NFR_HOLE)) add_seen(K_HOLE, x, y, HOLE_HALF, 0, 0, 0);
        else if (in_set(f, SPR_CATER, NFR_CATER)) add_seen(K_CATER, x, y, CATER_HALF, CATER_H, 0, 0);
        else if (in_set(f, SPR_SNAKE, NFR_SNAKE)) add_seen(K_CATER, x, y, SNAKE_HALF, SNAKE_H, 0, 0);
        else if (in_set(f, SPR_CONE, NFR_CONE)) add_seen(K_CONE, x, y, CONE_HALF, CONE_H, 0, 0);
        else if (in_set(f, SPR_BIGSHADOW, NFR_BIGSHADOW)) add_seen(K_OWL, x, y, OWL_HALF, OWL_H, 0, 0);
        else if (in_set(f, SPR_BEE, NFR_BEE)) add_seen(K_BEES, x, y + 4, BEE_HALF / 2, BEE_H, 0, 0);
        else if (in_set(f, SPR_SWING, NFR_SWING)) {           /* hanging from its pivot: its frame is its swing */
            int k = lod_k[1 + (f - SPR_SWING) % 7], ai = (f - SPR_SWING) / 7;
            int deg = ai * 15 - SWING_AMP;
            int32_t lat = (int32_t)((int64_t)SWING_LEN * sin_q14(DEG(deg)) / BRANCH_R);   /* Q14 */
            add_seen(K_SWING, x, y + (((SWING_PIVOT >> 8) * k) >> 8), SWING_HALF, SWING_H, 1, asin_q12(lat));
        }
    }
    /* its own shadow (just under its feet; below them in the air) gives the line it is over; the others are falling */
    int mine = -1;
    for (int i = 0; i < nsh && found; i++)
        if (shx[i] > *fx - 12 && shx[i] < *fx + 12 && shy[i] >= *fy && shy[i] < *fy + 64 && (mine < 0 || shy[i] < shy[mine]))
            mine = i;
    for (int i = 0; i < nsh; i++)
        if (i != mine) add_seen(K_CONE, shx[i], shy[i], CONE_HALF, CONE_H, 0, 0);
    if (mine >= 0 && shy[mine] > *fy + 3) { *fx = shx[mine]; *fy = shy[mine]; *air = 1; }
    /* the gaps: lines with no branch between lines with branch, near to far */
    int seen_bark = 0;
    int32_t z_near = 0;
    for (int y = RS_SCREEN_H - 1; y > 40; y--) {
        if (line_is_bark(y)) {
            if (seen_bark == 2 && nsn < SEEN_MAX)
                sn[nsn++] = (seen){K_GAP, z_near, 0, DEG(90), (int32_t)draw_line_a[y] * FOCAL};   /* h = the far edge */
            seen_bark = 1;
            z_near = (int32_t)draw_line_a[y] * FOCAL;
        } else if (seen_bark) {
            seen_bark = 2;
        }
    }
    return found;
}

void bot_decide(int player, sq_input *in)
{
    memset(in, 0, sizeof *in);
    int fx, fy, air;
    if (cool[player] > 0) cool[player]--;
    if (!look(player, &fx, &fy, &air)) { have_prev[player] = 0; return; }
    int32_t zq;
    int32_t ang = angle_at(fy - 1, fx, &zq);
    if (zq == 0) { have_prev[player] = 0; return; }
    /* the speed: the bark rows passing the squirrel's line */
    if (player == 0 && fy > 0 && fy < RS_SCREEN_H) {
        int r = draw_line_row[fy], pr = prev_row[fy];
        if (r < 32 && pr < 32) {
            int32_t d = ((r - pr) & 31) * 256;
            if (d > Q8(3) && d < Q8(12)) speed_est += (d - speed_est) / 8;
        }
        memcpy(prev_row, draw_line_row, sizeof prev_row);
    }
    int32_t v = speed_est;
    if (have_prev[player]) angv_est[player] += ((ang - prev_ang[player]) - angv_est[player]) / 2;
    else angv_est[player] = 0;
    prev_ang[player] = ang;
    have_prev[player] = 1;
    int32_t angv = angv_est[player];
    /* what is ahead: arrival time in frames */
    int32_t zs = zq;
    /* a candidate target angle: clear of solids / holes / webs / sap reaching it within the next 70 frames */
    int32_t best = 0, best_cost = 1 << 30;
    /* the forces it feels: a gust (the leaves), a curve (the branch bending away: its centre drifting between the
     * near and the far lines) at speed: keep nearer the top then */
    int32_t lim = DEG(42);
    int bend = line_is_bark(200) && line_is_bark(130) ? draw_line_x[130] - draw_line_x[200] : 0;
    if (bend < 0) bend = -bend;
    if (windy) lim = DEG(24);
    if (bend > 40 && v > Q8(8)) lim = lim < DEG(28) ? lim : DEG(28);
    for (int32_t cand = -lim; cand <= lim; cand += DEG(3)) {
        static const int32_t lane[MAX_PLAYERS] = {0, -DEG(16), DEG(16), -DEG(30)};   /* each bot its own line */
        int64_t cost = (int64_t)iabs32(cand) / 64 + (int64_t)iabs32(cand - ang) / 160 +
                       (int64_t)iabs32(cand - lane[player]) / 96 + (lim < DEG(42) ? (int64_t)iabs32(cand) / 12 : 0);
        if (dead_side && (cand > 0 ? 1 : -1) == dead_side) cost += 4000;
        if (dead_side && iabs32(cand) < DEG(12)) cost += 2000;
        for (int i = 0; i < nsn; i++) {
            const seen *o = &sn[i];
            int32_t dz = o->z - zs;
            if (dz < -Q8(8)) continue;
            int t = (int)(dz / (v > 0 ? v : 256));
            if (t > 80) continue;
            int32_t d = iabs32(cand - o->ang);
            if (o->kind == K_NUT) {
                if (d < DEG(10) && iabs32(cand) < DEG(46)) cost -= 120;
                continue;
            }
            if (o->kind == K_GAP || o->kind == K_SWING) continue;     /* jumped, wherever they are */
            if (o->kind == K_ARCH) {
                obj fo;
                memset(&fo, 0, sizeof fo);
                fo.kind = OB_TWIG;
                fo.len = 1;
                fo.side = (int8_t)o->ang;
                int32_t lo, hi;
                if (limb_at(&fo, cand, &lo, &hi) && lo < SQ_BODY_H + Q8(3)) cost += 3000 + (80 - t) * 30;
                continue;
            }
            if (o->kind == K_BEES) {
                if (d < o->half + DEG(16)) cost += 3000 + (80 - t) * 30;
                continue;
            }
            int32_t margin = o->half + DEG(12) + (o->kind == K_CATER || o->kind == K_OWL ? DEG(10) : 0);
            if (o->kind == K_SAP) margin = o->half + DEG(6);
            if (d < margin) cost += (o->kind == K_SAP ? 600 : o->kind == K_WEB ? 1400 : 2500) + (80 - t) * 20;
            /* can it get there in time? */
            int32_t reach = (int32_t)DEG(1) * 3 / 2 * (t > 4 ? t - 4 : 0);
            if (iabs32(cand - ang) > reach + DEG(4) && d >= margin) cost += 300;
        }
        if (cost < best_cost) { best_cost = cost; best = cand; }
    }
    /* steer: a PD controller toward the target */
    int32_t want_v = (best - ang) / 10;
    if (want_v > DEG(2)) want_v = DEG(2);
    if (want_v < -DEG(2)) want_v = -DEG(2);
    int32_t err = want_v - angv;
    in->steer = err > DEG(1) / 6 ? 1 : err < -DEG(1) / 6 ? -1 : 0;
    /* jump: something it will reach soon, on its path, that it cannot dodge */
    if (hold_left[player] > 0) {
        hold_left[player]--;
        in->jump_held = 1;
    }
    int under = 0;                                  /* an arch coming: no jumping under it */
    for (int i = 0; i < nsn; i++)
        if (sn[i].kind == K_ARCH && sn[i].z - zs > -Q8(8) && (sn[i].z - zs) / (v > 0 ? v : 256) < 40) under = 1;
    if (!air && cool[player] == 0 && !under) {
        for (int i = 0; i < nsn; i++) {
            const seen *o = &sn[i];
            int32_t dz = o->z - zs;
            if (dz < 0) continue;
            int t = (int)(dz / (v > 0 ? v : 256));
            int32_t pred = ang + angv * t;
            if (o->kind == K_GAP) {
                int len = (int)((o->h - o->z) / (v > 0 ? v : 256));
                if (t <= 2) {
                    in->jump_pressed = 1;
                    hold_left[player] = len > 20 ? JUMP_HOLD_MAX : 8;
                }
                continue;
            }
            if (o->kind == K_NUT || o->kind == K_SAP || o->kind == K_WEB || o->kind == K_BEES || o->kind == K_ARCH) continue;
            int32_t margin = o->half + SQ_HALF_ANG + DEG(3);
            if (o->kind == K_CATER) margin += DEG(14);          /* it crawls: jump it rather than guess */
            if (o->kind != K_OWL && o->kind != K_SWING && iabs32(pred - o->ang) >= margin && iabs32(ang - o->ang) >= margin)
                continue;
            int big = o->h > Q8(16) || o->kind == K_HOLE;
            int at = o->kind == K_SWING ? 20 : big ? 15 : 11;   /* the swinging leaves: be at the top of the jump */
            if (t <= at && t >= at - 3) {
                in->jump_pressed = 1;
                hold_left[player] = big ? JUMP_HOLD_MAX : 0;
            }
        }
        if (in->jump_pressed) cool[player] = 20;
    }
    if (rs_option_int("botlog", 0))
        rs_log("bot f%u p%d: feet (%d,%d) air %d ang %.1f angv %.2f v %.1f target %.1f steer %d jump %d dead %d seen %d",
               rs_frame_count(), player, fx, fy, air, ang / 4096.0, angv / 4096.0, v / 256.0, best / 4096.0, in->steer,
               in->jump_pressed, dead_side, nsn);
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("bot." #v, &(v), sizeof(v))
void bot_state(void)
{
    S(sn); S(nsn); S(prev_ang); S(angv_est); S(have_prev); S(hold_left); S(cool); S(prev_row); S(speed_est);
    S(dead_side); S(windy);
}
#undef S
