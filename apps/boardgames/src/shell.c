/*
 * RetroStone Board: the shell (shell.h). One app for all the board games: the menu (a card per game), the variant
 * picker, the players' bars, the buttons, the end-of-game card, the sounds, and a picture mode for tests and
 * screenshots. The games bring their rules and their boards (games/<id>.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   retrostoneboard                   the app (a window; full screen on Android)
 *   retrostoneboard --shot out.bmp [--size 720x1280] [--game chess] [--variant 0] [--moves "e2e4 e7e5"]
 *                   [--taps "0.5,0.9 0.5,0.7"] [--flip] [--f2f] [--keep] [--pick]
 *                                     one picture, no window: the menu (no --game), the picker (--pick) or a game
 *                                     after its moves (its own notation) and taps (fractions of the board)
 * Keys: Esc / Back home (in the menu: quit), U undo, F flip, T face to face, N new game, M sound.
 */
#include "shell.h"
#include "text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GAMES 32

struct ui ui;
static const game *games[MAX_GAMES];
static int ngames;
static SDL_Window *win;
static int quit, need_frame;

enum { SCR_MENU, SCR_PICK, SCR_GAME };
static int screen, cur = -1;                    /* the game shown (games[cur]) */
static int variant[MAX_GAMES], started[MAX_GAMES];
static int last_status, over_hidden, instant;   /* instant: the picture mode (no pause before the end card) */
static Uint32 end_seen;                         /* when the end of the game was first shown (0: not yet) */
#define END_PAUSE 900                           /* ms of the final position before the end card */
static Uint32 armed;                            /* "new game" tapped once: a second tap within 2.5 s confirms */

enum { B_HOME, B_UNDO, B_FLIP, B_F2F, B_NEW, B_HELP, NBTN };
static const int BTN_ICON[NBTN] = {ICON_HOME, ICON_UNDO, ICON_FLIP, ICON_F2F, ICON_NEW, ICON_HELP};

static struct {
    box board, bar[2];                          /* bar[player] */
    float bx[NBTN], by[NBTN], br;               /* the buttons: centres, radius */
    float tk;                                   /* a token's size */
    /* the menu and the picker */
    int cols, card, label;                      /* label: the height of a card's name under its picture */
    box cards[MAX_GAMES];
    float snd_x, snd_y, snd_r;
} L;

static SDL_Texture *t_bg, *t_btn, *t_btn_armed, *t_btn_on, *t_icon[ICONS], *t_card, *t_gold, *t_shadow, *t_dot,
    *t_ring, *t_glow, *t_tok[2], *t_big[2], *t_bartex, *t_menu_card, *t_thumb[MAX_GAMES][8], *t_name[MAX_GAMES];
static int made_unit = -1, made_game = -1, made_card = -1, bar_w, bar_h;

/* the rules page (a game's, or the app's from the menu): the text drawn in tiles, scrolled */
#define TILE 512
#define MAX_TILES 64
static struct {
    int open, game;                             /* game: games[game], -1 the app's */
    box card, view;                             /* the page, the text's window in it */
    float close_x, close_y, close_r, px;
    int h, tiles, made_w;
    SDL_Texture *tile[MAX_TILES];
    float scroll, grab_y, grab_scroll;
    int grabbing;
} help;

/* ---- the registry ------------------------------------------------------------------------------------------------- */
void game_register(const game *g)
{
    if (ngames < MAX_GAMES) games[ngames++] = g;
}

static int by_order(const void *a, const void *b)
{
    return (*(const game *const *)a)->order - (*(const game *const *)b)->order;
}

/* ---- helpers for the games ----------------------------------------------------------------------------------------- */
int ui_bottom(void) { return ui.flipped ? 1 : 0; }
int ui_upside(int player) { return ui.f2f && player != ui_bottom(); }
Uint32 ui_ms(void) { return SDL_GetTicks(); }
void ui_animate(void) { need_frame = 1; }

SDL_Texture *ui_tex(int w, int h, const uint8_t *rgba)
{
    SDL_Texture *t = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, rgba, w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

void ui_free(SDL_Texture **t)
{
    if (*t) SDL_DestroyTexture(*t);
    *t = NULL;
}

uint8_t *ui_buf(int w, int h)
{
    static uint8_t *b;
    static size_t cap;
    size_t need = (size_t)(w > 0 ? w : 1) * (h > 0 ? h : 1) * 4;
    if (need > cap) { free(b); b = malloc(need); cap = need; }
    return b;
}

void ui_put(SDL_Texture *t, float x, float y, float w, float h)
{
    SDL_FRect d = {x, y, w, h};
    if (t) SDL_RenderCopyF(ui.r, t, NULL, &d);
}

void ui_put_c(SDL_Texture *t, float cx, float cy, float w, float h) { ui_put(t, cx - w / 2, cy - h / 2, w, h); }

void ui_put_rot(SDL_Texture *t, float cx, float cy, float w, float h, double deg)
{
    SDL_FRect d = {cx - w / 2, cy - h / 2, w, h};
    if (t) SDL_RenderCopyExF(ui.r, t, NULL, &d, deg, NULL, SDL_FLIP_NONE);
}

void ui_fill(float x, float y, float w, float h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_FRect q = {x, y, w, h};
    SDL_SetRenderDrawColor(ui.r, r, g, b, a);
    SDL_RenderFillRectF(ui.r, &q);
}

void ui_shadow(float x, float y, float size)
{
    ui_put(t_shadow, x + size * 0.17f, y + size * 0.82f, size * 0.66f, size * 0.15f);
}

void ui_shadow_flat(float cx, float cy, float w, float h, float alpha)
{
    static SDL_Texture *t;
    if (!t) {                                   /* one soft disc, scaled smoothly to any piece */
        art_flat_shadow(128, ui_buf(128, 128));
        t = ui_tex(128, 128, ui_buf(128, 128));
    }
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha)));
    ui_put_c(t, cx + 0.05f * h, cy + 0.08f * h, w, h);
}

