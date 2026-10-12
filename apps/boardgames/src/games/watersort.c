/*
 * Water Sort in RetroStone Board, for one player: glass tubes on walnut shelves, liquids in the house's colours, each
 * layer with its mark; a tube flies over to another, tilts on its lip and pours, the stream running into the other
 * (the rules: watersort_rules.c and sorting.h; the screen, Ball Sort's too: sorting_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/sorting_ui.h"
#include "games/watersort.h"

SUI_GAME(watersort_game, "watersort", 420, 0, ws_new, ws_level, WS_LEVELS, WS_CHAPTERS)
