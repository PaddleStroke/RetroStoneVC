/*
 * War (la bataille) in RetroStone Board: 2 to 4 players round one device lying on the table (the rules: war_rules.c,
 * war.h). Nothing is secret and nobody knows the order (VIEW_OPEN; the view never turns: player 0 sits at the
 * bottom, the others round the table). Each player taps their pile to turn its top card, which flies in front of it;
 * once all are turned, the highest card lights up and every card turned flies to its owner's pile. A tie is a war:
 * the tied cards glow red and the tied players tap again (a card face down, a card face up). The short game counts
 * its battles down in the middle.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cards_table.h"
#include "games/war.h"

#define SLIDE_MS 280
#define PAUSE_MS 850                            /* the cards turned stay a moment before they fly to the winner */
#define FLY_MS 420

static wgame G;
static ct_moves MV = {.rest = 300};
enum { H_PILE = 1, H_SPOT };
static struct { int q; Uint32 from, to; float x, y; } win;   /* the winning card's gold light */

/* ---- the table: each player's pile and the place in front of it ------------------------------------------------- */
static struct {
    box b;
    float wc, px[WAR_MAXP], py[WAR_MAXP], sx[WAR_MAXP], sy[WAR_MAXP], dx[WAR_MAXP], dy[WAR_MAXP];
} T;

static void layout(box b)
{
    static const float at[3][WAR_MAXP][4] = {   /* pile x, y, place x, y (fractions of the table) */
        {{0.5f, 0.87f, 0.5f, 0.63f}, {0.5f, 0.13f, 0.5f, 0.37f}},
        {{0.5f, 0.86f, 0.5f, 0.64f}, {0.2f, 0.14f, 0.3f, 0.38f}, {0.8f, 0.14f, 0.7f, 0.38f}},
        {{0.5f, 0.86f, 0.5f, 0.66f}, {0.13f, 0.5f, 0.33f, 0.5f}, {0.5f, 0.14f, 0.5f, 0.34f},
         {0.87f, 0.5f, 0.67f, 0.5f}},
    };
    const wpos *p = &G.pos;
    int k = p->players - 2;
    T.b = b;
    float fh = k == 2 ? 0.17f : k ? 0.19f : 0.21f, fw = k == 2 ? 0.17f : k ? 0.2f : 0.24f;
    T.wc = floorf(fminf(b.h * fh / 1.4f, b.w * fw));
    for (int q = 0; q < p->players; q++) {
        T.px[q] = b.x + at[k][q][0] * b.w;
        T.py[q] = b.y + at[k][q][1] * b.h;
        T.sx[q] = b.x + at[k][q][2] * b.w;
        T.sy[q] = b.y + at[k][q][3] * b.h;
        int side = fabsf(T.px[q] - T.sx[q]) > fabsf(T.py[q] - T.sy[q]);   /* a war's cards spread across */
        T.dx[q] = side ? 0 : 0.24f * T.wc;
        T.dy[q] = side ? 0.3f * T.wc : 0;
    }
}

static void spot(int q, int i, float *x, float *y)
{
    *x = T.sx[q] + i * T.dx[q];
    *y = T.sy[q] + i * T.dy[q];
}

/* ---- the moving cards -------------------------------------------------------------------------------------------- */
static int busy(void) { return ct_busy(&MV) || (win.to && ui_ms() < win.to); }

