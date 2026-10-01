/*
 * Duck Parade: the test bot (--opt bot=1). It plays from the SCREEN STATE only, like a player looking at the
 * picture: the sprites in OAM (its duck, the cars, bikes, buses, joggers and mowers, the logs, pads and boats,
 * the trains, the crossing lights, the lost ducklings), the lanes' tiles in the BG3 map and the BG3 scroll
 * register (which map column is on the left of the screen). It never reads the world, the lanes' tables or
 * the RNG. Speeds are measured by following the sprites from frame to frame; what it cannot see is assumed
 * dangerous (a car may be just off screen; a light that is on means a train).
 *
 * When its duck stands still (it knows its own hops: 12 frames each, like a player knows the feel), it builds
 * a model of the lanes around it from what it sees and tries each move (wait, forward, up, down, back) with the
 * game's own reachability search (lanes.c judge_run, the fairness judge): the move that gets furthest ahead
 * soonest wins; a lost duckling nearby is worth a detour when it is safe.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/duckparade/LICENSE.
 */
#include "dp.h"
#include "assets.h"
#include "house_ui.h"
#include <stdio.h>
#include <string.h>

int draw_scroll_x(void);

#define VIS 22                    /* screen columns looked at */
#define AHEAD 6                   /* model columns ahead of the duck */
#define BEHIND 2
#define HORIZON 160               /* frames of the search */
#define HORIZON_LONG 420          /* ... when nothing ahead is in reach (a dead end: the way round is long) */
#define CROWD_PENALTY 150         /* co-op: hopping onto another parent's cell costs this (in frames of the score) */

enum { C_NONE, C_GRASS, C_BLOCK, C_ROAD, C_RAIL, C_PARK, C_POND, C_PAD, C_NEST, C_RIVER };
static uint8_t cls[65536];        /* BG3 map entry (a cell's top-left) -> what it shows */
static int cls_ready;

typedef struct obs { int y, h, kind, dir; } obs;   /* a sprite: field y, height, what, facing (+1 down, -1 up, 0 ?) */
enum { O_VEH, O_PLAT, O_TRAIN, O_LIGHT_ON, O_LIGHT_OFF, O_LOST };
#define OBS_MAX 12

typedef struct trk {              /* per BG3 map slot: the speed seen there */
    int32_t disp;                 /* px moved (signed) over `frames` */
    int frames, seen, lit_frames;
    int ny, y[OBS_MAX];
} trk;
static trk tr[32];
static int wait_frames[MAX_PLAYERS];
static int last_press[MAX_PLAYERS];
static uint16_t dead[32];         /* per map slot: rows found to be dead ends (lily pads with no way on) */
static uint32_t dead_until[32];
static uint32_t looked_at = 0xffffffffu;   /* the frame the screen was last looked at (one look a frame for all bots) */

static int slot_of(int scroll, int j) { return ((scroll + j * 16 - (scroll & 15)) >> 4) & 31; }
static int is_dead(int scroll, int j, int y)
{
    int s = slot_of(scroll, j);
    if (rs_frame_count() > dead_until[s]) dead[s] = 0;
    return y >= 0 && (dead[s] >> (y / CELL) & 1);
}

void bot_reset(void)
{
    memset(tr, 0, sizeof tr);
    memset(dead, 0, sizeof dead);
    for (int p = 0; p < MAX_PLAYERS; p++) wait_frames[p] = 0, last_press[p] = 0;
    looked_at = 0xffffffffu;
}

/* which parent a duck sprite is: its palette (the house rule: P1 OBJ 0, P2 OBJ 1, P3 OBJ 4, P4 OBJ 5) */
static int parent_of_pal(int pal)
{
    for (int p = 0; p < MAX_PLAYERS; p++)
        if (hu_player_pal(p) == pal) return p;
    return -1;
}

