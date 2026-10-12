/*
 * Ball Sort's rules: one ball a move, undo, the extra tube, solved and stuck, the generator over hundreds of seeds a
 * size (every puzzle solved by its own way; the time taken), the search against a breadth-first search, whole games
 * from a seed and the hints played to the end, the notation (sorting_check.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/ballsort.h"
#include "sorting_check.h"

int main(void)
{
    sc_moves(1);
    srt_game g;
    bs_new(&g, 0, 3, NULL);
    srt_game h = g;
    CHECK(bs_move(&h, 0, 4) == 1 && h.p.h[4] == 1 && h.p.h[0] == 3);   /* one ball, whatever the run */
    CHECK(!bs_move(&h, 5, 0));
    sc_no_way(1);
    sc_shortest(bs_new, 1);
    sc_generator(bs_new, 1);
    sc_games(bs_new, 1);
    sc_script(bs_new, 1);
    h = g;
    CHECK(bs_script(&h, "=2") && h.moves == 2);
    sc_levels(bs_level, bs_levels, bs_level_par, BS_LEVELS, BS_CHAPTERS, 1);
    if (fails) { printf("ballsort: %d checks failed\n", fails); return 1; }
    printf("ballsort: all checks passed\n");
    return 0;
}
