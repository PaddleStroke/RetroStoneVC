/*
 * Jigsaw's levels: src/games/jigsaw_levels.h, from the pictures of src/games/jigsaw_pictures.h and the plan below
 * (a chapter: picture sets, the piece counts rising along it, upright or turned). Deterministic (no chance: a level is
 * a picture, a count and a turn; its cut comes from its number, jig_load). Run from the repository's root:
 *
 *   gcc -O2 -std=c11 -Iapps/boardgames/src -o /tmp/jigsaw_gen apps/boardgames/tools/jigsaw_gen.c &&
 *       /tmp/jigsaw_gen > apps/boardgames/src/games/jigsaw_levels.h
 *
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>

#include "games/jigsaw_pictures.h"

#define NPICS ((int)(sizeof JIG_PICS / sizeof JIG_PICS[0]))

/* a chapter: up to five sets (-1 ends the list), turned or not, and its counts: `first` levels at counts[0], the
 * rest at counts[1] (indexes of jig_counts: 12, 24, 48, 96, 150) */
static const struct { const char *name; int sets[5], rotate, counts[2], first; } PLAN[] = {
    {"the trumps", {JS_TRUMPS, -1}, 0, {0, 1}, 8},
    {"the courts", {JS_COURTS, -1}, 0, {1, 2}, 6},
    {"waves and mountains", {JS_JAPAN, JS_ROMANTIC, -1}, 0, {2, 3}, 6},
    {"the impressionists", {JS_IMPRESSION, -1}, 0, {2, 3}, 3},
    {"colour and light", {JS_POST, -1}, 0, {3, 4}, 6},
    {"the old masters", {JS_MASTERS, -1}, 0, {3, 4}, 4},
    {"the courts turned", {JS_COURTS, -1}, 1, {1, 2}, 6},
    {"the gallery turned", {JS_JAPAN, JS_ROMANTIC, JS_IMPRESSION, JS_POST, JS_MASTERS}, 1, {2, 3}, 14},
};
#define NPLAN ((int)(sizeof PLAN / sizeof PLAN[0]))

int main(void)
{
    int lv[400][3], n = 0, start[NPLAN];
    for (int c = 0; c < NPLAN; c++) {
        start[c] = n;
        int in = 0;
        for (int s = 0; s < 5 && PLAN[c].sets[s] >= 0; s++)
            for (int p = 0; p < NPICS; p++) {
                if (JIG_PICS[p].set != PLAN[c].sets[s]) continue;
                lv[n][0] = p;
                lv[n][1] = PLAN[c].counts[in < PLAN[c].first ? 0 : 1];
                lv[n][2] = PLAN[c].rotate;
                n++;
                in++;
            }
    }
    printf("/*\n * Jigsaw's levels, made by tools/jigsaw_gen.c from jigsaw_pictures.h: do not edit. A level: its\n"
           " * picture, its count (jig_counts: 12, 24, 48, 96, 150), turned pieces. The chapters:\n");
    for (int c = 0; c < NPLAN; c++)
        printf(" *   %d. %s: levels %d..%d\n", c + 1, PLAN[c].name, start[c] + 1, c + 1 < NPLAN ? start[c + 1] : n);
    printf(" * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("#ifndef JIGSAW_LEVELS_H\n#define JIGSAW_LEVELS_H\n\n#include <stdint.h>\n\n");
    printf("#define JIG_LEVEL_COUNT %d\n#define JIG_CHAPTER_COUNT %d\n\n", n, NPLAN);
    printf("#ifndef JIG_LEVEL_COUNT_ONLY                    /* the counts alone: the game's screen */\n");
    printf("static const uint8_t JIG_LEVELS[JIG_LEVEL_COUNT][3] = {\n");
    for (int i = 0; i < n; i++)
        printf("%s{%d, %d, %d},%s", i % 8 ? " " : "    ", lv[i][0], lv[i][1], lv[i][2],
               i % 8 == 7 || i == n - 1 ? "\n" : "");
    printf("};\n\nstatic const int16_t JIG_CHAPTER_START[JIG_CHAPTER_COUNT] = {");
    for (int c = 0; c < NPLAN; c++) printf("%s%d", c ? ", " : "", start[c]);
    printf("};\n#endif\n\n#endif\n");
    return 0;
}
