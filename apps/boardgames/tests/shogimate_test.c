/*
 * Shogi Mate's rules: the defender's quick replies against shogi.h's own (random positions in check), and every
 * level: read back, its length its chapter's, each attacker move of its line the only one that mates in time (the
 * last aside), each defender reply one of the longest, the line played to mate with the hand empty, a wrong check
 * refuted and taken back, a move without check refused, the hints, undo. The slowest answer is printed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The Makefile links a game's test with its *_rules.c only; these rules need shogi_rules.c too, so it is compiled in
 * here (once the Makefile links it into this test, build it with -DSMT_SHOGI_LINKED).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/shogimate.h"
#ifndef SMT_SHOGI_LINKED
#include "games/shogi_rules.c"
#endif

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static smt_game G;
static double slowest;

static double now_ms(void) { return (double)clock() * 1000.0 / CLOCKS_PER_SEC; }

static int same(smove a, smove b) { return a.from == b.from && a.to == b.to && a.drop == b.drop && a.promo == b.promo; }

static int has(const smove *list, int n, smove m)
{
    for (int i = 0; i < n; i++)
        if (same(list[i], m)) return 1;
    return 0;
}

/* the replies against shogi.h's: the same moves, every one */
static void test_replies(void)
{
    uint32_t r = pr_seed(7);
    int tested = 0;
    for (int t = 0; t < 4000 && tested < 400; t++) {
        spos p;
        memset(&p, 0, sizeof p);
        p.n = 9;
        p.side = 1;
        int k = SG_SQ(pr_below(&r, 9), pr_below(&r, 3));
        p.sq[k] = SG_PC(1, SG_KING);
        static const int kinds[] = {SG_ROOK, SG_BISHOP, SG_LANCE, SG_GOLD, SG_SILVER, SG_KNIGHT, SG_PAWN};
        for (int i = 0; i < 4; i++) {
            int s = pr_below(&r, 81), kind = kinds[pr_below(&r, 7)], owner = i < 2 ? 0 : 1;
            if (p.sq[s] || (kind == SG_PAWN || kind == SG_LANCE || kind == SG_KNIGHT)) {
                if (p.sq[s]) continue;
                kind = SG_GOLD;
            }
            p.sq[s] = SG_PC(owner, kind);
        }
        if (!sg_in_check(&p, 1)) continue;
        smt_fill_hand(&p);
        smove a[SG_MAX_MOVES], b[SG_MAX_MOVES];
        int na = smt_replies(&p, a), nb = sg_legal(&p, b), ok = na == nb;
        for (int i = 0; i < na && ok; i++) ok = has(b, nb, a[i]);
        CHECK(ok);
        tested++;
    }
    CHECK(tested > 100);
}

/* the attacker's checks that mate within len */
static int mating(const spos *p, int len, smove *one)
{
    smove ch[SG_MAX_MOVES], rep[SG_MAX_MOVES];
    int n = smt_checks(p, ch), k = 0;
    for (int i = 0; i < n; i++) {
        spos q = *p;
        sg_make(&q, ch[i]);
        int nr = smt_replies(&q, rep), all = !nr || len >= 3;
        for (int j = 0; j < nr && all; j++) {
            spos r = q;
            sg_make(&r, rep[j]);
            all = smt_mate_len(&r, len - 2) > 0;
        }
        if (all) { if (!k) *one = ch[i]; k++; }
    }
    return k;
}

static void test_level(int level)
{
    smt_problem pz;
    CHECK(smt_level_load(&pz, level));
    if (!pz.n) return;
    int ch = level / 25, want = ch < 2 ? 1 : ch < 4 ? 3 : ch == 4 ? 5 : 7;
    CHECK(pz.n == want);
    char back[256];
    smt_write(&pz, back, sizeof back);
    smt_problem again;
    CHECK(smt_parse(&again, back) && again.n == pz.n && !memcmp(&again.start, &pz.start, sizeof pz.start));
    /* the line: each attacker move the only one in time (the last aside), each reply one of the longest */
    smt_forget();
    CHECK(smt_mate_len(&pz.start, pz.n) == pz.n);
    spos p = pz.start;
    for (int i = 0; i < pz.n; i++) {
        int left = pz.n - i;
        if (i % 2 == 0) {
            smove one = {0, 0, 0, 0};
            int k = mating(&p, left, &one);
            CHECK(k >= 1 && (k == 1 || left == 1));
            if (left > 1) CHECK(same(one, pz.line[i]));
        } else {
            smove rep[SG_MAX_MOVES];
            int nr = smt_replies(&p, rep), longest = 0;
            for (int j = 0; j < nr; j++) {
                spos r = p;
                sg_make(&r, rep[j]);
                int l = smt_mate_len(&r, left - 1);
                CHECK(l > 0);
                if (l > longest) longest = l;
            }
            spos r = p;
            sg_make(&r, pz.line[i]);
            CHECK(smt_mate_len(&r, left - 1) == longest && longest == left - 1);
        }
        sg_make(&p, pz.line[i]);
    }
    CHECK(smt_mated(&p));
    for (int k = SG_PAWN; k <= SG_ROOK; k++) CHECK(!p.hand[0][k]);
    /* played: hints, then the line */
    CHECK(smt_level(&G, level));
    CHECK(smt_hint(&G) == 1 && smt_hint(&G) == 2 && smt_hint(&G) == 0 && G.hints == 1);
    for (int i = 0; i < pz.n; i += 2) {
        double t = now_ms();
        int r = smt_try(&G, pz.line[i]);
        t = now_ms() - t;
        if (t > slowest) slowest = t;
        CHECK(r == (i == pz.n - 1 ? SMT_SOLVED : SMT_RIGHT));
    }
    CHECK(smt_solved(&G) && G.mistakes == 0);
    CHECK(smt_undo(&G) && !smt_solved(&G) && G.ply == pz.n - 1);
    /* a wrong check: refuted, counted, taken back; a move without check: refused */
    CHECK(smt_level(&G, level));
    smove all[SG_MAX_MOVES], ch_[SG_MAX_MOVES];
    int n = sg_legal(&G.pos[0], all), nc = smt_checks(&G.pos[0], ch_), wrong = 0, quiet = 0;
    for (int i = 0; i < n; i++) {
        if (!has(ch_, nc, all[i])) {
            if (!quiet) { CHECK(smt_try(&G, all[i]) == SMT_ILLEGAL && G.ply == 0); quiet = 1; }
            continue;
        }
        if (same(all[i], pz.line[0]) || wrong) continue;
        spos q = G.pos[0];
        sg_make(&q, all[i]);
        if (smt_mated(&q)) continue;            /* never: the first move is the only mate in time */
        double t = now_ms();
        CHECK(smt_try(&G, all[i]) == SMT_WRONG);
        t = now_ms() - t;
        if (t > slowest) slowest = t;
        CHECK(G.failed && G.mistakes == 1 && !smt_to_find(&G));
        CHECK(smt_undo(&G) && G.ply == 0 && !G.failed && smt_to_find(&G));
        wrong = 1;
    }
}

int main(void)
{
    test_replies();
    double t0 = now_ms();
    for (int level = 0; level < SMT_LEVELS; level++) test_level(level);
    CHECK(smt_script(&G, "#0") && G.level == 0);
    printf("shogimate: %d levels, %.0f ms, the slowest answer %.1f ms\n", SMT_LEVELS, now_ms() - t0, slowest);
    if (fails) printf("%d FAILED\n", fails);
    return fails != 0;
}
