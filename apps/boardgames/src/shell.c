/*
 * RetroStone Board: the shell (shell.h). One app for all the board games: the menu (a card per game), the variant
 * picker, the players' bars, the buttons, the end-of-game card, the sounds, and a picture mode for tests and
 * screenshots. The games bring their rules and their boards (games/<id>.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   retrostoneboard                   the app (a window; full screen on Android)
 *   retrostoneboard --shot out.bmp [--size 720x1280] [--game chess] [--variant 0] [--moves "e2e4 e7e5"]
 *                   [--taps "0.5,0.9 0.5,0.7"] [--flip] [--f2f] [--keep] [--pick] [--players n] [--reveal]
 *                                     one picture, no window: the menu (no --game), the picker (--pick) or a game
 *                                     after its moves (its own notation) and taps (fractions of the board); the
 *                                     hand-off cover lifted (--reveal: the start button tapped)
 * Keys: Esc / Back home (in the menu: quit), U undo, F flip, T face to face, N new game, M sound.
 */
#include "shell.h"
#include "cards.h"
#include "games/chess.h"
#include "text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GAMES 128
#define MAX_VARIANTS 15                         /* a picker card each (the thumbnails: one more, the menu's) */

struct ui ui = {.players = 2};
static const game *games[MAX_GAMES];
static int ngames;
static SDL_Window *win;
static int quit, need_frame;

enum { SCR_MENU, SCR_PICK, SCR_GAME };
static int screen, cur = -1;                    /* the game shown (games[cur]) */
static int variant[MAX_GAMES], started[MAX_GAMES];
static int nplayers[MAX_GAMES];                 /* how many play each game (the picker's choice; 0: not chosen) */

/* the hand-off (VIEW_* games): whose view each game shows (kept while away in the menu); the cover (up: hiding the
 * screen until `player` taps start); a hand-off waiting for the last move to land (pending: the player, or -1) */
static int view_of[MAX_GAMES];
static struct { int up, player, pending; } hand = {0, 0, -1};
static int is_view(void) { return cur >= 0 && games[cur]->view != VIEW_BOARD; }
static unsigned winners_of(int st);

/* the menu's sections, in this order: a heading (help/<lang>/_menu.txt, a line each), then their games by menu
 * order. The games not listed are strategy games. */
enum { SEC_STRATEGY, SEC_CHESS, SEC_RACE, SEC_CARDS, SEC_GUESS, SEC_SOLO, SECTIONS };
static const struct { const char *id; int section; } SECTION_OF[] = {
    {"chess", SEC_CHESS}, {"makruk", SEC_CHESS}, {"shogi", SEC_CHESS}, {"xiangqi", SEC_CHESS}, {"janggi", SEC_CHESS},
    {"jungle", SEC_CHESS},
    {"backgammon", SEC_RACE}, {"ur", SEC_RACE}, {"ludo", SEC_RACE}, {"pachisi", SEC_RACE}, {"snakes", SEC_RACE},
    {"shutthebox", SEC_RACE}, {"yacht", SEC_RACE}, {"farkle", SEC_RACE}, {"fourtwoone", SEC_RACE},
    {"liarsdice", SEC_RACE},
    {"golf", SEC_CARDS}, {"tilerummy", SEC_CARDS}, {"tarot", SEC_CARDS}, {"belote", SEC_CARDS},
    {"crazyeights", SEC_CARDS}, {"hearts", SEC_CARDS}, {"dominoes", SEC_CARDS}, {"cribbage", SEC_CARDS},
    {"ginrummy", SEC_CARDS}, {"spades", SEC_CARDS}, {"president", SEC_CARDS}, {"war", SEC_CARDS},
    {"oldmaid", SEC_CARDS}, {"families", SEC_CARDS},
    {"secretcode", SEC_GUESS}, {"mysteryface", SEC_GUESS},
    {"seabattle", SEC_GUESS}, {"wordtiles", SEC_CARDS},
    {"klondike", SEC_SOLO}, {"freecell", SEC_SOLO}, {"spider", SEC_SOLO}, {"pegsolitaire", SEC_SOLO},
    {"mahjong", SEC_SOLO},
    {"goose", SEC_RACE}, {"nainjaune", SEC_CARDS}, {"crapette", SEC_CARDS}, {"manille", SEC_CARDS}, {"slap", SEC_CARDS},
    {"mahjong4", SEC_CARDS}, {"koikoi", SEC_CARDS}, {"scopa", SEC_CARDS}, {"briscola", SEC_CARDS}, {"skat", SEC_CARDS},
    {"euchre", SEC_CARDS}, {"canasta", SEC_CARDS}, {"pairs", SEC_GUESS},
    {"sudoku", SEC_SOLO}, {"nonogram", SEC_SOLO}, {"taquin", SEC_SOLO}, {"hanoi", SEC_SOLO}, {"pyramid", SEC_SOLO},
    {"tripeaks", SEC_SOLO},
};

static int section_of(const game *g)
{
    for (size_t i = 0; i < sizeof SECTION_OF / sizeof SECTION_OF[0]; i++)
        if (!strcmp(SECTION_OF[i].id, g->id)) return SECTION_OF[i].section;
    return SEC_STRATEGY;
}

/* the counts a game can be played with: bit n, n players */
static unsigned player_counts(int g) { return games[g]->players ? games[g]->players : 1u << 2; }
static int many_counts(int g) { unsigned c = player_counts(g); return (c & (c - 1)) != 0; }

static int players_of(int g)
{
    unsigned c = player_counts(g);
    if (!nplayers[g] || !(c >> nplayers[g] & 1)) {     /* the usual count, else two if it can, else the fewest */
        int u = games[g]->players_usual;
        nplayers[g] = u > 0 && c >> u & 1 ? u : c >> 2 & 1 ? 2 : 1;
        while (!(c >> nplayers[g] & 1)) nplayers[g]++;
    }
    return nplayers[g];
}
static int last_status, over_hidden, instant;   /* instant: the picture mode (no pause before the end card) */
static Uint32 end_seen;                         /* when the end of the game was first shown (0: not yet) */
#define END_PAUSE 900                           /* ms of the final position before the end card */
static Uint32 armed;                            /* "new game" tapped once: a second tap within 2.5 s confirms */

enum { B_HOME, B_UNDO, B_FLIP, B_F2F, B_NEW, B_HELP, NBTN };
static const int BTN_ICON[NBTN] = {ICON_HOME, ICON_UNDO, ICON_FLIP, ICON_F2F, ICON_NEW, ICON_HELP};

static struct {
    box board, bar[MAX_PLAYERS];                /* bar[player] */
    float bx[NBTN], by[NBTN], br;               /* the buttons: centres, radius */
    float tk;                                   /* a token's size */
    /* the menu and the picker */
    int cols, card, label;                      /* label: the height of a card's name under its picture */
    box cards[MAX_GAMES];
    float snd_x, snd_y, snd_r;
    box pills[MAX_PLAYERS + 1];                 /* the picker: how many players (pill_n[i] each) */
    int npills, pill_n[MAX_PLAYERS + 1];
    float sec_y[SECTIONS], sec_x, sec_w, sec_h;   /* the menu's headings (sec_y -1: an empty section) */
    box tabs[SECTIONS];                         /* the menu's tabs, one a section with games */
    int ntabs, tab_sec[SECTIONS];
} L;

static SDL_Texture *t_bg, *t_btn, *t_btn_armed, *t_btn_on, *t_icon[ICONS], *t_card, *t_gold, *t_shadow, *t_ring,
    *t_glow, *t_tok[MAX_PLAYERS], *t_big[MAX_PLAYERS], *t_bartex, *t_menu_card, *t_thumb[MAX_GAMES][MAX_VARIANTS + 1],
    *t_name[MAX_GAMES], *t_pill, *t_pill_on, *t_ptok[MAX_PLAYERS], *t_sec[SECTIONS],
    *t_tab, *t_tab_on, *t_tabicon[SECTIONS];
static int made_unit = -1, made_game = -1, made_card = -1, made_pill = -1, made_tab = -1, bar_w, bar_h, bar_m;

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

/* the menu: more games than a screen holds, so the cards scroll under the buttons (dragged, or the mouse wheel) */
static struct {
    float scroll, max, top;                     /* top: where the cards' window starts, below the buttons */
    float grab_x, grab_y, grab_scroll, vel;     /* vel: the fling after a drag, pixels a ms */
    int grabbing, moved, axis;                  /* axis: the drag's, 1 sideways (the tabs), 2 up and down */
    int tab;                                    /* the section shown (its tab) */
    Uint32 t;                                   /* the last drag or fling step */
} menu;

/* ---- the registry ------------------------------------------------------------------------------------------------ */
void game_register(const game *g)
{
    if (ngames < MAX_GAMES) games[ngames++] = g;
}

static int by_order(const void *a, const void *b)
{
    int sa = section_of(*(const game *const *)a), sb = section_of(*(const game *const *)b);
    if (sa != sb) return sa - sb;
    return (*(const game *const *)a)->order - (*(const game *const *)b)->order;
}

/* ---- helpers for the games --------------------------------------------------------------------------------------- */
int ui_bottom(void)
{
    if (is_view()) return ui.viewer >= 0 ? ui.viewer : hand.player;   /* the viewer, at the bottom */
    return ui.flipped ? 1 : 0;
}