void ui_mark_gold(float cx, float cy, float d) { ui_put_c(t_gold, cx, cy, d, d); }
void ui_mark_last(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 100); }
void ui_mark_select(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 150); }
void ui_mark_dot(float cx, float cy, float d) { ui_put_c(t_dot, cx, cy, d, d); }
void ui_mark_ring(float cx, float cy, float d) { ui_put_c(t_ring, cx, cy, d, d); }
void ui_mark_glow(float cx, float cy, float d) { ui_put_c(t_glow, cx, cy, d, d); }

void ui_sound(int s) { snd_play(s); }

/* ---- chance: the shell's dice ------------------------------------------------------------------------------------------- */
static uint32_t rng = 0x9e3779b9u;              /* xorshift; seeded at start (--seed in the picture mode) */

static void seed_random(uint32_t s) { rng = s ? s : 0x9e3779b9u; }

int ui_random(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return n > 0 ? (int)(rng % (uint32_t)n) : 0;
}

#define MAX_DICE 4
#define DICE_MS 650
static struct {
    int n, v[MAX_DICE], player, made;
    float tilt[MAX_DICE];
    Uint32 t0;
    SDL_Texture *face[2][7];
} dice;

void ui_dice_throw(int n, const int *values, int player)
{
    dice.n = n < 0 ? 0 : n > MAX_DICE ? MAX_DICE : n;
    for (int i = 0; i < dice.n; i++) {
        dice.v[i] = values[i];
        dice.tilt[i] = (float)(ui_random(31) - 15);    /* they land a little askew */
    }
    dice.player = player ? 1 : 0;
    dice.t0 = SDL_GetTicks();
    snd_play(SND_DICE);
    need_frame = 1;
}

int ui_dice_busy(void) { return dice.n && !instant && SDL_GetTicks() - dice.t0 < DICE_MS; }

void ui_dice_draw(float cx, float cy, float size, unsigned used)
{
    int s = (int)size;
    if (s < 4 || !dice.n) return;
    if (dice.made != s) {
        dice.made = s;
        for (int p = 0; p < 2; p++)
            for (int v = 1; v <= 6; v++) {
                ui_free(&dice.face[p][v]);
                art_die(v, p, s, ui_buf(s, s));
                dice.face[p][v] = ui_tex(s, s, ui_buf(s, s));
            }
    }
    int rolling = ui_dice_busy();
    float t = rolling ? (SDL_GetTicks() - dice.t0) / (float)DICE_MS : 1, gap = 0.3f * size;
    float x = cx - (dice.n * size + (dice.n - 1) * gap) / 2 + size / 2;
    for (int i = 0; i < dice.n; i++, x += size + gap) {
        int v = dice.v[i];
        float a = dice.tilt[i], y = cy;
        if (rolling) {                          /* tumbling: the faces flicker, the die turns and bounces */
            v = 1 + (int)((SDL_GetTicks() / 70 + i * 3 + dice.v[i]) % 6);
            a += (1 - t) * (1 - t) * 540 * (i & 1 ? -1 : 1);
            y -= (1 - t) * size * 0.6f * fabsf(sinf(t * 9.4f));
        }
        Uint8 alpha = used >> i & 1 ? 110 : 255;
        SDL_Texture *f = dice.face[dice.player][v];
        ui_shadow_flat(x, cy, size * 1.1f, size * 1.1f, alpha / 255.0f);
        SDL_SetTextureAlphaMod(f, alpha);
        ui_put_rot(f, x, y, size, size, a);
        SDL_SetTextureAlphaMod(f, 255);
    }
    if (rolling) need_frame = 1;
}

/* digits: white textures per height (a few heights kept), tinted when drawn */
void ui_number(float cx, float cy, float h, int value, int light)
{
    static struct { int h; SDL_Texture *d[10]; } cache[4];
    static int next;
    int ih = (int)(h + 0.5f), k;
    if (ih < 4) return;
    for (k = 0; k < 4 && cache[k].h != ih; k++) {}
    if (k == 4) {
        k = next++ & 3;
        for (int i = 0; i < 10; i++) ui_free(&cache[k].d[i]);
        int w = (int)(0.62f * ih + 1);
        for (int i = 0; i < 10; i++) {
            art_digit(i, w, ih, ui_buf(w, ih));
            cache[k].d[i] = ui_tex(w, ih, ui_buf(w, ih));
        }
        cache[k].h = ih;
    }
    char s[12];
    int n = snprintf(s, sizeof s, "%d", value < 0 ? 0 : value);
    float dw = 0.62f * ih, step = 0.74f * ih, x = cx - (step * (n - 1) + dw) / 2;
    for (int i = 0; i < n; i++, x += step) {
        SDL_Texture *t = cache[k].d[s[i] - '0'];
        if (light) SDL_SetTextureColorMod(t, 246, 238, 220);
        else SDL_SetTextureColorMod(t, 40, 34, 30);
        ui_put(t, x, cy - h / 2, dw, (float)ih);
    }
}