static void build_cls(void)
{
    memset(cls, C_NONE, sizeof cls);
    for (int m = 0; m < META_COUNT; m++) {
        int c = C_GRASS;
        if (m >= META_TREE0 && m <= META_TREE2) c = C_BLOCK;
        else if (m == META_BUSH0 || m == META_BUSH1 || m == META_ROCK || m == META_HEDGE0 || m == META_HEDGE1) c = C_BLOCK;
        else if (m >= META_BIKE_DASH_KERB_0 && m <= META_BIKE_SYM) c = C_ROAD;
        else if (m >= META_ROAD_DASH_KERB_0 && m <= META_ROAD_PLAIN_PLAIN_0) c = C_ROAD;
        else if (m == META_RAIL0 || m == META_RAIL1) c = C_RAIL;
        else if (m >= META_PATH_00_0 && m <= META_PATH_11_1) c = C_PARK;
        else if (m >= META_POND0 && m <= META_POND2) c = C_POND;
        else if (m >= META_PONDPAD0 && m <= META_PONDPAD2) c = C_PAD;
        else if (m >= META_NEST_ISLAND_BOTTOM && m <= META_NEST_WATER1) c = C_NEST;
        if (cls[dp_meta[m][0]] != C_NONE && cls[dp_meta[m][0]] != c) rs_log("bot: meta %d shares its corner tile (class %d vs %d)", m, cls[dp_meta[m][0]], c);
        cls[dp_meta[m][0]] = (uint8_t)c;
    }
    for (int s = 0; s < 16; s++)
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) cls[dp_river[s][a][b][0]] = C_RIVER;
    cls_ready = 1;
}

static int spr_in(int tile, int id, int frames)
{
    int t0 = dp_spr[id].tile, n = dp_spr[id].w / 8 * (dp_spr[id].h / 8);
    return tile >= t0 && tile < t0 + n * frames;
}
static int spr_frame(int tile, int id) { return (tile - dp_spr[id].tile) / (dp_spr[id].w / 8 * (dp_spr[id].h / 8)); }

/* ---- looking at the screen ------------------------------------------------------------------------------------------ */
static obs seen[VIS][OBS_MAX];
static int nseen[VIS];
static int me_x[MAX_PLAYERS], me_y[MAX_PLAYERS], me_found[MAX_PLAYERS];

