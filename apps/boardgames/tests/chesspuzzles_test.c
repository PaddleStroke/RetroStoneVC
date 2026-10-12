/*
 * Chess Puzzles' rules: every puzzle of the collection unpacked and played through with chess.c's rules (each move of
 * its line legal where it stands, the mates really mates, the ratings in their pack's band, the ids unique), the
 * mistakes (a legal move off the line: counted, not played), another mate accepted, the hints, undo, and whole
 * sessions of random tries driven by seeds, the stars, and every level (played through, its rating in its chapter's
 * band; the random deals never give one).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The Makefile links a game's test with its *_rules.c only; these rules need chess.c too, so it is compiled in here
 * (once the Makefile links chess.c into this test, build it with -DCPZ_CHESS_LINKED).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/chesspuzzles.h"
#include "games/chesspuzzles_data.h"
#include "games/puzrand.h"
#ifndef CPZ_CHESS_LINKED
#include "games/chess.c"
#endif

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int is_mate(const cpos *p)
{
    cmove m[MAX_MOVES];
    return chess_in_check(p, p->side) && !chess_legal(p, m);
}

static int same_pos(const cpos *a, const cpos *b) { return !memcmp(a->sq, b->sq, 64) && a->side == b->side; }

static int cmp_id(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

/* a legal move that is neither the line's nor a mate: -1 none */
static int off_line(const cpz_game *g, cmove *out)
{
    cmove all[MAX_MOVES];
    int n = chess_legal(cpz_pos(g), all);
    cmove want = cpz_expected(g);
    for (int i = 0; i < n; i++) {
        if (all[i].from == want.from && cpz_lands(all[i]) == cpz_lands(want)) continue;
        if (all[i].promo && all[i].promo != QUEEN) continue;
        cpos q = *cpz_pos(g);
        chess_make(&q, all[i]);
        if (is_mate(&q)) continue;
        *out = all[i];
        return 1;
    }
    return 0;
}

