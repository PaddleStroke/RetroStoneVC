/*
 * Logic Gates: the player's progress (the stars won in each level, 0: not solved yet). Kept in memory for now; these
 * two functions are where it will be saved and read back (the shell's save API).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/logicgates.h"

static uint8_t kept[LG_LEVELS];

void lg_progress_load(uint8_t stars[LG_LEVELS]) { memcpy(stars, kept, LG_LEVELS); }

void lg_progress_save(const uint8_t stars[LG_LEVELS]) { memcpy(kept, stars, LG_LEVELS); }