static void look(int scroll)
{
    memset(nseen, 0, sizeof nseen);
    memset(me_found, 0, sizeof me_found);
    int fine = scroll & 15;
    for (int i = 0; i < RS_OAM_MAX; i++) {
        rs_sprite *s = rs_oam(i);
        if (!s || !s->used || (s->flags & RS_SPR_HIDE)) continue;
        int t = s->tile, kind = -1, dir = 0, y = s->y - FIELD_Y, h = s->h;
        if (spr_in(t, SPR_DUCK_RIGHT, 4) || spr_in(t, SPR_DUCK_LEFT, 4) || spr_in(t, SPR_DUCK_UP, 4) || spr_in(t, SPR_DUCK_DOWN, 4)) {
            int p = parent_of_pal(s->pal);
            if (s->prio == 2 && p >= 0) { me_x[p] = s->x; me_y[p] = s->y; me_found[p] = 1; }
            continue;
        }
        static const int cars[6] = {SPR_CAR0, SPR_CAR1, SPR_CAR2, SPR_CAR3, SPR_CAR4, SPR_CAR5};
        for (int c = 0; c < 6 && kind < 0; c++)
            if (spr_in(t, cars[c], 2)) { kind = O_VEH; dir = spr_frame(t, cars[c]) ? -1 : 1; y += 1; h = 22; }
        if (kind >= 0) {}
        else if (spr_in(t, SPR_BUS, 2)) { kind = O_VEH; dir = spr_frame(t, SPR_BUS) ? -1 : 1; }
        else if (spr_in(t, SPR_BIKE, 4)) { kind = O_VEH; dir = spr_frame(t, SPR_BIKE) >= 2 ? -1 : 1; y += 2; h = 12; }
        else if (spr_in(t, SPR_JOGGER, 8)) { kind = O_VEH; dir = spr_frame(t, SPR_JOGGER) >= 4 ? -1 : 1; y += 2; h = 12; }
        else if (spr_in(t, SPR_MOWER, 4)) { kind = O_VEH; dir = spr_frame(t, SPR_MOWER) >= 2 ? -1 : 1; y += 2; h = 20; }
        else if (spr_in(t, SPR_LOG2, 1) || spr_in(t, SPR_LOG3, 1) || spr_in(t, SPR_LOG4, 1) || spr_in(t, SPR_PAD, 3)) kind = O_PLAT;
        else if (spr_in(t, SPR_BOAT, 4)) { kind = O_PLAT; dir = spr_frame(t, SPR_BOAT) >= 2 ? -1 : 1; }
        else if (spr_in(t, SPR_LOCO, 2) || spr_in(t, SPR_CARRIAGE, 1)) kind = O_TRAIN;
        else if (spr_in(t, SPR_SIGNAL, 3)) kind = spr_frame(t, SPR_SIGNAL) ? O_LIGHT_ON : O_LIGHT_OFF;
        else if (spr_in(t, SPR_LING_PEEP, 2)) kind = O_LOST;
        if (kind < 0) continue;
        int j = (s->x + fine) >> 4;
        if (j < 0 || j >= VIS || ((s->x + fine) & 15)) continue;
        if (nseen[j] < OBS_MAX) {
            obs *o = &seen[j][nseen[j]++];
            o->y = y;
            o->h = h;
            o->kind = kind;
            o->dir = dir;
        }
    }
    /* the speeds: follow the movers of each map slot from the last frame */
    for (int j = 0; j < VIS; j++) {
        int slot = ((scroll + j * 16 - fine) >> 4) & 31;
        trk *k = &tr[slot];
        int ys[OBS_MAX], n = 0;
        for (int i = 0; i < nseen[j]; i++)
            if (seen[j][i].kind == O_VEH || seen[j][i].kind == O_PLAT) ys[n++] = seen[j][i].y;
        if (!k->seen) { k->disp = 0; k->frames = 0; k->lit_frames = 0; }
        else if (n && k->ny) {
            int best = 99, cnt = 0, sum = 0;
            for (int a = 0; a < n; a++)
                for (int b = 0; b < k->ny; b++) {
                    int d = ys[a] - k->y[b];
                    if (d >= -3 && d <= 3) { sum += d; cnt++; if (absi(d) < absi(best)) best = d; }
                }
            if (cnt) { k->disp += best; k->frames++; }
        }
        k->ny = n;
        memcpy(k->y, ys, sizeof(int) * (size_t)n);
        k->seen = 2;
        for (int i = 0; i < nseen[j]; i++) if (seen[j][i].kind == O_LIGHT_ON) k->lit_frames++;
    }
    for (int s = 0; s < 32; s++) if (tr[s].seen) tr[s].seen--;
}

