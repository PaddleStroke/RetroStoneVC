/*
 * Jigsaw Sudoku in RetroStone Board, for one player: the house plank ruled in 9 x 9 cells, nine irregular regions
 * outlined in thick lines, each in its own tone of wood, the clues in dark ink on cells stained darker and the
 * player's digits in blue ink, a row of wooden keys 1..9 beside the board (the rules: jigsawsudoku_rules.c and
 * sudokuvar.h; the screen: sudokuvar_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, jigsawsudoku_levels.h: singles with many clues
 * first, a digit tried and followed last), or its die tab a puzzle made at random at one of four levels.
 */
#define SVU_STYLE SVU_JIGSAW
#include "games/sudokuvar_ui.h"

#include "games/jigsawsudoku.h"
#include "games/jigsawsudoku_levels.h"

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!jsu_decode(&G, jsu_levels[level < 0 || level >= JSU_LEVELS ? 0 : level])) jsu_new(&G, SV_EASY, 1);
    begin();
}

static void start_random(int variant)           /* the die tab: a puzzle made at random at the variant's level */
{
    jsu_new(&G, variant < 0 || variant >= JSU_VARIANTS ? SV_EASY : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

/* ---- the menu's and the picker's cards: a level of the variant's level (its regions and clues) and as many ebony
 * stones as its level; the menu's a puzzle half done ------------------------------------------------------------ */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int LEVEL[JSU_VARIANTS] = {30, 60, 90, 132};
    sv_shape s;
    uint8_t p[SV_CELLS], sol[SV_CELLS], part[SV_CELLS] = {0};
    if (!jsu_shape(&s, p, jsu_levels[variant < 0 ? 40 : LEVEL[variant % JSU_VARIANTS]])) return;
    if (variant < 0) {
        if (sv_grade(&s, p, SV_HARD, sol, NULL, 0) >= SV_EASY)
            for (int c = 0; c < SV_CELLS; c++)
                if (!p[c] && c % 7 < 3 && c < 50) part[c] = sol[c];
        svu_picture(rgba, size, size, -1, -1, &s, p, part);
        return;
    }
    int side = (int)(0.84f * size);
    svu_picture(rgba, size, side, -1, 0, &s, p, NULL);
    bench_stones(rgba, size, side, variant % JSU_VARIANTS);
}

/* a chapter's tab: the regions of its middle level, each in its own wood, its clues' cells inked: fewer as the
 * chapters go */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= JSU_CHAPTERS ? JSU_CHAPTERS - 1 : chapter;
    sv_shape s;
    uint8_t p[SV_CELLS];
    if (jsu_shape(&s, p, jsu_levels[ch * (JSU_LEVELS / JSU_CHAPTERS) + JSU_LEVELS / JSU_CHAPTERS / 2]))
        svu_icon(rgba, size, &s, p);
}

static const game jigsawsudoku_game = {
    .id = "jigsawsudoku", .order = 352, .variants = JSU_VARIANTS, .thumb = thumb, .start = start,
    SVU_GAME_FIELDS,
    .levels = JSU_LEVELS, .chapters = JSU_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
};
REGISTER_GAME(jigsawsudoku_game)
