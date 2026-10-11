/*
 * Sea Battle: the rules (seabattle.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/seabattle.h"

static const uint8_t MODERN[] = {5, 4, 3, 3, 2}, PAPER[] = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};

static uint32_t next(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

int sb_cells(const sbship *s, int *cells)
{
    for (int i = 0; i < s->len; i++)
        cells[i] = (s->y + (s->vert ? i : 0)) * SB_N + s->x + (s->vert ? 0 : i);
    return s->len;
}

int sb_valid(const sbpos *p, const sbship *fleet)
{
    uint8_t seen[SB_CELLS] = {0};               /* the ship on each square, plus one */
    for (int i = 0; i < p->nships; i++) {
        const sbship *s = &fleet[i];
        int x1 = s->x + (s->vert ? 0 : s->len - 1), y1 = s->y + (s->vert ? s->len - 1 : 0);
        if (s->len != p->len[i] || s->x < 0 || s->y < 0 || x1 >= SB_N || y1 >= SB_N) return 0;
        int c[SB_N];
        sb_cells(s, c);
        for (int k = 0; k < s->len; k++) {
            if (seen[c[k]]) return 0;
            seen[c[k]] = (uint8_t)(i + 1);
        }
    }
    if (!p->apart) return 1;
    for (int c = 0; c < SB_CELLS; c++) {        /* no two ships touching, not even at a corner */
        if (!seen[c]) continue;
        int x = c % SB_N, y = c / SB_N;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= SB_N || ny >= SB_N) continue;
                int o = seen[ny * SB_N + nx];
                if (o && o != seen[c]) return 0;
            }
    }
    return 1;
}

int sb_ship_at(const sbpos *p, const sbship *fleet, int cell)
{
    for (int i = 0; i < p->nships; i++) {
        int c[SB_N], n = sb_cells(&fleet[i], c);
        for (int k = 0; k < n; k++)
            if (c[k] == cell) return i;
    }
    return -1;
}

void sb_random(const sbpos *p, uint32_t *rng, sbship *fleet)
{
    sbpos t = *p;
    for (;;) {                                  /* the biggest first; a fleet that jams starts again */
        int ok = 1;
        for (int i = 0; i < p->nships && ok; i++) {
            ok = 0;
            for (int tries = 0; tries < 300 && !ok; tries++) {
                sbship s;
                s.len = p->len[i];
                s.vert = (uint8_t)(next(rng) & 1);
                int span = SB_N - s.len + 1;
                s.x = (int8_t)(next(rng) % (uint32_t)(s.vert ? SB_N : span));
                s.y = (int8_t)(next(rng) % (uint32_t)(s.vert ? span : SB_N));
                fleet[i] = s;
                t.nships = (uint8_t)(i + 1);
                ok = sb_valid(&t, fleet);
            }
        }
        if (ok) return;
    }
}

int sb_sunk(const sbpos *p, int pl, int i)
{
    int c[SB_N], n = sb_cells(&p->ship[pl][i], c);
    for (int k = 0; k < n; k++)
        if (p->shot[pl][c[k]] != SHOT_HIT) return 0;
    return 1;
}

int sb_afloat(const sbpos *p, int pl)
{
    int n = 0;
    for (int i = 0; i < p->nships; i++) n += !sb_sunk(p, pl, i);
    return n;
}

int sb_shots(const sbpos *p)
{
    if (p->phase != SP_SHOOT) return 0;
    int open = 0;
    for (int c = 0; c < SB_CELLS; c++) open += p->shot[!p->turn][c] == SHOT_NONE;
    int n = p->salvo ? sb_afloat(p, p->turn) : 1;
    return n < open ? n : open;
}

void sb_new(sbgame *g, int variant, uint32_t seed)
{
    sbpos *p = &g->pos;
    memset(p, 0, sizeof *p);
    p->variant = (uint8_t)(variant >= 0 && variant < SB_VARIANTS ? variant : SB_CLASSIC);
    int paper = p->variant == SB_PAPER || p->variant == SB_PAPER_SALVO;
    p->salvo = p->variant == SB_SALVO || p->variant == SB_PAPER_SALVO;
    p->apart = (uint8_t)paper;
    p->nships = paper ? sizeof PAPER : sizeof MODERN;
    memcpy(p->len, paper ? PAPER : MODERN, p->nships);
    p->phase = SP_PLACE;
    p->rng = seed * 2654435761u + 0x7f4a7c15u;
    sb_random(p, &p->rng, p->ship[0]);          /* a fleet each to start from: move it, or roll another */
    sb_random(p, &p->rng, p->ship[1]);
    g->n = 0;
}

static void push(sbgame *g)
{
    if (g->n == SB_HIST) {
        memmove(g->hist, g->hist + 1, sizeof g->hist[0] * (SB_HIST - 1));
        g->n--;
    }
    g->hist[g->n++] = g->pos;
}

