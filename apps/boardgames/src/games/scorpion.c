/*
 * Scorpion in RetroStone Board, for one player (the rules: scorpion_rules.c, scorpion.h; the table, the finger, the
 * hint and the win: solitaire_ui.h). The reserve at the top left, the four foundations at the top right, the seven
 * columns under them. A tap sends any face-up card, with all the cards on it, onto the card it follows (a king to an
 * empty column); a drag carries them where they are dropped; a tap on the reserve deals its three cards. A whole suit
 * in order goes off to a foundation by itself. The bar: the moves; the hint is the shell's tool.
 * Levels: 150 deals of our own in six chapters (scorpion_levels.h, made and proved winnable by tools/scorpion_gen.c),
 * the level screen's die for a deal at random; the stars count the hints and undos (solitaire_ui.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards.h"
#include "games/scorpion.h"
#include "games/scorpion_levels.h"
#include "games/solitaire_ui.h"
#include "shell.h"

static solrec G;
static solui U = {.R = &scp_rules, .rec = &G, .deck = DECK_FRENCH, .waste_fan = 1, .down_step = 0.13f,
                  .up_step = 0.36f, .pile = -1, .made_bar = -1};
static int dealing;

/* ---- the table --------------------------------------------------------------------------------------------------- */
static void layout(box b)
{
    U.b = b;
    soltable t = solui_table(&U, b, 7, 2.8f, 18);
    for (int i = 0; i < SCP_PILES; i++) {
        solspot *s = &U.at[i];
        memset(s, 0, sizeof *s);
        int col = i < SCP_FOUND ? i : i < SCP_STOCK ? i - SCP_FOUND + 3 : 0;
        s->x = t.x0 + col * t.pitch;
        s->y = i < SCP_FOUND ? t.cy : t.ty;
        s->fan = i < SCP_FOUND ? SF_COLUMN : SF_PILE;
        s->first = i >= SCP_FOUND && i < SCP_STOCK; /* a suit gone off shows its king */
        s->room = t.room;
    }
}

static void begin(void)
{
    solui_new(&U);
    dealing = 1;
}

static void start(int level)                    /* a designed level (scorpion_levels.h) */
{
    uint8_t deck[52];
    int v = solui_level(scp_levels, SCP_LEVELS, level, deck, 52);
    scp_deal_cards(&G, v, deck);
    begin();
}

static void start_random(int v)                 /* the level screen's die: a deal at random */
{
    (void)v;
    scp_new(&G, (uint32_t)ui_random(1 << 30));
    begin();
}

static int status(void) { return scp_won(&G.pos) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int busy(void) { return dealing || solui_busy(&U); }
static int undo(void) { dealing = 0; return solui_undo(&U); }

static int script(const char *s)
{
    dealing = 0;
    return solui_scripted(&U, scp_script(&G, s));
}

static void draw(void)
{
    if (dealing) {
        dealing = 0;
        solui_deal(&U, U.at[SCP_STOCK].x, U.at[SCP_STOCK].y, 18);
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
    solui_token(size, rgba, CARD(SUIT_SPADES, 8), CARD(SUIT_SPADES, 7));
}

/* the reserve's three backs, a king gone off; three columns, backs under runs of one suit, a card out of order */
static void thumb(int v, int size, uint8_t *rgba)
{
    (void)v;
    memset(rgba, 0, (size_t)size * size * 4);
    const float w = 0.25f, top = 0.18f;
    static const int c0[] = {CARD(SUIT_HEARTS, 9), CARD(SUIT_HEARTS, 8), CARD(SUIT_HEARTS, 7)},
                     c1[] = {CARD(SUIT_SPADES, RANK_KING), CARD(SUIT_SPADES, 12), CARD(SUIT_CLUBS, 4)},
                     c2[] = {CARD(SUIT_DIAMONDS, 6), CARD(SUIT_DIAMONDS, 5), CARD(SUIT_DIAMONDS, 4)};
    for (int k = 0; k < 3; k++)
        cards_paint_at(rgba, size, DECK_FRENCH, CARD_NONE, (0.13f + 0.03f * k) * size, top * size, (int)(w * size));
    cards_paint_at(rgba, size, DECK_FRENCH, CARD(SUIT_CLUBS, RANK_KING), 0.83f * size, top * size, (int)(w * size));
    solui_paint_column(rgba, size, DECK_FRENCH, 0.17f, 0.46f, w, 2, c0, 3, 0.03f, 0.13f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.5f, 0.46f, w, 0, c1, 3, 0.03f, 0.13f);
    solui_paint_column(rgba, size, DECK_FRENCH, 0.83f, 0.46f, w, 3, c2, 3, 0.03f, 0.13f);
}

static const game scorpion_game = {
    .id = "scorpion", .order = 342, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .view = VIEW_OPEN,
    .tools = 1, .level_stars = level_stars, .score = score,
    .levels = SCP_LEVELS, .chapters = SCP_CHAPTERS, .start_random = start_random,
};
REGISTER_GAME(scorpion_game)
