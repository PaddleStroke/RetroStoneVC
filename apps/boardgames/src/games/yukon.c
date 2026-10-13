/*
 * Yukon in RetroStone Board, for one player (the rules: yukon_rules.c, yukon.h; the table, the finger, the hint and
 * the win: solitaire_ui.h). The four foundations at the top right, the seven columns under them; no stock. A tap
 * sends a card, or any face-up card with all the cards on it, to its best place (a foundation first), a quick second
 * tap only to a foundation; a drag carries them where they are dropped. The bar: the moves; the hint is the shell's
 * tool.
 * Levels: 150 deals of our own in six chapters (yukon_levels.h, made and proved winnable by tools/yukon_gen.c), the
 * level screen's die for a deal at random; the stars count the hints and undos (solitaire_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/solitaire_ui.h"
#include "games/yukon.h"
#include "games/yukon_levels.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &yk_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.36f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    soltable t = solui_table(&U, b, 7, 2.8f, 17);
    for (int i = 0; i < YK_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        s->x = t.x0 + (i < YK_FOUND ? i : i - YK_FOUND + 3) * t.pitch;
        s->y = i < YK_FOUND ? t.cy : t.ty;
        s->fan = i < YK_FOUND ? SF_COLUMN : SF_PILE;
        s->room = t.room;
    }
}

static void begin(void)
{
    solui_new(&U);
    dealing = 1;
}

static void start(int level)                    /* a designed level (yukon_levels.h) */
{
    uint8_t deck[52];
    int v = solui_level(yk_levels, YK_LEVELS, level, deck, 52);
    yk_deal_cards(&G, v, deck);
    begin();
}

static void start_random(int v)                 /* the level screen's die: a deal at random */
{
    (void)v;
    yk_new(&G, (uint32_t)ui_random(1 << 30));
    begin();
}

static int status(void) { return yk_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, yk_script(&G, s));
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.b.x + U.b.w / 2, U.at[YK_FOUND].y, 18);
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
    solui_bar(&U, b, 0);
}

static int level_stars(void) { return solui_stars(&U); }
static int score(void) { return G.moves; }

static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    solui_token(size, rgba, CARD(SUIT_CLUBS, 9), CARD(SUIT_HEARTS, RANK_KING));
}

/* three tall columns, backs under face-up cards out of order (a king grabbed with them), an ace up: no stock */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.25f, top = 0.18f;
    static const int c0[] = {CARD(SUIT_SPADES, 13), CARD(SUIT_HEARTS, 4), CARD(SUIT_CLUBS, 10)},
                     c1[] = {CARD(SUIT_DIAMONDS, 9), CARD(SUIT_SPADES, 2), CARD(SUIT_HEARTS, 12)},
                     c2[] = {CARD(SUIT_CLUBS, 6), CARD(SUIT_DIAMONDS, RANK_KING), CARD(SUIT_SPADES, 7)};
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_HEARTS, RANK_ACE), 0.5f * size, top * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_SPADES, 2), 0.83f * size, top * size, (int)(w * size));
    solui_paint_column(rgba, size, DECK_FRENCH, 0.17f, 0.46f, w, 1, c0, 3, 0.03f, 0.13f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.5f, 0.46f, w, 3, c1, 3, 0.03f, 0.13f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.83f, 0.46f, w, 2, c2, 3, 0.03f, 0.13f);
}

static const game yukon_game = {
    .id = "yukon", .order = 342, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
    .levels = YK_LEVELS, .chapters = YK_CHAPTERS, .start_random = start_random,
};
REGISTER_GAME(yukon_game)
