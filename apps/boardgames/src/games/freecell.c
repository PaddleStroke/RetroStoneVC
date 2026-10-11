/*
 * FreeCell in RetroStone Board, for one player (the rules: freecell_rules.c, freecell.h; the table, the finger, the
 * hint and the win: solitaire_ui.h). The four free cells and the four foundations in a row at the top, the eight
 * columns under them. A tap sends a card or a run to its best place (a foundation, a column, an empty column, a free
 * cell), a quick second tap only to a foundation; a drag carries a run where it is dropped. A new game deals a
 * numbered deal at random (1 to 32000); the bar shows its number by a small card back, then the moves and the hint.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/freecell.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &fc_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.4f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    float w = floorf(fminf(b.w / 8.9f, b.h / 6.6f)), pitch = fminf(1.14f * w, (b.w - w) / 7);
    float x0 = b.x + b.w / 2 - 3.5f * pitch, ty = b.y + 0.06f * w + 0.7f * w, cy = ty + 1.62f * w;
    float gap = fminf(0.3f * w, (b.w - w - 7 * pitch) / 2);   /* the cells and the foundations a little apart */
    U.w = w;
    for (int i = 0; i < FC_PILES; i++) {
        solspot *s = &U.at[i];
        int col = i < FC_CELL ? i : i - FC_CELL;
        s->x = x0 + col * pitch + (i >= FC_CELL ? (col < 4 ? -gap : gap) : 0);
        s->y = i < FC_CELL ? cy : ty;
        s->fan = i < FC_CELL ? SF_COLUMN : SF_PILE;
        s->room = b.y + b.h - 0.72f * w - cy;
        s->first = 0;
    }
}

static void start(int v)
{
    (void)v;
    fc_new(&G, 1 + ui_random(FC_DEALS));
    solui_calm(&U);
    U.won = 0;
    dealing = 1;
}

static int status(void) { return fc_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }

static int undo(void)
{
    if (!sol_undo(&G)) return 0;
    solui_calm(&U);
    U.won = 0;
    dealing = 0;
    ui_handoff(0);
    return 1;
}

static int script(const char *s)
{
    int ok = fc_script(&G, s);
    solui_calm(&U);
    dealing = 0;
    solui_check_win(&U, ui_ms());
    return ok;
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.b.x + U.b.w / 2, U.at[FC_CELL].y, 22);
    }
    solui_draw(&U);
}

static void press(float x, float y) { solui_press(&U, x, y); }
static void motion(float x, float y) { solui_motion(&U, x, y); }
static void release(float x, float y) { solui_release(&U, x, y); }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    solui_bar(&U, b, (int)G.seed);
}

static int bar_press(int player, float x, float y)
{
    (void)player;
    return solui_bar_press(&U, x, y);
}

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    solui_token(size, rgba, CARD(SUIT_CLUBS, RANK_QUEEN), CARD(SUIT_DIAMONDS, RANK_JACK));
}

/* two free cells, one of them holding a card; three columns of runs under them */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.25f, top = 0.2f;
    static const int c0[] = {CARD(SUIT_SPADES, 12), CARD(SUIT_HEARTS, 11)},
                     c1[] = {CARD(SUIT_DIAMONDS, 13), CARD(SUIT_CLUBS, 12)},
                     c2[] = {CARD(SUIT_CLUBS, 7), CARD(SUIT_HEARTS, 6)};
    int pw = (int)(w * size), ph = (int)(pw * 1.4f);
    uint8_t *place = malloc((size_t)pw * ph * 4);
    const uint8_t edge[4] = {246, 238, 220, 60};
    if (place) {                                /* the empty cell: a faint place */
        art_roundrect(pw, ph, 0.075f * pw, edge, place);
        art_blend(rgba, size, size, place, pw, ph, (int)(0.17f * size - pw / 2.0f), (int)(top * size - ph / 2.0f));
        free(place);
    }
    solui_paint(rgba, size, DECK_FRENCH, CARD(SUIT_DIAMONDS, 4), 0.5f * size, top * size, pw);
    solui_paint(rgba, size, DECK_FRENCH, CARD(SUIT_CLUBS, RANK_ACE), 0.83f * size, top * size, pw);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.17f, 0.6f, w, 0, c0, 2, 0, 0.17f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.5f, 0.6f, w, 0, c1, 2, 0, 0.17f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.83f, 0.6f, w, 0, c2, 2, 0, 0.17f);
}

static const game freecell_game = {
    .id = "freecell", .order = 330, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .bar_press = bar_press, .players = 1u << 1, .view = VIEW_OPEN,
};
REGISTER_GAME(freecell_game)
