/*
 * Carrom's rules (carrom.h): the board, the opening circle, the shot run by the table's physics (cuetable.h), then
 * the pocketing, the fouls, the queen and her cover, the last coin, the end. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/carrom.h"

void crm_table(cue_table *t)
{
    memset(t, 0, sizeof *t);
    t->w = t->h = CRM_SIZE;
    t->mu = 55;
    t->drag = 0.55f;
    t->e_ball = 0.9f;
    t->e_wall = 0.72f;
    t->slip = 0.92f;
    t->nholes = 4;
    for (int k = 0; k < 4; k++) {
        t->hx[k] = k & 1 ? CRM_SIZE - CRM_HOLE_AT : CRM_HOLE_AT;
        t->hy[k] = k & 2 ? CRM_SIZE - CRM_HOLE_AT : CRM_HOLE_AT;
        t->hr[k] = CRM_HOLE_R;
    }
}

static void put(cue_world *w, int i, float x, float y, float r, float m)
{
    cue_ball *b = &w->b[i];
    memset(b, 0, sizeof *b);
    b->x = x;
    b->y = y;
    b->r = r;
    b->m = m;
    b->down = -1;
}

/* the opening circle: the queen in the middle, six round her, twelve round them (white's three lines from her) */
void crm_new(crm_pos *p, int players)
{
    memset(p, 0, sizeof *p);
    p->players = players == 4 ? 4 : 2;
    p->queen_team = -1;
    cue_world *w = &p->w;
    w->n = CRM_N;
    float c = CRM_SIZE / 2, d = 2 * CRM_COIN_R + 0.06f;
    put(w, CRM_STRIKER, c, CRM_BASE, CRM_STRIKER_R, 2.6f);
    put(w, CRM_QUEEN, c, c, CRM_COIN_R, 1);
    int nw = CRM_WHITE, nb = CRM_BLACK;
    for (int k = 0; k < 6; k++) {
        float a = CUE_PI / 2 + k * CUE_PI / 3;
        int i = k & 1 ? nb++ : nw++;
        put(w, i, c + d * cosf(a), c + d * sinf(a), CRM_COIN_R, 1);
        i = k & 1 ? nb++ : nw++;                /* the corner of the second ring, the same colour */
        put(w, i, c + 2 * d * cosf(a), c + 2 * d * sinf(a), CRM_COIN_R, 1);
        float m = a + CUE_PI / 6, dm = d * 1.7320508f;
        i = k & 1 ? nw++ : nb++;                /* between two corners: the other colour */
        put(w, i, c + dm * cosf(m), c + dm * sinf(m), CRM_COIN_R, 1);
    }
}

int crm_pocketed(const crm_pos *p, int team)
{
    int n = 0, i0 = team ? CRM_BLACK : CRM_WHITE;
    for (int i = i0; i < i0 + CRM_COINS; i++) n += p->w.b[i].down >= 0;
    return n;
}

void crm_turn_pt(int side, float x, float y, float *ox, float *oy)
{
    float c = CRM_SIZE / 2, dx = x - c, dy = y - c;
    switch (side & 3) {
    case 0: *ox = c + dx; *oy = c + dy; break;
    case 1: *ox = c + dy; *oy = c - dx; break;
    case 2: *ox = c - dx; *oy = c - dy; break;
    default: *ox = c - dy; *oy = c + dx; break;
    }
}

void crm_spot(const crm_pos *p, int seat, int pos, float *x, float *y)
{
    float u = (float)(pos < -1000 ? -1000 : pos > 1000 ? 1000 : pos) / 1000 * CRM_BASE_HALF;
    crm_turn_pt(crm_side(p, seat), CRM_SIZE / 2 + u, CRM_BASE, x, y);
}

int crm_spot_free(const crm_pos *p, int seat, int pos)
{
    float x, y;
    if (pos < -1000 || pos > 1000) return 0;
    crm_spot(p, seat, pos, &x, &y);
    return !cue_overlaps(&p->w, x, y, CRM_STRIKER_R, CRM_STRIKER);
}

int crm_free_pos(const crm_pos *p, int seat, int want)
{
    for (int d = 0; d <= 2000; d += 10) {
        if (crm_spot_free(p, seat, want - d)) return want - d;
        if (crm_spot_free(p, seat, want + d)) return want + d;
    }
    return want;
}

int crm_next(const crm_pos *p, int seat) { return (seat + 1) % p->players; }

int crm_ready(const crm_pos *p, int pos, int ang, int pow, cue_world *w)
{
    if (p->status != CRM_PLAY || pow < 1 || pow > 1000 || ang < 0 || ang >= 36000 || !crm_spot_free(p, p->turn, pos))
        return 0;
    *w = p->w;
    cue_ball *s = &w->b[CRM_STRIKER];
    crm_spot(p, p->turn, pos, &s->x, &s->y);
    s->down = -1;
    s->vx = s->vy = 0;
    for (int i = 0; i < w->n; i++) w->b[i].vx = w->b[i].vy = 0;
    cue_strike(w, CRM_STRIKER, ang, CRM_VMAX * (float)pow / 1000);
    return 1;
}

