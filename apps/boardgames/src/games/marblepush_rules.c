/*
 * Marble Push's rules for RetroStone Board (marblepush.h): the hexagon of holes, the moves in line and sideways, the
 * pushes, the ends of the game, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/marblepush.h"

static const int8_t DR[6] = {0, 1, 1, 0, -1, -1}, DD[6] = {1, 1, 0, -1, -1, 0};   /* e ne nw w sw se */

/* ---- the board --------------------------------------------------------------------------------------------------- */
static int row_len(int r) { return 9 - (r > 4 ? r - 4 : 4 - r); }
static int row_first(int r) { return r > 4 ? r - 4 : 0; }    /* the row's first diagonal */

static int row_start(int r)                     /* the row's first hole */
{
    int h = 0;
    for (int i = 0; i < r; i++) h += row_len(i);
    return h;
}

int mp_row(int h)
{
    int r = 0;
    while (r < MP_ROWS - 1 && h >= row_start(r + 1)) r++;
    return r;
}

int mp_diag(int h)
{
    int r = mp_row(h);
    return row_first(r) + h - row_start(r);
}

int mp_at(int row, int diag)
{
    if (row < 0 || row >= MP_ROWS) return -1;
    int d = diag - row_first(row);
    return d < 0 || d >= row_len(row) ? -1 : row_start(row) + d;
}

int mp_next(int h, int dir)
{
    if (h < 0 || h >= MP_HOLES) return -1;
    return mp_at(mp_row(h) + DR[dir], mp_diag(h) + DD[dir]);
}

int mp_line(int a, int b, int8_t cells[3])
{
    if (a < 0 || b < 0 || a >= MP_HOLES || b >= MP_HOLES) return 0;
    cells[0] = (int8_t)a;
    if (a == b) return 1;
    for (int d = 0; d < 6; d++) {
        int x = mp_next(a, d);
        if (x < 0) continue;
        if (x == b) { cells[1] = (int8_t)b; return 2; }
        if (mp_next(x, d) == b) { cells[1] = (int8_t)x; cells[2] = (int8_t)b; return 3; }
    }
    return 0;
}

/* ---- the starting layouts ---------------------------------------------------------------------------------------- */
/* player 0's marbles. Player 1's: the standard layout's turned half round (row r, diagonal d -> 8 - r, 8 - d); the
 * daisies' mirrored left to right (each row kept: d -> r + 4 - d), as each player's two daisies already turn half
 * round onto each other */
static const char *const LAYOUT[MP_VARIANTS] = {
    "a1 a2 a3 a4 a5 b1 b2 b3 b4 b5 b6 c3 c4 c5",
    "a1 a2 b1 b2 b3 c2 c3 g7 g8 h7 h8 h9 i8 i9",   /* round b2 and h8 */
    "b1 b2 c1 c2 c3 d2 d3 f7 f8 g7 g8 g9 h8 h9",   /* round c2 and g8 */
};

void mp_start(mppos *p, int variant)
{
    memset(p, 0, sizeof *p);
    if (variant < 0 || variant >= MP_VARIANTS) variant = MP_STANDARD;
    for (const char *s = LAYOUT[variant]; *s;) {
        int len, h = mp_hole(s, &len);
        if (h < 0) break;
        int r = mp_row(h), d = mp_diag(h);
        p->cell[h] = 1;
        p->cell[variant == MP_STANDARD ? mp_at(8 - r, 8 - d) : mp_at(r, r + 4 - d)] = 2;
        s += len;
        while (*s == ' ') s++;
    }
}

/* ---- the moves --------------------------------------------------------------------------------------------------- */
/* the move of the marbles c[0..n-1] (in a row along axis ax, from c[0]) in direction dir, if legal */
static int try_move(const mppos *p, const int8_t *c, int n, int ax, int dir, mpmove *m)
{
    int them = 2 - p->side;
    m->n = (int8_t)n;
    m->dir = (int8_t)dir;
    m->push = m->off = 0;
    if (n > 1 && (dir == ax || dir == (ax + 3) % 6)) {   /* in line: the front marble first in the direction */
        for (int i = 0; i < n; i++) m->cell[i] = dir == ax ? c[i] : c[n - 1 - i];
        int x = mp_next(m->cell[n - 1], dir), k = 0;
        while (x >= 0 && p->cell[x] == them && k < n) { k++; x = mp_next(x, dir); }
        if (k >= n) return 0;                   /* not outnumbered */
        if (x >= 0 && p->cell[x]) return 0;     /* blocked: a marble right after (one's own, or a gap-less wall) */
        if (x < 0 && !k) return 0;              /* one's own marble off the board */
        m->push = (int8_t)k;
        m->off = (int8_t)(k && x < 0);
        return 1;
    }
    for (int i = 0; i < n; i++) {               /* sideways (one marble: any way): every hole ahead empty */
        int x = mp_next(c[i], dir);
        if (x < 0 || p->cell[x]) return 0;
        m->cell[i] = c[i];
    }
    return 1;
}

int mp_legal(const mppos *p, mpmove *out)
{
    int k = 0, me = p->side + 1;
    for (int h = 0; h < MP_HOLES; h++) {
        if (p->cell[h] != me) continue;
        int8_t c[3] = {(int8_t)h, 0, 0};
        for (int d = 0; d < 6; d++)
            if (try_move(p, c, 1, MP_E, d, &out[k])) k++;
        for (int ax = MP_E; ax <= MP_NW; ax++)  /* the rows of 2 and 3 starting here, each axis one way */
            for (int n = 2; n <= 3; n++) {
                int x = mp_next(c[n - 2], ax);
                if (x < 0 || p->cell[x] != me) break;
                c[n - 1] = (int8_t)x;
                for (int d = 0; d < 6; d++)
                    if (try_move(p, c, n, ax, d, &out[k])) k++;
            }
    }
    return k;
}

