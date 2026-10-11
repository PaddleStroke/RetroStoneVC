/*
 * Secret Code: the rules' test (src/games/secretcode_rules.c). The key pegs with repeated colours (the tricky cases),
 * the variants' sizes, codes with and without repeats, filling a row and undo, a win, a loss. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/secretcode.h"

static int checks, failed;

#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void pegs(const char *s, int8_t *out)
{
    for (int i = 0; s[i]; i++) out[i] = (int8_t)sc_colour(s[i]);
}

/* the key pegs a guess gets against a code (letters): black * 10 + white */
static int keys(const char *code, const char *guess)
{
    int8_t c[SC_MAXPEGS], g[SC_MAXPEGS];
    int b, w;
    pegs(code, c);
    pegs(guess, g);
    sc_score(c, g, (int)strlen(code), &b, &w);
    return b * 10 + w;
}

/* a new game of the variant with this code, then these guesses (each filled and checked); 0: one refused */
static int play(sc_game *g, int variant, const char *code, const char *guesses)
{
    int8_t c[SC_MAXPEGS];
    pegs(code, c);
    sc_new(g, variant, c);
    for (const char *s = guesses; *s;) {
        while (*s == ' ') s++;
        for (int i = 0; *s && *s != ' '; i++, s++)
            if (!sc_put(g, i, sc_colour(*s))) return 0;
        if (!sc_check(g)) return 0;
        while (*s == ' ') s++;
    }
    return 1;
}

static unsigned seed = 12345;
static int lcg(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (unsigned)n);
}

