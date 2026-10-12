/*
 * Water Sort's rules: the pours (the whole top run, as far as it fits), undo, the extra tube, solved and stuck, the
 * generator over hundreds of seeds a size (every puzzle solved by its own way; the time taken), the search against a
 * breadth-first search, whole games from a seed and the hints played to the end, the notation (sorting_check.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/watersort.h"
#include "sorting_check.h"

int main(void)
{
    sc_moves(0);
    srt_game g;
    ws_new(&g, 0, 3, NULL);
    srt_game h = g;
    int run = srt_run(&g.p, 0);
    CHECK(ws_pour(&h, 0, 4) == run && h.p.h[4] == run);   /* into an empty tube: the whole run */
    CHECK(!ws_pour(&h, 5, 0));
    sc_no_way(0);
    sc_shortest(ws_new, 0);
    sc_generator(ws_new, 0);
    sc_games(ws_new, 0);
    sc_script(ws_new, 0);
    h = g;
    CHECK(ws_script(&h, "=2") && h.moves == 2);
    if (fails) { printf("watersort: %d checks failed\n", fails); return 1; }
    printf("watersort: all checks passed\n");
    return 0;
}