/* a player's bar above the board (then turned round face to face): the game's home(), else the player not at the
 * bottom (VIEW_* games: everyone but the viewer) */
static int seat_top(int p)
{
    if (is_view() || cur < 0 || !games[cur]->home) return p != ui_bottom();
    float fx, fy;
    games[cur]->home(p, &fx, &fy);
    return ui.flipped ? fy > 0.5f : fy < 0.5f;
}

int ui_upside(int player) { return ui.f2f && !is_view() && seat_top(player); }
static Uint32 ms_ahead;                         /* the picture mode: the clock moved on past the last tap */
Uint32 ui_ms(void) { return SDL_GetTicks() + ms_ahead; }
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

void ui_flat_at(SDL_Texture *t, float cx, float cy, float d, float alpha, int upside)
{
    float a = alpha < 0 ? 0 : alpha > 1 ? 1 : alpha;
    ui_shadow_flat(cx, cy, d, d, a);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * a + 0.5f));
    if (upside) ui_put_rot(t, cx, cy, d, d, 180);
    else ui_put_c(t, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t, 255);
}

/* marks with thin edges, painted at the size they are drawn (a few sizes kept): scaled down, an edge would blur */
#define SIZES 4
typedef struct { int size[SIZES], next; SDL_Texture *t[SIZES]; } sized;

static SDL_Texture *sized_tex(sized *c, int size, void (*paint)(int size, uint8_t *rgba))
{
    if (size < 4) return NULL;
    for (int i = 0; i < SIZES; i++)
        if (c->t[i] && c->size[i] == size) return c->t[i];
    int i = c->next++ % SIZES;
    ui_free(&c->t[i]);
    paint(size, ui_buf(size, size));
    c->t[i] = ui_tex(size, size, ui_buf(size, size));
    c->size[i] = size;
    return c->t[i];
}

/* the move dot: dark, in a thin cream rim, so it reads on light wood, dark wood and a dark point or hole alike */
static void paint_dot(int size, uint8_t *rgba)
{
    static const uint8_t cream[3] = {246, 238, 220}, shade[3] = {24, 34, 26};
    float rim = fmaxf(1.1f, 0.07f * size), r_in = 1 - 2 * rim / size;
    art_ring(size, r_in, cream, 0.55f, rgba);
    int in = (int)(size * (r_in + 0.04f) + 0.5f), at = (size - in) / 2;
    uint8_t *disc = malloc((size_t)in * in * 4);
    if (!disc) return;
    art_disc(in, shade, 0.42f, 0, disc);
    art_blend(rgba, size, size, disc, in, in, at, at);
    free(disc);
}

static void paint_round(int size, uint8_t *rgba)
{
    static const uint8_t warm[3] = {250, 214, 90};
    art_disc(size, warm, 1, 0, rgba);
}

static void round_tint(float cx, float cy, float d, Uint8 alpha)
{
    static sized cache;
    SDL_Texture *t = sized_tex(&cache, (int)(d + 0.5f), paint_round);
    if (!t) return;
    SDL_SetTextureAlphaMod(t, alpha);
    ui_put_c(t, cx, cy, d, d);
}

void ui_mark_gold(float cx, float cy, float d) { ui_put_c(t_gold, cx, cy, d, d); }
void ui_mark_last(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 100); }
void ui_mark_select(float x, float y, float w, float h) { ui_fill(x, y, w, h, 250, 214, 90, 150); }
void ui_mark_last_round(float cx, float cy, float d) { round_tint(cx, cy, d, 100); }
void ui_mark_select_round(float cx, float cy, float d) { round_tint(cx, cy, d, 150); }

void ui_mark_dot(float cx, float cy, float d)
{
    static sized cache;
    ui_put_c(sized_tex(&cache, (int)(d + 0.5f), paint_dot), cx, cy, d, d);
}
void ui_mark_ring(float cx, float cy, float d) { ui_put_c(t_ring, cx, cy, d, d); }
void ui_mark_glow(float cx, float cy, float d) { ui_put_c(t_glow, cx, cy, d, d); }

void ui_sound(int s) { snd_play(s); }

/* ---- chance: the shell's dice ------------------------------------------------------------------------------------ */
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
    int n, v[MAX_DICE], player[MAX_DICE], made;
    float tilt[MAX_DICE];
    Uint32 t0;
    SDL_Texture *face[MAX_PLAYERS][7];
} dice;

/* the dice set to their values and colours (players: one per die, or NULL: all `player`'s), tumbling or at rest */
static void dice_set(int n, const int *values, const int *players, int player, int tumble)
{
    dice.n = n < 0 ? 0 : n > MAX_DICE ? MAX_DICE : n;
    for (int i = 0; i < dice.n; i++) {
        dice.v[i] = values[i];
        int p = players ? players[i] : player;
        dice.player[i] = p < 0 ? 0 : p >= MAX_PLAYERS ? MAX_PLAYERS - 1 : p;
        dice.tilt[i] = tumble ? (float)(ui_random(31) - 15) : 0;   /* they land a little askew */
    }
    if (!tumble) {
        dice.t0 = ui_ms() - DICE_MS;            /* already at rest */
        return;
    }
    dice.t0 = ui_ms();
    snd_play(SND_DICE);
    need_frame = 1;
}

void ui_dice_throw(int n, const int *values, int player) { dice_set(n, values, NULL, player, 1); }
void ui_dice_show(int n, const int *values, int player) { dice_set(n, values, NULL, player, 0); }
void ui_dice_throw_each(int n, const int *values, const int *players) { dice_set(n, values, players, 0, 1); }
void ui_dice_show_each(int n, const int *values, const int *players) { dice_set(n, values, players, 0, 0); }

int ui_dice_busy(void) { return dice.n && ui_ms() - dice.t0 < DICE_MS; }

static void dice_faces(int s)
{
    if (dice.made == s) return;
    dice.made = s;
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int v = 1; v <= 6; v++) {
            ui_free(&dice.face[p][v]);
            art_die(v, p, s, ui_buf(s, s));
            dice.face[p][v] = ui_tex(s, s, ui_buf(s, s));
        }
}

void ui_die_at(float cx, float cy, float size, int player, int value, float alpha)
{
    int s = (int)size;
    if (s < 4 || value < 1 || value > 6) return;
    dice_faces(s);
    SDL_Texture *f = dice.face[player < 0 ? 0 : player >= MAX_PLAYERS ? MAX_PLAYERS - 1 : player][value];
    Uint8 a = (Uint8)(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha) + 0.5f);
    ui_shadow_flat(cx, cy, size * 1.1f, size * 1.1f, alpha);
    SDL_SetTextureAlphaMod(f, a);
    ui_put_c(f, cx, cy, size, size);
    SDL_SetTextureAlphaMod(f, 255);
}

void ui_dice_draw(float cx, float cy, float size, unsigned used)
{
    int s = (int)size;
    if (s < 4 || !dice.n) return;
    dice_faces(s);
    int rolling = ui_dice_busy();
    float t = rolling ? (ui_ms() - dice.t0) / (float)DICE_MS : 1, gap = 0.3f * size;
    float x = cx - (dice.n * size + (dice.n - 1) * gap) / 2 + size / 2;
    for (int i = 0; i < dice.n; i++, x += size + gap) {
        int v = dice.v[i];
        float a = dice.tilt[i], y = cy;
        if (rolling) {                          /* tumbling: the faces flicker, the die turns and bounces */
            v = 1 + (int)((ui_ms() / 70 + i * 3 + dice.v[i]) % 6);
            a += (1 - t) * (1 - t) * 540 * (i & 1 ? -1 : 1);
            y -= (1 - t) * size * 0.6f * fabsf(sinf(t * 9.4f));
        }
        Uint8 alpha = used >> i & 1 ? 110 : 255;
        SDL_Texture *f = dice.face[dice.player[i]][v];
        ui_shadow_flat(x, cy, size * 1.1f, size * 1.1f, alpha / 255.0f);
        SDL_SetTextureAlphaMod(f, alpha);
        ui_put_rot(f, x, y, size, size, a);
        SDL_SetTextureAlphaMod(f, 255);
    }
    if (rolling) need_frame = 1;
}

void ui_sound_soft(int s, float volume) { snd_play_vol(s, volume); }
void ui_sound_later(int s, int ms) { snd_play_at(s, 1, ms); }

/* digits: white textures per height (a few heights kept), tinted when drawn, turned round their centre */
static void number(float cx, float cy, float h, int value, int light, double deg)
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
        static const uint8_t white[3] = {255, 255, 255};
        for (int i = 0; i < 10; i++) {          /* the font's bold digits (help/glyphs.txt keeps them), tinted */
            uint8_t *b = ui_buf(w, ih);
            memset(b, 0, (size_t)w * ih * 4);
            text_glyph('0' + i, ih * 1.2f, 1, white, b, w, ih);
            cache[k].d[i] = ui_tex(w, ih, b);
        }
        cache[k].h = ih;
    }
    char s[12];
    int n = snprintf(s, sizeof s, "%d", value < 0 ? 0 : value);
    float dw = 0.62f * ih, step = 0.74f * ih, x = cx - (step * (n - 1) + dw) / 2;
    float ca = cosf((float)(deg * 3.14159265 / 180)), sa = sinf((float)(deg * 3.14159265 / 180));
    for (int i = 0; i < n; i++, x += step) {
        SDL_Texture *t = cache[k].d[s[i] - '0'];
        if (light) SDL_SetTextureColorMod(t, 246, 238, 220);
        else SDL_SetTextureColorMod(t, 40, 34, 30);
        if (deg == 0) ui_put(t, x, cy - h / 2, dw, (float)ih);
        else {                                  /* the digit's centre turned round the number's */
            float ox = x + dw / 2 - cx;
            ui_put_rot(t, cx + ox * ca, cy + ox * sa, dw, (float)ih, deg);
        }
    }
}

