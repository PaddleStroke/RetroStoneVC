/*
 * Shogi Mate's levels: src/games/shogimate_levels.h, found by the game's own mate solver (shogimate_rules.c, with
 * shogi_rules.c's moves) among positions drawn at random here: gote's king on the far ranks, a few of its pieces
 * round it, a few of sente's pieces near it and a few in sente's hand (no sente king, as in tsume shogi).
 * A position is kept when the attacker mates in 1, 3, 5 or 7 moves (every one a check, the defender holding every
 * piece left and playing its longest defence) with exactly one first move, every attacker move of the main line the
 * only one that mates in time (the last one aside), and the attacker's hand empty at the end (no piece given for
 * nothing). Six chapters of 25: mates in one (two chapters), in three (two), in five, in seven (or five when sevens
 * are too few), each taken evenly from the easiest to the hardest by the solver's effort; no position twice.
 *   gcc -O2 -DSMT_NO_LEVELS -Iapps/boardgames/src -o /tmp/smtgen apps/boardgames/tools/shogimate_gen.c \
 *       apps/boardgames/src/games/shogimate_rules.c apps/boardgames/src/games/shogi_rules.c \
 *       && /tmp/smtgen > apps/boardgames/src/games/shogimate_levels.h
 * Deterministic (fixed seeds). MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/shogimate.h"

#define PER 25
#define POOL 160
#define MOST 7

static uint64_t rs = 0x5eed5eedu;
static int rnd(int n)
{
    rs ^= rs << 13;
    rs ^= rs >> 7;
    rs ^= rs << 17;
    return n > 0 ? (int)(rs % (uint64_t)n) : 0;
}


static int dead(int kind, int owner, int row)
{
    int a = owner ? 8 - row : row;
    return ((kind == SG_PAWN || kind == SG_LANCE) && a == 0) || (kind == SG_KNIGHT && a <= 1);
}

/* a position drawn at random; 0: not a good one */
static int draw_position(spos *p)
{
    memset(p, 0, sizeof *p);
    p->n = 9;
    int kc = 8 - rnd(rnd(3) ? 3 : 6), kr = rnd(4) ? 0 : 1;
    p->sq[SG_SQ(kc, kr)] = SG_PC(1, SG_KING);
    static const int total[8] = {0, 18, 4, 4, 4, 4, 2, 2};
    int used[8] = {0};
    static const int def_kinds[] = {SG_GOLD, SG_GOLD, SG_SILVER, SG_PAWN, SG_PAWN, SG_LANCE, SG_KNIGHT, SG_BISHOP};
    static const int att_kinds[] = {SG_PAWN, SG_LANCE, SG_KNIGHT, SG_SILVER, SG_GOLD, SG_GOLD, SG_BISHOP, SG_ROOK,
                                    SG_ROOK | SG_PROMOTED, SG_BISHOP | SG_PROMOTED, SG_PAWN | SG_PROMOTED};
    static const int hand_kinds[] = {SG_GOLD, SG_GOLD, SG_SILVER, SG_SILVER, SG_ROOK, SG_BISHOP, SG_KNIGHT, SG_LANCE,
                                     SG_PAWN};
    int nd = rnd(4), na = 1 + rnd(3), nh = rnd(3);
    for (int side = 1; side >= 0; side--) {
        int count = side ? nd : na, reach = side ? 2 : 3;
        for (int i = 0, guard = 0; i < count && guard < 50; guard++) {
            int pc = side ? def_kinds[rnd(8)] : att_kinds[rnd(11)], kind = SG_KIND(pc);
            int c = kc + rnd(2 * reach + 1) - reach, r = kr + rnd(reach + 2) - (side ? 1 : 0);
            if (c < 0 || c > 8 || r < 0 || r > 8 || p->sq[SG_SQ(c, r)] || used[kind] >= total[kind]) continue;
            if (!(pc & SG_PROMOTED) && dead(kind, side, r)) continue;
            if (kind == SG_PAWN && !(pc & SG_PROMOTED)) {   /* one pawn a file */
                int two = 0;
                for (int y = 0; y < 9; y++) two |= p->sq[SG_SQ(c, y)] == SG_PC(side, SG_PAWN);
                if (two) continue;
            }
            p->sq[SG_SQ(c, r)] = (uint8_t)(SG_PC(side, kind) | (pc & SG_PROMOTED));
            used[kind]++;
            i++;
        }
    }
    for (int i = 0; i < nh; i++) {
        int k = hand_kinds[rnd(9)];
        if (used[k] < total[k]) { p->hand[0][k]++; used[k]++; }
    }
    if (sg_in_check(p, 1)) return 0;
    smt_fill_hand(p);
    return 1;
}

