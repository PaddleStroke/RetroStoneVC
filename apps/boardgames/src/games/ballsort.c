/*
 * Ball Sort in RetroStone Board, for one player: glass tubes on walnut shelves, the house's glossy marbles in its
 * colours, each with its mark; a ball rises over its tube, glides over to another and drops in (the rules:
 * ballsort_rules.c and sorting.h; the screen, Water Sort's too: sorting_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/ballsort.h"
#include "games/sorting_ui.h"

SUI_GAME(ballsort_game, "ballsort", 421, 1, bs_new)