void ui_number(float cx, float cy, float h, int value, int light) { number(cx, cy, h, value, light, 0); }

void ui_number_rot(float cx, float cy, float h, int value, int light, double deg)
{
    number(cx, cy, h, value, light, deg);
}

void ui_number_signed(float cx, float cy, float h, int value, int light, int plus)
{
    int sign = value < 0 ? -1 : plus && value > 0 ? 1 : 0, a = abs(value), n = 1;
    if (!sign) { number(cx, cy, h, a, light, 0); return; }
    for (int v = a; v >= 10; v /= 10) n++;
    float ih = floorf(h + 0.5f), dw = 0.62f * ih, step = 0.74f * ih, sw = 0.46f * ih, gap = 0.1f * ih;
    float digits = step * (n - 1) + dw, x = cx - (sw + gap + digits) / 2, t = fmaxf(1, 0.13f * ih);
    Uint8 r = light ? 246 : 40, g = light ? 238 : 34, b = light ? 220 : 30;
    t = floorf(t + 0.5f);                       /* the sign on whole pixels: a bar, crossed for a plus (one bar
                                                 * each way, so no seam makes it look like a division sign) */
    float sx = floorf(x + 0.5f), sy = floorf(cy - t / 2 + 0.5f), s = floorf(sw + 0.5f);
    ui_fill(sx, sy, s, t, r, g, b, 255);
    if (sign > 0) ui_fill(sx + floorf((s - t) / 2), sy - floorf((s - t) / 2), t, s, r, g, b, 255);
    number(x + sw + gap + digits / 2, cy, h, a, light, 0);
}

void ui_bar_count(box b, SDL_Texture *piece, int n)
{
    if (n <= 0) return;
    float sz = 0.6f * ui.unit, dh = 0.46f * ui.unit, cy = b.y + b.h / 2;
    int digits = n >= 100 ? 3 : n >= 10 ? 2 : 1;
    ui_put(piece, b.x, cy - sz / 2, sz, sz);
    ui_number(b.x + sz + 0.12f * ui.unit + (0.74f * (digits - 1) + 0.62f) * dh / 2, cy, dh, n, 1);
}

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
static void shared_textures(void)
{
    int u = (int)ui.unit;
    if (made_unit == u) return;
    made_unit = u;
    static const uint8_t btn_c[3] = {54, 72, 64}, armed_c[3] = {176, 64, 52}, on_c[3] = {150, 118, 46},
                         gold[3] = {255, 196, 64}, shade[3] = {24, 34, 26}, red[3] = {235, 40, 30},
                         black[3] = {0, 0, 0};
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
    if (made_game == cur * 8 + ui.players) return;
    made_game = cur * 8 + ui.players;
    const game *g = games[cur];
    int tk = (int)L.tk, big = (int)(2.4f * ui.unit);
    uint8_t *b = calloc((size_t)big * big, 4);  /* its own buffer: the game may use ui_buf() while drawing */
    for (int p = 0; p < ui.players; p++) {
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

/* ---- layout ------------------------------------------------------------------------------------------------------ */
/* the buttons shown: alone (one player), no flip nor face to face */
static int shown(int i) { return (ui.players > 1 && !is_view()) || (i != B_FLIP && i != B_F2F); }

/* VIEW_* games: the table takes all the room between the bars. Portrait: the others' bars at the top of the screen,
 * the table, the viewer's bar, the buttons at the bottom. Landscape: the table on the left; on the right the others'
 * bars from the top, the buttons (two rows), the viewer's bar at the bottom. */
static float view_frame(float m, box *top, box *bottom)
{
    float u;
    int nb = 0;
    for (int i = 0; i < NBTN; i++) nb += shown(i);
    if (ui.w <= ui.h) {
        u = floorf(fminf((ui.w - 2 * m) / 9, (ui.h - 2 * m) / 12.9f));
        float x = floorf((ui.w - 9 * u) / 2), y = floorf(m);
        L.br = 0.56f * u;
        *top = (box){x, y, 9 * u, u};
        *bottom = (box){x, floorf(ui.h - m - 1.35f * u - 2 * L.br), 9 * u, u};
        L.board = (box){x, y + 1.2f * u, 9 * u, bottom->y - 0.2f * u - (y + 1.2f * u)};
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = ui.w / 2.0f + (k++ - (nb - 1) / 2.0f) * 1.5f * u;
            L.by[i] = bottom->y + bottom->h + 0.35f * u + L.br;
        }
    } else {
        u = floorf(fminf((ui.h - 2 * m) / 9, (ui.w - 2 * m) / 13.45f));
        float y = floorf((ui.h - 9 * u) / 2), cx = floorf(ui.w - m - 4 * u);
        L.board = (box){floorf(m), y, cx - 0.45f * u - floorf(m), 9 * u};
        *top = (box){cx, y, 4 * u, u};
        *bottom = (box){cx, y + 8 * u, 4 * u, u};
        L.br = 0.55f * u;
        int per = (nb + 1) / 2;
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = cx + 2 * u + (k % per - (per - 1) / 2.0f) * 1.35f * u;
            L.by[i] = y + (k / per ? 7.2f : 5.95f) * u;
            k++;
        }
    }
    return u;
}

static void layout_game(void)
{
    float m = fminf(ui.w, ui.h) * 0.025f, u;
    box top, bottom;
    if (is_view()) u = view_frame(m, &top, &bottom);
    else if (ui.w <= ui.h) {                    /* portrait: bar, board, bar, buttons */
        u = floorf(fminf((ui.w - 2 * m) / 9, (ui.h - 2 * m) / 12.9f));
        float y = floorf((ui.h - 12.9f * u) / 2), x = floorf((ui.w - 9 * u) / 2);
        top = (box){x, y, 9 * u, u};
        L.board = (box){x, y + 1.2f * u, 9 * u, 9 * u};
        bottom = (box){x, L.board.y + 9.2f * u, 9 * u, u};
        L.br = 0.56f * u;
        int nb = 0;
        for (int i = 0; i < NBTN; i++) nb += shown(i);
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = ui.w / 2.0f + (k++ - (nb - 1) / 2.0f) * 1.5f * u;
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
        int nb = 0;
        for (int i = 0; i < NBTN; i++) nb += shown(i);
        int per = (nb + 1) / 2;
        for (int i = 0, k = 0; i < NBTN; i++) {     /* two rows (of three) */
            if (!shown(i)) continue;
            L.bx[i] = cx + 2 * u + (k % per - (per - 1) / 2.0f) * 1.35f * u;
            L.by[i] = y + 4.5f * u + (k / per ? 0.7f : -0.7f) * u;
            k++;
        }
    }
    ui.unit = u;
    /* each player's bar on their side of the board (home()), in their order from the left: side by side sharing the
     * bar's width (portrait), or stacked (landscape: the column is narrow) */
    int side[2][MAX_PLAYERS], n[2] = {0, 0};
    float key[MAX_PLAYERS];
    for (int p = 0; p < ui.players; p++) {
        float fx = 0.5f, fy = 0;
        if (cur >= 0 && games[cur]->home && !is_view()) games[cur]->home(p, &fx, &fy);
        key[p] = ui.flipped ? 1 - fx : fx;
        if (is_view()) key[p] = (float)((p - ui_bottom() + ui.players) % ui.players);   /* in turn order */
        int s = seat_top(p), k = n[s]++;
        while (k > 0 && key[side[s][k - 1]] > key[p]) { side[s][k] = side[s][k - 1]; k--; }
        side[s][k] = p;
    }
    for (int s = 0; s < 2; s++) {
        box b = s ? top : bottom;
        for (int k = 0; k < n[s]; k++) {
            box c = b;
            if (ui.w <= ui.h) {
                float gap = (n[s] >= 5 ? 0.12f : 0.25f) * u;
                c.w = floorf((b.w - (n[s] - 1) * gap) / n[s]);
                c.x = b.x + k * (c.w + gap);
            } else if (is_view()) {             /* the others' bars from the top, above the buttons */
                c.y = b.y + k * fminf(1.1f * u, 4.2f * u / fmaxf(1, n[s] - 1));
            } else {
                c.y = b.y + (s ? 1 : -1) * k * 1.1f * u;
            }
            L.bar[side[s][k]] = c;
        }
    }
    L.tk = 0.72f * u;
    shared_textures();
    made_game = -1;
    if (bar_w != (int)top.w || bar_h != (int)top.h) {
        ui_free(&t_bartex);
        bar_w = (int)top.w;
        bar_h = (int)top.h;
        bar_m = (int)(0.7f * u);                /* room around the bar: the gold light spills over its edges */
        t_bartex = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, bar_w + 2 * bar_m,
                                     bar_h + 2 * bar_m);
        if (t_bartex) SDL_SetTextureBlendMode(t_bartex, SDL_BLENDMODE_BLEND);
    }
    if (cur >= 0) games[cur]->layout(L.board);
}

