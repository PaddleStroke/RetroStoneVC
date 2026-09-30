/*
 * Pancake Tower: the rules. One tower (the slider, the drop, the cut, the perfect window, the regrow, syrup,
 * toppings) and a match of one or two towers. No drawing, no sound and no randomness: the same presses give
 * the same game, and two players get exactly the same slide timings.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/pancaketower/LICENSE.
 */
#include "pt.h"
#include <string.h>

const geom GEOM_1P = {W0, SPAWN_DIST, SLIDE_SPEED0, PERFECT_MIN, PERFECT_MAX, REGROW_PX, SYRUP_SLIP};
const geom GEOM_2P = {W0_2P, SPAWN_DIST_2P, SLIDE_SPEED0_2P, PERFECT_MIN, PERFECT_MAX_2P, REGROW_PX_2P, SYRUP_SLIP_2P};

/* ---- pure helpers --------------------------------------------------------------------------------------- */
int tower_speed(const geom *g, int pancakes)
{
    int steps = pancakes / SPEED_EVERY;
    if (steps > SPEED_STEPS_MAX) steps = SPEED_STEPS_MAX;
    return (int)((int64_t)g->speed0 * (100 + SPEED_STEP_PCT * steps) / 100);
}

int tower_tolerance(const geom *g, int w)
{
    return clampi(w * PERFECT_PERMILLE / 1000, g->tol_min, g->tol_max);
}

int tower_side(int pancakes) { return (pancakes & 1) ? 1 : -1; }

int tower_topping_kind(int pancakes)
{
    if (pancakes <= 0 || pancakes % TOPPING_EVERY) return 0;
    return LK_STRAWBERRY + (pancakes / TOPPING_EVERY - 1) % TOPPINGS;
}

int tower_syrup_due(int pancakes) { return pancakes > 0 && pancakes % SYRUP_EVERY == SYRUP_OFFSET; }

int slip_offset(int slip, int k)
{
    const int n = SLIP_FRAMES;
    k = clampi(k, 0, n);
    return (slip * (n * n - (n - k) * (n - k)) + n * n / 2) / (n * n);    /* ease-out (quadratic) */
}

int cut_resolve(const geom *g, int x, int w, int bx, int bw, int *kx, int *kw, cut_piece *piece)
{
    int lo = x > bx ? x : bx, hi = x + w < bx + bw ? x + w : bx + bw;
    int off = x - bx;
    memset(piece, 0, sizeof *piece);
    if (hi - lo <= 0) {                           /* no overlap: the whole pancake falls */
        piece->x = x;
        piece->w = w;
        piece->side = off < 0 ? -1 : 1;
        *kx = x;
        *kw = 0;
        return 0;
    }
    if (absi(off) <= tower_tolerance(g, bw) && w == bw) {   /* perfect: snap onto the layer below */
        *kx = bx;
        *kw = bw;
        return 2;
    }
    *kx = lo;
    *kw = hi - lo;
    if (off > 0) {                                /* overhangs on the right */
        piece->x = bx + bw;
        piece->w = x + w - (bx + bw);
        piece->side = 1;
    } else {
        piece->x = x;
        piece->w = bx - x;
        piece->side = -1;
    }
    return 1;
}

int segment_of(int world_y)
{
    int s = (world_y - SEG_BASE_Y) / SEG_H;
    return clampi(s, SEG_KITCHEN, SEG_SPACE);
}

int medal_of(int score)
{
    return score >= MEDAL_PEARL ? 4 : score >= MEDAL_GOLD ? 3 : score >= MEDAL_SILVER ? 2 : score >= MEDAL_BRONZE ? 1 : 0;
}

/* ---- a tower ---------------------------------------------------------------------------------------------- */
const layer *tower_layer(const tower *tw, int i) { return &tw->ring[i % RING]; }
const layer *tower_top(const tower *tw) { return tower_layer(tw, tw->nlayers - 1); }
int tower_top_y(const tower *tw) { return tw->nlayers * ROW_H; }
int tower_alive(const tower *tw) { return tw->state != TS_MISSED; }

int tower_slider_x(const tower *tw)
{
    if (tw->state == TS_SLIDE) return (tw->sx + Q16_ONE / 2) >> 16;
    return tw->drop_x;
}

