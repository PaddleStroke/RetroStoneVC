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

enum { B_HOME, B_UNDO, B_FLIP, B_F2F, B_NEW, NBTN };
static const int BTN_ICON[NBTN] = {ICON_HOME, ICON_UNDO, ICON_FLIP, ICON_F2F, ICON_NEW};

static struct {
    box board, bar[2];                          /* bar[player] */
    float bx[NBTN], by[NBTN], br;               /* the buttons: centres, radius */
    float tk;                                   /* a token's size */
    /* the menu and the picker */
    int cols, card;
    box cards[MAX_GAMES];
    float snd_x, snd_y, snd_r;
} L;

static SDL_Texture *t_bg, *t_btn, *t_btn_armed, *t_btn_on, *t_icon[ICONS], *t_card, *t_gold, *t_shadow, *t_dot,
    *t_ring, *t_glow, *t_tok[2], *t_big[2], *t_bartex, *t_menu_card, *t_thumb[MAX_GAMES][8];
static int made_unit = -1, made_game = -1, made_card = -1, bar_w, bar_h;

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

void ui_mark_last(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 100); }
void ui_mark_select(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 150); }
void ui_mark_dot(float cx, float cy, float d) { ui_put_c(t_dot, cx, cy, d, d); }
void ui_mark_ring(float cx, float cy, float d) { ui_put_c(t_ring, cx, cy, d, d); }
void ui_mark_glow(float cx, float cy, float d) { ui_put_c(t_glow, cx, cy, d, d); }

void ui_sound(int s) { snd_play(s); }

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
        L.br = 0.575f * u;
        for (int i = 0; i < NBTN; i++) {
            L.bx[i] = ui.w / 2.0f + (i - 2) * 1.75f * u;
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
        for (int i = 0; i < NBTN; i++) {
            int row = i / 3, col = row ? i - 3 : i, n = row ? 2 : 3;
            L.bx[i] = cx + 2 * u + (col - (n - 1) / 2.0f) * 1.35f * u;
            L.by[i] = y + 4.5f * u + (row ? 0.7f : -0.7f) * u;
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
    float c = fminf((ui.w - 2 * m - (cols - 1) * g) / cols, (ui.h - head - 2 * m - (rows - 1) * g) / rows);
    c = floorf(fminf(c, fminf(ui.w, ui.h) * 0.62f));
    float x0 = (ui.w - (cols * c + (cols - 1) * g)) / 2, y0 = head + (ui.h - head - (rows * c + (rows - 1) * g)) / 2;
    for (int i = 0; i < n; i++)
        L.cards[i] = (box){floorf(x0 + (i % cols) * (c + g)), floorf(y0 + (i / cols) * (c + g)), c, c};
    L.cols = cols;
    L.card = (int)c;
    L.snd_r = head * 0.28f;
    L.snd_x = ui.w - m - L.snd_r;
    L.snd_y = head * 0.55f;
    ui.unit = fminf(ui.w, ui.h) / 12.0f;
    L.br = L.snd_r;
    shared_textures();
    if (made_card != L.card) {
        made_card = L.card;
        const uint8_t back[4] = {52, 72, 64, 255};
        ui_free(&t_menu_card);
        art_roundrect(L.card, L.card, 0.1f * L.card, back, ui_buf(L.card, L.card));
        t_menu_card = ui_tex(L.card, L.card, ui_buf(L.card, L.card));
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
        ui_put_c(thumb(gi, v), c.x + c.w / 2, c.y + c.h / 2, s, s);
    }
    if (screen == SCR_MENU) draw_button(L.snd_x, L.snd_y, L.snd_r, snd_muted ? ICON_MUTE : ICON_SOUND, NULL);
    else draw_button(fminf(ui.w, ui.h) * 0.05f + L.snd_r, L.snd_y, L.snd_r, ICON_HOME, NULL);
}

static void draw(void)
{
    ui_put(t_bg, 0, 0, (float)ui.w, (float)ui.h);
    if (screen == SCR_GAME) draw_game();
    else draw_cards();
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
    if (screen != SCR_GAME) {
        float home_x = fminf(ui.w, ui.h) * 0.05f + L.snd_r;
        if (screen == SCR_MENU && in_circle(x, y, L.snd_x, L.snd_y, L.snd_r)) {
            snd_muted = !snd_muted;
            snd_play(SND_TAP);
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
        else if (games[cur]->status() == GS_PLAY && !armed && games[cur]->turn() >= 0 && started[cur]) {
            armed = SDL_GetTicks();
            snd_play(SND_TAP);
        } else { snd_play(SND_TAP); new_game(); }
        last_status = screen == SCR_GAME ? games[cur]->status() : GS_PLAY;
        return;
    }
    if (press_bar(x, y)) return;
    if (games[cur]->status() != GS_PLAY && !over_hidden) { over_hidden = 1; return; }
    games[cur]->press(x, y);
}

static void motion(float x, float y)
{
    if (screen == SCR_GAME) games[cur]->motion(x, y);
}

static void release(float x, float y)
{
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
    const char *out = NULL, *id = NULL, *moves = NULL, *taps = NULL;
    int v = 0, keep = 0, pick = 0;
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
    }
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
        if (screen == SCR_MENU) quit = 1;
        else go_home();
        return;
    }
    if (k == SDLK_m) snd_muted = !snd_muted;
    if (screen != SCR_GAME) return;
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
                }
            } while (SDL_PollEvent(&e));
        }
        int w, h;
        SDL_GetRendererOutputSize(ui.r, &w, &h);
        if (w != ui.w || h != ui.h) { ui.w = w; ui.h = h; layout(); }
        draw();
        SDL_RenderPresent(ui.r);
    }
    SDL_Quit();
    return 0;
}
