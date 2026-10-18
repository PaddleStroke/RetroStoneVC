/*
 * Falling Notes' rules (games/pianofall.h): the timing judgement, the points and stars, the holds, the calibration,
 * every song's chart valid and played to 100% by a perfect script (and to less by late or absent players), random
 * melodies over many seeds.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pianofall.h"
#include "games/pianofall_levels.h"

static int fails;
#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            fails++;                                                                                                   \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                                \
            printf(__VA_ARGS__);                                                                                       \
            printf("\n");                                                                                              \
        }                                                                                                              \
    } while (0)

/* a little chart of our own: C4 at 1000 (a tap), E4 at 1500 (held 800), C4 again at 2400, a chord G4 + E4 at 3000 */
static void toy(pfl_game *g)
{
    memset(g, 0, sizeof *g);
    g->level = -1;
    g->tune = -1;
    g->bpm = 100;
    g->low = 60;
    g->high = 72;
    const pfl_note n[] = {{1000, 300, 60}, {1500, 800, 64}, {2400, 300, 60}, {3000, 300, 67}, {3000, 300, 64}};
    g->n = 5;
    memcpy(g->note, n, sizeof n);
    g->now = -1000000;
}

static void test_judgement(void)
{
    pfl_game g;
    toy(&g);
    CHECK(pfl_check(&g) == 0, "toy chart: %d", pfl_check(&g));
    CHECK(pfl_press(&g, 60, 500) == -1 && g.stray == 1, "too early: a stray press");
    int i = pfl_press(&g, 60, 1000 + PFL_PERFECT_MS);
    CHECK(i == 0 && g.judge[0] == PFL_PERFECT_HIT, "at the perfect window's edge: perfect");
    CHECK(g.combo == 1 && g.err[0] == PFL_PERFECT_MS, "combo %d err %d", g.combo, g.err[0]);
    i = pfl_press(&g, 64, 1500 - PFL_PERFECT_MS - 1);
    CHECK(i == 1 && g.judge[1] == PFL_GOOD_HIT && g.hold[1] == PFL_HOLDING, "just outside: good, holding");
    pfl_release(&g, 1, 1500 + 800 - PFL_HOLD_SLACK - 1);
    CHECK(g.hold[1] == PFL_BROKEN, "let go too soon: broken");
    CHECK(pfl_press(&g, 62, 2400) == -1 && g.combo == 0, "the wrong key: stray, combo broken");
    pfl_advance(&g, 2400 + PFL_GOOD_MS + 1);
    CHECK(g.judge[2] == PFL_MISS && g.miss == 1, "not pressed in time: missed");
    CHECK(pfl_press(&g, 67, 3000 - PFL_GOOD_MS) == 3 && g.judge[3] == PFL_GOOD_HIT, "the chord's top, early: good");
    CHECK(!pfl_over(&g), "the chord's lower note still to come");
    CHECK(pfl_press(&g, 64, 3010) == 4 && g.judge[4] == PFL_PERFECT_HIT, "the chord's lower note");
    CHECK(pfl_over(&g), "every note judged");
    /* points: perfect 100, good-but-broken 30, miss 0, good 60, perfect 100 */
    CHECK(pfl_points(&g) == 290 && pfl_accuracy(&g) == 58, "points %d accuracy %d", pfl_points(&g), pfl_accuracy(&g));
    CHECK(g.best_combo == 2, "best combo %d", g.best_combo);

    toy(&g);                                    /* the closest note of a key is the one taken */
    CHECK(pfl_press(&g, 60, 1149) == 0, "late on the first C");
    toy(&g);
    pfl_press(&g, 64, 1500);
    pfl_advance(&g, 2300);
    CHECK(g.hold[1] == PFL_HELD, "held to its end");
    pfl_release(&g, 1, 2400);
    CHECK(g.hold[1] == PFL_HELD, "let go afterwards: still held");
    toy(&g);
    pfl_press(&g, 64, 1500);
    pfl_release(&g, 1, 1500 + 800 - PFL_HOLD_SLACK);
    CHECK(g.hold[1] == PFL_HELD, "let go within the slack: held");
}

static void test_stars(void)
{
    CHECK(pfl_stars(100) == 3 && pfl_stars(95) == 3 && pfl_stars(94) == 2 && pfl_stars(80) == 2, "3 and 2 stars");
    CHECK(pfl_stars(79) == 1 && pfl_stars(50) == 1 && pfl_stars(49) == 0 && pfl_stars(0) == 0, "1 and 0 stars");
}

static void test_calibration(void)
{
    pfl_game g;
    memset(&g, 0, sizeof g);
    g.err_n = 7;
    g.err_sum = 7 * 80;
    CHECK(pfl_calibrate(&g, 10) == 10, "too few presses: unchanged");
    g.err_n = 10;
    g.err_sum = 10 * 60;
    CHECK(pfl_calibrate(&g, 0) == 30, "half way: %d", pfl_calibrate(&g, 0));
    g.err_sum = -10 * 60;
    CHECK(pfl_calibrate(&g, 20) == -10, "early presses: %d", pfl_calibrate(&g, 20));
    g.err_sum = 10 * 400;
    CHECK(pfl_calibrate(&g, 100) == PFL_OFFSET_MAX, "kept within the limit");
    /* a player always 60 ms late, the offset taken off their presses: it settles near 60 */
    int off = 0;
    for (int k = 0; k < 8; k++) {
        pfl_load(&g, 0);
        for (int i = 0; i < g.n; i++) {
            pfl_press(&g, g.note[i].pitch, g.note[i].t + 60 - off);
            pfl_release(&g, i, g.note[i].t + g.note[i].len);
        }
        off = pfl_calibrate(&g, off);
    }
    CHECK(off >= 55 && off <= 60, "calibrated to %d", off);
}