static int holds(const int8_t *cells, int n, int h)
{
    for (int i = 0; i < n; i++)
        if (cells[i] == h) return 1;
    return 0;
}

int mp_moves_of(const mppos *p, const int8_t *cells, int n, mpmove *out)
{
    mpmove all[MP_MAX_MOVES];
    int k = 0;
    for (int i = 0, m = mp_legal(p, all); i < m; i++) {
        if (all[i].n != n) continue;
        int ok = 1;
        for (int j = 0; j < n && ok; j++) ok = holds(cells, n, all[i].cell[j]);
        if (ok) out[k++] = all[i];
    }
    return k;
}

int mp_same(const mpmove *a, const mpmove *b)
{
    if (a->n != b->n || a->dir != b->dir) return 0;
    for (int i = 0; i < a->n; i++)
        if (!holds(b->cell, b->n, a->cell[i])) return 0;
    return 1;
}

int mp_moving(const mpmove *m, int8_t from[5], int8_t to[5])
{
    int k = 0, x = m->cell[m->n - 1];
    for (int i = 0; i < m->n; i++) {
        from[k] = m->cell[i];
        to[k++] = (int8_t)mp_next(m->cell[i], m->dir);
    }
    for (int i = 0; i < m->push; i++) {         /* the pushed marbles, ahead of the front one */
        x = mp_next(x, m->dir);
        from[k] = (int8_t)x;
        to[k++] = (int8_t)mp_next(x, m->dir);
    }
    return k;
}

void mp_make(mppos *p, const mpmove *m)
{
    int8_t from[5], to[5];
    uint8_t who[5];
    int k = mp_moving(m, from, to);
    for (int i = 0; i < k; i++) {               /* lifted all at once, then set down */
        who[i] = p->cell[from[i]];
        p->cell[from[i]] = 0;
    }
    for (int i = 0; i < k; i++) {
        if (to[i] >= 0) p->cell[to[i]] = who[i];
        else p->lost[who[i] - 1]++;
    }
    p->quiet = m->off ? 0 : (uint16_t)(p->quiet + 1);
    p->side ^= 1;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void mp_new(mpgame *g, int variant)
{
    g->ply = 0;
    g->variant = variant >= 0 && variant < MP_VARIANTS ? variant : MP_STANDARD;
    mp_start(&g->pos[0], g->variant);
}

/* how many times the position now has been seen (only since the last marble pushed off: none can come back) */
static int seen(const mpgame *g)
{
    const mppos *now = mp_pos(g);
    int n = 1;
    for (int k = g->ply - 2; k >= 0 && k >= g->ply - now->quiet; k -= 2)
        if (!memcmp(g->pos[k].cell, now->cell, MP_HOLES)) n++;
    return n;
}

int mp_status(const mpgame *g)
{
    const mppos *p = mp_pos(g);
    mpmove m[MP_MAX_MOVES];
    if (p->lost[1] >= MP_WIN) return MP_WIN0;
    if (p->lost[0] >= MP_WIN) return MP_WIN1;
    if (seen(g) >= 3) return MP_DRAW_REPEAT;
    if (p->quiet >= MP_QUIET) return MP_DRAW_QUIET;
    if (!mp_legal(p, m)) return MP_DRAW_STUCK;
    return MP_PLAY;
}

int mp_play(mpgame *g, const mpmove *m)
{
    if (g->ply >= MP_MAX_PLY || mp_status(g) != MP_PLAY) return 0;
    mpmove all[MP_MAX_MOVES];
    for (int i = 0, n = mp_legal(mp_pos(g), all); i < n; i++) {
        if (!mp_same(&all[i], m)) continue;
        g->pos[g->ply + 1] = g->pos[g->ply];
        mp_make(&g->pos[g->ply + 1], &all[i]);
        g->move[g->ply++] = all[i];
        return 1;
    }
    return 0;
}

int mp_undo(mpgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int mp_hole(const char *s, int *len)
{
    *len = 2;
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '1' || s[1] > '9') return -1;
    return mp_at(s[0] - 'a', s[1] - '1');
}

void mp_name(int h, char out[3])
{
    out[0] = (char)('a' + mp_row(h));
    out[1] = (char)('1' + mp_diag(h));
    out[2] = 0;
}

static const char *const DIR_NAME[6] = {"e", "ne", "nw", "w", "sw", "se"};

int mp_script(mpgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int h[3], k = 0, len;
        while (k < 3 && (h[k] = mp_hole(s, &len)) >= 0) { k++; s += len; }
        if (!k || *s++ != ':') return 0;
        int dir = -1;
        for (int pass = 2; pass >= 1 && dir < 0; pass--)   /* the two-letter names first: "ne" is not "e" */
            for (int d = 0; d < 6; d++)
                if ((int)strlen(DIR_NAME[d]) == pass && !strncmp(s, DIR_NAME[d], (size_t)pass)) { dir = d; break; }
        if (dir < 0) return 0;
        s += strlen(DIR_NAME[dir]);
        if (*s && *s != ' ') return 0;
        mpmove m = {.dir = (int8_t)dir};
        m.n = (int8_t)mp_line(h[0], h[k - 1], m.cell);
        if (!m.n || (k == 3 && (m.n != 3 || m.cell[1] != h[1])) || (k == 2 && m.n == 1)) return 0;
        if (!mp_play(g, &m)) return 0;
    }
    return 1;
}
