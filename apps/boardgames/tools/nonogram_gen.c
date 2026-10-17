/*
 * Nonograms' levels: src/games/nonogram_levels.h, from our own pictures (tools/nonogram_pictures.txt: little pixel
 * pictures drawn for this game, each named). Each one is checked and graded by the game's own line solver:
 *   - well formed (square, 5 to 15 a side, its colours known), not the same as another (turned or mirrored);
 *   - solvable line by line alone (ng_sweeps): so it has one solution and never needs a guess;
 *   - its difficulty: the sweeps over every line the solver takes, and the squares still unknown after the first one
 *     (a picture whose lines give little at first is harder), with its size first.
 * The levels go by size (5 x 5 first, 15 x 15 last), each size from its easiest to its hardest; six chapters of 25,
 * each ending with a showpiece (a picture marked "= * name", moved to the chapter's end within its size).
 * A picture that fails is reported with its unknown squares and the tool stops: draw it again (or change a square)
 * in the pictures' file. Deterministic: run it again after a change to the pictures or the solver and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/nggen apps/boardgames/tools/nonogram_gen.c \
 *       apps/boardgames/src/games/nonogram_rules.c && /tmp/nggen > apps/boardgames/src/games/nonogram_levels.h
 * (run from the repository's root; another pictures' file: /tmp/nggen FILE)
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/nonogram.h"

#define MAXP 400

typedef struct {
    char name[64];
    int n, at, sweeps, left, show;   /* show: a showpiece ("= * name"), kept for a chapter's end */
    char rows[NG_MAX][NG_MAX + 1];
    uint8_t pic[NG_SQ];
    double score;
} picture;

static picture P[MAXP];
static int np;

static int by_score(const void *a, const void *b)
{
    const picture *x = a, *y = b;
    if (x->score != y->score) return x->score < y->score ? -1 : 1;
    return x->at - y->at;
}

/* the picture's shape whichever way it lies: the smallest of the eight (turned, mirrored) */
static void canon(const picture *p, char *key)
{
    int n = p->n;
    char k[NG_SQ + 1];
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int u = sx; sx = sy; sy = u; }
                k[y * n + x] = (char)('0' + p->pic[sy * n + sx]);
            }
        k[n * n] = 0;
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