/* ---- pictures ------------------------------------------------------------------------------------------------------ */
static void shared_textures(void)
{
    int u = (int)ui.unit;
    if (made_unit == u) return;
    made_unit = u;
    static const uint8_t btn_c[3] = {54, 72, 64}, armed_c[3] = {176, 64, 52}, on_c[3] = {150, 118, 46},
                         gold[3] = {255, 196, 64}, shade[3] = {24, 34, 26}, red[3] = {235, 40, 30}, black[3] = {0, 0, 0};
    int d = (int)(2 * L.br + 0.5f);
    ui_free(&t_btn);
    art_disc(d, btn_c, 1, 0, ui_buf(d, d));
    t_btn = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_btn_armed);
    art_disc(d, armed_c, 1, 0, ui_buf(d, d));
    t_btn_armed = ui_tex(d, d, ui_buf(d, d));
    ui_free(&t_btn_on);
    art_disc(d, on_c, 1, 0, ui_buf(d, d));
    t_btn_on = ui_tex(d, d, ui_buf(d, d));
    int ic = (int)(0.62f * d);
    for (int i = 0; i < ICONS; i++) {
        ui_free(&t_icon[i]);
        art_icon(i, ic, ui_buf(ic, ic));
        t_icon[i] = ui_tex(ic, ic, ui_buf(ic, ic));
        SDL_SetTextureColorMod(t_icon[i], 252, 244, 226);
    }
    int big = (int)(2.6f * u), m = u;
    ui_free(&t_gold);
    art_disc(big, gold, 0.75f, 1.3f, ui_buf(big, big));
    t_gold = ui_tex(big, big, ui_buf(big, big));
    ui_free(&t_dot);
    art_disc(m, shade, 0.30f, 0, ui_buf(m, m));
    t_dot = ui_tex(m, m, ui_buf(m, m));
    ui_free(&t_ring);
    art_ring(m, 0.82f, shade, 0.30f, ui_buf(m, m));
    t_ring = ui_tex(m, m, ui_buf(m, m));
    ui_free(&t_glow);
    art_disc(m, red, 0.9f, 1.5f, ui_buf(m, m));
    t_glow = ui_tex(m, m, ui_buf(m, m));
    if (!t_shadow) {
        art_disc(64, black, 0.42f, 1.2f, ui_buf(64, 64));
        t_shadow = ui_tex(64, 64, ui_buf(64, 64));
    }
    const uint8_t card_c[4] = {28, 34, 32, 236};
    int cw = (int)(6.8f * u), ch = (int)(3.8f * u);
    ui_free(&t_card);
    art_roundrect(cw, ch, 0.4f * u, card_c, ui_buf(cw, ch));
    t_card = ui_tex(cw, ch, ui_buf(cw, ch));
    if (!t_bg) {                                /* the table: a dark green, lighter in the middle */
        uint8_t *b = ui_buf(2, 256);
        for (int y = 0; y < 256; y++)
            for (int x = 0; x < 2; x++) {
                float k = 1 - fabsf((float)(y - 110)) / 256.0f * 0.55f;
                uint8_t *p = b + 4 * (y * 2 + x);
                p[0] = (uint8_t)(34 * k);
                p[1] = (uint8_t)(52 * k);
                p[2] = (uint8_t)(46 * k);
                p[3] = 255;
            }
        t_bg = ui_tex(2, 256, b);
        SDL_SetTextureBlendMode(t_bg, SDL_BLENDMODE_NONE);
    }
    made_game = -1;                             /* the tokens follow the unit */
}

static void game_textures(void)
{
    if (made_game == cur) return;
    made_game = cur;
    const game *g = games[cur];
    int tk = (int)L.tk, big = (int)(2.4f * ui.unit);
    uint8_t *b = calloc((size_t)big * big, 4);  /* its own buffer: the game may use ui_buf() while drawing */
    for (int p = 0; p < 2; p++) {
        ui_free(&t_tok[p]);
        memset(b, 0, (size_t)tk * tk * 4);
        g->token(p, tk, b);
        t_tok[p] = ui_tex(tk, tk, b);
        ui_free(&t_big[p]);
        memset(b, 0, (size_t)big * big * 4);
        g->token(p, big, b);
        t_big[p] = ui_tex(big, big, b);
    }
    free(b);
}

/* ---- layout -------------------------------------------------------------------------------------------------------- */
static void layout_game(void)
{
    float m = fminf(ui.w, ui.h) * 0.025f, u;
    box top, bottom;
    if (ui.w <= ui.h) {                         /* portrait: bar, board, bar, buttons */
        u = floorf(fminf((ui.w - 2 * m) / 9, (ui.h - 2 * m) / 12.9f));
        float y = floorf((ui.h - 12.9f * u) / 2), x = floorf((ui.w - 9 * u) / 2);
        top = (box){x, y, 9 * u, u};
        L.board = (box){x, y + 1.2f * u, 9 * u, 9 * u};
        bottom = (box){x, L.board.y + 9.2f * u, 9 * u, u};
        L.br = 0.56f * u;
        for (int i = 0; i < NBTN; i++) {
            L.bx[i] = ui.w / 2.0f + (i - (NBTN - 1) / 2.0f) * 1.5f * u;
            L.by[i] = bottom.y + bottom.h + 0.35f * u + L.br;
        }
    } else {                                    /* landscape: the board, then a column: bar, buttons, bar */
        u = floorf(fminf((ui.h - 2 * m) / 9, (ui.w - 2 * m) / 13.45f));
        float x = floorf((ui.w - 13.45f * u) / 2), y = floorf((ui.h - 9 * u) / 2);
        L.board = (box){x, y, 9 * u, 9 * u};
        float cx = x + 9.45f * u;
        top = (box){cx, y, 4 * u, u};
        bottom = (box){cx, y + 8 * u, 4 * u, u};
        L.br = 0.55f * u;
        for (int i = 0; i < NBTN; i++) {        /* two rows of three */
            L.bx[i] = cx + 2 * u + (i % 3 - 1) * 1.35f * u;
            L.by[i] = y + 4.5f * u + (i / 3 ? 0.7f : -0.7f) * u;
        }
    }
    ui.unit = u;
    L.bar[ui_bottom()] = bottom;
    L.bar[!ui_bottom()] = top;
    L.tk = 0.72f * u;
    shared_textures();
    made_game = -1;
    if (bar_w != (int)top.w || bar_h != (int)top.h) {
        ui_free(&t_bartex);
        bar_w = (int)top.w;
        bar_h = (int)top.h;
        t_bartex = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, bar_w, bar_h);
        if (t_bartex) SDL_SetTextureBlendMode(t_bartex, SDL_BLENDMODE_BLEND);
    }
    if (cur >= 0) games[cur]->layout(L.board);
}

