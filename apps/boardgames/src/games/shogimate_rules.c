/*
 * Shogi Mate's rules (shogimate.h): the checks, the defender's replies, the mate solver, the problems read back and
 * played, all with shogi_rules.c's moves.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/shogimate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SMT_NO_LEVELS
#include "games/shogimate_levels.h"
#endif

/* ---- the moves --------------------------------------------------------------------------------------------------- */
int smt_checks(const spos *p, smove *out)
{
    smove all[SG_MAX_MOVES];
    int n = sg_legal(p, all), k = 0;
    for (int i = 0; i < n; i++) {
        spos q = *p;
        sg_make(&q, all[i]);
        if (sg_in_check(&q, q.side)) out[k++] = all[i];
    }
    return k;
}

/* 1: a piece of this kind could never move again from the row (shogi's rule for drops) */
static int dead_row(const spos *p, int kind, int player, int row)
{
    int a = player ? p->n - 1 - row : row;
    return ((kind == SG_PAWN || kind == SG_LANCE) && a == 0) || (kind == SG_KNIGHT && a <= 1);
}

/* the defender's moves in check: the board's from shogi.h (its hand left out), the drops only where a piece blocks
 * the check (a square that a stone there would shield, found by trying one) */
int smt_replies(const spos *p, smove *out)
{
    int me = p->side, king = sg_king(p, me);
    if (king < 0 || !sg_in_check(p, me) || sg_king(p, !me) >= 0) return sg_legal(p, out);   /* not tsume's case */
    spos q = *p;
    memset(q.hand[me], 0, sizeof q.hand[me]);
    int n = sg_legal(&q, out), any = 0;
    for (int k = SG_PAWN; k <= SG_ROOK; k++) any |= p->hand[me][k];
    if (!any) return n;
    int pawn_on[9] = {0};
    for (int s = 0; s < 81; s++)
        if (p->sq[s] == SG_PC(me, SG_PAWN)) pawn_on[SG_COL(s)] = 1;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            for (int x = SG_COL(king) + dx, y = SG_ROW(king) + dy; x >= 0 && x < p->n && y >= 0 && y < p->n;
                 x += dx, y += dy) {
                int s = SG_SQ(x, y);
                if (p->sq[s]) break;
                spos t = *p;
                t.sq[s] = SG_PC(me, SG_GOLD);
                if (sg_in_check(&t, me)) continue;   /* no shield here */
                for (int k = SG_PAWN; k <= SG_ROOK; k++)
                    if (p->hand[me][k] && !dead_row(p, k, me, y) && !(k == SG_PAWN && pawn_on[x]))
                        out[n++] = (smove){0, (uint8_t)s, (uint8_t)k, 0};
            }
        }
    return n;
}

int smt_mated(const spos *p)
{
    smove m[SG_MAX_MOVES];
    return sg_in_check(p, p->side) && !smt_replies(p, m);
}

void smt_fill_hand(spos *p)
{
    static const int total[8] = {0, 18, 4, 4, 4, 4, 2, 2};
    int used[8] = {0};
    for (int s = 0; s < 81; s++)
        if (p->sq[s] && SG_KIND(p->sq[s]) != SG_KING) used[SG_KIND(p->sq[s])]++;
    for (int k = SG_PAWN; k <= SG_ROOK; k++) {
        int left = total[k] - used[k] - p->hand[0][k];
        p->hand[1][k] = (uint8_t)(left > 0 ? left : 0);
    }
}

/* ---- the solver -------------------------------------------------------------------------------------------------- */
#define TT_BITS 16
typedef struct { uint64_t key; int8_t mate, none; } tt_entry;   /* mate within `mate` (0: not known); none within */
static tt_entry tt[1u << TT_BITS];

void smt_forget(void) { memset(tt, 0, sizeof tt); }

static uint64_t key_of(const spos *p)
{
    uint64_t h = 14695981039346656037ull;
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0; i < sizeof *p; i++) h = (h ^ b[i]) * 1099511628211ull;
    return h | 1;
}

static long searched;

long smt_searched(void) { return searched; }