/* the lane of screen column j, as the screen shows it, turned into the rules' lane (time 0 = now) */
static int model_lane(lane *l, int j, int scroll, uint16_t *lost_rows)
{
    int fine = scroll & 15, slot = ((scroll + j * 16 - fine) >> 4) & 31;
    const trk *k = &tr[slot];
    memset(l, 0, sizeof *l);
    int counts[10] = {0};
    uint16_t block = 0, pads = 0;
    for (int r = 0; r < ROWS; r++) {
        int c = cls[rs_bg_get(RS_BG3, slot * 2, 2 + r * 2)];
        counts[c]++;
        if (c == C_BLOCK) block |= (uint16_t)(1 << r);
        if (c == C_PAD) pads |= (uint16_t)(1 << r);
    }
    *lost_rows = 0;
    int lit = 0, train = 0;
    for (int i = 0; i < nseen[j]; i++) {
        if (seen[j][i].kind == O_LOST && seen[j][i].y >= 0) *lost_rows |= (uint16_t)(1 << (seen[j][i].y / 16));
        if (seen[j][i].kind == O_LIGHT_ON) lit = 1;
        if (seen[j][i].kind == O_TRAIN) train = 1;
    }
    int kind = counts[C_RIVER] ? LK_RIVER : counts[C_NEST] ? LK_NEST : (counts[C_POND] || counts[C_PAD]) ? LK_POND :
               counts[C_ROAD] ? LK_ROAD : counts[C_RAIL] ? LK_RAIL : counts[C_PARK] ? LK_PARK : LK_GRASS;
    l->kind = (uint8_t)kind;
    if (rs_frame_count() > dead_until[slot]) dead[slot] = 0;
    if (kind == LK_GRASS) { l->block = block | dead[slot]; return 1; }      /* dead ends are as good as trees */
    if (kind == LK_POND) { l->block = pads & (uint16_t)~dead[slot]; return 1; }
    if (kind == LK_NEST) return 1;
    if (kind == LK_RAIL) {
        if (lit || train) {                        /* a train is coming or passing: the whole track is deadly */
            l->kind = LK_ROAD;
            l->n = 2;
            l->m[0].p0 = 0, l->m[0].len = 176;
            l->m[1].p0 = 176, l->m[1].len = 176;
        } else {                                   /* no light: no train can come for TRAIN_WARN frames */
            l->period = 60000;
            l->phase = 0;
            l->cars = 4;
        }
        return 1;
    }
    /* moving lanes: the speed followed from frame to frame; unknown yet = keep out */
    int known = k->frames >= 16;
    int dir = 0;
    for (int i = 0; i < nseen[j]; i++) if (seen[j][i].dir) dir = seen[j][i].dir;
    if (!dir) dir = k->disp > 0 ? 1 : k->disp < 0 ? -1 : 0;
    int v = known ? (int)(absi(k->disp) * 256 / k->frames) : 0;
    l->down = (uint8_t)(dir > 0);
    l->v = (uint16_t)v;
    if (kind == LK_RIVER) {
        if (!known || !dir) return 0;              /* not measured yet */
        for (int i = 0; i < nseen[j] && l->n < MOVER_MAX; i++) {
            const obs *o = &seen[j][i];
            if (o->kind != O_PLAT) continue;
            int sh = 1 + 32 / k->frames;                                      /* a margin at each end */
            l->m[l->n].p0 = (int16_t)modi(o->y + sh + LOOP_TOP, LOOP_PX);
            l->m[l->n].len = (uint8_t)(o->h - 2 * sh);
            l->n++;
        }
        /* platforms leaving the view are still there for a moment: keep the model's speed error small */
        l->v = (uint16_t)(v > 4 ? v : 4);
        return 1;
    }
    if (!known && k->ny) return 0;
    /* vehicles as seen (a 2-px margin), and what may be hidden off screen: assumed a vehicle */
    int margin = 3 + 64 / (k->frames > 0 ? k->frames : 1);     /* the speed is known to 1/frames px per frame */
    for (int i = 0; i < nseen[j] && l->n < MOVER_MAX - 1; i++) {
        const obs *o = &seen[j][i];
        if (o->kind != O_VEH) continue;
        l->m[l->n].p0 = (int16_t)modi(o->y - margin + LOOP_TOP, LOOP_PX);
        l->m[l->n].len = (uint8_t)(o->h + 2 * margin);
        l->n++;
    }
    if (v) {                                       /* the part of the loop off screen (field y < -40 or > 231) */
        l->m[l->n].p0 = (int16_t)(FIELD_H + LOOP_TOP + 8);
        l->m[l->n].len = (uint8_t)(LOOP_PX - FIELD_H - 8 - 40);
        l->n++;
    }
    return 1;
}

/* ---- deciding ---------------------------------------------------------------------------------------------------------- */
static void bits_set(uint32_t *b, int y) { if (y >= 0 && y <= FIELD_H - CELL) b[y >> 5] |= 1u << (y & 31); }

