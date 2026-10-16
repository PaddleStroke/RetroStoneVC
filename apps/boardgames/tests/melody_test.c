/*
 * Play by Ear's rules: every level (its notes on the keyboard it shows, C4..C6, its gaps, its tune), the levels
 * ordered by grade and the chapters' curve, no tune twice in a row; the keys played back (right, wrong and going on
 * from the same place, the last one), the hint (counted once a place), listening again (the first free), undo; the
 * stars' rule; random play (a phrase of the library of the variant's length, only moved to another key) over many
 * seeds; the notes' names and the script; the titles' file (two lines a tune).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/melody.h"
#include "games/melody_levels.h"
#include "games/melody_synth.c"              /* the piano: no SDL either (the test links only the rules) */

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void test_levels(void)
{
    int per = MEL_LEVELS / MEL_CHAPTERS, bad = 0;
    for (int i = 0; i < MEL_LEVELS; i++) {
        const mel_level *L = &mel_levels[i];
        int ok = L->n >= 3 && L->n <= MEL_MAX && L->tune < MEL_TUNES &&
                 (L->whites == 8 || L->whites == 12 || L->whites == 15);
        for (int k = 0; ok && k < L->n; k++)   /* each note a key of the keyboard shown */
            ok = L->p[k] >= MEL_LOW && L->p[k] <= mel_top(L->whites) && L->p[k] <= MEL_HIGH && L->d[k] > 0;
        if (!ok) printf("level %d: a note off its keyboard, or a bad length\n", i + 1);
        bad += !ok;
        /* ordered by grade: a level is never easier than the one before by more than the swaps allow (one tune
         * never twice in a row), and never easier than any level two chapters before */
        if (i) CHECK(L->grade + 10 >= mel_levels[i - 1].grade);
        if (i >= 2 * per) CHECK(L->grade >= mel_levels[(i / per - 1) * per - 1].grade);
        if (i) CHECK(L->tune != mel_levels[i - 1].tune);
    }
    CHECK(bad == 0);
    float mean[MEL_CHAPTERS] = {0};
    for (int i = 0; i < MEL_LEVELS; i++) mean[i / per] += mel_levels[i].grade / (float)per;
    for (int c = 1; c < MEL_CHAPTERS; c++) CHECK(mean[c] > mean[c - 1] + 30);   /* each chapter clearly harder */
    for (int i = 0; i < per; i++) {             /* the first chapter: short, white keys */
        CHECK(mel_levels[i].n <= 6);
        for (int k = 0; k < mel_levels[i].n; k++) CHECK(!mel_black(mel_levels[i].p[k]));
    }
    CHECK(mel_levels[MEL_LEVELS - 1].n >= 30);  /* the last: a long famous phrase */
    for (int t = 0; t < MEL_TUNES; t++) {       /* each tune used, and in the public domain */
        int used = 0;
        for (int i = 0; i < MEL_LEVELS; i++) used += mel_levels[i].tune == t;
        CHECK(used >= 1);
        CHECK(mel_tunes[t].died == 0 || mel_tunes[t].died <= 1955);
        CHECK(mel_tunes[t].bpm >= 40);
    }
}

static void test_play(void)
{
    mel_game g;
    mel_load(&g, 0);
    CHECK(g.n == mel_levels[0].n && g.pos == 0 && !mel_solved(&g));
    int first = g.p[0], other = first == MEL_LOW ? MEL_LOW + 2 : first - 1;
    CHECK(mel_press(&g, other) == MEL_WRONG);   /* a wrong key: a mistake, the same place */
    CHECK(g.pos == 0 && g.mistakes == 1);
    CHECK(mel_press(&g, other) == MEL_WRONG && g.mistakes == 2);
    CHECK(mel_press(&g, first) == MEL_RIGHT && g.pos == 1);
    CHECK(mel_hint(&g) == g.p[1] && g.hints == 1);
    CHECK(mel_hint(&g) == g.p[1] && g.hints == 1);   /* asked again at the same place: once */
    CHECK(mel_undo(&g) && g.pos == 0 && g.mistakes == 2);
    CHECK(!mel_undo(&g));
    for (int k = 0; k < g.n; k++) CHECK(mel_press(&g, g.p[k]) == (k + 1 == g.n ? MEL_DONE : MEL_RIGHT));
    CHECK(mel_solved(&g) && mel_next(&g) == -1 && mel_press(&g, first) == MEL_OVER && mel_hint(&g) == -1);
    CHECK(g.mistakes == 2 && g.hints == 1);

    /* the stars: 3 clean (one listen again free), then the logic puzzles' rule */
    mel_load(&g, 10);
    CHECK(mel_stars(&g) == 3);
    mel_listen(&g);
    CHECK(mel_help(&g) == 0 && mel_stars(&g) == 3);
    mel_listen(&g);
    CHECK(mel_help(&g) == 1 && mel_stars(&g) == 2);
    g.mistakes = 1;
    CHECK(mel_stars(&g) == 2);
    g.mistakes = 2;
    CHECK(mel_stars(&g) == 1);
    g.mistakes = 0;
    g.listens = 0;
    g.hints = 2;
    CHECK(mel_stars(&g) == 2);
    g.hints = 3;
    CHECK(mel_stars(&g) == 1);
    for (int h = 0; h < 5; h++)
        for (int m = 0; m < 5; m++) {
            g.hints = h;
            g.mistakes = m;
            g.listens = 1;
            CHECK(mel_stars(&g) == (h + m == 0 ? 3 : h + m <= 2 ? 2 : 1));
        }

    /* the rhythm: the first note at 0, onsets growing */
    mel_load(&g, MEL_LEVELS - 1);
    CHECK(mel_ms(&g, 0) == 0);
    for (int k = 1; k < g.n; k++) CHECK(mel_ms(&g, k) > mel_ms(&g, k - 1) && mel_len_ms(&g, k) > 0);
}