int tower_slider_y(const tower *tw)
{
    int y = tower_top_y(tw);
    if (tw->state == TS_SLIDE) return y + HOVER;
    if (tw->state == TS_DROP) return y + HOVER - HOVER * tw->t / DROP_FRAMES;
    return y;
}

static layer *push(tower *tw, int x, int w, int kind, int flags)
{
    layer *l = &tw->ring[tw->nlayers % RING];
    l->x = (int16_t)x;
    l->w = (int16_t)w;
    l->kind = (uint8_t)kind;
    l->flags = (uint8_t)flags;
    l->n = (uint16_t)tw->pancakes;
    int y = tw->nlayers * ROW_H;
    tw->nlayers++;
    if (y == CEILING_Y) tw->events |= EV_CEILING;
    if (y == ROOF_Y) tw->events |= EV_ROOF;
    return l;
}

static void spawn(tower *tw)
{
    const layer *top = tower_top(tw);
    int side = tower_side(tw->pancakes);
    int32_t centre = ((int32_t)top->x << 16) + ((int32_t)top->w << 15);
    tw->sw = top->w;
    tw->sx = centre + side * ((int32_t)tw->g.spawn << 16) - ((int32_t)tw->sw << 15);
    tw->vx = -side * tower_speed(&tw->g, tw->pancakes);
    if (tw->splash_in) {                          /* a splash that came while the last pancake was landing */
        tw->splash_in = 0;
        tw->syrup = 1;
        tw->ring[(tw->nlayers - 1) % RING].flags |= LF_SYRUP;
    }
    tw->state = TS_SLIDE;
    tw->t = 0;
    tw->events |= EV_SPAWN;
}

void tower_init(tower *tw, const geom *g)
{
    memset(tw, 0, sizeof *tw);
    tw->g = *g;
    push(tw, -g->w0 / 2, g->w0, LK_PANCAKE, 0);    /* the first pancake is already on the plate */
    tw->events = 0;
    spawn(tw);
    tw->events = 0;
}

static void next(tower *tw)
{
    int k = tower_topping_kind(tw->pancakes);
    if (k) {
        tw->state = TS_TOPPING;
        tw->t = 0;
        tw->events |= EV_TOPPING;
    } else if (tower_syrup_due(tw->pancakes)) {
        tw->state = TS_POUR;
        tw->t = 0;
        tw->events |= EV_POUR;
    } else {
        spawn(tw);
    }
}

static void resolve(tower *tw)
{
    const layer *below = tower_top(tw);
    int kx, kw;
    int r = cut_resolve(&tw->g, tw->drop_x, tw->sw, below->x, below->w, &kx, &kw, &tw->cut);
    tw->cut.kind = LK_PANCAKE;
    tw->cut.n = (uint16_t)(tw->pancakes + 1);
    tw->bonus = 0;
    if (r == 0) {
        tw->state = TS_MISSED;
        tw->t = 0;
        tw->chain = 0;
        tw->events |= EV_MISS;
        return;
    }
    int flags = 0;
    if (r == 2) {
        tw->chain++;
        tw->perfects++;
        if (tw->chain > tw->best_chain) tw->best_chain = tw->chain;
        flags = LF_PERFECT | LF_BUTTER;
        tw->events |= EV_PERFECT;
        if (tw->chain >= REGROW_CHAIN && kw < tw->g.w0) {
            int nw = kw + tw->g.regrow < tw->g.w0 ? kw + tw->g.regrow : tw->g.w0;
            kx -= (nw - kw) / 2;
            kw = nw;
            flags |= LF_BIG_BUTTER;
            tw->events |= EV_REGROW;
        }
        tw->bonus = tw->chain >= BONUS_CHAIN_AT ? BONUS_CHAIN : BONUS_PERFECT;
        if (tw->chain % SPLASH_EVERY == 0) tw->events |= EV_SPLASH;
    } else {
        tw->chain = 0;
        tw->events |= EV_CUT;
    }
    tw->pancakes++;
    tw->score += 1 + tw->bonus;
    push(tw, kx, kw, LK_PANCAKE, flags);
    tw->events |= EV_LAND;
    if (tw->pancakes % SPEED_EVERY == 0 && tw->pancakes / SPEED_EVERY <= SPEED_STEPS_MAX) tw->events |= EV_SPEEDUP;
    next(tw);
}