int main(void)
{
    static const int lo[CPZ_VARIANTS] = {0, 1000, 1400, 1800, 2200, 0},
                     hi[CPZ_VARIANTS] = {1000, 1400, 1800, 2200, 4000, 4000};
    static char ids[CPZ_TOTAL][6];
    static cpz_game G;
    int total = 0, mates_end = 0, alt_mates = 0, wrongs = 0;
    CHECK(CPZ_PACKS == CPZ_VARIANTS);
    CHECK(CPZ_DATA_BYTES <= 1024 * 1024);
    for (int pack = 0; pack < CPZ_VARIANTS; pack++) {
        int n = cpz_count(pack), mates = 0, sum = 0;
        CHECK(n >= 1000);
        for (int i = 0; i < n; i++) {
            if (!cpz_start(&G, pack, i)) {
                printf("FAIL pack %d puzzle %d: not loaded\n", pack, i);
                fails++;
                continue;
            }
            const cpz_puzzle *pz = &G.pz;
            memcpy(ids[total++], pz->id, 6);
            CHECK(strlen(pz->id) == 5);
            CHECK(pz->n >= 2 && pz->n <= CPZ_MAX_MOVES && pz->n % 2 == 0);
            CHECK(pz->rating >= lo[pack] - 8 && pz->rating < hi[pack] + 8);
            CHECK(G.solver == !pz->start.side && G.ply == 1);
            CHECK(cpz_to_find(&G) && !cpz_solved(&G) && !cpz_reply_due(&G));
            sum += pz->rating;
            /* a mistake first: counted, nothing played */
            cmove w;
            cpos before = *cpz_pos(&G);
            if (off_line(&G, &w)) {
                CHECK(cpz_try(&G, w.from, cpz_lands(w)) == CPZ_WRONG);
                CHECK(G.mistakes == 1 && G.ply == 1 && same_pos(cpz_pos(&G), &before));
                wrongs++;
            }
            /* the line played through */
            while (!cpz_solved(&G)) {
                cmove m = cpz_expected(&G);
                int r = cpz_try(&G, m.from, cpz_lands(m));
                CHECK(r == CPZ_RIGHT || r == CPZ_SOLVED);
                if (r != CPZ_RIGHT && r != CPZ_SOLVED) break;
                if (r == CPZ_RIGHT) {
                    CHECK(cpz_reply_due(&G));
                    cpz_reply(&G);
                    CHECK(cpz_to_find(&G));
                }
            }
            CHECK(G.ply == pz->n);
            if (pz->themes & CPZ_TH_MATE) {
                CHECK(is_mate(cpz_pos(&G)));
                mates_end++;
            }
            if (pack == CPZ_MATES) {
                mates++;
                CHECK(pz->themes & (CPZ_TH_MATE_IN1 | CPZ_TH_MATE_IN2 | CPZ_TH_MATE_IN3));
                CHECK(pz->n == 2 * (pz->themes & CPZ_TH_MATE_IN1 ? 1 : pz->themes & CPZ_TH_MATE_IN2 ? 2 : 3));
            }
            /* undo: back to the start's move to find, the positions the same */
            int undos = 0;
            while (cpz_undo(&G)) undos++;
            CHECK(undos == pz->n / 2 && G.ply == 1 && same_pos(cpz_pos(&G), &before));
            /* the last move: any other mate is accepted */
            if (pz->themes & CPZ_TH_MATE) {
                while (G.ply < pz->n - 1) {
                    cmove m = cpz_expected(&G);
                    cpz_try(&G, m.from, cpz_lands(m));
                    cpz_reply(&G);
                }
                cmove all[MAX_MOVES], want = cpz_expected(&G);
                int mistakes_before = G.mistakes;
                int k = chess_legal(cpz_pos(&G), all);
                for (int j = 0; j < k; j++) {
                    if (all[j].from == want.from && cpz_lands(all[j]) == cpz_lands(want)) continue;
                    if (all[j].promo && all[j].promo != QUEEN) continue;
                    cpos q = *cpz_pos(&G);
                    chess_make(&q, all[j]);
                    if (!is_mate(&q)) continue;
                    CHECK(cpz_try(&G, all[j].from, cpz_lands(all[j])) == CPZ_SOLVED);
                    CHECK(cpz_solved(&G) && G.mistakes == mistakes_before);
                    alt_mates++;
                    break;
                }
            }
        }
        if (pack == CPZ_MATES) CHECK(mates == n);
        printf("pack %d: %d puzzles, mean rating %d\n", pack, n, n ? sum / n : 0);
    }
    CHECK(total == CPZ_TOTAL);
    qsort(ids, (size_t)total, 6, cmp_id);
    for (int i = 1; i < total; i++) CHECK(strcmp(ids[i - 1], ids[i]) != 0);
    printf("%d puzzles, %d bytes; %d end in a mate, %d with another mate at the end, %d mistakes tried\n", total,
           CPZ_DATA_BYTES, mates_end, alt_mates, wrongs);

    /* the hints: the piece, then its square, then nothing more; counted once a move */
    CHECK(cpz_start(&G, CPZ_EASY, 0));
    CHECK(cpz_hint(&G) == 1 && cpz_hint(&G) == 2 && cpz_hint(&G) == 0 && G.hints == 1);
    {
        cmove m = cpz_expected(&G);
        int r = cpz_try(&G, m.from, cpz_lands(m));
        CHECK(G.hint_level == 0);
        if (r == CPZ_RIGHT) {
            cpz_reply(&G);
            CHECK(cpz_hint(&G) == 1 && G.hints == 2);
        }
    }
    /* illegal tries: nothing counted */
    CHECK(cpz_start(&G, CPZ_MEDIUM, 3));
    {
        int empty = -1;
        for (int s = 0; s < 64 && empty < 0; s++)
            if (!cpz_pos(&G)->sq[s]) empty = s;
        CHECK(cpz_try(&G, empty, (empty + 8) % 64) == CPZ_ILLEGAL && G.mistakes == 0);
        int theirs = -1;
        for (int s = 0; s < 64 && theirs < 0; s++)
            if (cpz_pos(&G)->sq[s] && COLOR(cpz_pos(&G)->sq[s]) != G.solver) theirs = s;
        CHECK(cpz_try(&G, theirs, theirs) == CPZ_ILLEGAL && G.mistakes == 0);
    }
    /* the script: the player's moves, the replies by themselves */
    CHECK(cpz_start(&G, CPZ_HARD, 7));
    {
        char line[64] = "";
        for (int k = 1; k < G.pz.n; k += 2) {
            cmove m = G.pz.move[k];
            char mv[6] = {(char)('a' + FILE_OF(m.from)), (char)('1' + RANK_OF(m.from)),
                          (char)('a' + FILE_OF(cpz_lands(m))), (char)('1' + RANK_OF(cpz_lands(m))), ' ', 0};
            strcat(line, mv);
        }
        CHECK(cpz_script(&G, line) && cpz_solved(&G));
        CHECK(!cpz_script(&G, "zz99"));
    }

    /* whole sessions: random puzzles, random tries (mistakes, hints, undos), always solved in the end */
    for (int seed = 1; seed <= 300; seed++) {
        uint32_t rs = pr_seed((uint32_t)seed);
        int pack = pr_below(&rs, CPZ_VARIANTS), idx = pr_below(&rs, cpz_count(pack));
        CHECK(cpz_start(&G, pack, idx));
        int steps = 0, mistakes = 0;
        while (!cpz_solved(&G) && steps++ < 200) {
            int r = pr_below(&rs, 10);
            cmove w;
            if (r < 3 && off_line(&G, &w)) {
                CHECK(cpz_try(&G, w.from, cpz_lands(w)) == CPZ_WRONG);
                mistakes++;
            } else if (r == 3) {
                cpz_hint(&G);
            } else if (r == 4) {
                cpz_undo(&G);
            } else {
                cmove m = cpz_expected(&G);
                if (cpz_try(&G, m.from, cpz_lands(m)) == CPZ_RIGHT) cpz_reply(&G);
            }
            CHECK(cpz_to_find(&G) || cpz_solved(&G));
        }
        CHECK(cpz_solved(&G) && G.mistakes == mistakes);
    }

    /* the stars: 3 clean, 2 with one slip (a mistake or a hint), else 1 */
    CHECK(cpz_start(&G, CPZ_EASY, 5));
    CHECK(cpz_stars(&G) == 3);
    G.hints = 1;
    CHECK(cpz_stars(&G) == 2);
    G.mistakes = 1;
    CHECK(cpz_stars(&G) == 1);
    G.hints = 0;
    CHECK(cpz_stars(&G) == 2);

    /* every level: loaded, its line played through to the end (its mate a mate), in its chapter's band of rating and
     * rising within it, no level twice; and the packs' random deals never give a level's puzzle */
    {
        static const int lo[CPZ_CHAPTERS] = {0, 900, 1250, 1550, 1850, 2150};
        static char lids[CPZ_LEVELS][6];
        int last = -1;
        for (int l = 0; l < CPZ_LEVELS; l++) {
            if (!cpz_level(&G, l)) {
                printf("FAIL level %d: not loaded\n", l + 1);
                fails++;
                continue;
            }
            const cpz_puzzle *pz = &G.pz;
            int c = l / (CPZ_LEVELS / CPZ_CHAPTERS);
            if (l % (CPZ_LEVELS / CPZ_CHAPTERS) == 0) last = -1;
            CHECK(pz->rating >= lo[c] - 8 && (c == CPZ_CHAPTERS - 1 || pz->rating < lo[c + 1] + 8));
            CHECK(pz->rating >= last);
            last = pz->rating;
            CHECK(G.pack == -1 && G.index == l && G.ply == 1 && cpz_to_find(&G));
            CHECK(cpz_is_level(pz->id));
            memcpy(lids[l], pz->id, 6);
            for (int m = 0; m < l; m++) CHECK(strcmp(lids[m], lids[l]) != 0);
            if (c == 0 || c == 2) CHECK((pz->themes & CPZ_TH_MATE) && pz->n == 2 * (c == 0 ? 1 : 2));
            while (!cpz_solved(&G)) {
                cmove m = cpz_expected(&G);
                int r = cpz_try(&G, m.from, cpz_lands(m));
                CHECK(r == CPZ_RIGHT || r == CPZ_SOLVED);
                if (r != CPZ_RIGHT && r != CPZ_SOLVED) break;
                if (r == CPZ_RIGHT) cpz_reply(&G);
            }
            CHECK(G.ply == pz->n && cpz_stars(&G) == 3);
            if (pz->themes & CPZ_TH_MATE) CHECK(is_mate(cpz_pos(&G)));
        }
        CHECK(!cpz_level(&G, -1) && !cpz_level(&G, CPZ_LEVELS));
        int dealt = 0;
        for (int seed = 1; seed <= 3000; seed++) {
            uint32_t rs = pr_seed((uint32_t)seed);
            int pack = pr_below(&rs, CPZ_VARIANTS);
            CHECK(cpz_start_random(&G, pack, (int)(pr_next(&rs) % 100000)));
            CHECK(G.pack == pack && !cpz_is_level(G.pz.id));
            dealt++;
        }
        int levels_in_packs = 0;                /* the levels are the collection's own puzzles */
        for (int i = 0; i < total; i++) levels_in_packs += cpz_is_level(ids[i]);
        printf("%d levels; %d of them in the packs; %d random deals, none a level\n", CPZ_LEVELS, levels_in_packs,
               dealt);
        CHECK(levels_in_packs == CPZ_LEVELS);
    }

    if (fails) {
        printf("chesspuzzles: %d failures\n", fails);
        return 1;
    }
    printf("chesspuzzles: ok\n");
    return 0;
}