/* is the first move itself safe (the duck at (j, y) at time 0), and where does it stand when it lands? */
static int first_move(const lane *cols, int n, int j, int y, int dir, int *j2, int *y2, int *t2)
{
    const lane *a = &cols[j];
    int river = a->kind == LK_RIVER;
    *j2 = j, *y2 = y, *t2 = 1;
    if (dir == 0) {                                /* wait a frame */
        int ny = y + (river ? (a->down ? 1 : -1) * ((int)(((int64_t)a->v * 1) >> 8)) : 0);
        *y2 = ny;
        return lane_can_stand(a, 1, ny, DUCK_HIT_H);
    }
    int dj = dir == HOP_FWD ? 1 : dir == HOP_BACK ? -1 : 0;
    if (j + dj < 0 || j + dj >= n) return 0;
    const lane *b = &cols[j + dj];
    int ty = y;
    if (dj) {
        if (lane_is_ground(b)) ty = clampi((y + CELL / 2) / CELL, 0, ROWS - 1) * CELL;
        if ((b->kind == LK_GRASS) && (b->block >> (ty / CELL) & 1)) return 0;
    } else {
        ty = y + (dir == HOP_UP ? -CELL : CELL);
        if (ty < 0 || ty > FIELD_H - CELL) return 0;
        if (a->kind == LK_GRASS && (a->block >> (ty / CELL) & 1)) return 0;
    }
    for (int k = 0; k < HOP_FRAMES; k++) {
        const lane *l = k < HOP_MID ? a : b;
        int yy = k < HOP_MID ? y : ty;
        if (!dj && river) yy = (k < HOP_MID ? y : ty) + (a->down ? 1 : -1) * (int)(((int64_t)a->v * k) >> 8);
        if (lane_hits(l, (uint32_t)k, yy + (CELL - DUCK_HIT_H) / 2, yy + (CELL + DUCK_HIT_H) / 2)) return 0;
    }
    if (!dj && river) ty += (a->down ? 1 : -1) * (int)(((int64_t)a->v * HOP_FRAMES) >> 8);
    if (!lane_can_stand(b, HOP_FRAMES, ty, DUCK_HIT_H)) return 0;
    *j2 = j + dj;
    *y2 = ty;
    *t2 = HOP_FRAMES;
    return 1;
}