/* the menu (all the games) or the picker (one game's variants): cards in a grid */
static void layout_cards(int n)
{
    float m = fminf(ui.w, ui.h) * 0.05f, g = fminf(ui.w, ui.h) * 0.04f, head = fminf(ui.w, ui.h) * 0.16f;
    int cols = ui.w > ui.h ? (n > 3 ? 3 : n) : (n > 1 ? 2 : 1);
    if (screen == SCR_PICK) cols = ui.w > ui.h || n <= 3 ? n : 2;
    if (screen == SCR_PICK && ui.w <= ui.h && n == 3) cols = 1;
    int rows = (n + cols - 1) / cols;
    float lab = screen == SCR_MENU ? 0.17f : 0;  /* the menu's cards: the game's name under its picture */
    float c = fminf((ui.w - 2 * m - (cols - 1) * g) / cols, (ui.h - head - 2 * m - (rows - 1) * g) / rows / (1 + lab));
    c = floorf(fminf(c, fminf(ui.w, ui.h) * 0.62f));
    float ch = floorf(c * (1 + lab));
    float x0 = (ui.w - (cols * c + (cols - 1) * g)) / 2, y0 = head + (ui.h - head - (rows * ch + (rows - 1) * g)) / 2;
    for (int i = 0; i < n; i++)
        L.cards[i] = (box){floorf(x0 + (i % cols) * (c + g)), floorf(y0 + (i / cols) * (ch + g)), c, ch};
    L.cols = cols;
    L.label = (int)(ch - c);
    if (made_card != (int)c * 1000 + L.label) {
        for (int i = 0; i < MAX_GAMES; i++) ui_free(&t_name[i]);
    }
    L.card = (int)c;
    L.snd_r = head * 0.28f;
    L.snd_x = ui.w - m - L.snd_r;
    L.snd_y = head * 0.55f;
    ui.unit = fminf(ui.w, ui.h) / 12.0f;
    L.br = L.snd_r;
    shared_textures();
    if (made_card != L.card * 1000 + L.label) {
        made_card = L.card * 1000 + L.label;
        const uint8_t back[4] = {52, 72, 64, 255};
        int h = L.card + L.label;
        ui_free(&t_menu_card);
        art_roundrect(L.card, h, 0.1f * L.card, back, ui_buf(L.card, h));
        t_menu_card = ui_tex(L.card, h, ui_buf(L.card, h));
        for (int i = 0; i < MAX_GAMES; i++)
            for (int v = 0; v < 8; v++) ui_free(&t_thumb[i][v]);
    }
}

static void layout(void)
{
    if (screen == SCR_GAME) layout_game();
    else layout_cards(screen == SCR_MENU ? ngames : games[cur]->variants);
}

/* ---- the screens ----------------------------------------------------------------------------------------------------- */
static void open_game(int i, int fresh_variant)
{
    cur = i;
    if (fresh_variant >= 0) {
        variant[i] = fresh_variant;
        games[i]->start(fresh_variant);
        started[i] = 1;
    } else if (!started[i]) {
        if (games[i]->variants > 1) {
            screen = SCR_PICK;
            layout();
            return;
        }
        games[i]->start(0);
        started[i] = 1;
    }
    screen = SCR_GAME;
    over_hidden = 0;
    armed = 0;
    layout();
    last_status = games[cur]->status();
    end_seen = last_status != GS_PLAY;          /* a game that had ended: its card at once, no tune again */
}

static void go_home(void)
{
    screen = SCR_MENU;
    layout();
}

static void new_game(void)
{
    armed = 0;
    if (games[cur]->variants > 1) {
        started[cur] = 0;
        screen = SCR_PICK;
        layout();
        return;
    }
    games[cur]->start(variant[cur]);
    over_hidden = 0;
    last_status = GS_PLAY;
    end_seen = 0;
}

/* after anything a player did: a new end of the game (or none any more) is noticed; draw_game() shows it */
static void after_input(void)
{
    if (screen != SCR_GAME) return;
    int s = games[cur]->status();
    if (s != last_status) { over_hidden = 0; end_seen = 0; }
    last_status = s;
}

/* ---- drawing ---------------------------------------------------------------------------------------------------------- */
static void draw_button(float x, float y, float r, int icon, SDL_Texture *disc)
{
    ui_put_c(disc ? disc : t_btn, x, y, 2 * r, 2 * r);
    ui_put_c(t_icon[icon], x, y, 1.24f * r, 1.24f * r);
}

