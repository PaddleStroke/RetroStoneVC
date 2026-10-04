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
enum { K_NUT, K_TWIG, K_KNOT, K_WEB, K_SAP, K_HOLE, K_CATER, K_CONE, K_OWL, K_STUMP, K_GAP };

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

static int in_spr(int tile, int id, int frames)
{
    int t0 = gm_spr[id].tile;
    int last = gm_spr[id + frames - 1].tile + gm_spr[id + frames - 1].w / 8 * (gm_spr[id + frames - 1].h / 8);
    return tile >= t0 && tile < last;
}

/* the frame index of a sprite id range a tile belongs to */
static int frame_of(int tile, int id, int frames)
{
    for (int f = 0; f < frames; f++)
        if (tile == gm_spr[id + f].tile) return f;
    return -1;
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
    if (row >= TEX_H || a <= 0) return 0;
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

/* the anchor of an OAM sprite of def id: the point on the bark */
static void anchor(const rs_sprite *s, int id, int *x, int *y)
{
    const gm_sprite_def *d = &gm_spr[id];
    if (d->mirror) *x = (s->flags & RS_SPR_HFLIP) ? s->x : s->x + d->w;
    else *x = (s->flags & RS_SPR_HFLIP) ? s->x + (d->w - 1 - d->ax) : s->x + d->ax;
    *y = s->y + d->ay;
}

static void add_seen(int kind, int x, int y, int32_t half, int32_t h)
{
    if (nsn >= SEEN_MAX) return;
    int32_t z;
    int32_t a = angle_at(y, x, &z);
    if (z == 0) return;
    for (int i = 0; i < nsn; i++)                 /* the two halves of a mirrored sprite: one thing */
        if (sn[i].kind == kind && iabs32(sn[i].z - z) < 256 && iabs32(sn[i].ang - a) < DEG(3)) return;
    sn[nsn++] = (seen){kind, z, a, half, h};
}

/* reads the screen: the player's squirrel (its feet, its lean) and the things on the branch */
static int look(int player, int *fx, int *fy, int *air)
{
    int found = 0, pal = hu_player_pal(player);
    nsn = 0;
    dead_side = 0;
    windy = 0;
    *air = 0;
    int shx[8], shy[8], nsh = 0, stump_y = -1;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        int t = s->tile, x, y;
        if (in_spr(t, SPR_LEAF, NFR_LEAF)) { windy = 1; continue; }
        if (s->pal == pal && (in_spr(t, SPR_SQ_RUN, NFR_SQ_RUN) || in_spr(t, SPR_SQ_JUMP, NFR_SQ_JUMP))) {
            int id = in_spr(t, SPR_SQ_RUN, NFR_SQ_RUN) ? SPR_SQ_RUN + frame_of(t, SPR_SQ_RUN, NFR_SQ_RUN)
                                                       : SPR_SQ_JUMP + frame_of(t, SPR_SQ_JUMP, NFR_SQ_JUMP);
            if (id < 0) continue;
            anchor(s, id, fx, fy);
            if (in_spr(t, SPR_SQ_JUMP, NFR_SQ_JUMP)) *air = 1;
            found = 1;
            continue;
        }
        struct { int id, n, kind; int32_t half, h; } K[] = {
            {SPR_ACORN, NFR_ACORN, K_NUT, 0, 0}, {SPR_HAZEL, NFR_HAZEL, K_NUT, 0, 0},
            {SPR_WALNUT, NFR_WALNUT, K_NUT, 0, 0}, {SPR_TWIG, NFR_TWIG, K_TWIG, TWIG_HALF, TWIG_H},
            {SPR_KNOT, NFR_KNOT, K_KNOT, KNOT_HALF, KNOT_H}, {SPR_WEB, NFR_WEB, K_WEB, WEB_HALF, WEB_H},
            {SPR_SAP, NFR_SAP, K_SAP, SAP_HALF, 0}, {SPR_HOLE, NFR_HOLE, K_HOLE, HOLE_HALF, 0},
            {SPR_CATER, NFR_CATER, K_CATER, CATER_HALF, CATER_H}, {SPR_CONE, NFR_CONE, K_CONE, CONE_HALF, CONE_H},
            {SPR_SHADOW, NFR_SHADOW, K_CONE, CONE_HALF, CONE_H}, {SPR_OWL, NFR_OWL, K_OWL, OWL_HALF, OWL_H},
            {SPR_SPLINTER, NFR_SPLINTER, K_STUMP, 0, 0}};
        for (unsigned k = 0; k < sizeof K / sizeof K[0]; k++) {
            if (!in_spr(t, K[k].id, K[k].n)) continue;
            int f = frame_of(t, K[k].id, K[k].n);
            if (f < 0) break;
            if (K[k].id == SPR_SHADOW) {           /* a falling cone's, or a squirrel's in the air: sorted below */
                anchor(s, K[k].id + f, &x, &y);
                if (nsh < 8) { shx[nsh] = x; shy[nsh++] = y; }
                break;
            }
            anchor(s, K[k].id + f, &x, &y);
            if (K[k].kind == K_STUMP) {               /* the nearest broken end: which side is dead */
                if (y > stump_y && y >= 0 && y < RS_SCREEN_H) {
                    stump_y = y;
                    dead_side = x < draw_line_x[y] ? -1 : 1;
                }
                break;
            }
            if (K[k].kind == K_OWL) {                 /* it flies: its size tells how high it is above the bark */
                static const int lod_k[4] = {340, 230, 154, 97};
                y += ((OWL_H + Q8(6)) >> 8) * lod_k[f % 4] >> 8;
            }
            add_seen(K[k].kind, x, y, K[k].half, K[k].h);
            break;
        }
    }
    /* its own shadow (below it, in the air) gives the line it is over; the others are falling pinecones */
    int mine = -1;
    for (int i = 0; i < nsh && found; i++)
        if (shx[i] > *fx - 12 && shx[i] < *fx + 12 && shy[i] > *fy && shy[i] < *fy + 64 && (mine < 0 || shy[i] < shy[mine]))
            mine = i;
    for (int i = 0; i < nsh; i++)
        if (i != mine) add_seen(K_CONE, shx[i], shy[i], CONE_HALF, CONE_H);
    if (mine >= 0) { *fx = shx[mine]; *fy = shy[mine]; *air = 1; }
    /* the gaps: lines with no branch between lines with branch, near to far */
    int seen_bark = 0;
    int32_t z_near = 0;
    for (int y = RS_SCREEN_H - 1; y > 40; y--) {
        int row = draw_line_row[y];
        if (row < TEX_H && row >= ROW_GRAIN && row < ROW_FORK) row = ROW_GRAIN;  /* the gap's edges */
        if (draw_line_row[y] < TEX_H) {
            if (seen_bark == 2 && nsn < SEEN_MAX) {
                sn[nsn++] = (seen){K_GAP, z_near, 0, DEG(90), (int32_t)draw_line_a[y] * FOCAL};   /* h = the far edge */
            }
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
    int danger_t = 9999;
    /* a candidate target angle: clear of solids / holes / webs / sap reaching it within the next 70 frames */
    int32_t best = 0, best_cost = 1 << 30;
    /* the forces it feels: a gust (the leaves), a curve (the branch bending away: its centre drifting between the
     * near and the far lines) at speed: keep nearer the top then */
    int32_t lim = DEG(42);
    int bend = draw_line_row[200] < TEX_H && draw_line_row[130] < TEX_H ? draw_line_x[130] - draw_line_x[200] : 0;
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
            if (o->kind == K_GAP) continue;
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
    if (!air && cool[player] == 0) {
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
                    danger_t = t;
                }
                continue;
            }
            if (o->kind == K_NUT || o->kind == K_SAP || o->kind == K_WEB) continue;
            int32_t margin = o->half + SQ_HALF_ANG + DEG(3);
            if (o->kind == K_CATER) margin += DEG(14);          /* it crawls: jump it rather than guess */
            if (o->kind != K_OWL && iabs32(pred - o->ang) >= margin && iabs32(ang - o->ang) >= margin) continue;
            int big = o->h > Q8(16) || o->kind == K_HOLE;
            int at = big ? 15 : 11;
            if (t <= at && t >= at - 3) {
                in->jump_pressed = 1;
                hold_left[player] = big ? JUMP_HOLD_MAX : 0;
                danger_t = t;
            }
        }
        if (in->jump_pressed) cool[player] = 20;
    }
    if (rs_option_int("botlog", 0))
        rs_log("bot f%u p%d: feet (%d,%d) air %d ang %.1f angv %.2f v %.1f target %.1f steer %d jump %d dead %d seen %d",
               rs_frame_count(), player, fx, fy, air, ang / 4096.0, angv / 4096.0, v / 256.0, best / 4096.0, in->steer,
               in->jump_pressed, dead_side, nsn);
    (void)danger_t;
}

/* ---- save states (main.c) ---- */
#define S(v) rs_state_var("bot." #v, &(v), sizeof(v))
void bot_state(void)
{
    S(sn); S(nsn); S(prev_ang); S(angv_est); S(have_prev); S(hold_left); S(cool); S(prev_row); S(speed_est);
    S(dead_side); S(windy);
}
#undef S