int bot_decide(int player)
{
    if (!cls_ready) build_cls();
    int scroll = draw_scroll_x();
    if (looked_at != rs_frame_count()) {           /* the first bot to decide this frame looks for all of them */
        look(scroll);
        looked_at = rs_frame_count();
    }
    if (wait_frames[player] > 0) { wait_frames[player]--; return 0; }
    if (!me_found[player]) return 0;
    int fine = scroll & 15;
    int jm = (me_x[player] + fine + 8) >> 4;
    int ym = me_y[player] - FIELD_Y;
    /* the model: from BEHIND columns behind the duck to AHEAD columns ahead */
    lane cols[BEHIND + AHEAD + 1];
    uint16_t lost[BEHIND + AHEAD + 1];
    int j0 = jm - BEHIND, n = 0, jn = jm + AHEAD;
    if (j0 < 0) j0 = 0;
    if (jn > VIS - 1) jn = VIS - 1;
    for (int j = j0; j <= jn; j++) {
        if (!model_lane(&cols[n], j, scroll, &lost[n])) break;
        n++;
    }
    int me = jm - j0;
    if (me >= n) return 0;
    const lane *here = &cols[me];
    if (lane_is_ground(here)) ym = clampi((ym + CELL / 2) / CELL, 0, ROWS - 1) * CELL;
    /* try each move: the furthest column reached, then the soonest; a lost duckling close by is a goal too */
    judge_set_land_delay(1);                    /* the bot looks at the landing before it hops again */
    static const int moves[5] = {HOP_FWD, 0, HOP_UP, HOP_DOWN, HOP_BACK};
    int best = -1, best_score = -1000000, best_reach = -1;
    int want_lost = -1, want_row = 0;
    for (int j = me; j < n && j <= me + 3; j++)
        if (lost[j]) {
            for (int r = 0; r < ROWS; r++)
                if (lost[j] >> r & 1) { if (want_lost < 0 || absi(r * CELL - ym) < absi(want_row * CELL - ym)) { want_lost = j; want_row = r; } }
        }
    int fox_near = me_x[player] < 4 * CELL;
    int used_long = 0;
    for (int pass = 0; pass < 2 && best_reach <= me; pass++) {
    used_long = pass;
    int horizon = pass ? HORIZON_LONG : HORIZON;     /* nothing ahead in reach: look much further */
    best = -1, best_score = -1000000;
    for (int m = 0; m < 5; m++) {
        int j2, y2, t2;
        if (!first_move(cols, n, me, ym, moves[m], &j2, &y2, &t2)) continue;
        if (j2 != me && lane_is_ground(&cols[j2]) && is_dead(scroll, j0 + j2, y2)) continue;
        uint32_t start[JUDGE_WORDS] = {0};
        bits_set(start, y2);
        int16_t arr[BEHIND + AHEAD + 1];
        int f = judge_run(cols, n, j2, start, (uint32_t)t2, horizon, n - 1, NULL, arr);
        /* progress counts where the duck can rest (grass, a nest pond, a lily pad): a column reached in traffic may be
         * a trap; with no resting place in reach, the furthest column at all (far lower) */
        int reach = -1, when = 0, any = -1, any_when = 0;
        for (int j = n - 1; j >= 0; j--) {
            if (arr[j] < 0) continue;
            if (any < 0) { any = j; any_when = arr[j] + t2; }
            if (lane_is_safe_kind(&cols[j]) || cols[j].kind == LK_POND) { reach = j; when = arr[j] + t2; break; }
        }
        if (any < 0) continue;
        int score = reach >= 0 ? reach * 1000 - when : -100000 + any * 1000 - any_when;
        if (f >= 0 && reach == n - 1) score += 500;
        /* co-op: the other parents it sees on the screen: it would rather not land on one of them (four bots do not
         * walk in one pile; alone, nothing changes) */
        for (int q = 0; q < MAX_PLAYERS; q++) {
            if (q == player || !me_found[q]) continue;
            int jq = (me_x[q] + fine + 8) >> 4, rq = clampi((me_y[q] - FIELD_Y + CELL / 2) / CELL, 0, ROWS - 1);
            if (moves[m] && j0 + j2 == jq && (y2 + CELL / 2) / CELL == rq) score -= CROWD_PENALTY;
        }
        if (want_lost >= 0 && !fox_near) {
            uint32_t goal[JUDGE_WORDS] = {0};
            bits_set(goal, want_row * CELL);
            int g = judge_run(cols, n, j2, start, (uint32_t)t2, 90, want_lost, goal, NULL);
            if (g >= 0) score += 4000 - (g + t2) * 8;
        }
        if (rs_option_int("botlog", 0) >= 2) {
            char a[64];
            int len = 0;
            for (int j = 0; j < n && len < 56; j++) len += snprintf(a + len, sizeof a - (size_t)len, " %d", arr[j]);
            rs_log("   try %d: land (%d,%d) at %d, f %d, arrivals%s -> score %d", moves[m], j2 - me, y2, t2, f, a, score);
        }
        if (score > best_score) { best_score = score; best = moves[m]; best_reach = reach; }
    }
    }
    judge_set_land_delay(0);
    if (used_long && best && best != HOP_FWD && lane_is_ground(here))   /* a dead end: do not step back into it soon */
        dead[slot_of(scroll, j0 + me)] |= (uint16_t)(1 << (ym / CELL)), dead_until[slot_of(scroll, j0 + me)] = rs_frame_count() + 400;
    if (best < 0) best = lane_is_ground(here) && here->kind != LK_POND ? 0 : HOP_FWD;
    if (rs_option_int("botlog", 0)) {
        rs_log("bot f%u: col %d y %d kind %d -> %d (score %d)", rs_frame_count(), jm, ym, here->kind, best, best_score);
        for (int j = 0; j < n; j++) {
            const lane *l = &cols[j];
            char s[200];
            int len = 0;
            for (int i = 0; i < l->n && len < 180; i++) len += snprintf(s + len, sizeof s - (size_t)len, " %d/%d", field_y_of_loop(l->m[i].p0), l->m[i].len);
            s[len] = 0;
            rs_log("   col+%d kind %d block %04x v %d %s n %d:%s", j - me, l->kind, l->block, l->v, l->down ? "dn" : "up", l->n, s);
        }
    }
    if (best) {
        wait_frames[player] = HOP_FRAMES;      /* decide again once the picture shows it landed */
        last_press[player] = best;
    }
    return best;
}

/* ---- save states (main.c): the class table is a cache made again when needed ---- */
#define S(v) rs_state_var("bot." #v, &(v), sizeof(v))
void bot_state(void)
{
    S(tr); S(wait_frames); S(last_press); S(seen); S(nseen); S(me_x); S(me_y); S(me_found); S(dead); S(dead_until);
    S(looked_at);
}
#undef S

void bot_state_loaded(void) {}