/* the pass button sits at the right end of the bar (in the bar's own frame: it turns with it) */
static int shows_pass(int p)
{
    const game *g = games[cur];
    return g->can_pass && g->status() == GS_PLAY && g->turn() == p && g->can_pass();
}

static void bar_content(int p, box b)
{
    const game *g = games[cur];
    int st = g->status(), turn = st == GS_PLAY && g->turn() == p;
    int won = (st == GS_WIN0 && p == 0) || (st == GS_WIN1 && p == 1);
    float cy = b.y + b.h / 2, tx = b.x + 0.15f * ui.unit + L.tk / 2;
    if (turn || won) {                          /* the gold light: whose turn it is, or who won */
        float k = turn ? 1.8f : 2.3f;
        ui_put_c(t_gold, tx, cy, L.tk * k, L.tk * k);
    }
    ui_put_c(t_tok[p], tx, cy, L.tk, L.tk);
    float right = b.x + b.w;
    if (shows_pass(p)) {
        float r = 0.42f * ui.unit;
        draw_button(right - r - 0.05f * ui.unit, cy, r, ICON_PASS, NULL);
        right -= 2 * r + 0.25f * ui.unit;
    }
    if (g->bar) {
        float x = tx + L.tk / 2 + 0.3f * ui.unit;
        g->bar(p, (box){x, b.y, right - x, b.h});
    }
}

static void draw_bar(int p)
{
    box b = L.bar[p];
    if (!ui_upside(p) || !t_bartex) {
        bar_content(p, b);
        return;
    }
    SDL_SetRenderTarget(ui.r, t_bartex);         /* face to face: drawn in its own frame, then turned round */
    SDL_SetRenderDrawColor(ui.r, 0, 0, 0, 0);
    SDL_RenderClear(ui.r);
    bar_content(p, (box){0, 0, (float)bar_w, (float)bar_h});
    SDL_SetRenderTarget(ui.r, NULL);
    ui_put_rot(t_bartex, b.x + b.w / 2, b.y + b.h / 2, (float)bar_w, (float)bar_h, 180);
}

static void draw_over(void)
{
    const game *g = games[cur];
    int st = g->status();
    box b = L.board;
    float u = ui.unit, cx = b.x + b.w / 2, cy = b.y + b.h / 2, big = 2.4f * u;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 90);
    ui_put_c(t_card, cx, cy, 6.8f * u, 3.8f * u);
    if (st == GS_DRAW) {                        /* both standing, an equals sign between them */
        float k = 2.0f * u;
        ui_put_c(t_big[0], cx - 1.7f * u, cy, k, k);
        ui_put_c(t_big[1], cx + 1.7f * u, cy, k, k);
        ui_fill(cx - 0.32f * u, cy - 0.22f * u, 0.64f * u, 0.13f * u, 246, 238, 220, 255);
        ui_fill(cx - 0.32f * u, cy + 0.09f * u, 0.64f * u, 0.13f * u, 246, 238, 220, 255);
    } else {                                    /* the winner in gold light, the loser lying down */
        int w = st == GS_WIN1;
        ui_put_c(t_gold, cx - 1.0f * u, cy, big * 1.25f, big * 1.25f);
        ui_put_c(t_big[w], cx - 1.0f * u, cy, big, big);
        ui_put_rot(t_big[!w], cx + 1.6f * u, cy + 0.3f * u, 1.7f * u, 1.7f * u, 90);
    }
}

static void draw_game(void)
{
    const game *g = games[cur];
    game_textures();
    g->draw();
    draw_bar(0);
    draw_bar(1);
    if (armed && SDL_GetTicks() - armed > 2500) armed = 0;
    for (int i = 0; i < NBTN; i++) {
        SDL_Texture *disc = i == B_NEW && armed ? t_btn_armed : i == B_F2F && ui.f2f ? t_btn_on : NULL;
        draw_button(L.bx[i], L.by[i], L.br, BTN_ICON[i], disc);
    }
    if (g->status() == GS_PLAY) return;
    if (g->busy && g->busy()) { end_seen = 0; return; }   /* the last move still moving: the end waits */
    Uint32 now = SDL_GetTicks();
    if (!end_seen) {                            /* the end: its tune, the final position a moment, then the card */
        end_seen = now ? now : 1;
        snd_play(SND_END);
    }
    if (!instant && now - end_seen < END_PAUSE) ui_animate();
    else if (!over_hidden) draw_over();
}

static SDL_Texture *thumb(int i, int v)
{
    int k = v + 1;                              /* -1: the game's card */
    if (k < 0 || k >= 8) return NULL;
    if (!t_thumb[i][k]) {                       /* its own buffer: the game may use ui_buf() while drawing */
        int s = (int)(L.card * 0.86f);
        uint8_t *b = calloc((size_t)s * s, 4);
        games[i]->thumb(v, s, b);
        t_thumb[i][k] = ui_tex(s, s, b);
        free(b);
    }
    return t_thumb[i][k];
}

static const uint8_t CREAM[3] = {246, 238, 220};