/* the attacker's checks that mate within len (the defender's every reply mated in time): their count, the first's */
static int mating(const spos *p, int len, smove *first)
{
    smove ch[SG_MAX_MOVES], rep[SG_MAX_MOVES];
    int n = smt_checks(p, ch), k = 0;
    for (int i = 0; i < n; i++) {
        spos q = *p;
        sg_make(&q, ch[i]);
        int nr = smt_replies(&q, rep), all = 1;
        if (nr && len < 3) all = 0;
        for (int j = 0; j < nr && all; j++) {
            spos r = q;
            sg_make(&r, rep[j]);
            all = smt_mate_len(&r, len - 2) > 0;
        }
        if (all) {
            if (!k) *first = ch[i];
            k++;
        }
    }
    return k;
}

/* the main line from the start: the only mating move each time (any at the last), the longest defence; 0: none */
static int main_line(smt_problem *pz, int len)
{
    spos p = pz->start;
    pz->n = 0;
    for (int left = len; left >= 1; left -= 2) {
        smove m;
        int k = mating(&p, left, &m);
        if (!k || (k > 1 && left > 1)) return 0;
        pz->line[pz->n++] = m;
        sg_make(&p, m);
        if (left == 1) break;
        smove rep[SG_MAX_MOVES];
        int nr = smt_replies(&p, rep), best = -1, best_len = 0;
        for (int j = 0; j < nr; j++) {
            spos r = p;
            sg_make(&r, rep[j]);
            int l = smt_mate_len(&r, left - 2);
            if (!l) return 0;
            if (l > best_len) { best_len = l; best = j; }
        }
        if (best < 0 || best_len != left - 2) return 0;
        pz->line[pz->n++] = rep[best];
        sg_make(&p, rep[best]);
    }
    if (!smt_mated(&p)) return 0;
    for (int k = SG_PAWN; k <= SG_ROOK; k++)
        if (p.hand[0][k]) return 0;             /* a piece left in hand: given for nothing */
    return 1;
}

typedef struct { char text[200]; long effort; } cand;
static cand pool[4][POOL];
static int npool[4];

static int by_effort(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->effort < y->effort ? -1 : x->effort > y->effort ? 1 : strcmp(x->text, y->text);
}

static void take(int bucket, int count, const char *what)
{
    printf("    /* %s */\n", what);
    int n = npool[bucket];
    for (int i = 0; i < count; i++) {
        int k = n >= count && count > 1 ? (int)((long)i * (n - 1) / (count - 1)) : i;
        printf("    \"%s\",\n", k < n ? pool[bucket][k].text : "");
    }
    if (n < count) fprintf(stderr, "%s: only %d\n", what, n);
}

int main(int argc, char **argv)
{
    long tries, most = argc > 1 ? atol(argv[1]) : 200000;
    for (tries = 0; tries < most; tries++) {
        if (argc > 2 && tries % 2000 == 0)
            fprintf(stderr, "%ld: %d %d %d %d\n", tries, npool[0], npool[1], npool[2], npool[3]);
        if (npool[0] >= POOL && npool[1] >= POOL && npool[2] >= POOL && npool[3] >= POOL) break;
        smt_problem pz;
        memset(&pz, 0, sizeof pz);
        if (!draw_position(&pz.start)) continue;
        smt_forget();
        long e0 = smt_searched();
        int len = smt_mate_len(&pz.start, MOST);
        if (!len || npool[len / 2] >= POOL) continue;
        smove first;
        if (mating(&pz.start, len, &first) != 1 || !main_line(&pz, len)) continue;
        cand *c = &pool[len / 2][npool[len / 2]];
        smt_write(&pz, c->text, sizeof c->text);
        smt_problem back;
        if (!smt_parse(&back, c->text)) continue;
        int dup = 0;
        for (int i = 0; i < npool[len / 2] && !dup; i++) {
            const char *a = pool[len / 2][i].text, *b = c->text;
            dup = !strncmp(a, b, (size_t)(strchr(strchr(a, ' ') + 1, ' ') - a));
        }
        if (dup) continue;
        c->effort = smt_searched() - e0;
        npool[len / 2]++;
    }
    fprintf(stderr, "%ld tries: mates in 1 %d, 3 %d, 5 %d, 7 %d\n", tries, npool[0], npool[1], npool[2], npool[3]);
    for (int b = 0; b < 4; b++) qsort(pool[b], (size_t)npool[b], sizeof pool[b][0], by_effort);
    printf("/*\n * Shogi Mate's levels: 150 problems in six chapters of 25, made by tools/shogimate_gen.c\n"
           " * (its header says how): the board, sente's hand, the main line in USI (shogimate.h). Do not edit.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const smt_level_text[SMT_LEVELS] = {\n");
    int sevens = npool[3] >= PER;
    take(0, 2 * PER, "chapters 1 and 2: mates in one");
    take(1, 2 * PER, "chapters 3 and 4: mates in three");
    if (sevens) {
        take(2, PER, "chapter 5: mates in five");
        take(3, PER, "chapter 6: mates in seven");
    } else take(2, 2 * PER, "chapters 5 and 6: mates in five");
    printf("};\n");
    return 0;
}