static int add(picture *p, int nr)
{
    p->n = nr;
    if (nr < 5 || nr > NG_MAX) {
        fprintf(stderr, "%s: %d rows\n", p->name, nr);
        return 0;
    }
    for (int y = 0; y < nr; y++) {
        if ((int)strlen(p->rows[y]) != nr) {
            fprintf(stderr, "%s: row %d is not %d long\n", p->name, y + 1, nr);
            return 0;
        }
        for (int x = 0; x < nr; x++) {
            if (!strchr(ng_letters, p->rows[y][x])) {
                fprintf(stderr, "%s: letter %c\n", p->name, p->rows[y][x]);
                return 0;
            }
            p->pic[y * nr + x] = p->rows[y][x] != '.';
        }
    }
    p->sweeps = ng_sweeps(nr, p->pic, &p->left);
    if (!p->sweeps) {
        fprintf(stderr, "%s (%dx%d): not solvable line by line\n", p->name, nr, nr);
        return 0;
    }
    p->score = nr * 1000.0 + p->sweeps * 100.0 + 100.0 * p->left / (nr * nr);
    p->at = np;
    return 1;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "apps/boardgames/tools/nonogram_pictures.txt";
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot read %s\n", path);
        return 1;
    }
    char buf[256];
    int nr = -1;
    while (fgets(buf, sizeof buf, f)) {
        buf[strcspn(buf, "\r\n")] = 0;
        if (buf[0] == '#') continue;
        if (buf[0] == '=' || !buf[0]) {
            if (nr > 0) {
                if (!add(&P[np], nr)) return 1;
                np++;
                nr = -1;
            }
            if (buf[0] == '=') {
                if (np >= MAXP) return 1;
                const char *nm = buf + 1 + (buf[1] == ' ');
                P[np].show = nm[0] == '*';
                if (P[np].show) nm += 1 + (nm[1] == ' ');
                snprintf(P[np].name, sizeof P[np].name, "%s", nm);
                nr = 0;
            }
            continue;
        }
        if (nr < 0 || nr >= NG_MAX) {
            fprintf(stderr, "a row outside a picture: %s\n", buf);
            return 1;
        }
        if (strlen(buf) > NG_MAX) {
            fprintf(stderr, "a row too long: %s\n", buf);
            return 1;
        }
        memcpy(P[np].rows[nr++], buf, strlen(buf) + 1);
    }
    fclose(f);
    if (nr > 0) {
        if (!add(&P[np], nr)) return 1;
        np++;
    }
    if (np != NG_LEVELS) {
        fprintf(stderr, "%d pictures, %d wanted\n", np, NG_LEVELS);
        return 1;
    }
    for (int i = 0; i < np; i++) {
        char a[NG_SQ + 1], b[NG_SQ + 1];
        canon(&P[i], a);
        for (int j = 0; j < i; j++) {
            if (P[j].n != P[i].n) continue;
            canon(&P[j], b);
            if (!strcmp(a, b)) {
                fprintf(stderr, "%s and %s are the same shape\n", P[i].name, P[j].name);
                return 1;
            }
        }
    }
    qsort(P, (size_t)np, sizeof P[0], by_score);
    /* each chapter ends with a showpiece: the chapter's (or a later) first one of the size it ends with moves to its end
       (within one size, so the sizes still only grow) */
    for (int e = 24; e < np; e += 25) {
        int k = -1;
        for (int i = e - 24; i < np && k < 0; i++)   /* from the chapter's start: the ends placed stay put */
            if (P[i].show == 1 && P[i].n == P[e].n) k = i;
        if (k < 0) {
            fprintf(stderr, "no showpiece of %dx%d left for level %d\n", P[e].n, P[e].n, e + 1);
            return 1;
        }
        picture s = P[k];
        s.show = 2;
        if (k < e) memmove(&P[k], &P[k + 1], (size_t)(e - k) * sizeof P[0]);
        else memmove(&P[e + 1], &P[e], (size_t)(k - e) * sizeof P[0]);
        P[e] = s;
    }
    for (int i = 0; i < np; i++)
        if (P[i].show == 1) {
            fprintf(stderr, "%s: a showpiece without a chapter's end\n", P[i].name);
            return 1;
        }
    printf("/*\n * Nonograms' levels: the house's own pictures (tools/nonogram_pictures.txt, drawn for this game),\n"
           " * checked and ordered by tools/nonogram_gen.c: each one solvable line by line (one solution, never a\n"
           " * guess), by size from 5 x 5 to 15 x 15, each size from its easiest (the line solver's sweeps, the squares\n"
           " * left after the first one); six chapters of 25, each ending with a showpiece. Each: its size and rows ('.' empty, else a colour:\n"
           " * nonogram.h, ng_letters); in the comment its name (never shown: no words in play) and its grade.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const ng_picture ng_levels[NG_LEVELS] = {\n");
    for (int i = 0; i < np; i++) {
        const picture *p = &P[i];
        printf("    /* %d: %s%s, %dx%d, %d sweep%s, %d left after the first */\n    {%d, {", i + 1, p->name,
               p->show ? " (a showpiece)" : "", p->n, p->n, p->sweeps, p->sweeps > 1 ? "s" : "", p->left, p->n);
        int per = p->n <= 8 ? p->n : p->n <= 12 ? 6 : 5;   /* rows a line */
        for (int y = 0; y < p->n; y++)
            printf("%s\"%s\"%s", y && y % per == 0 ? "\n          " : "", p->rows[y], y < p->n - 1 ? ", " : "");
        printf("}},\n");
    }
    printf("};\n");
    return 0;
}
