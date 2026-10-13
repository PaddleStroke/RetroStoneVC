/*
 * Big Two's bot (src/games/bigtwo_bot.c): the climbing games' tests (tests/ai_shed.h), 4 and 3 players.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"
#include "games/bigtwo.h"

#define SHED_ID "bigtwo"
#define SHED_RULES bigt_rules
#define SHED_LIVE bigtwo_live
#define SHED_SHOW bigtwo_show
#define SHED_CHOOSE bigtwo_bot_choose
#define SHED_MINP 3
#define SHED_MAXP 4
#define SHED_USUAL 4
#include "ai_shed.h"

int main(int argc, char **argv) { return shed_main(argc, argv); }