/* a menu card's name: the first line of the game's text, in the language, shrunk to fit */
static SDL_Texture *label(int i)
{
    if (t_name[i] || L.label < 4) return t_name[i];
    char name[96];
    if (!text_name(games[i]->id, name, sizeof name)) return NULL;
    int w = L.card, h = L.label;
    float px = h * 0.5f;
    int tw = text_line(name, px, 1, CREAM, NULL, 0, 0, 0, 0);
    if (tw > 0.9f * w) {
        px *= 0.9f * w / tw;
        tw = text_line(name, px, 1, CREAM, NULL, 0, 0, 0, 0);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    text_line(name, px, 1, CREAM, b, w, h, (w - tw) / 2, (int)((h - 1.25f * px) / 2));
    t_name[i] = ui_tex(w, h, b);
    free(b);
    return t_name[i];
}

static void draw_cards(void)
{
    int n = screen == SCR_MENU ? ngames : games[cur]->variants;
    for (int i = 0; i < n; i++) {
        box c = L.cards[i];
        int gi = screen == SCR_MENU ? i : cur, v = screen == SCR_MENU ? -1 : i;
        if (screen == SCR_MENU && started[i] && games[i]->status() == GS_PLAY)   /* a game in progress */
            ui_put_c(t_gold, c.x + c.w / 2, c.y + c.h / 2, c.w * 1.25f, c.h * 1.25f);
        ui_put(t_menu_card, c.x, c.y, c.w, c.h);
        float s = c.w * 0.86f;
        ui_put_c(thumb(gi, v), c.x + c.w / 2, c.y + c.w / 2, s, s);
        if (screen == SCR_MENU) ui_put(label(i), c.x, c.y + c.w - 0.04f * c.w, c.w, (float)L.label);
    }
    float home_x = fminf(ui.w, ui.h) * 0.05f + L.snd_r;
    if (screen == SCR_MENU) {
        draw_button(L.snd_x, L.snd_y, L.snd_r, snd_muted ? ICON_MUTE : ICON_SOUND, NULL);
        draw_button(home_x, L.snd_y, L.snd_r, ICON_HELP, NULL);
    } else {
        draw_button(home_x, L.snd_y, L.snd_r, ICON_HOME, NULL);
    }
}

/* ---- the rules page ---------------------------------------------------------------------------------------------------- */
static char *help_text;                         /* the game's rules, then the app's buttons */

static void help_free(void)
{
    for (int i = 0; i < MAX_TILES; i++) ui_free(&help.tile[i]);
    help.made_w = 0;
}

static void help_compose(void)
{
    const char *g = help.game >= 0 ? text_get(games[help.game]->id) : NULL, *a = text_get("_app");
    if (a && g) a += strcspn(a, "\n") + (a[strcspn(a, "\n")] ? 1 : 0);   /* after a game: the app's part, untitled */
    size_t n = (g ? strlen(g) : 0) + (a ? strlen(a) : 0) + 2;
    free(help_text);
    help_text = malloc(n);
    snprintf(help_text, n, "%s%s%s", g ? g : "", g ? "\n" : "", a ? a : "");
}

static void help_layout(void)
{
    float mn = fminf(ui.w, ui.h), m = mn * 0.035f, pad = mn * 0.05f;
    help.card = (box){m, m, ui.w - 2 * m, ui.h - 2 * m};
    help.px = fmaxf(13, fminf(44, mn * 0.036f));
    help.close_r = fmaxf(help.px * 1.05f, mn * 0.05f);
    help.close_x = help.card.x + help.card.w - pad * 0.5f - help.close_r;
    help.close_y = help.card.y + pad * 0.5f + help.close_r;
    float top = help.card.y + pad * 0.5f + 2 * help.close_r + pad * 0.2f;
    help.view = (box){help.card.x + pad, top, help.card.w - 2 * pad, help.card.y + help.card.h - pad * 0.6f - top};
    int w = (int)help.view.w;
    if (help.made_w == w) return;
    help_free();
    help.made_w = w;
    help.h = text_page(help_text, w, help.px, CREAM, NULL, 0, 0);
    help.tiles = (help.h + TILE - 1) / TILE;
    if (help.tiles > MAX_TILES) help.tiles = MAX_TILES;
    uint8_t *b = malloc((size_t)w * TILE * 4);
    for (int k = 0; k < help.tiles; k++) {
        memset(b, 0, (size_t)w * TILE * 4);
        text_page(help_text, w, help.px, CREAM, b, k * TILE, TILE);
        help.tile[k] = ui_tex(w, TILE, b);
    }
    free(b);
}

static float help_max(void) { return fmaxf(0, help.h - help.view.h); }

static void open_help(int g)
{
    help.open = 1;
    help.game = g;
    help.scroll = 0;
    help.grabbing = 0;
    help_compose();
    help_free();
    help_layout();
}

static void draw_help(void)
{
    static SDL_Texture *page;
    static int page_w, page_h;
    ui_fill(0, 0, (float)ui.w, (float)ui.h, 0, 0, 0, 150);
    if (page_w != (int)help.card.w || page_h != (int)help.card.h) {
        page_w = (int)help.card.w;
        page_h = (int)help.card.h;
        const uint8_t c[4] = {28, 34, 32, 248};
        ui_free(&page);
        art_roundrect(page_w, page_h, fminf(ui.w, ui.h) * 0.04f, c, ui_buf(page_w, page_h));
        page = ui_tex(page_w, page_h, ui_buf(page_w, page_h));
    }
    ui_put(page, help.card.x, help.card.y, help.card.w, help.card.h);
    SDL_Rect clip = {(int)help.view.x, (int)help.view.y, (int)help.view.w, (int)help.view.h};
    SDL_RenderSetClipRect(ui.r, &clip);
    for (int k = 0; k < help.tiles; k++) {
        float y = help.view.y + k * TILE - help.scroll;
        if (y + TILE < help.view.y || y > help.view.y + help.view.h) continue;
        ui_put(help.tile[k], help.view.x, y, help.view.w, TILE);
    }
    SDL_RenderSetClipRect(ui.r, NULL);
    if (help_max() > 0) {                       /* where the page is: a thin bar on the right */
        float th = help.view.h * help.view.h / help.h, ty = help.view.y + (help.view.h - th) * help.scroll / help_max();
        ui_fill(help.card.x + help.card.w - fminf(ui.w, ui.h) * 0.022f, ty, fminf(ui.w, ui.h) * 0.008f, th,
                246, 238, 220, 90);
    }
    draw_button(help.close_x, help.close_y, help.close_r, ICON_CLOSE, NULL);
}

static void help_scroll(float dy)
{
    help.scroll = fminf(help_max(), fmaxf(0, help.scroll + dy));
}

static void draw(void)
{
    ui_put(t_bg, 0, 0, (float)ui.w, (float)ui.h);
    if (screen == SCR_GAME) draw_game();
    else draw_cards();
    if (help.open) draw_help();
}

/* ---- input ------------------------------------------------------------------------------------------------------------ */
static int in_circle(float x, float y, float cx, float cy, float r) { return hypotf(x - cx, y - cy) < r * 1.1f; }

static int press_bar(float x, float y)
{
    for (int p = 0; p < 2; p++) {
        if (!shows_pass(p)) continue;
        box b = L.bar[p];
        float lx = x - b.x, ly = y - b.y;
        if (ui_upside(p)) { lx = b.w - lx; ly = b.h - ly; }
        float r = 0.42f * ui.unit;
        if (in_circle(lx, ly, b.w - r - 0.05f * ui.unit, b.h / 2, r)) {
            games[cur]->pass();
            snd_play(SND_TAP);
            return 1;
        }
    }
    return 0;
}

static void press(float x, float y)
{
    if (help.open) {                            /* the rules page: scroll it, or close it */
        box c = help.card;
        if (in_circle(x, y, help.close_x, help.close_y, help.close_r) || x < c.x || y < c.y || x > c.x + c.w ||
            y > c.y + c.h) {
            help.open = 0;
            snd_play(SND_TAP);
            return;
        }
        help.grabbing = 1;
        help.grab_y = y;
        help.grab_scroll = help.scroll;
        return;
    }
    if (screen != SCR_GAME) {
        float home_x = fminf(ui.w, ui.h) * 0.05f + L.snd_r;
        if (screen == SCR_MENU && in_circle(x, y, L.snd_x, L.snd_y, L.snd_r)) {
            snd_muted = !snd_muted;
            snd_play(SND_TAP);
            return;
        }
        if (screen == SCR_MENU && in_circle(x, y, home_x, L.snd_y, L.snd_r)) {
            snd_play(SND_TAP);
            open_help(-1);
            return;
        }
        if (screen == SCR_PICK && in_circle(x, y, home_x, L.snd_y, L.snd_r)) { snd_play(SND_TAP); go_home(); return; }
        int n = screen == SCR_MENU ? ngames : games[cur]->variants;
        for (int i = 0; i < n; i++) {
            box c = L.cards[i];
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
                snd_play(SND_TAP);
                if (screen == SCR_MENU) open_game(i, -1);
                else open_game(cur, i);
                return;
            }
        }
        return;
    }
    for (int i = 0; i < NBTN; i++) {
        if (!in_circle(x, y, L.bx[i], L.by[i], L.br)) continue;
        if (i == B_HOME) { snd_play(SND_TAP); go_home(); }
        else if (i == B_UNDO) { snd_play(games[cur]->undo() ? SND_TAP : SND_DENY); over_hidden = 0; }
        else if (i == B_FLIP) { snd_play(SND_TAP); ui.flipped = !ui.flipped; layout(); }
        else if (i == B_F2F) { snd_play(SND_TAP); ui.f2f = !ui.f2f; }
        else if (i == B_HELP) { snd_play(SND_TAP); open_help(cur); }
        else if (games[cur]->status() == GS_PLAY && !armed && games[cur]->turn() >= 0 && started[cur]) {
            armed = SDL_GetTicks();
            snd_play(SND_TAP);
        } else { snd_play(SND_TAP); new_game(); }
        last_status = screen == SCR_GAME ? games[cur]->status() : GS_PLAY;
        return;
    }
    if (press_bar(x, y)) return;
    if (games[cur]->status() != GS_PLAY && !over_hidden) { over_hidden = 1; return; }
    for (int p = 0; p < 2 && games[cur]->bar_press; p++) {   /* the game's own use of a bar */
        box b = L.bar[p];
        if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
        float fx = x, fy = y;                   /* in the frame bar() drew in */
        if (ui_upside(p) && t_bartex) { fx = b.w - (x - b.x); fy = b.h - (y - b.y); }
        if (games[cur]->bar_press(p, fx, fy)) return;
    }
    games[cur]->press(x, y);
}