/* a tab's picture: the pieces of its section (a draughts man, a knight, a die, a card, a question, a marble) */
static void tab_icon(int s, int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    if (s == SEC_STRATEGY) art_counter(COUNTER_MAN, 0, d, o);
    else if (s == SEC_CHESS) art_piece(KNIGHT, WHITE, d, o);
    else if (s == SEC_GUESS) art_icon(ICON_HELP, d, o);
    else if (s == SEC_SOLO) art_counter(COUNTER_STONE, 5, d, o);
    else {                                      /* a die, a card: smaller, in the middle */
        int h = (int)(d * (s == SEC_RACE ? 0.78f : 0.92f));
        int w = s == SEC_RACE ? h : (int)(h / cards_aspect(DECK_FRENCH));
        uint8_t *b = calloc((size_t)w * h, 4);
        if (s == SEC_RACE) art_die(5, 0, h, b);
        else cards_paint(DECK_FRENCH, CARD(SUIT_SPADES, 1), w, h, b);
        art_blend(o, d, d, b, w, h, (d - w) / 2, (d - h) / 2);
        free(b);
    }
}

static void tab_textures(int tw, int th)
{
    if (made_tab == tw * 1000 + th) return;
    made_tab = tw * 1000 + th;
    const uint8_t off[4] = {40, 56, 50, 255}, on[4] = {150, 118, 46, 255};
    ui_free(&t_tab);
    art_roundrect(tw, th, th * 0.3f, off, ui_buf(tw, th));
    t_tab = ui_tex(tw, th, ui_buf(tw, th));
    ui_free(&t_tab_on);
    art_roundrect(tw, th, th * 0.3f, on, ui_buf(tw, th));
    t_tab_on = ui_tex(tw, th, ui_buf(tw, th));
    int d = (int)(th * 0.66f);
    uint8_t *b = calloc((size_t)d * d, 4);       /* its own buffer: cards_paint may use ui_buf() */
    for (int s = 0; s < SECTIONS; s++) {
        ui_free(&t_tabicon[s]);
        tab_icon(s, d, b);
        t_tabicon[s] = ui_tex(d, d, b);
    }
    free(b);
    SDL_SetTextureColorMod(t_tabicon[SEC_GUESS], 246, 238, 220);
}

/* the menu (all the games) or the picker (one game's variants): cards in a grid */
static void layout_cards(int n)
{
    float m = fminf(ui.w, ui.h) * 0.05f, g = fminf(ui.w, ui.h) * 0.04f, head = fminf(ui.w, ui.h) * 0.16f;
    float top = head;                           /* the buttons' row (the tabs, the pills go below it) */
    L.ntabs = 0;
    if (screen == SCR_MENU) {                   /* the sections as tabs under the buttons, a picture each */
        for (int s = 0; s < SECTIONS; s++)
            for (int i = 0; i < n; i++)
                if (section_of(games[i]) == s) { L.tab_sec[L.ntabs++] = s; break; }
        int shown_tab = 0;
        for (int k = 0; k < L.ntabs; k++) shown_tab |= L.tab_sec[k] == menu.tab;
        if (!shown_tab && L.ntabs) menu.tab = L.tab_sec[0];
        float th = floorf(fminf(ui.w, ui.h) * 0.12f), tg = floorf(g * 0.4f);
        float tw = floorf((ui.w - 2 * m - (L.ntabs - 1) * tg) / (L.ntabs ? L.ntabs : 1));
        float tx = (ui.w - (L.ntabs * tw + (L.ntabs - 1) * tg)) / 2;
        for (int k = 0; k < L.ntabs; k++) L.tabs[k] = (box){floorf(tx + k * (tw + tg)), floorf(head), tw, th};
        head += th + floorf(g * 0.3f);
        tab_textures((int)tw, (int)th);
    }
    L.npills = 0;
    if (screen == SCR_PICK && many_counts(cur)) {   /* how many play: a row of pills under the buttons, each with
                                                     * that many of the game's tokens */
        unsigned cnt = player_counts(cur);
        int most = 0;
        for (int k = 1; k <= MAX_PLAYERS; k++)
            if (cnt >> k & 1) { L.pill_n[L.npills++] = k; most = k; }
        float ph = fminf(ui.w, ui.h) * 0.12f, pw = ph * (0.5f + most * 0.44f), pg = ph * 0.3f;
        float fit = (ui.w - 2 * m) / (L.npills * pw + (L.npills - 1) * pg);
        if (fit < 1) { ph *= fit; pw *= fit; pg *= fit; }
        ph = floorf(ph);
        pw = floorf(pw);
        float x = (ui.w - (L.npills * pw + (L.npills - 1) * pg)) / 2;
        for (int k = 0; k < L.npills; k++) L.pills[k] = (box){floorf(x + k * (pw + pg)), floorf(head), pw, ph};
        head += ph + g;
        int ts = (int)(ph * 0.56f), key = cur * 100000 + (int)pw * 100 + (int)ph;
        if (made_pill != key) {
            made_pill = key;
            const uint8_t off[4] = {54, 72, 64, 255}, on[4] = {150, 118, 46, 255};
            ui_free(&t_pill);
            art_roundrect((int)pw, (int)ph, ph / 2, off, ui_buf((int)pw, (int)ph));
            t_pill = ui_tex((int)pw, (int)ph, ui_buf((int)pw, (int)ph));
            ui_free(&t_pill_on);
            art_roundrect((int)pw, (int)ph, ph / 2, on, ui_buf((int)pw, (int)ph));
            t_pill_on = ui_tex((int)pw, (int)ph, ui_buf((int)pw, (int)ph));
            uint8_t *b = calloc((size_t)ts * ts, 4);   /* its own buffer: the game may use ui_buf() */
            for (int p = 0; p < MAX_PLAYERS; p++) {
                ui_free(&t_ptok[p]);
                if (p >= most) continue;
                memset(b, 0, (size_t)ts * ts * 4);
                games[cur]->token(p, ts, b);
                t_ptok[p] = ui_tex(ts, ts, b);
            }
            free(b);
        }
    }
    int cols = ui.w > ui.h ? (n > 3 ? 3 : n) : (n > 1 ? 2 : 1);
    if (screen == SCR_PICK) cols = ui.w > ui.h ? (n <= 5 ? n : (n + 1) / 2) : n <= 3 ? n : 2;   /* landscape: two rows
                                                 * past five (draughts' eight) */
    if (screen == SCR_PICK && ui.w <= ui.h && n == 3) cols = 1;
    if (screen == SCR_MENU) {                   /* cards about a quarter of the screen's width: as many as fit */
        cols = (int)((ui.w - 2 * m + g) / (fminf(ui.w, ui.h) * 0.26f + g));
        cols = cols < 2 ? 2 : cols > n ? n : cols;
        if (n == 1) cols = 1;
    }
    int rows = (n + cols - 1) / cols;
    float lab = screen == SCR_MENU ? 0.22f : 0;  /* the menu's cards: the game's name under its picture */
    float c = (ui.w - 2 * m - (cols - 1) * g) / cols;
    if (screen != SCR_MENU) c = fminf(c, (ui.h - head - 2 * m - (rows - 1) * g) / rows / (1 + lab));
    c = floorf(fminf(c, fminf(ui.w, ui.h) * 0.62f));
    float ch = floorf(c * (1 + lab)), x0 = (ui.w - (cols * c + (cols - 1) * g)) / 2, tall = 0;
    L.sec_h = floorf(c * 0.34f);
    L.sec_x = floorf(x0);
    L.sec_w = cols * c + (cols - 1) * g;
    for (int s = 0; s < SECTIONS; s++) L.sec_y[s] = -1;
    if (screen == SCR_MENU) {                   /* the sections one after the other: a heading, then its games */
        for (int s = 0, i = 0; s < SECTIONS; s++) {
            int first = i;
            while (i < n && section_of(games[i]) == s) i++;
            if (i == first) continue;
            if (s != menu.tab) {                /* another tab's: not shown */
                for (int j = first; j < i; j++) L.cards[j] = (box){0, 0, 0, 0};
                continue;
            }
            L.sec_y[s] = tall;
            tall += L.sec_h;
            for (int j = first; j < i; j++)
                L.cards[j] = (box){floorf(x0 + (j - first) % cols * (c + g)), tall + (j - first) / cols * (ch + g), c,
                                   ch};
            tall += (i - first + cols - 1) / cols * (ch + g);
        }
        tall -= g;
    } else {
        for (int i = 0; i < n; i++) L.cards[i] = (box){floorf(x0 + (i % cols) * (c + g)), (i / cols) * (ch + g), c, ch};
        tall = rows * ch + (rows - 1) * g;
    }
    float y0 = head + (ui.h - head - tall) / 2;
    if (screen == SCR_MENU) {                   /* from the top, scrolling when taller than the screen (the picker
                                                 * keeps the menu's place, for the way back) */
        y0 = head;
        menu.top = head;
        menu.max = fmaxf(0, y0 + tall + m - ui.h);
        menu.scroll = fminf(menu.max, fmaxf(0, menu.scroll));
    }
    if (L.npills) {                             /* the picker: the pills and the cards, one block in the middle */
        float ph = L.pills[0].h, start = top + fmaxf(0, (ui.h - top - m - (ph + g + tall)) / 2);
        for (int k = 0; k < L.npills; k++) L.pills[k].y = floorf(start);
        y0 = start + ph + g;
    }
    for (int i = 0; i < n; i++) L.cards[i].y = floorf(L.cards[i].y + y0);
    for (int s = 0; s < SECTIONS; s++)
        if (L.sec_y[s] >= 0) L.sec_y[s] = floorf(L.sec_y[s] + y0);
    L.cols = cols;
    L.label = (int)(ch - c);
    if (made_card != (int)c * 1000 + L.label) {
        for (int i = 0; i < MAX_GAMES; i++) ui_free(&t_name[i]);
        for (int s = 0; s < SECTIONS; s++) ui_free(&t_sec[s]);
    }
    L.card = (int)c;
    L.snd_r = top * 0.28f;
    L.snd_x = ui.w - m - L.snd_r;
    L.snd_y = top * 0.55f;
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
            for (int v = 0; v <= MAX_VARIANTS; v++) ui_free(&t_thumb[i][v]);
    }
}

