/*
 * Killer Sudoku in RetroStone Board, for one player: the house plank ruled in 9 x 9 cells, the boxes' thick lines,
 * the cages' dashed lines with their sums in small dark digits, the player's digits in blue ink, a row of wooden keys
 * 1..9 beside the board (the rules: killersudoku_rules.c and sudokuvar.h; the screen: sudokuvar_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, killersudoku_levels.h: small cages and the cages'
 * own sums first, a digit tried and followed last), or its die tab a puzzle made at random at one of four levels.
 */
#define SVU_STYLE SVU_KILLER
#include "games/sudokuvar_ui.h"

#include "games/killersudoku.h"
#include "games/killersudoku_levels.h"

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!ksu_decode(&G, ksu_levels[level < 0 || level >= KSU_LEVELS ? 0 : level])) ksu_new(&G, SV_EASY, 1);
    begin();
}

static void start_random(int variant)           /* the die tab: a puzzle made at random at the variant's level */
{
    ksu_new(&G, variant < 0 || variant >= KSU_VARIANTS ? SV_EASY : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

/* ---- the menu's and the picker's cards: a level's cages (one of the variant's level) and as many ebony stones as
 * its level; the menu's a puzzle half done ---------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int LEVEL[KSU_VARIANTS] = {8, 60, 90, 132};
    sv_shape s;
    uint8_t sol[SV_CELLS], part[SV_CELLS] = {0};
    if (!ksu_shape(&s, ksu_levels[variant < 0 ? 30 : LEVEL[variant % KSU_VARIANTS]])) return;
    if (variant < 0) {
        if (sv_grade(&s, NULL, SV_MEDIUM, sol, NULL, 0) >= SV_EASY)
            for (int c = 0; c < SV_CELLS; c++)
                if (c % 7 < 3 && c < 50) part[c] = sol[c];
        svu_picture(rgba, size, size, -1, -1, &s, NULL, part);
        return;
    }
    int side = (int)(0.84f * size);
    svu_picture(rgba, size, side, -1, 0, &s, NULL, NULL);
    bench_stones(rgba, size, side, variant % KSU_VARIANTS);
}

/* a chapter's tab: the cages of its middle level, each in its own wood: larger as the chapters go */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= KSU_CHAPTERS ? KSU_CHAPTERS - 1 : chapter;
    sv_shape s;
    if (ksu_shape(&s, ksu_levels[ch * (KSU_LEVELS / KSU_CHAPTERS) + KSU_LEVELS / KSU_CHAPTERS / 2]))
        svu_icon(rgba, size, &s, NULL);
}

static const game killersudoku_game = {
    .id = "killersudoku", .order = 351, .variants = KSU_VARIANTS, .thumb = thumb, .start = start,
    SVU_GAME_FIELDS,
    .levels = KSU_LEVELS, .chapters = KSU_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
};
REGISTER_GAME(killersudoku_game)
