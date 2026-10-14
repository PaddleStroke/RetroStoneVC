/*
 * RetroStone Board's editions: the paid app (apps/android/app-board.conf: every game, every level) or RetroStone Board
 * Free (apps/android/app-boardfree.conf: the puzzles' first FREE_LEVELS levels and the FREE_GAMES, the other games
 * shown locked, a panel that opens the paid app's store page). Read from the .conf at build time
 * (tools/gen_edition.py: $(B)/gen/edition.c; make -f apps/boardgames/Makefile EDITION=boardfree).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef EDITION_H
#define EDITION_H

extern const char ed_app_name[];                /* the window's title: "RetroStone Board Free" */
extern const char ed_exe_name[];                /* the save folder's name (SDL_GetPrefPath): "RetroStoneBoardFree" */
extern const int ed_free;                       /* 1: the free edition (locked games, levels and the panel) */
extern const char ed_free_games[];              /* the games playable besides the puzzles, ids between spaces */
extern const int ed_free_levels;                /* the levels playable in each game of levels (0: all) */
extern const char ed_paid_name[], ed_paid_id[], ed_paid_url[];   /* the paid app: its name, package id, store page */
extern const char ed_iap_product[];             /* the in-app purchase that unlocks everything (apps/common/iap.h;
                                                 * "": none): the free set plays until it is bought */

#endif