static void layout(void)
{
    if (screen == SCR_GAME) layout_game();
    else layout_cards(screen == SCR_MENU ? ngames : games[cur]->variants);
}

/* ---- the screens ------------------------------------------------------------------------------------------------- */
/* the picker comes before a new game when there is something to choose: a variant, how many play */
static int needs_picker(int g) { return games[g]->variants > 1 || many_counts(g); }

/* ---- the hand-off ------------------------------------------------------------------------------------------------ */
/* the view goes to `player`: covered first (VIEW_HIDDEN, while the game is on), or at once */
static void view_to(int player)
{
    hand.pending = -1;
    view_of[cur] = player;
    if (games[cur]->view == VIEW_HIDDEN && games[cur]->status() == GS_PLAY) {
        hand.up = 1;
        hand.player = player;
        ui.viewer = -1;
    } else {
        hand.up = 0;
        hand.player = player;
        ui.viewer = player;
    }
    if (screen == SCR_GAME) layout();
}

/* the start button: the cover lifts, the view is the player's */
static void uncover(void)
{
    hand.up = 0;
    ui.viewer = view_of[cur] = hand.player;
    if (screen == SCR_GAME) layout();
}

void ui_handoff(int player)
{
    if (!is_view() || player < 0 || player >= ui.players) return;
    if (player == ui.viewer && !hand.up) { hand.pending = -1; return; }
    hand.pending = player;
    need_frame = 1;
}

int ui_handing(void) { return is_view() && (hand.up || hand.pending >= 0); }

/* a waiting hand-off happens once the last move has landed (the game over: none, the view stays) */
static void handoff_settle(void)
{
    if (!is_view() || hand.pending < 0) return;
    const game *g = games[cur];
    if (g->status() != GS_PLAY) { hand.pending = -1; return; }
    if (g->busy && g->busy()) { need_frame = 1; return; }
    view_to(hand.pending);
}

/* a new game: the player to move gets the view (covered: VIEW_HIDDEN); a game opened again: the cover first */
static void view_start(int fresh)
{
    hand.pending = -1;
    hand.up = 0;
    if (!is_view()) { ui.viewer = -1; return; }
    int p = fresh ? games[cur]->turn() : view_of[cur];
    view_to(p < 0 || p >= ui.players ? 0 : p);
}

/* one player: no flip nor face to face (their buttons are hidden); the settings come back for the next game of
 * two or more */
static void solo_view(void)
{
    static int saved = -1, saved_f2f;
    if (ui.players == 1 && saved < 0) {
        saved = ui.flipped;
        saved_f2f = ui.f2f;
        ui.flipped = ui.f2f = 0;
    } else if (ui.players > 1 && saved >= 0) {
        ui.flipped = saved;
        ui.f2f = saved_f2f;
        saved = -1;
    }
}

static void open_game(int i, int fresh_variant)
{
    cur = i;
    ui.players = players_of(i);
    solo_view();
    int fresh = 0;
    if (fresh_variant >= 0) {
        variant[i] = fresh_variant;
        games[i]->start(fresh_variant);
        started[i] = fresh = 1;
    } else if (!started[i]) {
        if (needs_picker(i)) {
            screen = SCR_PICK;
            layout();
            return;
        }
        games[i]->start(0);
        started[i] = fresh = 1;
    }
    view_start(fresh);
    screen = SCR_GAME;
    over_hidden = 0;
    armed = 0;
    layout();
    last_status = games[cur]->status();
    end_seen = last_status != GS_PLAY;          /* a game that had ended: its card at once, no tune again */
}

static void go_home(void)
{
    if (is_view() && hand.pending >= 0) view_of[cur] = hand.pending;   /* the next player's, when it comes back */
    hand.pending = -1;
    screen = SCR_MENU;
    layout();
}

static void new_game(void)
{
    armed = 0;
    if (needs_picker(cur)) {
        started[cur] = 0;
        screen = SCR_PICK;
        layout();
        return;
    }
    games[cur]->start(variant[cur]);
    view_start(1);
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

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
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
    int won = (ui.players > 1 && (winners_of(st) >> p & 1)) || (ui.players == 1 && st == GS_WIN0);
    int narrow = b.w < 1.9f * ui.unit;          /* five or six bars side by side: closer */
    float cy = b.y + b.h / 2, tx = b.x + (narrow ? 0.06f : 0.15f) * ui.unit + L.tk / 2;
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
        float x = tx + L.tk / 2 + (narrow ? 0.12f : 0.3f) * ui.unit;
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
    bar_content(p, (box){(float)bar_m, (float)bar_m, b.w, b.h});
    SDL_SetRenderTarget(ui.r, NULL);
    SDL_Rect src = {0, 0, (int)b.w + 2 * bar_m, (int)b.h + 2 * bar_m};   /* this bar's part (bars may share a side) */
    SDL_FRect d = {b.x + b.w / 2 - src.w / 2.0f, b.y + b.h / 2 - src.h / 2.0f, (float)src.w, (float)src.h};
    SDL_RenderCopyExF(ui.r, t_bartex, &src, &d, 180, NULL, SDL_FLIP_NONE);
}

/* the players who share the win (the end card, the bars): winners() when the game has it, else gs_winner(); 0: none
 * (playing, or a draw without winners()) */
static unsigned winners_of(int st)
{
    if (st == GS_PLAY) return 0;
    const game *g = games[cur];
    unsigned m = g->winners ? g->winners() & ((1u << ui.players) - 1) : 0;
    if (m) return m;
    int w = gs_winner(st);
    return w >= 0 ? 1u << w : 0;
}

/* the end card with winners(): the winners standing side by side (in gold light, unless it is a draw: then the tied
 * players only), the others lying down, dimmed */
static void draw_winners(unsigned won, int draw)
{
    box b = L.board;
    float u = ui.unit, cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    int nw = 0, nl = 0;
    for (int p = 0; p < ui.players; p++) nw += won >> p & 1;
    nl = ui.players - nw;
    float wx = nl ? cx - 1.25f * u : cx, ww = nl ? 3.9f * u : 6.2f * u, s = fminf(2.1f * u, ww / nw / 1.05f);
    for (int p = 0, k = 0; p < ui.players; p++) {
        if (!(won >> p & 1)) continue;
        float x = wx + (k++ - (nw - 1) / 2.0f) * s * 1.05f;
        if (!draw) ui_put_c(t_gold, x, cy, s * 1.3f, s * 1.3f);
        ui_put_c(t_big[p], x, cy, s, s);
    }
    if (!nl) return;
    int rows = nl > 2 ? 2 : 1, per = (nl + rows - 1) / rows;
    float ls = fminf(1.3f * u, 2.3f * u / per), lx = cx + 2.1f * u;
    for (int p = 0, k = 0; p < ui.players; p++) {
        if (won >> p & 1) continue;
        int r = k / per, c = k % per;
        SDL_SetTextureAlphaMod(t_big[p], 150);
        ui_put_rot(t_big[p], lx + (c - (per - 1) / 2.0f) * ls * 1.1f, cy + (r - (rows - 1) / 2.0f) * ls * 1.15f, ls,
                   ls, 90);
        SDL_SetTextureAlphaMod(t_big[p], 255);
        k++;
    }
}

