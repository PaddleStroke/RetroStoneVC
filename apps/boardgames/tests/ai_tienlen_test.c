/*
 * Tiến lên's bot (src/games/tienlen_bot.c): the climbing games' tests (tests/ai_shed.h), 2 to 4 players.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_cardgames.h"
#include "games/tienlen.h"

#define SHED_ID "tienlen"
#define SHED_RULES tln_rules
#define SHED_LIVE tienlen_live
#define SHED_SHOW tienlen_show
#define SHED_CHOOSE tienlen_bot_choose
#define SHED_MINP 2
#define SHED_MAXP 4
#define SHED_USUAL 4
#include "ai_shed.h"

int main(int argc, char **argv) { return shed_main(argc, argv); }
