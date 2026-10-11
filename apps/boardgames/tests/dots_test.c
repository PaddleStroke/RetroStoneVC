/*
 * Dots and boxes: the rules' test (the extra turn, two boxes at once, the end and the count, a draw, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>

#include "games/dots.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static dgame G;

static const dpos *pos(void) { return dots_pos(&G); }

/* the board: the lines of each size, the notation, a box's sides */
static void test_board(void)
{
    CHECK(dots_lines(3) == 24 && dots_lines(5) == 60 && dots_lines(7) == 112);
    int len, s[4];
    CHECK(dots_parse(3, "h1,1", &len) == 0 && len == 4);
    CHECK(dots_parse(3, "v4,3", &len) == dots_v(3, 3, 2));
    CHECK(dots_parse(3, "h4,1", &len) < 0 && dots_parse(3, "v1,4", &len) < 0 && dots_parse(3, "x1,1", &len) < 0);
    dots_sides(3, 4, s);                        /* the middle box */
    CHECK(s[0] == dots_h(3, 1, 1) && s[1] == dots_h(3, 1, 2) && s[2] == dots_v(3, 1, 1) && s[3] == dots_v(3, 2, 1));
    int x0, y0, x1, y1, ok = 1;
    for (int l = 0; l < dots_lines(5); l++) {
        dots_ends(5, l, &x0, &y0, &x1, &y1);
        ok &= (y1 == y0 ? dots_h(5, x0, y0) : dots_v(5, x0, y0)) == l && x1 + y1 == x0 + y0 + 1;
    }
    CHECK(ok);
}

/* one box: no box, the turn passes; the fourth side claims it and the player draws again; the end */
static void test_one_box(void)
{
    dots_new(&G, 1);
    CHECK(dots_script(&G, "h1,1") && pos()->side == 1);
    CHECK(dots_script(&G, "h1,2 v1,1") && pos()->side == 1 && pos()->boxes[0] + pos()->boxes[1] == 0);
    CHECK(dots_play(&G, dots_v(1, 1, 0)) == 1);
    CHECK(pos()->side == 1 && pos()->box[0] == 1 && pos()->boxes[1] == 1 && dots_status(&G) == DS_WIN1);
    CHECK(dots_play(&G, dots_h(1, 0, 0)) < 0 && G.ply == 4);   /* a line drawn already */
    /* undo through the claim, all the way */
    CHECK(dots_undo(&G) && pos()->box[0] < 0 && pos()->boxes[1] == 0 && pos()->side == 1 && pos()->drawn == 3);
    CHECK(dots_status(&G) == DS_PLAY);
    CHECK(dots_undo(&G) && dots_undo(&G) && dots_undo(&G) && !dots_undo(&G) && pos()->side == 0 && !pos()->drawn);
}

/* 2 x 2 boxes: two boxes with one line, then the count at the end; a draw */
static void test_two_at_once(void)
{
    dots_new(&G, 2);
    CHECK(dots_script(&G, "h1,1 h2,1 h1,2 h2,2 v1,1 v3,1") && pos()->side == 0);
    CHECK(dots_play(&G, dots_v(2, 1, 0)) == 2 && pos()->side == 0 && pos()->boxes[0] == 2);
    /* ivory draws again; ebony closes the last two boxes one after the other */
    CHECK(dots_script(&G, "h1,3 v1,2 h2,3") && pos()->side == 1);
    CHECK(dots_play(&G, dots_v(2, 1, 1)) == 1 && pos()->side == 1 && dots_status(&G) == DS_PLAY);
    CHECK(dots_play(&G, dots_v(2, 2, 1)) == 1 && pos()->boxes[0] == 2 && pos()->boxes[1] == 2);
    CHECK(pos()->drawn == 12 && dots_status(&G) == DS_DRAW);
    /* the same game, ebony gives the last two to ivory instead: 4 to 0 */
    CHECK(dots_undo(&G) && dots_undo(&G) && pos()->side == 1);
    CHECK(dots_script(&G, "v3,2") && pos()->side == 0);
    CHECK(dots_play(&G, dots_v(2, 1, 1)) == 2 && pos()->boxes[0] == 4 && dots_status(&G) == DS_WIN0);
}

/* a whole 3 x 3 game, every line in order: the boxes add up, the game ends with the last line */
static void test_full(void)
{
    dots_new(&G, 3);
    int claimed = 0;
    for (int l = 0; l < dots_lines(3); l++) {
        CHECK(dots_status(&G) == DS_PLAY);
        claimed += dots_play(&G, l);
    }
    CHECK(claimed == 9 && pos()->boxes[0] + pos()->boxes[1] == 9 && dots_status(&G) != DS_PLAY);
    CHECK(dots_status(&G) == (pos()->boxes[0] > pos()->boxes[1] ? DS_WIN0 : DS_WIN1));
    CHECK(!dots_script(&G, "h1,1"));
}

int main(void)
{
    test_board();
    test_one_box();
    test_two_at_once();
    test_full();
    printf("dots: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
