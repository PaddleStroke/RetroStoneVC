/*
 * Chess Puzzles' rules (chesspuzzles.h): the collection unpacked, a puzzle's line followed with chess.c's rules.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/chesspuzzles.h"

#include <string.h>

#include "games/chesspuzzles_data.h"

static const char B62[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static uint32_t offset[CPZ_TOTAL];              /* each record's start in cpz_data, found once */
static int indexed;

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static int popcount64(uint64_t v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

/* a record's length: 19 bytes, a nibble a piece, two bytes a move */
static uint32_t length(const uint8_t *r) { return 19 + (popcount64(le64(r + 11)) + 1) / 2 + 2 * (r[0] & 15); }

static void index_all(void)
{
    if (indexed) return;
    uint32_t at = 0;
    for (int i = 0; i < CPZ_TOTAL; i++) {
        offset[i] = at;
        at += at + 19 <= CPZ_DATA_BYTES ? length(cpz_data + at) : 0;
        if (at > CPZ_DATA_BYTES) at = CPZ_DATA_BYTES;
    }
    indexed = 1;
}

int cpz_count(int pack) { return pack >= 0 && pack < CPZ_PACKS ? cpz_pack_size[pack] : 0; }
int cpz_total(void) { return CPZ_TOTAL; }

int cpz_find(const cpos *p, int from, int to, int promo, cmove *m)
{
    cmove all[MAX_MOVES];
    int n = chess_legal(p, all);
    if (!promo) promo = QUEEN;
    for (int i = 0; i < n; i++)
        if (all[i].from == from && all[i].to == to && !(all[i].flags & MV_CASTLE) && (!all[i].promo ||
                                                                                     all[i].promo == promo)) {
            *m = all[i];
            return 1;
        }
    for (int i = 0; i < n; i++)                 /* castling: the king onto its destination (e1g1) */
        if (all[i].from == from && (all[i].flags & MV_CASTLE) && chess_castle_king(all[i]) == to) {
            *m = all[i];
            return 1;
        }
    return 0;
}

int cpz_lands(cmove m) { return m.flags & MV_CASTLE ? chess_castle_king(m) : m.to; }

int cpz_load(cpz_puzzle *pz, int pack, int index)
{
    memset(pz, 0, sizeof *pz);
    if (pack < 0 || pack >= CPZ_PACKS || index < 0 || index >= cpz_pack_size[pack]) return 0;
    index_all();
    int k = index;
    for (int p = 0; p < pack; p++) k += cpz_pack_size[p];
    uint32_t at = offset[k];
    if (at + 19 > CPZ_DATA_BYTES || at + length(cpz_data + at) > CPZ_DATA_BYTES) return 0;
    const uint8_t *r = cpz_data + at;
    pz->n = r[0] & 15;
    if (pz->n < 2 || pz->n > CPZ_MAX_MOVES) return 0;
    pz->rating = r[2] * 16;
    pz->themes = le32(r + 3);
    uint32_t id = le32(r + 7);
    for (int i = 0; i < 5; i++, id /= 62) pz->id[i] = B62[id % 62];
    pz->id[5] = 0;
    cpos *s = &pz->start;
    memset(s, 0, sizeof *s);
    static const uint8_t usual[4] = {7, 0, 7, 0};
    memcpy(s->rook, usual, 4);
    s->side = r[1] >> 7;
    s->castle = r[0] >> 4;
    s->ep = -1;
    if (r[1] & 15) s->ep = (int8_t)SQ((r[1] & 15) - 1, s->side == WHITE ? 5 : 2);
    s->fullmove = 1;
    uint64_t occ = le64(r + 11);
    const uint8_t *nib = r + 19;
    int i = 0;
    for (int q = 0; q < 64; q++) {
        if (!(occ >> q & 1)) continue;
        int v = nib[i / 2] >> (i % 2 * 4) & 15, t = v & 7;
        if (t < PAWN || t > KING) return 0;
        s->sq[q] = PC(v >> 3, t);
        i++;
    }
    if (chess_king(s, WHITE) < 0 || chess_king(s, BLACK) < 0) return 0;
    const uint8_t *mv = nib + (i + 1) / 2;
    cpos p = *s;
    for (int j = 0; j < pz->n; j++) {           /* each move must be legal where it is played */
        int from = mv[2 * j] & 63, to = mv[2 * j + 1] & 63, pr = mv[2 * j] >> 6 | (mv[2 * j + 1] >> 6) << 2;
        static const int promo[5] = {0, KNIGHT, BISHOP, ROOK, QUEEN};
        if (pr > 4 || !cpz_find(&p, from, to, promo[pr], &pz->move[j])) return 0;
        if (pz->move[j].promo != promo[pr]) return 0;
        chess_make(&p, pz->move[j]);
    }
    return 1;
}

int cpz_start(cpz_game *g, int pack, int index)
{
    memset(g, 0, sizeof *g);
    g->pack = pack;
    g->index = index;
    if (!cpz_load(&g->pz, pack, index)) return 0;
    g->pos[0] = g->pz.start;
    g->solver = !g->pz.start.side;
    g->played[0] = g->pz.move[0];
    g->pos[1] = g->pos[0];
    chess_make(&g->pos[1], g->pz.move[0]);
    g->ply = 1;
    return 1;
}

static int mated(const cpos *p)
{
    cmove m[MAX_MOVES];
    return chess_in_check(p, p->side) && !chess_legal(p, m);
}

int cpz_solved(const cpz_game *g)
{
    if (g->ply < 2) return 0;
    return g->ply >= g->pz.n || (g->ply % 2 == 0 && mated(cpz_pos(g)));
}

int cpz_to_find(const cpz_game *g) { return g->ply >= 1 && g->ply % 2 == 1 && !cpz_solved(g) && g->ply < g->pz.n; }
int cpz_reply_due(const cpz_game *g) { return g->ply % 2 == 0 && !cpz_solved(g) && g->ply < g->pz.n; }
cmove cpz_expected(const cpz_game *g) { return g->pz.move[g->ply < g->pz.n ? g->ply : g->pz.n - 1]; }

static void play(cpz_game *g, cmove m)
{
    g->played[g->ply] = m;
    g->pos[g->ply + 1] = g->pos[g->ply];
    chess_make(&g->pos[g->ply + 1], m);
    g->ply++;
    g->hint_level = 0;
}

int cpz_try(cpz_game *g, int from, int to)
{
    cmove m;
    if (!cpz_to_find(g) || !cpz_find(cpz_pos(g), from, to, 0, &m)) return CPZ_ILLEGAL;
    cmove want = cpz_expected(g);
    int right = m.from == want.from && m.to == want.to && (m.flags & MV_CASTLE) == (want.flags & MV_CASTLE);
    if (right) m = want;                        /* the line's own promotion */
    else {
        cpos q = *cpz_pos(g);
        chess_make(&q, m);
        if (!mated(&q)) {                       /* any mate is as good as the line's */
            g->mistakes++;
            return CPZ_WRONG;
        }
    }
    play(g, m);
    return cpz_solved(g) ? CPZ_SOLVED : CPZ_RIGHT;
}

cmove cpz_reply(cpz_game *g)
{
    cmove m = g->pz.move[g->ply];
    if (cpz_reply_due(g)) play(g, m);
    return m;
}

int cpz_hint(cpz_game *g)
{
    if (!cpz_to_find(g) || g->hint_level >= 2) return 0;
    if (!g->hint_level) g->hints++;
    return ++g->hint_level;
}

int cpz_undo(cpz_game *g)
{
    if (g->ply <= 1) return 0;
    g->ply--;
    while (g->ply > 1 && g->ply % 2 == 0) g->ply--;   /* back to the player's turn */
    g->hint_level = 0;
    return 1;
}

int cpz_script(cpz_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8' || s[2] < 'a' || s[2] > 'h' || s[3] < '1' ||
            s[3] > '8')
            return 0;
        int r = cpz_try(g, SQ(s[0] - 'a', s[1] - '1'), SQ(s[2] - 'a', s[3] - '1'));
        if (r == CPZ_ILLEGAL) return 0;
        while (cpz_reply_due(g)) cpz_reply(g);
        s += 4;
        while (*s && *s != ' ') s++;
    }
    return 1;
}
