/*
 * Diagonal Sudoku in RetroStone Board, for one player: the house plank ruled in 9 x 9 cells, the boxes' thick lines,
 * the two long diagonals' cells in honey wood, the clues in dark ink on cells stained darker and the player's digits
 * in blue ink, a row of wooden keys 1..9 beside the board (the rules: diagonalsudoku_rules.c and sudokuvar.h; the
 * screen: sudokuvar_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's level screen chooses a level (150 in six chapters, diagonalsudoku_levels.h: singles with many clues
 * first, a digit tried and followed last), or its die tab a puzzle made at random at one of four levels.
 */
#define SVU_STYLE SVU_DIAGONAL
#include "games/sudokuvar_ui.h"

#include "games/diagonalsudoku.h"
#include "games/diagonalsudoku_levels.h"

static void start(int level)                    /* a level of the shell's level screen */
{
    if (!dsu_decode(&G, dsu_levels[level < 0 || level >= DSU_LEVELS ? 0 : level])) dsu_new(&G, SV_EASY, 1);
    begin();
}

static void start_random(int variant)           /* the die tab: a puzzle made at random at the variant's level */
{
    dsu_new(&G, variant < 0 || variant >= DSU_VARIANTS ? SV_EASY : variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

/* ---- the menu's and the picker's cards: a level of the variant's level (its clues) and as many ebony stones as its
 * level; the menu's a puzzle half done ---------------------------------------------------------------------------- */
static void thumb(int variant, int size, uint8_t *rgba)
{
    static const int LEVEL[DSU_VARIANTS] = {30, 60, 90, 132};
    sv_shape s;
    uint8_t p[SV_CELLS], sol[SV_CELLS], part[SV_CELLS] = {0};
    dsu_shape(&s);
    if (!sv_clues_decode(p, dsu_levels[variant < 0 ? 40 : LEVEL[variant % DSU_VARIANTS]])) return;
    if (variant < 0) {
        if (sv_grade(&s, p, SV_HARD, sol, NULL, 0) >= SV_EASY)
            for (int c = 0; c < SV_CELLS; c++)
                if (!p[c] && c % 7 < 3 && c < 50) part[c] = sol[c];
        svu_picture(rgba, size, size, -1, -1, &s, p, part);
        return;
    }
    int side = (int)(0.84f * size);
    svu_picture(rgba, size, side, -1, 0, &s, p, NULL);
    bench_stones(rgba, size, side, variant % DSU_VARIANTS);
}

/* a chapter's tab: the board of its middle level, the diagonals in honey wood, its clues' cells inked: fewer as the
 * chapters go */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= DSU_CHAPTERS ? DSU_CHAPTERS - 1 : chapter;
    sv_shape s;
    uint8_t p[SV_CELLS];
    dsu_shape(&s);
    if (sv_clues_decode(p, dsu_levels[ch * (DSU_LEVELS / DSU_CHAPTERS) + DSU_LEVELS / DSU_CHAPTERS / 2]))
        svu_icon(rgba, size, &s, p);
}

static const game diagonalsudoku_game = {
    .id = "diagonalsudoku", .order = 353, .variants = DSU_VARIANTS, .thumb = thumb, .start = start,
    SVU_GAME_FIELDS,
    .levels = DSU_LEVELS, .chapters = DSU_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
};
REGISTER_GAME(diagonalsudoku_game)
