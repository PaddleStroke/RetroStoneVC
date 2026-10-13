/*
 * Banqi's rules (banqi.h): the deal, the moves, the end. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/banqi.h"

static const int8_t DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

int bq_beats(int a, int t)
{
    if (a == BQ_CANNON) return 0;               /* only by its jump */
    if (a == BQ_SOLDIER) return t == BQ_SOLDIER || t == BQ_GENERAL;
    if (a == BQ_GENERAL) return t != BQ_SOLDIER;
    return a <= t;
}

static int inside(int x, int y) { return x >= 0 && y >= 0 && x < BQ_W && y < BQ_H; }

int bq_legal(const bqpos *p, bqmove *out)
{
    int n = 0;
    for (int s = 0; s < BQ_N; s++) {
        uint8_t pc = p->sq[s];
        if (!pc) continue;
        if (pc & BQ_HIDDEN) {
            out[n++] = (bqmove){BQ_FLIP, (uint8_t)s, (uint8_t)s, (uint8_t)(pc & ~BQ_HIDDEN)};
            continue;
        }
        if (!p->decided || BQ_COLOUR(pc) != p->side) continue;
        int type = BQ_TYPE(pc);
        for (int d = 0; d < 4; d++) {
            int x = BQ_X(s) + DX[d], y = BQ_Y(s) + DY[d];
            if (!inside(x, y)) continue;
            uint8_t t = p->sq[BQ_SQ(x, y)];
            if (!t) out[n++] = (bqmove){BQ_STEP, (uint8_t)s, (uint8_t)BQ_SQ(x, y), 0};
            else if (!(t & BQ_HIDDEN) && BQ_COLOUR(t) != p->side && bq_beats(type, BQ_TYPE(t)))
                out[n++] = (bqmove){BQ_TAKE, (uint8_t)s, (uint8_t)BQ_SQ(x, y), t};
            if (type != BQ_CANNON) continue;
            /* the cannon's jump: past the first piece, onto the next one */
            int screens = 0;
            for (; inside(x, y); x += DX[d], y += DY[d]) {
                uint8_t q = p->sq[BQ_SQ(x, y)];
                if (!q) continue;
                if (screens++ == 0) continue;
                if (!(q & BQ_HIDDEN) && BQ_COLOUR(q) != p->side)
                    out[n++] = (bqmove){BQ_TAKE, (uint8_t)s, (uint8_t)BQ_SQ(x, y), q};
                break;
            }
        }
    }
    return n;
}

void bq_make(bqpos *p, bqmove m)
{
    if (m.kind == BQ_FLIP) {
        p->sq[m.from] &= (uint8_t)~BQ_HIDDEN;
        if (!p->decided) {                      /* the flipper (seat 0) takes this colour: the other moves next */
            p->decided = 1;
            p->side = p->first = (uint8_t)BQ_COLOUR(p->sq[m.from]);
        }
        p->quiet = 0;
    } else {
        if (m.kind == BQ_TAKE) {
            p->taken[BQ_COLOUR(p->sq[m.to])]++;
            p->quiet = 0;
        } else p->quiet++;
        p->sq[m.to] = p->sq[m.from];
        p->sq[m.from] = 0;
    }
    p->side ^= 1;
}

void bq_deal(bqpos *p, uint32_t seed)
{
    static const uint8_t SET[16] = {BQ_GENERAL, BQ_ADVISOR, BQ_ADVISOR, BQ_ELEPHANT, BQ_ELEPHANT, BQ_CHARIOT,
                                    BQ_CHARIOT, BQ_HORSE, BQ_HORSE, BQ_CANNON, BQ_CANNON, BQ_SOLDIER, BQ_SOLDIER,
                                    BQ_SOLDIER, BQ_SOLDIER, BQ_SOLDIER};
    memset(p, 0, sizeof *p);
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 16; i++) p->sq[c * 16 + i] = (uint8_t)(BQ_PC(c, SET[i]) | BQ_HIDDEN);
    uint32_t r = seed * 2654435761u + 0x9E3779B9u;
    for (int i = BQ_N - 1; i > 0; i--) {        /* Fisher-Yates on a xorshift */
        r ^= r << 13;
        r ^= r >> 17;
        r ^= r << 5;
        int j = (int)(r % (uint32_t)(i + 1));
        uint8_t t = p->sq[i];
        p->sq[i] = p->sq[j];
        p->sq[j] = t;
    }
}

int bq_square(const char *s)
{
    int x = s[0] - 'a', y = s[1] - '1';
    return inside(x, y) ? BQ_SQ(x, y) : -1;
}

void bq_new(bqgame *g, uint32_t seed)
{
    g->ply = 0;
    bq_deal(&g->pos[0], seed);
}

int bq_status(const bqgame *g)
{
    const bqpos *p = bq_pos(g);
    bqmove m[BQ_MAX_MOVES];
    if (p->taken[p->side] >= 16 || !bq_legal(p, m)) return p->side ? BQ_WIN0 : BQ_WIN1;
    if (p->quiet >= BQ_QUIET) return BQ_DRAW;
    return BQ_PLAY;
}

int bq_play(bqgame *g, bqmove m)
{
    if (g->ply >= BQ_MAX_PLY || bq_status(g) != BQ_PLAY) return 0;
    bqmove l[BQ_MAX_MOVES];
    int n = bq_legal(bq_pos(g), l), ok = 0;
    for (int i = 0; i < n && !ok; i++) ok = l[i].kind == m.kind && l[i].from == m.from && l[i].to == m.to;
    if (!ok) return 0;
    for (int i = 0; i < n; i++)
        if (l[i].from == m.from && l[i].to == m.to) m = l[i];
    g->pos[g->ply + 1] = g->pos[g->ply];
    bq_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int bq_undo(bqgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int bq_script(bqgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s == '@') {                       /* "@123": a new deal from that seed (before any move) */
            uint32_t seed = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) seed = seed * 10 + (uint32_t)(*s - '0');
            if (g->ply) return 0;
            bq_new(g, seed);
            continue;
        }
        int a = bq_square(s), b = s[2] && s[2] != ' ' ? bq_square(s + 2) : -1;
        if (a < 0) return 0;
        bqmove m = b < 0 ? (bqmove){BQ_FLIP, (uint8_t)a, (uint8_t)a, 0}
                         : (bqmove){bq_pos(g)->sq[b] ? BQ_TAKE : BQ_STEP, (uint8_t)a, (uint8_t)b, 0};
        if (!bq_play(g, m)) return 0;
        s += b < 0 ? 2 : 4;
    }
    return 1;
}