int sb_set(sbgame *g, const sbship *fleet)
{
    sbpos *p = &g->pos;
    if (p->phase != SP_PLACE || !sb_valid(p, fleet)) return 0;
    if (!memcmp(fleet, p->ship[p->turn], sizeof(sbship) * p->nships)) return 1;
    push(g);
    memcpy(p->ship[p->turn], fleet, sizeof(sbship) * p->nships);
    return 1;
}

int sb_reroll(sbgame *g)
{
    sbpos *p = &g->pos;
    if (p->phase != SP_PLACE) return 0;
    push(g);
    sb_random(p, &p->rng, p->ship[p->turn]);
    return 1;
}

int sb_ready(sbgame *g)
{
    sbpos *p = &g->pos;
    if (p->phase != SP_PLACE || !sb_valid(p, p->ship[p->turn])) return 0;
    if (p->turn == 1) p->phase = SP_SHOOT;      /* both fleets placed: player 0 fires first */
    p->turn = !p->turn;
    g->n = 0;
    return 1;
}

int sb_fire(sbgame *g, const int *cells, int n)
{
    sbpos *p = &g->pos;
    if (p->phase != SP_SHOOT || n != sb_shots(p) || n < 1) return 0;
    int me = p->turn, them = !me;
    for (int i = 0; i < n; i++) {
        if (cells[i] < 0 || cells[i] >= SB_CELLS || p->shot[them][cells[i]] != SHOT_NONE) return 0;
        for (int k = 0; k < i; k++)
            if (cells[k] == cells[i]) return 0;
    }
    for (int i = 0; i < n; i++) {
        int c = cells[i];
        p->shot[them][c] = sb_ship_at(p, p->ship[them], c) >= 0 ? SHOT_HIT : SHOT_MISS;
        p->last[them][i] = (uint8_t)c;
    }
    p->nlast[them] = (uint8_t)n;
    if (!sb_afloat(p, them)) {
        p->phase = SP_OVER;
        p->winner = (uint8_t)me;
    } else {
        p->turn = (uint8_t)them;
    }
    g->n = 0;                                   /* the device passes: no undo past it */
    return 1;
}

int sb_undo(sbgame *g)
{
    if (g->n == 0) return 0;
    g->pos = g->hist[--g->n];
    return 1;
}

/* a square: a..j and 1..10 (-1: bad); the end of it in *end */
static int square(const char *s, const char **end)
{
    if (*s < 'a' || *s > 'j') return -1;
    int x = *s++ - 'a';
    char *e;
    long y = strtol(s, &e, 10);
    if (e == s || y < 1 || y > SB_N) return -1;
    *end = e;
    return (int)(y - 1) * SB_N + x;
}

int sb_script(sbgame *g, const char *s)
{
    sbpos *p = &g->pos;
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == 'r' && (s[1] == ' ' || !s[1])) {
            if (!sb_reroll(g)) return 0;
            s++;
        } else if (!strncmp(s, "ok", 2)) {
            if (!sb_ready(g)) return 0;
            s += 2;
        } else if (*s == '#') {                 /* the rest of the game at random */
            s++;
            while (p->phase != SP_OVER)
                if (!sb_script(g, "*")) return 0;
        } else if (*s == '*') {                 /* a fleet ready, or a turn's shots at random */
            s++;
            if (p->phase == SP_PLACE) { if (!sb_ready(g)) return 0; continue; }
            int n = sb_shots(p), c[SB_MAXSHIPS], k = 0;
            while (k < n) {
                int cell = (int)(next(&p->rng) % SB_CELLS), dup = 0;
                for (int i = 0; i < k; i++) dup |= c[i] == cell;
                if (!dup && p->shot[!p->turn][cell] == SHOT_NONE) c[k++] = cell;
            }
            if (!sb_fire(g, c, n)) return 0;
        } else if (*s >= '0' && *s <= '9') {    /* ships placed: 0=a1h 1=a3h... (the words in a row: one move) */
            if (p->phase != SP_PLACE) return 0;
            sbship f[SB_MAXSHIPS];
            memcpy(f, p->ship[p->turn], sizeof f);
            while (*s >= '0' && *s <= '9') {
                char *e;
                long i = strtol(s, &e, 10);
                if (*e != '=' || i < 0 || i >= p->nships) return 0;
                const char *t;
                int c = square(e + 1, &t);
                if (c < 0 || (*t != 'h' && *t != 'v')) return 0;
                f[i].x = (int8_t)(c % SB_N);
                f[i].y = (int8_t)(c / SB_N);
                f[i].vert = *t == 'v';
                s = t + 1;
                while (*s == ' ') s++;
            }
            if (!sb_set(g, f)) return 0;
        } else {                                /* shots: e5, a1+b2+c3 */
            int c[SB_MAXSHIPS], n = 0;
            for (;;) {
                const char *t;
                int cell = square(s, &t);
                if (cell < 0 || n >= SB_MAXSHIPS) return 0;
                c[n++] = cell;
                s = t;
                if (*s != '+') break;
                s++;
            }
            if (!sb_fire(g, c, n)) return 0;
        }
    }
    return 1;
}