static void test_script(void)
{
    int used;
    CHECK(mel_parse_note("C4", &used) == 60 && used == 2);
    CHECK(mel_parse_note("F#5", &used) == 78 && used == 3);
    CHECK(mel_parse_note("Bb4", &used) == 70 && used == 3);
    CHECK(mel_parse_note("H4", &used) == -1 && mel_parse_note("C", &used) == -1);
    mel_game g;
    mel_load(&g, 0);
    char s[400] = "";
    for (int k = 0; k < g.n; k++) {             /* a wrong key, a hint, a listen, then the right notes */
        char nm[8];
        static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        snprintf(nm, sizeof nm, "%s%d ", N[g.p[k] % 12], g.p[k] / 12 - 1);
        if (k == 1) strcat(s, "C6 h l ");
        strcat(s, nm);
    }
    CHECK(mel_script(&g, s));
    CHECK(mel_solved(&g) && g.mistakes == (g.p[1] == 84 ? 0 : 1) && g.hints == 1 && g.listens == 1);
    CHECK(!mel_script(&g, "X9"));
}

static void test_random(void)
{
    for (uint32_t seed = 1; seed <= 600; seed++)
        for (int v = 0; v < MEL_VARIANTS; v++) {
            mel_game g;
            mel_random(&g, v, seed);
            CHECK(mel_variant_of(g.n) == v);
            const mel_level *L = &mel_levels[g.level];
            CHECK(L->n == g.n);
            int shift = g.p[0] - L->p[0], top = 0;
            for (int k = 0; k < g.n; k++) {     /* the library's phrase, moved as a whole: never notes at random */
                CHECK(g.p[k] - L->p[k] == shift && g.d[k] == L->d[k]);
                CHECK(g.p[k] >= MEL_LOW && g.p[k] <= mel_top(g.whites));
                if (g.p[k] > top) top = g.p[k];
            }
            CHECK(shift >= -7 && shift <= 7 && g.whites == (shift ? mel_whites_for(top) : L->whites));
        }
    mel_game a, b;                              /* the same seed: the same phrase */
    mel_random(&a, MEL_LONG, 77);
    mel_random(&b, MEL_LONG, 77);
    CHECK(!memcmp(&a, &b, sizeof a));
}

static void test_titles(void)
{
    FILE *f = fopen("apps/boardgames/help/en/melody_titles.txt", "r");
    if (!f) {
        printf("(the titles' file not found from here: not checked)\n");
        return;
    }
    char line[256];
    int n = 0;
    while (fgets(line, sizeof line, f)) n++;
    fclose(f);
    CHECK(n == 1 + 2 * MEL_TUNES);              /* a title line, then a tune's title and its composer */
}

static void test_synth(void)
{
    static int16_t b[4 * MEL_RATE];
    for (int p = MEL_LOW; p <= MEL_HIGH; p += 6)
        for (int dull = 0; dull < 2; dull++) {
            int n = mel_synth_len(p, dull), peak = 0;
            CHECK(n > MEL_RATE / 4 && n <= 4 * MEL_RATE);
            mel_synth(p, dull, b, n);
            for (int i = 0; i < n; i++) peak = abs(b[i]) > peak ? abs(b[i]) : peak;
            CHECK(peak > 3000 && peak < 32700);
            CHECK(abs(b[n - 1]) < 200);         /* it ends in silence: no click */
        }
}

int main(void)
{
    test_levels();
    test_play();
    test_script();
    test_random();
    test_titles();
    test_synth();
    printf("melody: %d levels, %d tunes: %s\n", MEL_LEVELS, MEL_TUNES, fails ? "FAILED" : "ok");
    return fails != 0;
}
