/*
 * Chess Puzzles' levels, chosen from the collection the app carries (chesspuzzles_data.h, made by
 * pick_chess_puzzles.py): src/games/chesspuzzles_levels.h. Six chapters of 25 on a rising rating, each chapter a kind
 * of puzzle within a band of rating (the puzzle's themes and its moves to find):
 *   1 mates in one move, rated under 900;
 *   2 material won (a fork, a pin, a skewer, a piece left hanging or trapped, a discovered attack, the defender taken),
 *     one or two moves to find, no mate, 900 to 1250;
 *   3 mates in two moves, 1250 to 1550;
 *   4 combinations (a deflection, an attraction, a sacrifice, a clearance, an interference, an in-between move, an
 *     x-ray, a discovered attack, a double check), two moves or more, no mate, 1550 to 1850;
 *   5 endgames (a promotion, an advanced pawn, zugzwang, a quiet or a defensive move), 1850 to 2150;
 *   6 the hardest: any kind, two moves or more, from 2150.
 * In each chapter the candidates are sorted by rating (then by id) and 25 taken evenly from the easiest to the
 * hardest; no puzzle twice. Each level's record (the collection's layout) is copied whole: the levels never change
 * when the collection is made again (the test checks every one).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/cpzgen apps/boardgames/tools/chesspuzzles_gen.c \
 *       apps/boardgames/src/games/chesspuzzles_rules.c apps/boardgames/src/games/chess.c \
 *       && /tmp/cpzgen > apps/boardgames/src/games/chesspuzzles_levels.h
 * Deterministic. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/chesspuzzles.h"
#include "games/chesspuzzles_data.h"

#define PER (CPZ_LEVELS / CPZ_CHAPTERS)

typedef struct { int lo, hi; uint32_t any, none; int min_n, max_n; const char *what; } rule;

typedef struct { int pack, index, rating, n; uint32_t themes; char id[6]; } cand;

static const rule RULES[CPZ_CHAPTERS] = {
    {0, 900, CPZ_TH_MATE_IN1, 0, 2, 2, "mate in one"},
    {900, 1250, CPZ_TH_FORK | CPZ_TH_PIN | CPZ_TH_SKEWER | CPZ_TH_HANGING_PIECE | CPZ_TH_TRAPPED_PIECE |
                    CPZ_TH_DISCOVERED_ATTACK | CPZ_TH_CAPTURING_DEFENDER, CPZ_TH_MATE, 2, 4, "material won"},
    {1250, 1550, CPZ_TH_MATE_IN2, 0, 4, 4, "mate in two"},
    {1550, 1850, CPZ_TH_DEFLECTION | CPZ_TH_ATTRACTION | CPZ_TH_SACRIFICE | CPZ_TH_CLEARANCE | CPZ_TH_INTERFERENCE |
                     CPZ_TH_INTERMEZZO | CPZ_TH_X_RAY_ATTACK | CPZ_TH_DISCOVERED_ATTACK | CPZ_TH_DOUBLE_CHECK,
     CPZ_TH_MATE, 4, CPZ_MAX_MOVES, "combination"},
    {1850, 2150, CPZ_TH_ENDGAME | CPZ_TH_PROMOTION | CPZ_TH_ADVANCED_PAWN | CPZ_TH_ZUGZWANG | CPZ_TH_QUIET_MOVE |
                     CPZ_TH_DEFENSIVE_MOVE, 0, 4, CPZ_MAX_MOVES, "endgame"},
    {2150, 10000, 0, 0, 4, CPZ_MAX_MOVES, "the hardest"},
};

static int cmp(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->rating != y->rating ? x->rating - y->rating : strcmp(x->id, y->id);
}

/* the bytes as C string literals (printable characters as they are, the others as 3-digit octal escapes) */
static void c_string(const uint8_t *d, int n)
{
    int col = 0;
    for (int i = 0; i < n; i++) {
        if (!col) printf("    \"");
        if (d[i] >= 32 && d[i] < 127 && d[i] != '\\' && d[i] != '"' && d[i] != '?') {
            putchar(d[i]);
            col++;
        } else {
            printf("\\%03o", d[i]);
            col += 4;
        }
        if (col >= 100 || i == n - 1) {
            printf("\"%s\n", i == n - 1 ? ";" : "");
            col = 0;
        }
    }
    if (!n) printf("    \"\";\n");
}