/* a song played: perfectly (100%, 3 stars, every note), late by 100 ms (all good: 60%), not at all (0) */
static void play_all(pfl_game *g, const char *what)
{
    int c = pfl_check(g);
    CHECK(c == 0, "%s: chart check %d", what, c);
    pfl_game h = *g;
    CHECK(pfl_script(&h, "end"), "%s: script", what);
    CHECK(pfl_over(&h) && pfl_accuracy(&h) == 100 && pfl_stars(pfl_accuracy(&h)) == 3, "%s: perfect play %d%%",
          what, pfl_accuracy(&h));
    CHECK(h.perfect == g->n && h.miss == 0 && h.stray == 0 && h.best_combo == g->n, "%s: %d perfect of %d, combo %d",
          what, h.perfect, g->n, h.best_combo);
    for (int i = 0; i < g->n; i++)
        if (pfl_is_hold(&h, i)) CHECK(h.hold[i] == PFL_HELD, "%s: hold %d", what, i);
    h = *g;
    pfl_script(&h, "m999999");
    CHECK(pfl_over(&h) && pfl_accuracy(&h) == 0 && h.miss == g->n, "%s: nothing played", what);
    h = *g;                                     /* half the song perfect, then nothing */
    char buf[64];
    snprintf(buf, sizeof buf, "p%d m999999", pfl_end_ms(g) / 2);
    pfl_script(&h, buf);
    CHECK(pfl_over(&h) && pfl_accuracy(&h) > 20 && pfl_accuracy(&h) < 80, "%s: half: %d", what, pfl_accuracy(&h));
}

static void test_levels(void)
{
    CHECK(pfl_levels_count() == PFL_LEVEL_COUNT, "the count");
    int chords = 0, blacks_before = 0, longest = 0, shortest = 1 << 30;
    for (int l = 0; l < PFL_LEVELS; l++) {
        pfl_game g;
        pfl_load(&g, l);
        char what[32];
        snprintf(what, sizeof what, "song %d", l + 1);
        CHECK(g.n == pfl_songs[l].n && g.tune >= 0 && g.tune < 60, "%s: loaded", what);
        play_all(&g, what);
        int two = 0;
        for (int i = 1; i < g.n; i++) two |= g.note[i].t == g.note[i - 1].t;
        chords += two;
        CHECK(two == pfl_songs[l].chords, "%s: two notes at once only in the chords chapters", what);
        for (int i = 0; i < g.n && l < 20; i++) blacks_before += pfl_black(g.note[i].pitch);
        int e = pfl_end_ms(&g);
        if (e > longest) longest = e;
        if (e < shortest) shortest = e;
    }
    CHECK(blacks_before == 0, "the first two chapters: white keys only");
    CHECK(chords == 20, "two chapters of chords: %d", chords);
    CHECK(shortest >= 15000 && longest <= 60000, "songs from %d to %d ms", shortest, longest);
    printf("pianofall: %d songs, %d to %d s: ok\n", PFL_LEVELS, shortest / 1000, longest / 1000);
}

static void test_random(void)
{
    for (int v = 0; v < PFL_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 400; seed++) {
            pfl_game g;
            pfl_random(&g, v, seed);
            char what[48];
            snprintf(what, sizeof what, "random %d seed %u", v, (unsigned)seed);
            CHECK(g.n >= 16 && g.level == -1 && g.tune == -1, "%s: %d notes", what, g.n);
            int black = 0, two = 0;
            for (int i = 0; i < g.n; i++) black += pfl_black(g.note[i].pitch);
            for (int i = 1; i < g.n; i++) two |= g.note[i].t == g.note[i - 1].t;
            if (v == PFL_EASY) CHECK(!black && !two, "%s: easy: white keys, one at a time", what);
            if (v == PFL_MEDIUM) CHECK(!two, "%s: medium: one at a time", what);
            if (seed > 20) {
                play_all(&g, what);
                continue;
            }
            play_all(&g, what);
            pfl_game h;
            pfl_random(&h, v, seed);
            CHECK(h.n == g.n && !memcmp(h.note, g.note, sizeof(pfl_note) * (size_t)g.n), "%s: repeats", what);
        }
    int hard_two = 0;
    for (uint32_t seed = 1; seed <= 50; seed++) {
        pfl_game g;
        pfl_random(&g, PFL_HARD, seed);
        for (int i = 1; i < g.n; i++) hard_two += g.note[i].t == g.note[i - 1].t;
    }
    CHECK(hard_two > 50, "hard: two notes at once often (%d)", hard_two);
}

int main(void)
{
    test_judgement();
    test_stars();
    test_calibration();
    test_levels();
    test_random();
    if (fails) {
        printf("pianofall: %d failures\n", fails);
        return 1;
    }
    printf("pianofall: all passed\n");
    return 0;
}