/* a tap: the card(s) from the pile to the place; a battle won: everything turned flies to the winner */
static void animate(const wpos *b, int q)
{
    const wpos *p = &G.pos;
    int down = b->war && b->n[q] >= 2, k = down ? 2 : 1, idx[2] = {-1, -1};
    for (int j = 0; j < k; j++) {
        int i = b->nt[q] + j, card = b->pile[q][b->n[q] - 1 - j], up = j == k - 1;
        float x, y;
        spot(q, i, &x, &y);
        idx[j] = ct_slide(&MV, DECK_FRENCH, card, T.px[q], T.py[q], T.wc, 0, 0, x, y, T.wc, 0, up, SLIDE_MS, 90 * j,
                          H_SPOT, q, i, up ? SND_PLACE : SND_MOVE);
    }
    if (p->battles == b->battles) return;
    int w = p->winner, n = 0, landed = SLIDE_MS + 90 * (k - 1);
    float bx, by;
    spot(w, b->nt[w] + (w == q ? k : 0) - 1, &bx, &by);   /* the winning card: the last one turned */
    win.q = w;
    win.from = ui_ms() + (Uint32)landed;
    win.to = win.from + PAUSE_MS;
    win.x = bx, win.y = by;
    for (int r = 0; r < b->players; r++) {
        int nt = b->nt[r] + (r == q ? k : 0);
        for (int i = 0; i < nt; i++) {
            int card, up;
            if (r == q && i >= b->nt[r]) card = b->pile[q][b->n[q] - 1 - (i - b->nt[r])], up = i == nt - 1;
            else card = b->table[r][i], up = (int)(b->up[r] >> i & 1);
            float x, y;
            spot(r, i, &x, &y);
            int m = ct_slide(&MV, DECK_FRENCH, card, x, y, T.wc, 0, up, T.px[w], T.py[w], T.wc, 0, 0, FLY_MS,
                             landed + PAUSE_MS + 50 * n, H_PILE, w, -1, n ? -1 : SND_CAPTURE);
            if (r == q && i >= b->nt[r]) ct_after(&MV, m, idx[i - b->nt[r]]);
            n++;
        }
    }
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    war_new(&G, v == WAR_SHORT ? WAR_SHORT : WAR_FULL, ui.players, (uint32_t)ui_random(1 << 30));
    ct_calm(&MV);
    win.to = 0;
}

static unsigned winners(void) { return war_winners(&G.pos); }

static int status(void)
{
    unsigned w = winners();
    if (!w) return GS_PLAY;
    if (w & (w - 1)) return GS_DRAW;
    return gs_win(__builtin_ctz(w));
}

static int turn(void)
{
    int t = war_turn(&G.pos);
    return t < 0 ? 0 : t;
}

static void tap(int q)
{
    wpos before = G.pos;
    if (!war_play(&G, q)) { ui_sound(SND_DENY); return; }
    animate(&before, q);
}

/* the bot (war_bot.c): the game read-only; a pile tapped */
const wgame *war_live(void) { return &G; }

int war_show(const char *move)
{
    int q = move[0] - '0';
    if (move[0] < '0' || move[0] > '9' || move[1] || !war_legal(&G.pos, q)) return 0;
    tap(q);
    return 1;
}

static int undo(void)
{
    if (!war_undo(&G)) return 0;
    ct_calm(&MV);
    win.to = 0;
    return 1;
}