static void motion(float x, float y)
{
    if (help.open) {
        if (help.grabbing) help.scroll = fminf(help_max(), fmaxf(0, help.grab_scroll - (y - help.grab_y)));
        return;
    }
    if (screen == SCR_GAME) games[cur]->motion(x, y);
}

static void release(float x, float y)
{
    if (help.open) {
        help.grabbing = 0;
        return;
    }
    if (screen == SCR_GAME) games[cur]->release(x, y);
}

/* ---- the picture mode ---------------------------------------------------------------------------------------------------- */
static int find_game(const char *id)
{
    for (int i = 0; i < ngames; i++)
        if (!strcmp(games[i]->id, id)) return i;
    return -1;
}

static int shot(int argc, char **argv)
{
    const char *out = NULL, *id = NULL, *moves = NULL, *taps = NULL, *lang = NULL;
    int v = 0, keep = 0, pick = 0, show_help = 0;
    float scroll = 0;
    uint32_t seed = 1;
    ui.w = 720;
    ui.h = 1280;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &ui.w, &ui.h);
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) id = argv[++i];
        else if (!strcmp(argv[i], "--variant") && i + 1 < argc) v = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--moves") && i + 1 < argc) moves = argv[++i];
        else if (!strcmp(argv[i], "--taps") && i + 1 < argc) taps = argv[++i];
        else if (!strcmp(argv[i], "--flip")) ui.flipped = 1;
        else if (!strcmp(argv[i], "--f2f")) ui.f2f = 1;
        else if (!strcmp(argv[i], "--keep")) keep = 1;
        else if (!strcmp(argv[i], "--pick")) pick = 1;
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = argv[++i];
        else if (!strcmp(argv[i], "--help")) show_help = 1;
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) scroll = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 10);
    }
    seed_random(seed);                          /* the same dice every time: tests and screenshots repeat */
    text_init(lang ? lang : "en");
    instant = 1;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, ui.w, ui.h, 32, SDL_PIXELFORMAT_RGBA32);
    ui.r = SDL_CreateSoftwareRenderer(surf);
    if (!ui.r) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    screen = SCR_MENU;
    if (id) {
        cur = find_game(id);
        if (cur < 0) { fprintf(stderr, "no game %s\n", id); return 1; }
        if (pick) { screen = SCR_PICK; layout(); }
        else {
            open_game(cur, v);
            if (moves && (!games[cur]->script || !games[cur]->script(moves))) { fprintf(stderr, "bad moves\n"); return 1; }
            for (const char *s = taps; s && *s;) {
                float fx, fy;
                int n = 0;
                if (sscanf(s, "%f,%f%n", &fx, &fy, &n) < 2) break;
                float x = L.board.x + fx * L.board.w, y = L.board.y + fy * L.board.h;
                press(x, y);
                release(x, y);
                after_input();
                s += n;
                while (*s == ' ') s++;
            }
            over_hidden = keep;
        }
    } else {
        layout();
    }
    if (show_help) {                            /* the rules page (the game's, or the app's), scrolled by --scroll px */
        open_help(id ? cur : -1);
        help_scroll(scroll);
    }
    draw();
    SDL_RenderPresent(ui.r);
    if (!out || SDL_SaveBMP(surf, out)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    printf("%s: %dx%d %s status %d\n", out, ui.w, ui.h, id ? id : "menu", id ? games[cur]->status() : -1);
    return 0;
}