static int mate_in(const spos *p, int d)
{
    if (d < 1) return 0;
    searched++;
    uint64_t key = key_of(p);
    tt_entry *e = &tt[key & ((1u << TT_BITS) - 1)];
    if (e->key == key) {
        if (e->mate && e->mate <= d) return 1;
        if (e->none >= d) return 0;
    } else {
        e->key = key;
        e->mate = 0;
        e->none = 0;
    }
    smove ch[SG_MAX_MOVES], rep[SG_MAX_MOVES];
    int n = smt_checks(p, ch), found = 0;
    for (int i = 0; i < n && !found; i++) {     /* a mate at once first */
        spos q = *p;
        sg_make(&q, ch[i]);
        if (!smt_replies(&q, rep)) found = 1;
    }
    if (!found && d >= 3)
        for (int i = 0; i < n && !found; i++) {
            spos q = *p;
            sg_make(&q, ch[i]);
            int nr = smt_replies(&q, rep), all = 1;
            for (int j = 0; j < nr && all; j++) {
                spos r = q;
                sg_make(&r, rep[j]);
                all = mate_in(&r, d - 2);
            }
            found = all;
        }
    e = &tt[key & ((1u << TT_BITS) - 1)];       /* the search may have used the slot meanwhile */
    if (e->key != key) { e->key = key; e->mate = 0; e->none = 0; }
    if (found) { if (!e->mate || e->mate > d) e->mate = (int8_t)(found && d >= 3 ? d : 1); }
    else if (e->none < d) e->none = (int8_t)d;
    return found;
}

int smt_mate_len(const spos *p, int most)
{
    for (int d = 1; d <= most; d += 2)
        if (mate_in(p, d)) return d;
    return 0;
}

/* ---- the problems ------------------------------------------------------------------------------------------------ */
static int same(smove a, smove b) { return a.from == b.from && a.to == b.to && a.drop == b.drop && a.promo == b.promo; }

int smt_parse(smt_problem *pz, const char *s)
{
    memset(pz, 0, sizeof *pz);
    char sfen[160];
    const char *sp = strchr(s, ' ');
    if (!sp || sp - s > 100) return 0;
    const char *hand = sp + 1, *end = strchr(hand, ' ');
    int hl = end ? (int)(end - hand) : (int)strlen(hand);
    snprintf(sfen, sizeof sfen, "%.*s b %.*s 1", (int)(sp - s), s, hl, hand);
    if (!sg_from_sfen(&pz->start, sfen) || pz->start.n != 9 || sg_king(&pz->start, 0) >= 0 ||
        sg_king(&pz->start, 1) < 0 || sg_in_check(&pz->start, 1))
        return 0;
    smt_fill_hand(&pz->start);
    spos p = pz->start;
    s = end;
    while (s && *s == ' ' && pz->n < SMT_MAX) {
        s++;
        smove all[SG_MAX_MOVES];
        int n = pz->n % 2 ? smt_replies(&p, all) : smt_checks(&p, all), used, k = sg_parse(9, all, n, s, &used);
        if (k < 0) return 0;
        pz->line[pz->n++] = all[k];
        sg_make(&p, all[k]);
        s += used;
    }
    return s && !*s && pz->n % 2 == 1 && smt_mated(&p);
}

void smt_write(const smt_problem *pz, char *out, int size)
{
    const spos *p = &pz->start;
    static const char letter[] = "?PLNSGBRK";
    int k = 0;
    for (int r = 0; r < 9; r++) {
        int empty = 0;
        for (int c = 0; c < 9; c++) {
            int pc = p->sq[SG_SQ(c, r)];
            if (!pc) { empty++; continue; }
            if (empty) out[k++] = (char)('0' + empty), empty = 0;
            if (pc & SG_PROMOTED) out[k++] = '+';
            char l = letter[SG_KIND(pc)];
            out[k++] = SG_OWNER(pc) ? (char)(l + 32) : l;
        }
        if (empty) out[k++] = (char)('0' + empty);
        if (r < 8) out[k++] = '/';
    }
    out[k++] = ' ';
    int any = 0;
    for (int kd = SG_ROOK; kd >= SG_PAWN; kd--) {
        if (!p->hand[0][kd]) continue;
        if (p->hand[0][kd] > 1) k += snprintf(out + k, (size_t)(size - k), "%d", p->hand[0][kd]);
        out[k++] = letter[kd];
        any = 1;
    }
    if (!any) out[k++] = '-';
    out[k] = 0;
    for (int i = 0; i < pz->n && k < size - 8; i++) {
        char u[8];
        sg_usi(9, pz->line[i], u);
        k += snprintf(out + k, (size_t)(size - k), " %s", u);
    }
}