static int script(const char *s)
{
    int ok = war_script(&G, s);
    ct_calm(&MV);
    win.to = 0;
    return ok;
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static SDL_Texture *t_disc;
static int disc_made, tok_made;

static void draw(void)
{
    const wpos *p = &G.pos;
    float u = ui.unit;
    if (p->variant == WAR_SHORT) {              /* the battles left */
        int d = (int)(1.2f * u);
        if (disc_made != d) {
            static const uint8_t c[3] = {28, 34, 32};
            disc_made = d;
            ui_free(&t_disc);
            art_disc(d, c, 0.85f, 0, ui_buf(d, d));
            t_disc = ui_tex(d, d, ui_buf(d, d));
        }
        float cx = T.b.x + T.b.w / 2, cy = T.b.y + T.b.h / 2;
        ui_put_c(t_disc, cx, cy, (float)d, (float)d);
        ui_number(cx, cy, 0.5f * u, WAR_SHORT_BATTLES - p->battles, 1);
    }
    int moving = ct_moving(&MV);
    tok_made = (int)(0.32f * T.wc);
    for (int q = 0; q < p->players; q++) {
        int n = p->n[q] - ct_hides(&MV, H_PILE, q, -1);
        cards_pile(DECK_FRENCH, T.px[q], T.py[q], T.wc, n, CARD_NONE, war_legal(p, q) && !busy() ? CD_GLOW : 0);
        float cx = T.b.x + T.b.w / 2, nx, ny;   /* the count: outside the pile (under it at the sides) */
        if (fabsf(T.px[q] - T.sx[q]) > fabsf(T.py[q] - T.sy[q])) {
            nx = T.px[q];
            ny = T.py[q] + 0.95f * T.wc;
        } else {
            nx = T.px[q] + (T.px[q] > cx + 1 ? -1.1f : 1.1f) * T.wc;
            ny = T.py[q] + 0.35f * T.wc;
        }
        ct_label(ct_token(q, tok_made), nx, ny, 0.32f * T.wc, war_cards(p, q) - ct_hides(&MV, H_PILE, q, -1));
        for (int i = 0; i < p->nt[q]; i++) {    /* the cards turned (the last ones of a war on top) */
            if (ct_hides(&MV, H_SPOT, q, i)) continue;
            float x, y;
            spot(q, i, &x, &y);
            int up = (int)(p->up[q] >> i & 1);
            if (p->war && up && i == p->nt[q] - 1 && !moving) ui_mark_glow(x, y, 1.9f * T.wc);
            cards_draw(DECK_FRENCH, p->table[q][i], x, y, T.wc, 0, up ? 0 : CD_DOWN);
        }
    }
    Uint32 now = ui_ms();
    if (win.to && now >= win.from && now < win.to) { ui_mark_gold(win.x, win.y, 2.1f * T.wc); ui_animate(); }
    ct_draw(&MV);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const wpos *p = &G.pos;
    if (p->phase != WP_TURN) return;
    for (int q = 0; q < p->players; q++)
        if (cards_in(T.px[q], T.py[q], T.wc * 1.25f, 1.3f, x, y)) {
            if (win.to && ui_ms() < win.to) return;   /* the last battle still on show */
            tap(q);
            return;
        }
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the player's cards */
{
    int v = war_cards(&G.pos, player);
    float h = fminf(0.46f * ui.unit, b.w / 1.6f);
    ui_number(b.x + (v >= 10 ? 0.74f : 0.37f) * h, b.y + b.h / 2, h, v, 1);
}

static void token(int player, int size, uint8_t *rgba) { cards_token(player, size, rgba); }

/* the menu's and the picker's cards: a king against a queen over two piles (the short game: a third, smaller pile) */
static void thumb(int v, int size, uint8_t *rgba)
{
    float w = size * 0.42f;
    int cw = (int)w, ch = (int)(w * 1.4f);
    uint8_t *b = malloc((size_t)cw * ch * 4);
    if (!b) return;
    memset(rgba, 0, (size_t)size * size * 4);
    cards_paint(DECK_FRENCH, CARD_NONE, cw, ch, b);
    art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.04f), (int)(size * 0.04f));
    art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.96f) - cw, (int)(size * 0.96f) - ch);
    if (v == WAR_SHORT) art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.96f) - cw, (int)(size * 0.04f));
    cards_paint(DECK_FRENCH, CARD(SUIT_SPADES, RANK_QUEEN), cw, ch, b);
    art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.2f), (int)(size * 0.24f));
    cards_paint(DECK_FRENCH, CARD(SUIT_HEARTS, RANK_KING), cw, ch, b);
    art_blend(rgba, size, size, b, cw, ch, (int)(size * 0.4f), (int)(size * 0.3f));
    free(b);
}

static const game war_game = {
    .id = "war", .order = 298, .variants = WAR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .winners = winners,
    .players = 1 << 2 | 1 << 3 | 1 << 4, .view = VIEW_OPEN,
};
REGISTER_GAME(war_game)