int main(void)
{
    static cand all[CPZ_TOTAL], pool[CPZ_TOTAL];
    static uint8_t blob[CPZ_LEVELS * 64];
    static cand chosen[CPZ_LEVELS];
    static uint8_t taken[CPZ_TOTAL];
    int n = 0, size = 0;
    for (int p = 0; p < CPZ_PACKS; p++)
        for (int i = 0; i < cpz_pack_size[p]; i++) {
            cpz_puzzle pz;
            if (!cpz_load(&pz, p, i)) {
                fprintf(stderr, "pack %d puzzle %d: not a good record\n", p, i);
                return 1;
            }
            cand c = {p, i, pz.rating, pz.n, pz.themes, {0}};
            memcpy(c.id, pz.id, 6);
            all[n++] = c;
        }
    fprintf(stderr, "%d puzzles in the collection (%d bytes)\n", n, (int)sizeof cpz_data - 1);
    for (int ch = 0; ch < CPZ_CHAPTERS; ch++) {
        const rule *r = &RULES[ch];
        int np = 0;
        for (int i = 0; i < n; i++) {
            const cand *c = &all[i];
            if (taken[i] || c->rating < r->lo || c->rating >= r->hi || c->n < r->min_n || c->n > r->max_n) continue;
            if ((r->any && !(c->themes & r->any)) || (c->themes & r->none)) continue;
            pool[np] = *c;
            pool[np++].index = i;               /* where it is in all[] (the pack's index is in its record) */
        }
        qsort(pool, (size_t)np, sizeof(cand), cmp);
        fprintf(stderr, "chapter %d (%s): %d candidates, ratings %d..%d\n", ch + 1, r->what, np,
                np ? pool[0].rating : 0, np ? pool[np - 1].rating : 0);
        if (np < PER) {
            fprintf(stderr, "chapter %d: too few candidates\n", ch + 1);
            return 1;
        }
        for (int k = 0; k < PER; k++) {
            cand c = pool[(int)((long)(np - 1) * k / (PER - 1))];
            int at = c.index;
            if (taken[at]) {                    /* never: evenly spaced picks are distinct (np >= PER) */
                fprintf(stderr, "a puzzle twice\n");
                return 1;
            }
            taken[at] = 1;
            c.index = all[at].index;
            chosen[ch * PER + k] = c;
            const uint8_t *rec;
            int len = cpz_record(c.pack, c.index, &rec);
            if (!len || size + len > (int)sizeof blob) {
                fprintf(stderr, "chapter %d: a record lost\n", ch + 1);
                return 1;
            }
            memcpy(blob + size, rec, (size_t)len);
            size += len;
        }
    }
    printf("/*\n"
           " * Chess Puzzles' levels: 150 puzzles of the collection (chesspuzzles_data.h), chosen by\n"
           " * tools/chesspuzzles_gen.c on a rising rating, six chapters of 25: mates in one, material won, mates in\n"
           " * two, combinations, endgames, the hardest. Their records, whole, in the collection's layout (described in\n"
           " * tools/pick_chess_puzzles.py), %d bytes. Do not edit.\n"
           " * The levels' ratings, in order:\n", size);
    for (int ch = 0; ch < CPZ_CHAPTERS; ch++) {
        printf(" *  %d %s:", ch + 1, RULES[ch].what);
        for (int k = 0; k < PER; k++) printf("%s %d", k && k % 13 == 0 ? "\n *   " : "", chosen[ch * PER + k].rating);
        printf("\n");
    }
    printf(" * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("#define CPZ_LEVEL_BYTES %d\nstatic const uint8_t cpz_level_data[CPZ_LEVEL_BYTES + 1] =\n", size);
    c_string(blob, size);
    return 0;
}