/* ---- the program -------------------------------------------------------------------------------------------------------- */
static void to_pixels(int x, int y, float *fx, float *fy)
{
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
    *fx = x * (float)ui.w / (ww ? ww : 1);
    *fy = y * (float)ui.h / (wh ? wh : 1);
}

static void key(SDL_Keycode k)
{
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
        if (help.open) help.open = 0;
        else if (screen == SCR_MENU) quit = 1;
        else go_home();
        return;
    }
    if (k == SDLK_m) snd_muted = !snd_muted;
    if (k == SDLK_h || k == SDLK_F1) { if (!help.open) open_help(screen == SCR_GAME ? cur : -1); return; }
    if (screen != SCR_GAME || help.open) return;
    if (k == SDLK_u || k == SDLK_BACKSPACE) games[cur]->undo();
    else if (k == SDLK_f) { ui.flipped = !ui.flipped; layout(); }
    else if (k == SDLK_t) ui.f2f = !ui.f2f;
    else if (k == SDLK_n) new_game();
}

int main(int argc, char **argv)
{
    qsort(games, (size_t)ngames, sizeof games[0], by_order);
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--shot")) return shot(argc, argv);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait PortraitUpsideDown");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    const char *lang = NULL;                    /* --lang fr: another language than the device's */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--lang")) lang = argv[i + 1];
    text_init(lang);
    seed_random((uint32_t)SDL_GetPerformanceCounter());
    snd_init();
    win = SDL_CreateWindow("RetroStone Board", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 540, 900,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ui.r = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (win && !ui.r) ui.r = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!win || !ui.r) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    screen = SCR_MENU;
    while (!quit) {
        SDL_Event e;
        int busy = need_frame || armed;
        need_frame = 0;
        if (busy ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 250)) {
            do {
                float fx, fy;
                switch (e.type) {
                case SDL_QUIT: quit = 1; break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    to_pixels(e.button.x, e.button.y, &fx, &fy);
                    press(fx, fy);
                    after_input();
                    break;
                case SDL_MOUSEMOTION:
                    to_pixels(e.motion.x, e.motion.y, &fx, &fy);
                    motion(fx, fy);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    to_pixels(e.button.x, e.button.y, &fx, &fy);
                    release(fx, fy);
                    after_input();
                    break;
                case SDL_KEYDOWN:
                    key(e.key.keysym.sym);
                    after_input();
                    break;
                case SDL_MOUSEWHEEL:
                    if (help.open) help_scroll(-e.wheel.y * help.px * 3);
                    break;
                }
            } while (SDL_PollEvent(&e));
        }
        int w, h;
        SDL_GetRendererOutputSize(ui.r, &w, &h);
        if (w != ui.w || h != ui.h) {
            ui.w = w;
            ui.h = h;
            layout();
            if (help.open) help_layout();
        }
        draw();
        SDL_RenderPresent(ui.r);
    }
    SDL_Quit();
    return 0;
}