static void draw_over(void)
{
    const game *g = games[cur];
    int st = g->status();
    box b = L.board;
    float u = ui.unit, cx = b.x + b.w / 2, cy = b.y + b.h / 2, big = 2.4f * u;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 90);
    ui_put_c(t_card, cx, cy, 6.8f * u, 3.8f * u);
    int np = ui.players;
    unsigned won = g->winners ? winners_of(st) : 0;
    if (np > 1 && won) {                       /* winners() (partners, a shared win, a tie): they stand together */
        draw_winners(won, st == GS_DRAW);
    } else if (np == 1) {                              /* alone: in gold light when it won, else lying down */
        if (st == GS_WIN0) ui_put_c(t_gold, cx, cy, big * 1.25f, big * 1.25f);
        if (st == GS_WIN0) ui_put_c(t_big[0], cx, cy, big, big);
        else ui_put_rot(t_big[0], cx, cy + 0.3f * u, 2.0f * u, 2.0f * u, 90);
    } else if (st == GS_DRAW && np > 2) {      /* everyone standing in a row */
        float k = fminf(2.0f * u, 6.0f * u / np);
        for (int p = 0; p < np; p++) ui_put_c(t_big[p], cx + (p - (np - 1) / 2.0f) * k * 1.05f, cy, k, k);
    } else if (np > 2) {                        /* the winner in gold light, the others lying down, smaller */
        int w = gs_winner(st), k = 0;
        ui_put_c(t_gold, cx - 1.6f * u, cy, big * 1.25f, big * 1.25f);
        ui_put_c(t_big[w], cx - 1.6f * u, cy, big, big);
        int rows = np > 3 ? 2 : 1, per = (np - 1 + rows - 1) / rows;
        float s = fminf(1.3f * u, 2.9f * u / (per * 1.1f)), x0 = cx + 0.15f * u + (3.0f * u - per * s * 1.1f) / 2;
        for (int p = 0; p < np; p++) {         /* in the card's right part, dimmed (a round piece lying down
                                                 * looks the same) */
            if (p == w) continue;
            int r = k / per, c = k % per;
            SDL_SetTextureAlphaMod(t_big[p], 150);
            ui_put_rot(t_big[p], x0 + (c + 0.5f) * s * 1.1f, cy + (r - (rows - 1) / 2.0f) * s * 1.15f, s, s, 90);
            SDL_SetTextureAlphaMod(t_big[p], 255);
            k++;
        }
    } else if (st == GS_DRAW) {                 /* both standing, an equals sign between them */
        float k = 2.0f * u;
        ui_put_c(t_big[0], cx - 1.7f * u, cy, k, k);
        ui_put_c(t_big[1], cx + 1.7f * u, cy, k, k);
        ui_fill(cx - 0.32f * u, cy - 0.22f * u, 0.64f * u, 0.13f * u, 246, 238, 220, 255);
        ui_fill(cx - 0.32f * u, cy + 0.09f * u, 0.64f * u, 0.13f * u, 246, 238, 220, 255);
    } else {                                    /* the winner in gold light, the loser lying down */
        int w = gs_winner(st) == 1;
        ui_put_c(t_gold, cx - 1.0f * u, cy, big * 1.25f, big * 1.25f);
        ui_put_c(t_big[w], cx - 1.0f * u, cy, big, big);
        ui_put_rot(t_big[!w], cx + 1.6f * u, cy + 0.3f * u, 1.7f * u, 1.7f * u, 90);
    }
}

/* the hand-off cover, over the table greyed (drawn with ui.viewer -1: every hand hidden, only what all players see)
 * and its bars and buttons. The next player's token big in gold light, the picture "pass the device",
 * a big round start button; home and the rules in the corners (as on the picker). Portrait: a column; landscape: a
 * row. */
static struct { float tx, ty, td, ix, iy, id, sx, sy, sr, hx, hy, qx, qy, r; } C;

static void cover_layout(void)
{
    float s = fminf(ui.w, ui.h), head = s * 0.16f;
    C.r = head * 0.28f;
    C.hx = s * 0.05f + C.r;
    C.hy = C.qy = head * 0.55f;
    C.qx = ui.w - s * 0.05f - C.r;
    C.td = floorf(s * 0.34f);
    C.id = floorf(s * 0.26f);
    C.sr = floorf(s * 0.12f);
    if (ui.w <= ui.h) {
        C.tx = C.ix = C.sx = ui.w / 2.0f;
        C.ty = ui.h * 0.31f;
        C.iy = ui.h * 0.54f;
        C.sy = ui.h * 0.75f;
    } else {
        C.ty = C.iy = C.sy = ui.h * 0.54f;
        C.tx = ui.w * 0.22f;
        C.ix = ui.w * 0.5f;
        C.sx = ui.w * 0.78f;
    }
}

static void draw_cover(void)
{
    static SDL_Texture *tok, *icon, *disc, *play;
    static int tok_key = -1, icon_d, disc_d;
    cover_layout();
    int key = (cur * 8 + hand.player) * 4096 + (int)C.td;
    if (tok_key != key) {                       /* its own buffer: the game may use ui_buf() */
        tok_key = key;
        int d = (int)C.td;
        uint8_t *b = calloc((size_t)d * d, 4);
        games[cur]->token(hand.player, d, b);
        ui_free(&tok);
        tok = ui_tex(d, d, b);
        free(b);
    }
    if (icon_d != (int)C.id || disc_d != (int)(2 * C.sr)) {
        static const uint8_t gold[3] = {150, 118, 46};
        icon_d = (int)C.id;
        disc_d = (int)(2 * C.sr);
        ui_free(&icon);
        art_icon(ICON_DEVICE, icon_d, ui_buf(icon_d, icon_d));
        icon = ui_tex(icon_d, icon_d, ui_buf(icon_d, icon_d));
        SDL_SetTextureColorMod(icon, 246, 238, 220);
        SDL_SetTextureAlphaMod(icon, 200);
        ui_free(&disc);
        art_disc(disc_d, gold, 1, 0, ui_buf(disc_d, disc_d));
        disc = ui_tex(disc_d, disc_d, ui_buf(disc_d, disc_d));
        int pd = (int)(1.2f * C.sr);
        ui_free(&play);
        art_icon(ICON_PLAY, pd, ui_buf(pd, pd));
        play = ui_tex(pd, pd, ui_buf(pd, pd));
        SDL_SetTextureColorMod(play, 252, 244, 226);
    }
    ui_put_c(t_gold, C.tx, C.ty, C.td * 1.7f, C.td * 1.7f);
    ui_put_c(tok, C.tx, C.ty, C.td, C.td);
    ui_put_c(icon, C.ix, C.iy, C.id, C.id);
    ui_put_c(t_gold, C.sx, C.sy, C.sr * 3.2f, C.sr * 3.2f);
    ui_put_c(disc, C.sx, C.sy, 2 * C.sr, 2 * C.sr);
    ui_put_c(play, C.sx + 0.06f * C.sr, C.sy, 1.2f * C.sr, 1.2f * C.sr);
    draw_button(C.hx, C.hy, C.r, ICON_HOME, NULL);
    draw_button(C.qx, C.qy, C.r, ICON_HELP, NULL);
}