/* coin i back on the board: the free place nearest the middle */
static void back(crm_pos *p, int i)
{
    cue_ball *b = &p->w.b[i];
    b->down = -1;
    b->vx = b->vy = 0;
    float lo = CRM_COIN_R + 6, hi = CRM_SIZE - CRM_COIN_R - 6;
    cue_free_spot(&p->w, b->r, CRM_SIZE / 2, CRM_SIZE / 2, lo, lo, hi, hi, i, &b->x, &b->y);
}

/* one of the side's coins pocketed before goes back (the last of them); 0: none */
static int give_back(crm_pos *p, int team, uint32_t not)
{
    int i0 = team ? CRM_BLACK : CRM_WHITE;
    for (int i = i0 + CRM_COINS - 1; i >= i0; i--)
        if (p->w.b[i].down >= 0 && !(not >> i & 1)) { back(p, i); return 1; }
    for (int i = i0 + CRM_COINS - 1; i >= i0; i--)
        if (p->w.b[i].down >= 0) { back(p, i); return 1; }
    return 0;
}

void crm_apply(crm_pos *p, const cue_events *ev)
{
    int seat = p->turn, team = crm_team(p, seat), own = 0, queen_in = 0, foul = 0;
    uint32_t mine = 0;                          /* the coins of the shooter's side pocketed in this shot */
    for (int k = 0; k < ev->npot; k++) {
        int i = ev->pot[k];
        if (i == CRM_STRIKER) foul = 1;
        else if (i == CRM_QUEEN) queen_in = 1;
        else if (crm_colour(i) == team) { own++; mine |= 1u << i; }
    }
    p->foul = foul;
    int keep = 0;
    if (foul) {
        for (int i = 1; i < CRM_QUEEN; i++)
            if (mine >> i & 1) back(p, i);
        if (queen_in || (p->queen == CRM_Q_PENDING && p->queen_team == team)) {
            if (p->w.b[CRM_QUEEN].down >= 0) back(p, CRM_QUEEN);
            p->queen = CRM_Q_BOARD;
            p->queen_team = -1;
        }
        if (!give_back(p, team, 0)) p->debt[team]++;
    } else {
        if (p->queen == CRM_Q_PENDING && p->queen_team == team) {   /* the cover, or the queen back */
            if (own) p->queen = CRM_Q_COVERED;
            else {
                back(p, CRM_QUEEN);
                p->queen = CRM_Q_BOARD;
                p->queen_team = -1;
            }
        }
        if (queen_in) {
            p->queen = own ? CRM_Q_COVERED : CRM_Q_PENDING;
            p->queen_team = team;
        }
        while (p->debt[team] > 0 && crm_pocketed(p, team) > 0 && give_back(p, team, mine)) p->debt[team]--;
        keep = own > 0 || queen_in;
    }
    for (int t = 0; t < 2; t++)                 /* a last coin pocketed before the queen is covered goes back */
        if (crm_pocketed(p, t) == CRM_COINS && p->queen != CRM_Q_COVERED) {
            give_back(p, t, 0);
            keep = 0;
        }
    if (p->queen == CRM_Q_COVERED) {
        if (crm_pocketed(p, team) == CRM_COINS) p->status = team ? CRM_WIN1 : CRM_WIN0;
        else if (crm_pocketed(p, !team) == CRM_COINS) p->status = team ? CRM_WIN0 : CRM_WIN1;
    }
    cue_ball *s = &p->w.b[CRM_STRIKER];         /* the striker comes back to the next shooter's baseline */
    s->down = -1;
    s->vx = s->vy = 0;
    if (!keep && p->status == CRM_PLAY) p->turn = crm_next(p, seat);
    crm_spot(p, p->turn, crm_free_pos(p, p->turn, 0), &s->x, &s->y);
    p->shots++;
}

int crm_shot(crm_pos *p, int pos, int ang, int pow, cue_events *ev)
{
    cue_table t;
    cue_events e;
    cue_world w;
    if (!crm_ready(p, pos, ang, pow, &w)) return 0;
    crm_table(&t);
    cue_events_clear(&e);
    cue_run(&t, &w, &e);
    p->w = w;
    crm_apply(p, &e);
    if (ev) *ev = e;
    return 1;
}

int crm_parse(const char *s, int *pos, int *ang, int *pow, int *len)
{
    int n = 0;
    if (sscanf(s, "%d:%d:%d%n", pos, ang, pow, &n) != 3) return 0;
    if (len) *len = n;
    return 1;
}

void crm_game_new(crm_game *g, int players)
{
    g->ply = 0;
    crm_new(&g->pos[0], players);
}

int crm_play(crm_game *g, int pos, int ang, int pow, cue_events *ev)
{
    if (g->ply >= CRM_HIST) {                   /* a very long game: the oldest shot can no longer be undone */
        memmove(&g->pos[0], &g->pos[1], sizeof g->pos[0] * CRM_HIST);
        g->ply--;
    }
    g->pos[g->ply + 1] = g->pos[g->ply];
    if (!crm_shot(&g->pos[g->ply + 1], pos, ang, pow, ev)) return 0;
    g->ply++;
    return 1;
}

int crm_undo(crm_game *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}