void tower_splash(tower *tw)
{
    if (tw->state == TS_MISSED) return;
    tw->events |= EV_SPLASHED;
    if (tw->state == TS_DROP || tw->state == TS_SLIP) {
        tw->splash_in = 1;
        return;
    }
    tw->syrup = 1;
    tw->ring[(tw->nlayers - 1) % RING].flags |= LF_SYRUP;
}

void tower_step(tower *tw, int press)
{
    tw->events = 0;
    switch (tw->state) {
    case TS_SLIDE: {
        if (press) {                              /* frozen where it was last drawn */
            tw->drop_x = tower_slider_x(tw);
            tw->state = TS_DROP;
            tw->t = 0;
            tw->events |= EV_DROP;
            break;
        }
        const layer *top = tower_top(tw);
        int32_t c = ((int32_t)top->x << 16) + ((int32_t)top->w << 15);
        int32_t lo = c - ((int32_t)tw->g.spawn << 16), hi = c + ((int32_t)tw->g.spawn << 16);
        int32_t half = (int32_t)tw->sw << 15;
        int32_t mid = tw->sx + half + tw->vx;
        if (mid > hi) { mid = 2 * hi - mid; tw->vx = -tw->vx; }
        if (mid < lo) { mid = 2 * lo - mid; tw->vx = -tw->vx; }
        tw->sx = mid - half;
        tw->t++;
        break;
    }
    case TS_DROP:
        if (++tw->t >= DROP_FRAMES) {
            if (tw->syrup) {
                tw->syrup = 0;
                tw->slip_dir = tw->vx > 0 ? 1 : -1;
                tw->slip_x0 = tw->drop_x;
                tw->state = TS_SLIP;
                tw->t = 0;
                tw->events |= EV_SLIP;
            } else {
                resolve(tw);
            }
        }
        break;
    case TS_SLIP:
        tw->t++;
        tw->drop_x = tw->slip_x0 + tw->slip_dir * slip_offset(tw->g.slip, tw->t);
        if (tw->t >= SLIP_FRAMES) resolve(tw);
        break;
    case TS_TOPPING:
        if (++tw->t >= TOPPING_FRAMES) {
            const layer *top = tower_top(tw);
            int x = top->x, w = top->w;
            push(tw, x, w, tower_topping_kind(tw->pancakes), 0);
            tw->events |= EV_TOPPING_LAND;
            if (tower_syrup_due(tw->pancakes)) { tw->state = TS_POUR; tw->t = 0; tw->events |= EV_POUR; }
            else spawn(tw);
        }
        break;
    case TS_POUR:
        if (++tw->t >= POUR_FRAMES) {
            tw->syrup = 1;
            tw->ring[(tw->nlayers - 1) % RING].flags |= LF_SYRUP;
            spawn(tw);
        }
        break;
    case TS_MISSED:
        tw->t++;
        break;
    }
}

/* ---- a match -------------------------------------------------------------------------------------------- */
void match_init(match *m, int players)
{
    memset(m, 0, sizeof *m);
    m->players = clampi(players, 1, MAX_PLAYERS);
    for (int p = 0; p < m->players; p++) tower_init(&m->tw[p], m->players == 2 ? &GEOM_2P : &GEOM_1P);
}

void match_step(match *m, const int press[MAX_PLAYERS])
{
    for (int p = 0; p < m->players; p++) tower_step(&m->tw[p], press[p]);
    if (m->players == 2)
        for (int p = 0; p < 2; p++)
            if (m->tw[p].events & EV_SPLASH) tower_splash(&m->tw[1 - p]);
    m->t++;
}

int match_over(const match *m)
{
    for (int p = 0; p < m->players; p++)
        if (tower_alive(&m->tw[p])) return 0;
    return 1;
}

int match_winner(const match *m)
{
    const tower *a = &m->tw[0], *b = &m->tw[1];
    if (a->pancakes != b->pancakes) return a->pancakes > b->pancakes ? 0 : 1;
    if (a->score != b->score) return a->score > b->score ? 0 : 1;
    return -1;
}