int main(void)
{
    sc_game g;

    /* the key pegs: blacks for the right holes, whites for the colours elsewhere, each code peg answering once */
    CHECK(keys("RYGB", "RYGB") == 40);
    CHECK(keys("RYGB", "BGYR") == 4);
    CHECK(keys("RYGB", "WKWK") == 0);
    CHECK(keys("RRGB", "RRRR") == 20);          /* the extra reds get nothing */
    CHECK(keys("RYGB", "RRRR") == 10);
    CHECK(keys("RRGB", "GGRR") == 3);           /* two reds and one green, all elsewhere */
    CHECK(keys("RRYY", "YYRR") == 4);
    CHECK(keys("RRYG", "RYRY") == 12);          /* one red right; one red and one yellow elsewhere */
    CHECK(keys("RGGG", "GGRR") == 12);
    CHECK(keys("BBBB", "BBBB") == 40);
    CHECK(keys("BBBB", "BRRR") == 10);
    CHECK(keys("RYYY", "YRRR") == 2);           /* one red and one yellow elsewhere, not more */
    CHECK(keys("WKWK", "KWKW") == 4);
    CHECK(keys("RYGBW", "WRYGB") == 5);
    CHECK(keys("RRRRY", "YRRRR") == 32);
    CHECK(keys("PSPSK", "SPSPK") == 14);
    CHECK(keys("RYGBW", "KPSKP") == 0);

    /* the letters */
    CHECK(sc_colour('R') == 0 && sc_colour('s') == 7 && sc_colour('K') == 5 && sc_colour('X') == -1);
    CHECK(sc_colour(0) == -1);

    /* the variants' sizes: gentle and classic 4 pegs, 6 colours, 10 rows; harder 5, 8, 12 */
    CHECK(sc_variants[0].pegs == 4 && sc_variants[0].colours == 6 && sc_variants[0].rows == 10 &&
          !sc_variants[0].repeats);
    CHECK(sc_variants[1].pegs == 4 && sc_variants[1].colours == 6 && sc_variants[1].rows == 10 &&
          sc_variants[1].repeats);
    CHECK(sc_variants[2].pegs == 5 && sc_variants[2].colours == 8 && sc_variants[2].rows == 12 &&
          sc_variants[2].repeats);

    /* the codes: in range; gentle never repeats a colour, the others sometimes do */
    for (int v = 0; v < SC_VARIANTS; v++) {
        int bad = 0, repeats = 0, seen[SC_MAXCOLOURS] = {0};
        for (int k = 0; k < 2000; k++) {
            int8_t c[SC_MAXPEGS];
            int twice = 0;
            sc_make_code(v, lcg, c);
            for (int i = 0; i < sc_variants[v].pegs; i++) {
                if (c[i] < 0 || c[i] >= sc_variants[v].colours) bad++;
                else seen[c[i]]++;
                for (int j = 0; j < i; j++) twice |= c[i] == c[j];
            }
            repeats += twice;
        }
        CHECK(!bad);
        CHECK(sc_variants[v].repeats ? repeats > 0 : repeats == 0);
        for (int c = 0; c < sc_variants[v].colours; c++) CHECK(seen[c] > 0);   /* every colour comes up */
    }

    /* the start */
    CHECK(play(&g, 1, "RRGB", "") && sc_status(&g) == SC_PLAY && sc_left(&g) == 10 && g.n == 0 && !sc_full(&g));
    CHECK(play(&g, 2, "RYGBS", "") && sc_left(&g) == 12);

    /* filling a row: colours and holes of the variant only; a guess may repeat colours even in gentle */
    CHECK(play(&g, 1, "RRGB", ""));
    CHECK(!sc_put(&g, 4, 0) && !sc_put(&g, -1, 0) && !sc_put(&g, 0, 6) && !sc_put(&g, 0, -1));
    CHECK(play(&g, 2, "RYGBS", ""));
    CHECK(sc_put(&g, 4, 7) && g.guess[0][4] == 7);
    CHECK(play(&g, 0, "RYGB", "RRRR") && g.black[0] == 1 && g.white[0] == 0);

    /* not full: no check; undo takes the last peg placed back, and what it replaced */
    CHECK(play(&g, 1, "RRGB", ""));
    CHECK(sc_put(&g, 0, 2) && sc_put(&g, 1, 3) && sc_put(&g, 1, 4) && !sc_full(&g) && !sc_check(&g));
    CHECK(sc_undo(&g) == 1 && g.guess[0][1] == 3);
    CHECK(sc_undo(&g) == 1 && g.guess[0][1] == -1);
    CHECK(sc_undo(&g) == 0 && g.guess[0][0] == -1 && sc_undo(&g) == -1);
    CHECK(sc_put(&g, 0, 0) && sc_put(&g, 0, 0) && g.nh == 1);   /* the same colour again: nothing new */

    /* a checked guess stays: undo has nothing to take back */
    CHECK(play(&g, 1, "RRGB", "GGRR") && g.n == 1 && g.black[0] == 0 && g.white[0] == 3 && sc_left(&g) == 9);
    CHECK(sc_undo(&g) == -1 && g.n == 1);
    CHECK(sc_put(&g, 0, 1) && sc_undo(&g) == 0 && g.n == 1 && g.guess[0][0] == 2);

    /* the history is long, the oldest forgotten */
    CHECK(play(&g, 1, "RRGB", ""));
    for (int k = 0; k < SC_HIST + 10; k++) sc_put(&g, 0, k % 2);
    CHECK(g.nh == SC_HIST);
    int undone = 0;
    while (sc_undo(&g) >= 0) undone++;
    CHECK(undone == SC_HIST && g.guess[0][0] >= 0);

    /* a win on the third guess: solved, nothing more */
    CHECK(play(&g, 1, "RRGB", "RYGB RRGK RRGB") && sc_status(&g) == SC_SOLVED && g.n == 3);
    CHECK(g.black[0] == 3 && g.white[0] == 0 && g.black[1] == 3 && g.black[2] == 4 && g.white[2] == 0);
    CHECK(!sc_put(&g, 0, 0) && !sc_full(&g) && !sc_check(&g) && sc_undo(&g) == -1);

    /* a win on the last row */
    CHECK(play(&g, 1, "KKKK", "RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR KKKK") && sc_status(&g) == SC_SOLVED);

    /* a loss: ten guesses, none right */
    CHECK(play(&g, 1, "KKKK", "RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR") && sc_status(&g) == SC_PLAY &&
          sc_left(&g) == 1);
    CHECK(sc_put(&g, 0, 5) && sc_put(&g, 1, 5) && sc_put(&g, 2, 5) && sc_put(&g, 3, 4) && sc_check(&g));
    CHECK(sc_status(&g) == SC_FAILED && sc_left(&g) == 0 && g.black[9] == 3 && !sc_put(&g, 0, 0));

    /* harder: twelve rows of five */
    CHECK(play(&g, 2, "PSPSK",
               "RRRRR RRRRR RRRRR RRRRR RRRRR RRRRR RRRRR RRRRR RRRRR RRRRR RRRRR SPSPK") &&
          sc_status(&g) == SC_FAILED && g.black[11] == 1 && g.white[11] == 4);
    CHECK(play(&g, 2, "PSPSK", "PSPSK") && sc_status(&g) == SC_SOLVED);
    CHECK(!play(&g, 1, "RRGB", "RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR RRRR"));   /* an 11th guess */

    printf("secretcode: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