static void draw_game(void)
{
    const game *g = games[cur];
    handoff_settle();
    game_textures();
    g->draw();                                  /* under the cover too: ui.viewer -1, no hand shown */
    for (int p = 0; p < ui.players; p++) draw_bar(p);
    if (armed && SDL_GetTicks() - armed > 2500) armed = 0;
    for (int i = 0; i < NBTN; i++) {
        if (!shown(i)) continue;
        SDL_Texture *disc = i == B_NEW && armed ? t_btn_armed : i == B_F2F && ui.f2f ? t_btn_on : NULL;
        draw_button(L.bx[i], L.by[i], L.br, BTN_ICON[i], disc);
    }
    if (hand.up) {
        ui_fill(0, 0, (float)ui.w, (float)ui.h, 14, 22, 18, 175);
        draw_cover();
        return;
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
    if (k < 0 || k > MAX_VARIANTS) return NULL;
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
    float px = h * 0.42f;
    int tw = text_line(name, px, 1, CREAM, NULL, 0, 0, 0, 0);
    if (tw > 0.9f * w) {
        px *= 0.9f * w / tw;
        tw = text_line(name, px, 1, CREAM, NULL, 0, 0, 0, 0);
    }
    /* too long for one line ("Snakes and Ladders"): two, split at the space nearest the middle, if that is bigger */
    char *cut = NULL;
    for (char *s = strchr(name, ' '); s; s = strchr(s + 1, ' '))
        if (!cut || abs((int)(s - name) * 2 - (int)strlen(name)) < abs((int)(cut - name) * 2 - (int)strlen(name)))
            cut = s;
    float px2 = h * 0.36f;
    int w1 = 0, w2 = 0;
    if (cut && px < 0.8f * h * 0.42f) {
        *cut = 0;
        w1 = text_line(name, px2, 1, CREAM, NULL, 0, 0, 0, 0);
        w2 = text_line(cut + 1, px2, 1, CREAM, NULL, 0, 0, 0, 0);
        int wide = w1 > w2 ? w1 : w2;
        if (wide > 0.9f * w) px2 *= 0.9f * w / wide;
        if (px2 > px) {
            w1 = text_line(name, px2, 1, CREAM, NULL, 0, 0, 0, 0);
            w2 = text_line(cut + 1, px2, 1, CREAM, NULL, 0, 0, 0, 0);
        } else {
            *cut = ' ';
            cut = NULL;
        }
    } else {
        cut = NULL;
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    if (cut) {
        float lh = 1.18f * px2, y = (h - 2 * lh) / 2;
        text_line(name, px2, 1, CREAM, b, w, h, (w - w1) / 2, (int)y);
        text_line(cut + 1, px2, 1, CREAM, b, w, h, (w - w2) / 2, (int)(y + lh));
    } else {
        text_line(name, px, 1, CREAM, b, w, h, (w - tw) / 2, (int)((h - 1.25f * px) / 2));
    }
    t_name[i] = ui_tex(w, h, b);
    free(b);
    return t_name[i];
}

/* a section's heading: its line of help/<lang>/_menu.txt, bold, cream, then a thin rule to the right edge */
static SDL_Texture *heading(int s)
{
    if (t_sec[s] || L.sec_h < 4) return t_sec[s];
    const char *t = text_get("_menu");
    for (int k = 0; t && k < s; k++) {
        t = strchr(t, '\n');
        if (t) t++;
    }
    if (!t || !*t) return NULL;
    char name[96];
    size_t n = strcspn(t, "\n");
    if (n >= sizeof name) n = sizeof name - 1;
    memcpy(name, t, n);
    name[n] = 0;
    int w = (int)L.sec_w, h = (int)L.sec_h;
    float px = h * 0.40f;
    int tw = text_line(name, px, 1, CREAM, NULL, 0, 0, 0, 0);
    if (tw > 0.8f * w) {
        px *= 0.8f * w / tw;
        tw = text_line(name, px, 1, CREAM, NULL, 0, 0, 0, 0);
    }
    uint8_t *b = calloc((size_t)w * h, 4);
    int ty = (int)(h * 0.62f - px * 0.75f);
    text_line(name, px, 1, CREAM, b, w, h, 0, ty);
    for (int y = (int)(h * 0.62f - px * 0.32f), x = tw + (int)(px * 0.6f); x < w; x++) {   /* the rule */
        uint8_t *p = b + 4 * (y * w + x);
        p[0] = CREAM[0], p[1] = CREAM[1], p[2] = CREAM[2], p[3] = 60;
    }
    t_sec[s] = ui_tex(w, h, b);
    free(b);
    return t_sec[s];
}

static void draw_cards(void)
{
    int n = screen == SCR_MENU ? ngames : games[cur]->variants;
    for (int k = 0; k < L.ntabs && screen == SCR_MENU; k++) {   /* the tabs */
        box b = L.tabs[k];
        int s = L.tab_sec[k];
        float d = floorf(b.h * 0.66f);
        ui_put(s == menu.tab ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
        ui_put_c(t_tabicon[s], b.x + b.w / 2, b.y + b.h / 2, d, d);
    }
    for (int k = 0; k < L.npills; k++) {       /* the picker: how many play */
        box b = L.pills[k];
        int np = L.pill_n[k];
        float ts = floorf(b.h * 0.56f), step = b.h * 0.44f, x0 = b.x + b.w / 2 - (np - 1) * step / 2;
        ui_put(np == ui.players ? t_pill_on : t_pill, b.x, b.y, b.w, b.h);
        for (int p = 0; p < np; p++) ui_put_c(t_ptok[p], x0 + p * step, b.y + b.h / 2, ts, ts);
    }
    float dy = 0;
    if (screen == SCR_MENU && menu.max > 0) {   /* the cards' window, below the buttons */
        if (menu.vel != 0 && !menu.grabbing) {  /* the fling: slows down, stops at the ends */
            Uint32 t = SDL_GetTicks();
            float dt = fminf(50, (float)(t - menu.t));
            menu.t = t;
            menu.scroll = fminf(menu.max, fmaxf(0, menu.scroll + menu.vel * dt));
            menu.vel *= powf(0.996f, dt);
            if (fabsf(menu.vel) < 0.02f || menu.scroll <= 0 || menu.scroll >= menu.max) menu.vel = 0;
            else need_frame = 1;
        }
        dy = -menu.scroll;
        SDL_Rect clip = {0, (int)menu.top, ui.w, ui.h - (int)menu.top};
        SDL_RenderSetClipRect(ui.r, &clip);
    }
    for (int i = 0; i < n; i++) {
        box c = L.cards[i];
        c.y += dy;
        if (c.w <= 0) continue;                 /* another tab's game */
        if (c.y > ui.h || c.y + c.h * 1.2f < menu.top) continue;
        int gi = screen == SCR_MENU ? i : cur, v = screen == SCR_MENU ? -1 : i;
        if (screen == SCR_MENU && started[i] && games[i]->status() == GS_PLAY)   /* a game in progress */
            ui_put_c(t_gold, c.x + c.w / 2, c.y + c.h / 2, c.w * 1.25f, c.h * 1.25f);
        ui_put(t_menu_card, c.x, c.y, c.w, c.h);
        float s = c.w * 0.86f;
        ui_put_c(thumb(gi, v), c.x + c.w / 2, c.y + c.w / 2, s, s);
        if (screen == SCR_MENU) ui_put(label(i), c.x, c.y + c.w - 0.04f * c.w, c.w, (float)L.label);
    }
    for (int s = 0; s < SECTIONS && screen == SCR_MENU; s++) {
        float y = L.sec_y[s] + dy;
        if (L.sec_y[s] < 0 || y > ui.h || y + L.sec_h < menu.top) continue;
        ui_put(heading(s), L.sec_x, y, L.sec_w, L.sec_h);
    }
    if (screen == SCR_MENU && menu.max > 0) {   /* where the list is: a thin bar on the right */
        SDL_RenderSetClipRect(ui.r, NULL);
        float vh = ui.h - menu.top, th = vh * vh / (vh + menu.max), ty = menu.top + (vh - th) * menu.scroll / menu.max;
        ui_fill(ui.w - fminf(ui.w, ui.h) * 0.016f, ty, fminf(ui.w, ui.h) * 0.008f, th, 246, 238, 220, 70);
    }
    float home_x = fminf(ui.w, ui.h) * 0.05f + L.snd_r;
    if (screen == SCR_MENU) {
        draw_button(L.snd_x, L.snd_y, L.snd_r, snd_muted ? ICON_MUTE : ICON_SOUND, NULL);
        draw_button(home_x, L.snd_y, L.snd_r, ICON_HELP, NULL);
    } else {
        draw_button(home_x, L.snd_y, L.snd_r, ICON_HOME, NULL);
    }
}

/* ---- the rules page ---------------------------------------------------------------------------------------------- */
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

/* ---- input ------------------------------------------------------------------------------------------------------- */
static int in_circle(float x, float y, float cx, float cy, float r) { return hypotf(x - cx, y - cy) < r * 1.1f; }

static int press_bar(float x, float y)
{
    for (int p = 0; p < ui.players; p++) {
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

/* the menu's tab: another section's games, from the top */
static void menu_tab(int s)
{
    if (menu.tab == s) return;
    menu.tab = s;
    menu.scroll = menu.vel = 0;
    layout();
}

static void menu_step(int d)                    /* the tab on the left (-1) or the right (1) */
{
    for (int k = 0; k < L.ntabs; k++)
        if (L.tab_sec[k] == menu.tab && k + d >= 0 && k + d < L.ntabs) {
            snd_play(SND_TAP);
            menu_tab(L.tab_sec[k + d]);
            return;
        }
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
        for (int k = 0; k < L.npills; k++) {
            box b = L.pills[k];
            if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
            snd_play(SND_TAP);
            nplayers[cur] = ui.players = L.pill_n[k];
            solo_view();
            return;
        }
        for (int k = 0; k < L.ntabs && screen == SCR_MENU; k++) {
            box b = L.tabs[k];
            if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
            snd_play(SND_TAP);
            menu_tab(L.tab_sec[k]);
            return;
        }
        if (screen == SCR_MENU) {               /* a card opens on release, unless the finger scrolled the list */
            menu.grabbing = y >= menu.top;
            menu.moved = menu.vel != 0;         /* a tap that stops a fling opens nothing */
            menu.axis = 0;
            menu.vel = 0;
            menu.grab_x = x;
            menu.grab_y = y;
            menu.grab_scroll = menu.scroll;
            menu.t = SDL_GetTicks();
            return;
        }
        int n = games[cur]->variants;
        for (int i = 0; i < n; i++) {
            box c = L.cards[i];
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
                snd_play(SND_TAP);
                open_game(cur, i);
                return;
            }
        }
        return;
    }
    if (is_view() && hand.up) {                 /* the cover: start (the view is the player's), home, the rules */
        cover_layout();
        if (in_circle(x, y, C.sx, C.sy, C.sr)) { snd_play(SND_TAP); uncover(); }
        else if (in_circle(x, y, C.hx, C.hy, C.r)) { snd_play(SND_TAP); go_home(); }
        else if (in_circle(x, y, C.qx, C.qy, C.r)) { snd_play(SND_TAP); open_help(cur); }
        return;
    }
    for (int i = 0; i < NBTN; i++) {
        if (!shown(i) || !in_circle(x, y, L.bx[i], L.by[i], L.br)) continue;
        if (i == B_HOME) { snd_play(SND_TAP); go_home(); }
        else if (i == B_UNDO && ui_handing()) snd_play(SND_DENY);   /* the turn has passed: no going back */
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
    if (ui_handing()) return;                   /* the device is changing hands */
    if (press_bar(x, y)) return;
    if (games[cur]->status() != GS_PLAY && !over_hidden) { over_hidden = 1; return; }
    for (int p = 0; p < ui.players && games[cur]->bar_press; p++) {   /* the game's own use of a bar */
        box b = L.bar[p];
        if (x < b.x || y < b.y || x >= b.x + b.w || y >= b.y + b.h) continue;
        float fx = x, fy = y;                   /* in the frame bar() drew in */
        if (ui_upside(p) && t_bartex) { fx = bar_m + b.w - (x - b.x); fy = bar_m + b.h - (y - b.y); }
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
    if (screen == SCR_MENU && menu.grabbing) {
        if (!menu.moved && hypotf(x - menu.grab_x, y - menu.grab_y) > fminf(ui.w, ui.h) * 0.02f) {
            menu.moved = 1;
            menu.axis = fabsf(x - menu.grab_x) > fabsf(y - menu.grab_y) ? 1 : 2;
        }
        if (!menu.moved || menu.axis == 1) return;   /* sideways: another tab, on release */
        float s = fminf(menu.max, fmaxf(0, menu.grab_scroll - (y - menu.grab_y)));
        Uint32 t = SDL_GetTicks();
        if (t > menu.t) menu.vel = 0.5f * menu.vel + 0.5f * (s - menu.scroll) / (float)(t - menu.t);
        menu.t = t;
        menu.scroll = s;
        return;
    }
    if (screen == SCR_GAME && !(is_view() && hand.up)) games[cur]->motion(x, y);
}

static void release(float x, float y)
{
    if (help.open) {
        help.grabbing = 0;
        return;
    }
    if (screen == SCR_MENU && menu.grabbing) {
        menu.grabbing = 0;
        if (menu.moved) {                       /* the end of a drag: the list flies on if the finger was quick */
            if (menu.axis == 1) {               /* a swipe: the next tab that way */
                if (fabsf(x - menu.grab_x) > fminf(ui.w, ui.h) * 0.12f) menu_step(x < menu.grab_x ? 1 : -1);
                return;
            }
            if (SDL_GetTicks() - menu.t > 80) menu.vel = 0;
            menu.t = SDL_GetTicks();
            need_frame = 1;
            return;
        }
        for (int i = 0; i < ngames; i++) {
            box c = L.cards[i];
            c.y -= menu.scroll;
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
                snd_play(SND_TAP);
                open_game(i, -1);
                return;
            }
        }
        return;
    }
    if (screen == SCR_GAME && !(is_view() && hand.up)) games[cur]->release(x, y);
}

static void menu_wheel(float dy)
{
    menu.vel = 0;
    menu.scroll = fminf(menu.max, fmaxf(0, menu.scroll + dy));
}

/* ---- several fingers at once (a game's finger(): slaps) -------------------------------------------------------- */
static int fingers_on(void)
{
    return screen == SCR_GAME && games[cur]->finger && !help.open && !(is_view() && hand.up);
}

/* a finger: on the shell's buttons or the bars it is an ordinary tap (press); elsewhere the game's, with its id */
static void touch(Uint32 type, long long id, float x, float y)
{
    enum { MAX_FINGERS = 10 };
    static long long shell_id[MAX_FINGERS];
    static int nshell;
    if (type == SDL_FINGERDOWN) {
        int hit = 0;
        for (int i = 0; i < NBTN && !hit; i++) hit = shown(i) && in_circle(x, y, L.bx[i], L.by[i], L.br);
        for (int p = 0; p < ui.players && !hit; p++)
            hit = x >= L.bar[p].x && y >= L.bar[p].y && x < L.bar[p].x + L.bar[p].w && y < L.bar[p].y + L.bar[p].h;
        if (hit) {
            if (nshell < MAX_FINGERS) shell_id[nshell++] = id;
            press(x, y);
            return;
        }
    }
    for (int k = 0; k < nshell; k++)            /* a finger that began on a button: the shell's to its end */
        if (shell_id[k] == id) {
            if (type == SDL_FINGERUP) shell_id[k] = shell_id[--nshell];
            return;
        }
    if (!fingers_on()) return;                  /* the button left the game */
    int phase = type == SDL_FINGERDOWN ? FINGER_DOWN : type == SDL_FINGERUP ? FINGER_UP : FINGER_MOVE;
    games[cur]->finger((int)(id & 0x7fffffff), phase, x, y);
}

/* ---- the picture mode -------------------------------------------------------------------------------------------- */
static int find_game(const char *id)
{
    for (int i = 0; i < ngames; i++)
        if (!strcmp(games[i]->id, id)) return i;
    return -1;
}

/* VIEW_* games: the clock on past the last tap (it has landed), the hand-off done; reveal: the cover lifted too (the
 * start button tapped) */
static void shot_settle(int reveal, int advance)
{
    if (advance) ms_ahead += 600000;
    handoff_settle();
    if (reveal && is_view() && hand.up) uncover();
}

static int shot(int argc, char **argv)
{
    const char *out = NULL, *id = NULL, *moves = NULL, *taps = NULL, *lang = NULL;
    int v = 0, keep = 0, pick = 0, show_help = 0, np = 0, tab = -1, reveal = 0;
    float scroll = 0;
    uint32_t seed = 1;
    int at = -1;                                /* --at ms: the picture that long after the last tap (else at rest) */
    int first = 1;
    Uint32 base = 0;                            /* the clock before the last tap */
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
        else if (!strcmp(argv[i], "--players") && i + 1 < argc) np = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) tab = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--at") && i + 1 < argc) at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--reveal")) reveal = 1;
    }
    seed_random(seed);                          /* the same dice every time: tests and screenshots repeat */
    text_init(lang ? lang : "en");
    instant = 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");     /* smooth scaling, as in the app */
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, ui.w, ui.h, 32, SDL_PIXELFORMAT_RGBA32);
    ui.r = SDL_CreateSoftwareRenderer(surf);
    if (!ui.r) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_SetRenderDrawBlendMode(ui.r, SDL_BLENDMODE_BLEND);
    screen = SCR_MENU;
    if (id) {
        cur = find_game(id);
        if (cur < 0) { fprintf(stderr, "no game %s\n", id); return 1; }
        if (np) nplayers[cur] = np;             /* --players n: how many play */
        ui.players = players_of(cur);
        if (pick) { screen = SCR_PICK; layout(); }
        else {
            open_game(cur, v);
            if (moves && (!games[cur]->script || !games[cur]->script(moves))) {
                fprintf(stderr, "bad moves\n");
                return 1;
            }
            draw();                             /* once before the taps, as the app always has: the bars' layout */
            if (reveal) shot_settle(1, 0);
            for (const char *s = taps; s && *s; first = 0) {
                float fx, fy;
                int n = 0;
                if (sscanf(s, "%f,%f%n", &fx, &fy, &n) < 2) break;
                if (!first && is_view()) shot_settle(reveal, 1);   /* the last tap has landed, the device passed */
                base = ms_ahead;
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
        if (tab >= 0) menu_tab(tab);           /* --tab n: that section's tab (0 strategy .. 5 solitaires) */
        if (!show_help) menu_wheel(scroll);     /* --scroll px: further down the menu */
    }
    ms_ahead = base + (at >= 0 ? (Uint32)at : 600000);   /* the taps' animations: over, or caught --at ms in */
    if (id && !pick && reveal) shot_settle(1, 0);
    if (show_help) {                            /* the rules page (the game's, or the app's), scrolled by --scroll px */
        open_help(id ? cur : -1);
        help_scroll(scroll);
    }
    draw();
    SDL_RenderPresent(ui.r);
    if (!out || SDL_SaveBMP(surf, out)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    int st = id && started[cur] ? games[cur]->status() : -1;   /* -1: the menu, or a game not started (--pick) */
    printf("%s: %dx%d %s status %d\n", out, ui.w, ui.h, id ? id : "menu", st);
    return 0;
}

/* ---- the program ------------------------------------------------------------------------------------------------- */
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
    if (screen == SCR_MENU && !help.open && (k == SDLK_LEFT || k == SDLK_RIGHT)) {   /* the tabs */
        menu_step(k == SDLK_RIGHT ? 1 : -1);
        return;
    }
    if (screen == SCR_MENU && !help.open &&
        (k == SDLK_DOWN || k == SDLK_UP || k == SDLK_PAGEDOWN || k == SDLK_PAGEUP)) {
        float step = (k == SDLK_PAGEDOWN || k == SDLK_PAGEUP ? 3 : 1) * (L.card + L.label) * 0.5f;
        menu_wheel(k == SDLK_DOWN || k == SDLK_PAGEDOWN ? step : -step);
        return;
    }
    if (screen != SCR_GAME || help.open) return;
    if (k == SDLK_u || k == SDLK_BACKSPACE) { if (!ui_handing()) games[cur]->undo(); }
    else if (k == SDLK_f && shown(B_FLIP)) { ui.flipped = !ui.flipped; layout(); }
    else if (k == SDLK_t && shown(B_F2F)) ui.f2f = !ui.f2f;
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
                case SDL_FINGERDOWN:
                case SDL_FINGERMOTION:
                case SDL_FINGERUP:
                    if (fingers_on()) {
                        touch(e.type, (long long)e.tfinger.fingerId, e.tfinger.x * ui.w, e.tfinger.y * ui.h);
                        after_input();
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    if (e.button.which == SDL_TOUCH_MOUSEID && fingers_on()) break;   /* the fingers say it */
                    to_pixels(e.button.x, e.button.y, &fx, &fy);
                    press(fx, fy);
                    after_input();
                    break;
                case SDL_MOUSEMOTION:
                    if (e.motion.which == SDL_TOUCH_MOUSEID && fingers_on()) break;
                    to_pixels(e.motion.x, e.motion.y, &fx, &fy);
                    motion(fx, fy);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (e.button.button != SDL_BUTTON_LEFT) break;
                    if (e.button.which == SDL_TOUCH_MOUSEID && fingers_on()) break;
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
                    else if (screen == SCR_MENU) menu_wheel(-e.wheel.y * (L.card + L.label) * 0.4f);
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
