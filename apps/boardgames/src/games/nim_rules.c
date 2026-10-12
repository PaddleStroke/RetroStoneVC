/*
 * The Marienbad game (Nim): the rules (nim.h). No SDL: tested by tests/nim_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/nim.h"

static uint32_t next_rand(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

void nim_new(nim_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->variant = variant >= 0 && variant < NIM_VARIANTS ? variant : NIM_MARIENBAD;
    g->misere = g->variant == NIM_MARIENBAD || g->variant == NIM_RANDOM;
    if (g->variant == NIM_MARIENBAD || g->variant == NIM_MARIENBAD_NORMAL) {
        g->rows = 4;
        for (int r = 0; r < 4; r++) g->start[r] = (uint8_t)(2 * r + 1);
    } else {
        uint32_t s = seed * 2654435761u + 0x6d2b79f5u;
        next_rand(&s);
        g->rows = 3 + (int)(next_rand(&s) % 3);
        for (int r = 0; r < g->rows; r++) g->start[r] = (uint8_t)(1 + next_rand(&s) % NIM_LONG);
        for (int i = 1; i < g->rows; i++)       /* the shortest row at the top */
            for (int j = i; j > 0 && g->start[j - 1] > g->start[j]; j--) {
                uint8_t t = g->start[j];
                g->start[j] = g->start[j - 1];
                g->start[j - 1] = t;
            }
    }
    for (int r = 0; r < g->rows; r++) g->mask[r] = (uint8_t)((1u << g->start[r]) - 1);
}

int nim_count(uint8_t mask)
{
    int n = 0;
    for (; mask; mask &= (uint8_t)(mask - 1)) n++;
    return n;
}

int nim_left(const nim_game *g)
{
    int n = 0;
    for (int r = 0; r < g->rows; r++) n += nim_count(g->mask[r]);
    return n;
}

int nim_status(const nim_game *g)
{
    if (nim_left(g) || !g->n) return NIM_PLAY;
    int last = (g->n - 1) & 1;                  /* who took the last match: misere, they lose */
    return NIM_WIN0 + (g->misere ? !last : last);
}

int nim_legal(const nim_game *g, int row, int mask)
{
    return nim_status(g) == NIM_PLAY && row >= 0 && row < g->rows && mask > 0 && mask < 256 &&
           (mask & ~g->mask[row]) == 0;
}

int nim_play(nim_game *g, int row, int mask)
{
    if (!nim_legal(g, row, mask) || g->n >= NIM_MAX_MOVES) return 0;
    g->mask[row] &= (uint8_t)~mask;
    g->taken[g->turn] += nim_count((uint8_t)mask);
    g->move[g->n++] = (nim_move){(uint8_t)row, (uint8_t)mask};
    g->turn = !g->turn;
    return 1;
}

int nim_undo(nim_game *g)
{
    if (!g->n) return 0;
    nim_move m = g->move[--g->n];
    g->turn = !g->turn;
    g->mask[m.row] |= m.mask;
    g->taken[g->turn] -= nim_count(m.mask);
    return 1;
}

/* the matches to take from a row to leave `keep` of them: the rightmost ones */
static int rightmost(uint8_t mask, int take)
{
    int m = 0;
    for (int i = 7; i >= 0 && take > 0; i--)
        if (mask >> i & 1) m |= 1 << i, take--;
    return m;
}

int nim_best(const nim_game *g, int *row, int *mask)
{
    int x = 0, big = 0, ones = 0;
    for (int r = 0; r < g->rows; r++) {
        int c = nim_count(g->mask[r]);
        x ^= c;
        big += c > 1;
        ones += c == 1;
    }
    if (!nim_left(g)) return 0;
    if (g->misere && big <= 1) {                /* the end of misere: leave an odd number of single matches */
        for (int r = 0; r < g->rows; r++) {
            int c = nim_count(g->mask[r]);
            if (big == 1 && c > 1) {            /* the one big row: down to 0 or 1, whichever leaves odd ones */
                int keep = ones & 1 ? 0 : 1;
                *row = r;
                *mask = rightmost(g->mask[r], c - keep);
                return 1;
            }
        }
        if (ones & 1) return 0;                 /* only single matches, an odd number: every move loses */
        for (int r = 0; r < g->rows; r++)
            if (g->mask[r]) { *row = r; *mask = g->mask[r]; return 1; }
    }
    if (!x) return 0;                           /* the nim-sum is zero: every move loses */
    for (int r = 0; r < g->rows; r++) {
        int c = nim_count(g->mask[r]), want = c ^ x;
        if (want < c) {
            *row = r;
            *mask = rightmost(g->mask[r], c - want);
            return 1;
        }
    }
    return 0;
}

int nim_script(nim_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s < 'a' || *s > 'a' + NIM_ROWS - 1) return 0;
        int row = *s++ - 'a', mask = 0;
        if (row >= g->rows) return 0;
        if (*s == ':') {                        /* the places, from the left */
            for (s++; *s >= '0' && *s <= '7'; s++) mask |= 1 << (*s - '0');
        } else {
            int k = 0;
            for (; *s >= '0' && *s <= '9'; s++) k = 10 * k + (*s - '0');
            if (k < 1 || k > nim_count(g->mask[row])) return 0;
            mask = rightmost(g->mask[row], k);
        }
        if (!nim_play(g, row, mask)) return 0;
    }
    return 1;
}