#ifndef SMT_NO_LEVELS
int smt_level_load(smt_problem *pz, int level)
{
    if (level < 0 || level >= SMT_LEVELS) { memset(pz, 0, sizeof *pz); return 0; }
    return smt_parse(pz, smt_level_text[level]);
}
#else
int smt_level_load(smt_problem *pz, int level) { (void)level; memset(pz, 0, sizeof *pz); return 0; }
#endif

/* ---- a problem played -------------------------------------------------------------------------------------------- */
void smt_begin(smt_game *g, const smt_problem *pz, int level)
{
    memset(g, 0, sizeof *g);
    g->pz = *pz;
    g->level = level;
    g->pos[0] = pz->start;
}

int smt_level(smt_game *g, int level)
{
    smt_problem pz;
    if (!smt_level_load(&pz, level)) return 0;
    smt_begin(g, &pz, level);
    return 1;
}

int smt_solved(const smt_game *g) { return !g->failed && g->ply % 2 == 1 && smt_mated(smt_pos(g)); }

int smt_to_find(const smt_game *g) { return !g->failed && g->ply % 2 == 0 && g->ply < g->pz.n; }

smove smt_expected(const smt_game *g) { return g->pz.line[g->ply < g->pz.n ? g->ply : g->pz.n - 1]; }

static void play(smt_game *g, smove m)
{
    g->played[g->ply] = m;
    g->pos[g->ply + 1] = g->pos[g->ply];
    sg_make(&g->pos[g->ply + 1], m);
    g->ply++;
    g->hint_level = 0;
}

int smt_try(smt_game *g, smove m)
{
    if (!smt_to_find(g)) return SMT_ILLEGAL;
    smove ch[SG_MAX_MOVES], rep[SG_MAX_MOVES];
    int n = smt_checks(smt_pos(g), ch), at = -1;
    for (int i = 0; i < n && at < 0; i++)
        if (same(ch[i], m)) at = i;
    if (at < 0) return SMT_ILLEGAL;
    play(g, m);
    if (smt_mated(smt_pos(g))) return SMT_SOLVED;   /* any mate is right */
    if (g->ply < g->pz.n && same(m, g->pz.line[g->ply - 1])) {
        play(g, g->pz.line[g->ply]);
        return SMT_RIGHT;
    }
    /* a mistake: the defender's reply after which no quick mate is left (else its first) */
    int nr = smt_replies(smt_pos(g), rep), pick = 0, left = g->pz.n - g->ply - 1;
    if (left > 3) left = 3;
    for (int j = 0; j < nr && left >= 1; j++) {
        spos r = *smt_pos(g);
        sg_make(&r, rep[j]);
        if (!smt_mate_len(&r, left)) { pick = j; break; }
    }
    if (nr) play(g, rep[pick]);
    g->failed = 1;
    g->mistakes++;
    return SMT_WRONG;
}

int smt_hint(smt_game *g)
{
    if (!smt_to_find(g) || g->hint_level >= 2) return 0;
    if (!g->hint_level && g->hint_ply != g->ply + 1) { g->hints++; g->hint_ply = g->ply + 1; }
    return ++g->hint_level;
}

int smt_undo(smt_game *g)
{
    if (!g->ply) return 0;
    g->ply--;
    if (g->ply % 2) g->ply--;
    g->failed = 0;
    g->hint_level = 0;
    return 1;
}

int smt_script(smt_game *g, const char *s)
{
    if (*s == '#') {
        if (!smt_level(g, atoi(s + 1))) return 0;
        while (*s && *s != ' ') s++;
    }
    for (;;) {
        while (*s == ' ') s++;
        if (!*s) return 1;
        smove all[SG_MAX_MOVES];
        int n = smt_checks(smt_pos(g), all), used, k = sg_parse(9, all, n, s, &used);
        if (k < 0) return 0;
        int r = smt_try(g, all[k]);
        if (r == SMT_ILLEGAL) return 0;
        if (r == SMT_WRONG) smt_undo(g);
        s += used;
        while (*s && *s != ' ') s++;
    }
}
