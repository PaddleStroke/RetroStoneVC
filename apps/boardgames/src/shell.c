/*
 * RetroStone Board: the shell (shell.h). One app for all the board games: the menu (a card per game), the variant
 * picker, the players' bars, the buttons, the end-of-game card, the sounds, and a picture mode for tests and
 * screenshots. The games bring their rules and their boards (games/<id>.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   retrostoneboard [--save DIR]      the app (a window; full screen on Android); its settings and progress kept in
 *                                     the device's folder for apps (SDL_GetPrefPath), or in DIR
 *   retrostoneboard --shot out.bmp [--size 720x1280] [--game chess] [--variant 0] [--moves "e2e4 e7e5"]
 *                   [--taps "0.5,0.9 0.5,0.7"] [--drag "0.1,0.9 0.5,0.5"] [--hold "0.5,0.5 800"] [--flip] [--f2f]
 *                   [--keep] [--pick] [--players n] [--reveal] [--at ms] [--level n] [--solved "1-12:3 13:2"]
 *                   [--keys "Up Left"] [--random] [--icons] [--save DIR]
 *                                     one picture, no window: the menu (no --game), the picker (--pick) or a game
 *                                     after its moves (its own notation) and gestures (fractions of the board; of the
 *                                     screen on the menu and the pickers); the hand-off cover lifted (--reveal: the
 *                                     start button tapped); a clock of its own (the same picture every time); nothing
 *                                     kept from a past run (--save DIR: that folder's); a game with levels:
 *                                     --pick its level screen, --level n (1..) that level, --solved the progress;
 *                                     --random a random puzzle of a game of levels (--variant n); --pick --tab n,
 *                                     n its chapters' count: its random tab
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

#define MAX_GAMES 256
#define MAX_VARIANTS 15                         /* a picker card each (the thumbnails: one more, the menu's) */

struct ui ui = {.players = 2, .level = -1};
static const game *games[MAX_GAMES];
static int ngames;
static SDL_Window *win;
static int quit, need_frame;

enum { SCR_MENU, SCR_PICK, SCR_GAME, SCR_LEVELS };   /* SCR_LEVELS: the picker of a game with levels */
static int screen, cur = -1;                    /* the game shown (games[cur]) */
static int variant[MAX_GAMES], started[MAX_GAMES];   /* variant: a game with levels, the level */
static int chosen[MAX_GAMES];                   /* a variant was chosen (this run, or kept from the last): the
                                                 * picker shows it in gold light */
static int nplayers[MAX_GAMES];                 /* how many play each game (the picker's choice; 0: not chosen) */
/* a game of levels that also plays at random (start_random): in random play (the level screen's last tab), and the
 * variant picked there */
static int rnd_mode[MAX_GAMES], rvariant[MAX_GAMES];
static int has_random(int g) { return games[g]->levels > 0 && games[g]->start_random; }
static int in_random(int g) { return has_random(g) && rnd_mode[g]; }

/* the hand-off (VIEW_* games): whose view each game shows (kept while away in the menu); the cover (up: hiding the
 * screen until `player` taps start); a hand-off waiting for the last move to land (pending: the player, or -1) */
static int view_of[MAX_GAMES];
static struct { int up, player, pending; } hand = {0, 0, -1};
static int is_view(void) { return cur >= 0 && games[cur]->view != VIEW_BOARD; }
static unsigned winners_of(int st);

/* the menu's sections, in this order: a heading (help/<lang>/_menu.txt, a line each), then their games by menu
 * order. The games not listed are strategy games. */
enum { SEC_STRATEGY, SEC_CHESS, SEC_RACE, SEC_CARDS, SEC_GUESS, SEC_SOLO, SEC_PUZZLE, SECTIONS };
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
    {"pyramid", SEC_SOLO}, {"tripeaks", SEC_SOLO},
    {"sudoku", SEC_PUZZLE}, {"nonogram", SEC_PUZZLE}, {"taquin", SEC_PUZZLE}, {"hanoi", SEC_PUZZLE},
    {"logicgates", SEC_PUZZLE}, {"pipes", SEC_PUZZLE}, {"paths", SEC_PUZZLE}, {"mirrors", SEC_PUZZLE},
    {"tracks", SEC_PUZZLE}, {"untangle", SEC_PUZZLE}, {"lightup", SEC_PUZZLE}, {"binairo", SEC_PUZZLE},
    {"tents", SEC_PUZZLE}, {"skyscrapers", SEC_PUZZLE}, {"futoshiki", SEC_PUZZLE}, {"rectangles", SEC_PUZZLE},
    {"starbattle", SEC_PUZZLE}, {"dominosa", SEC_PUZZLE}, {"watersort", SEC_PUZZLE}, {"ballsort", SEC_PUZZLE},
    {"samegame", SEC_PUZZLE}, {"iceslide", SEC_PUZZLE}, {"merge", SEC_PUZZLE}, {"chesspuzzles", SEC_PUZZLE},
    {"knightstour", SEC_PUZZLE}, {"queens", SEC_PUZZLE},
    {"mines", SEC_PUZZLE}, {"klotski", SEC_PUZZLE}, {"boxpusher", SEC_PUZZLE}, {"calcudoku", SEC_PUZZLE},
    {"kakuro", SEC_PUZZLE}, {"hashi", SEC_PUZZLE}, {"loop", SEC_PUZZLE}, {"lights", SEC_PUZZLE},
    {"tangram", SEC_PUZZLE},
    {"durak", SEC_CARDS}, {"barbu", SEC_CARDS}, {"rikiki", SEC_CARDS}, {"piquet", SEC_CARDS}, {"ecarte", SEC_CARDS},
    {"bigtwo", SEC_CARDS}, {"tienlen", SEC_CARDS}, {"doppelkopf", SEC_CARDS}, {"schafkopf", SEC_CARDS},
    {"jass", SEC_CARDS}, {"klaverjas", SEC_CARDS}, {"mus", SEC_CARDS}, {"truco", SEC_CARDS}, {"bridge", SEC_CARDS},
    {"bellelucie", SEC_SOLO}, {"yukon", SEC_SOLO}, {"fortythieves", SEC_SOLO}, {"golfpatience", SEC_SOLO},
    {"canfield", SEC_SOLO}, {"montana", SEC_SOLO}, {"clock", SEC_SOLO},
    {"banqi", SEC_CHESS}, {"mexico", SEC_RACE}, {"pig", SEC_RACE}, {"shipcaptain", SEC_RACE},
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
    *t_glow, *t_tok[MAX_PLAYERS], *t_big[MAX_PLAYERS], *t_bartex, *t_menu_card, *t_menu_card_on, *t_thumb[MAX_GAMES][MAX_VARIANTS + 1],
    *t_name[MAX_GAMES], *t_pill, *t_pill_on, *t_ptok[MAX_PLAYERS], *t_sec[SECTIONS],
    *t_tab, *t_tab_on, *t_tabicon[SECTIONS];
static int made_unit = -1, made_game = -1, made_card = -1, made_pill = -1, made_tab = -1, bar_w, bar_h, bar_m;
static int tok_gen;                             /* the tokens made again (ui_tokens_changed): the cover's too */

void ui_tokens_changed(void)
{
    made_game = made_pill = -1;
    tok_gen++;
    need_frame = 1;
}

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

/* the level screen (a game with levels): the chapters' tabs, the chapter's levels as tiles */
#define MAX_LEVELS 2048
#define MAX_CHAPTERS 12
#define CHAPTER_MAX 100                         /* the most levels a chapter shows */
static struct {
    int g;                                      /* whose progress best[] holds (-1: none read yet) */
    uint8_t best[MAX_LEVELS];                   /* a level each: 0 not solved, else 1 + its best stars */
    int chapter, nch, first[MAX_CHAPTERS + 1];  /* the tab shown; the chapters (their first levels) */
    box tabs[MAX_CHAPTERS + 1], tiles[CHAPTER_MAX];   /* tabs: the chapters', then random play's */
    float grab_x, grab_y;
    int grabbing, moved;
} LV = {.g = -1};
/* the solved puzzle's card: its next and replay buttons as last drawn (d 0: none); when it was first shown (its
 * stars pop in) */
static struct { float x, y, d; } next_btn, replay_btn;
static Uint32 card_t0;

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
/* the picture mode's clock is its own (shot_clock): it stands still, and moves on only past a gesture (its
 * animations have ended) or to --at ms after the last one, so the same picture comes out every time */
static Uint32 ms_ahead;
static int shot_clock;
Uint32 ui_ms(void) { return (shot_clock ? 1000 : SDL_GetTicks()) + ms_ahead; }
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

void ui_mark_gold_a(float cx, float cy, float d, float a)
{
    SDL_SetTextureAlphaMod(t_gold, (Uint8)(255 * clampf(a, 0, 1) + 0.5f));
    ui_put_c(t_gold, cx, cy, d, d);
    SDL_SetTextureAlphaMod(t_gold, 255);
}

/* ---- the games' round buttons ------------------------------------------------------------------------------------ */
#define BTN_CREAM 252, 244, 226                 /* the icons' tint, as the shell's buttons' */

/* a button's disc: the shell's dark green, or its gold (kind bit 0); an ivory medallion inside the rim (bit 1) */
static void paint_btn_disc(int kind, int size, uint8_t *o)
{
    static const uint8_t dark[3] = {54, 72, 64}, gold[3] = {150, 118, 46}, ivory[3] = {242, 234, 216};
    art_disc(size, kind & 1 ? gold : dark, 1, 0, o);
    if (!(kind & 2)) return;
    int m = (int)(0.8f * size), off = (size - m) / 2;
    uint8_t *in = malloc((size_t)m * m * 4);
    if (!in) return;
    art_disc(m, ivory, 1, 0, in);
    art_blend(o, size, size, in, m, m, off, off);
    free(in);
}

static void paint_btn_icon(int icon, int size, uint8_t *o) { art_icon(icon, size, o); }

/* the buttons' pictures, made at the size shown (painter, kind, size), the least used one remade */
static SDL_Texture *btn_tex(ui_painter paint, int kind, int size)
{
    enum { KEEP = 64 };
    static struct { ui_painter paint; int kind, size; Uint32 used; SDL_Texture *t; } c[KEEP];
    static Uint32 clock;
    if (size < 2 || !paint) return NULL;
    int lru = 0;
    clock++;
    for (int i = 0; i < KEEP; i++) {
        if (c[i].t && c[i].paint == paint && c[i].kind == kind && c[i].size == size) {
            c[i].used = clock;
            return c[i].t;
        }
        if (!c[i].t) { lru = i; break; }
        if (c[i].used < c[lru].used) lru = i;
    }
    uint8_t *b = calloc((size_t)size * size, 4);   /* its own: a painter may use ui_buf() */
    if (!b) return NULL;
    paint(kind, size, b);
    ui_free(&c[lru].t);
    c[lru].t = ui_tex(size, size, b);
    free(b);
    c[lru].paint = paint;
    c[lru].kind = kind;
    c[lru].size = size;
    c[lru].used = clock;
    return c[lru].t;
}

void ui_button_paint(float cx, float cy, float d, ui_painter paint, int kind, float k, int how)
{
    if (how & UI_LIT) ui_mark_gold(cx, cy, 2.0f * d);
    Uint8 a = how & UI_DIM ? 100 : 255;
    SDL_Texture *disc = btn_tex(paint_btn_disc, (how & UI_ON ? 1 : 0) | (how & UI_IVORY ? 2 : 0), (int)(d + 0.5f));
    if (disc) {
        SDL_SetTextureAlphaMod(disc, a);
        ui_put_c(disc, cx, cy, d, d);
        SDL_SetTextureAlphaMod(disc, 255);
    }
    SDL_Texture *t = btn_tex(paint, kind, (int)(k * d));
    if (!t) return;
    if (how & UI_TINT) SDL_SetTextureColorMod(t, BTN_CREAM);
    SDL_SetTextureAlphaMod(t, a);
    if (how & UI_UPSIDE) ui_put_rot(t, cx, cy, k * d, k * d, 180);
    else ui_put_c(t, cx, cy, k * d, k * d);
    SDL_SetTextureAlphaMod(t, 255);
    SDL_SetTextureColorMod(t, 255, 255, 255);
}

void ui_button(float cx, float cy, float d, int icon, int how)
{
    ui_button_paint(cx, cy, d, paint_btn_icon, icon, 0.62f, how | UI_TINT);
}

int ui_button_hit(float cx, float cy, float d, float x, float y) { return hypotf(x - cx, y - cy) <= 0.56f * d; }

SDL_Texture *ui_word(const char *key, int i, float px, float maxw, const uint8_t rgb[3], int *w, int *h)
{
    enum { KEEP = 32 };
    static struct { char key[32]; int i, px4, maxw; uint8_t rgb[3]; Uint32 used; int w, h; SDL_Texture *t; } c[KEEP];
    static Uint32 clock;
    int px4 = (int)(px * 4), mw = (int)maxw, k = 0;
    clock++;
    for (int j = 0; j < KEEP; j++) {
        if (c[j].t && c[j].i == i && c[j].px4 == px4 && c[j].maxw == mw && !memcmp(c[j].rgb, rgb, 3) &&
            !strcmp(c[j].key, key)) {
            c[j].used = clock;
            *w = c[j].w;
            *h = c[j].h;
            return c[j].t;
        }
        if (!c[j].t) { k = j; break; }
        if (c[j].used < c[k].used) k = j;
    }
    char s[96];
    if (!text_word(key, i, s, sizeof s)) return NULL;
    int tw = text_line(s, px, 1, rgb, NULL, 0, 0, 0, 0);
    if (tw > maxw && tw > 0) {
        px = fmaxf(6, px * maxw / tw);
        tw = text_line(s, px, 1, rgb, NULL, 0, 0, 0, 0);
    }
    int W = tw + 4, H = (int)ceilf(px * 1.55f);
    uint8_t *b = calloc((size_t)W * H, 4);
    if (!b) return NULL;
    text_line(s, px, 1, rgb, b, W, H, 2, (int)((H - 1.25f * px) / 2));
    ui_free(&c[k].t);
    c[k].t = ui_tex(W, H, b);
    free(b);
    snprintf(c[k].key, sizeof c[k].key, "%s", key);
    c[k].i = i;
    c[k].px4 = px4;
    c[k].maxw = mw;
    memcpy(c[k].rgb, rgb, 3);
    c[k].used = clock;
    *w = c[k].w = W;
    *h = c[k].h = H;
    return c[k].t;
}

void ui_button_pic(uint8_t *o, int w, int h, float cx, float cy, int d, ui_painter paint, int kind, float k, int how)
{
    int s = (int)(k * d), x0 = (int)lroundf(cx - d / 2.0f), y0 = (int)lroundf(cy - d / 2.0f);
    uint8_t *b = calloc((size_t)d * d, 4);
    if (!b) return;
    paint_btn_disc((how & UI_ON ? 1 : 0) | (how & UI_IVORY ? 2 : 0), d, b);
    if (s >= 2 && paint) {
        uint8_t *p = calloc((size_t)s * s, 4);
        if (p) {
            paint(kind, s, p);
            static const uint8_t cream[3] = {BTN_CREAM};
            for (int i = 0; i < s * s && (how & UI_TINT); i++)
                for (int c = 0; c < 3; c++) p[4 * i + c] = (uint8_t)(p[4 * i + c] * cream[c] / 255);
            art_blend(b, d, d, p, s, s, (d - s) / 2, (d - s) / 2);
            free(p);
        }
    }
    art_blend(o, w, h, b, d, d, x0, y0);
    free(b);
}

/* ---- stars and the level screen's tiles -------------------------------------------------------------------------- */
/* a star: 0 gold, 1 a faint dark hollow (on wood, on gold), 2 a faint light one (on the dark end card) */
static void paint_star(int kind, int size, uint8_t *o)
{
    static const uint8_t gold[3] = {255, 200, 80}, ink[3] = {150, 102, 28}, dark[3] = {0, 0, 0},
                         light[3] = {246, 238, 220};
    art_fig *f = art_fig_new();
    if (!f) return;
    art_star(f, ART_PART, 0.5f, 0.53f, 0.46f);
    if (kind == 0) {
        art_fig_paint(o, size, f, gold, ink, 0.045f, 0.5f);
        return;
    }
    art_fig_paint(o, size, f, kind == 1 ? dark : light, NULL, 0, 0);
    for (int i = 0; i < size * size; i++) o[4 * i + 3] = (uint8_t)(o[4 * i + 3] * (kind == 1 ? 0.3f : 0.2f));
}

static void star_at(float cx, float cy, float d, int kind)
{
    ui_put_c(btn_tex(paint_star, kind, (int)(d + 0.5f)), cx, cy, d, d);
}

void ui_star(float cx, float cy, float d, int lit) { star_at(cx, cy, d, lit ? 0 : 1); }

int ui_par_stars(int moves, int par)
{
    if (par <= 0 || moves <= par) return 3;
    return 2 * moves <= 3 * par ? 2 : 1;
}

/* a level's tile: 0 gold (solved), 1 wood (open), 2 dark walnut (locked); rounded, a bevel lit from the upper left */
static void paint_tile(int kind, int size, uint8_t *o)
{
    static const float col[3][3] = {{214, 172, 76}, {222, 184, 128}, {92, 60, 40}};
    float rad = 0.16f * size, bev = fmaxf(1.5f, 0.055f * size), h = size * 0.5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float qx = fabsf(x + 0.5f - h) - (h - rad), qy = fabsf(y + 0.5f - h) - (h - rad);
            float d = hypotf(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - rad;   /* < 0: inside */
            float u = (float)x / size, v = (float)y / size, k;
            if (kind == 0) k = (1.12f - 0.12f * (u + v)) * (1 + 0.03f * art_noise(x, y));
            else {                              /* the house's plank: its grain across */
                float g = sinf(v * 23 + 2.4f * sinf(u * 0.9f + 1.3f) + 1.7f * sinf(u * 0.21f)) * 0.5f + 0.5f;
                k = (1 + 0.06f * (g - 0.5f)) * (kind == 2 ? 0.72f : 1);
            }
            if (-d < bev) k *= x + y < size ? 1.16f : 0.8f;
            uint8_t *p = o + 4 * (y * size + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)clampf(col[kind][c] * k, 0, 255);
            p[3] = (uint8_t)(255 * clampf(0.5f - d, 0, 1));
        }
}

/* a small dark disc under a number on a level's picture */
static void paint_badge(int kind, int size, uint8_t *o)
{
    static const uint8_t c[3] = {28, 34, 32};
    (void)kind;
    art_disc(size, c, 0.86f, 0, o);
}

void ui_sound(int s) { snd_play(s); }

/* ---- chance: the shell's dice ------------------------------------------------------------------------------------ */
static uint32_t rng = 0x9e3779b9u;              /* xorshift; seeded at start (--seed in the picture mode) */
static uint32_t start_rng[MAX_GAMES];            /* the state each game's last start() was given: a replay gives it
                                                 * again (the same deal, the same puzzle) */

static void seed_random(uint32_t s) { rng = s ? s : 0x9e3779b9u; }

int ui_random(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return n > 0 ? (int)(rng % (uint32_t)n) : 0;
}

#define DICE_MS UI_DICE_MS
static struct {
    int n, v[UI_MAX_DICE], player[UI_MAX_DICE];
    float tilt[UI_MAX_DICE];
    Uint32 t0;
} dice;

/* the dice's faces at a few sizes, each face made when first shown */
#define FACE_SIZES 4
static SDL_Texture *die_face(int size, int player, int value)
{
    static struct { int size; SDL_Texture *t[MAX_PLAYERS][7]; } c[FACE_SIZES];
    static int next;
    int k;
    for (k = 0; k < FACE_SIZES && c[k].size != size; k++) {}
    if (k == FACE_SIZES) {
        k = next++ % FACE_SIZES;
        for (int p = 0; p < MAX_PLAYERS; p++)
            for (int v = 0; v < 7; v++) ui_free(&c[k].t[p][v]);
        c[k].size = size;
    }
    player = player < 0 ? 0 : player >= MAX_PLAYERS ? MAX_PLAYERS - 1 : player;
    value = value < 1 ? 1 : value > 6 ? 6 : value;
    if (!c[k].t[player][value]) {
        art_die(value, player, size, ui_buf(size, size));
        c[k].t[player][value] = ui_tex(size, size, ui_buf(size, size));
    }
    return c[k].t[player][value];
}

/* the dice set to their values and colours (players: one per die, or NULL: all `player`'s), tumbling or at rest */
static void dice_set(int n, const int *values, const int *players, int player, int tumble)
{
    dice.n = n < 0 ? 0 : n > UI_MAX_DICE ? UI_MAX_DICE : n;
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

void ui_die_tumble(int t, int i, float *turn, float *lift)
{
    float f = clampf(t / (float)DICE_MS, 0, 1);
    *turn = (1 - f) * (1 - f) * 540 * (i & 1 ? -1 : 1);
    *lift = (1 - f) * 0.6f * fabsf(sinf(f * 9.4f));
}

/* a tumbling die's face: they flicker */
static int tumble_face(int i, int value) { return 1 + (int)((unsigned)(ui_ms() / 70 + i * 3 + value) % 6); }

void ui_die_at(float cx, float cy, float size, int player, int value, float alpha)
{
    int s = (int)size;
    if (s < 4 || value < 1 || value > 6) return;
    SDL_Texture *f = die_face(s, player, value);
    Uint8 a = (Uint8)(255 * (alpha < 0 ? 0 : alpha > 1 ? 1 : alpha) + 0.5f);
    ui_shadow_flat(cx, cy, size * 1.1f, size * 1.1f, alpha);
    SDL_SetTextureAlphaMod(f, a);
    ui_put_c(f, cx, cy, size, size);
    SDL_SetTextureAlphaMod(f, 255);
}

void ui_die(float cx, float cy, float size, int player, int value, float deg, float lift, float alpha, float shade)
{
    int s = (int)(size + 0.5f);
    if (s < 4) return;
    SDL_Texture *f = die_face(s, player, value);
    ui_shadow_flat(cx, cy, size * 1.1f, size * 1.1f, alpha * (lift > 0 ? 0.7f : 1));
    Uint8 k = (Uint8)(255 * clampf(shade, 0, 1) + 0.5f);
    SDL_SetTextureAlphaMod(f, (Uint8)(255 * clampf(alpha, 0, 1) + 0.5f));
    SDL_SetTextureColorMod(f, k, k, k);
    ui_put_rot(f, cx, cy - lift, size, size, deg);
    SDL_SetTextureAlphaMod(f, 255);
    SDL_SetTextureColorMod(f, 255, 255, 255);
}

int ui_die_thrown(float cx, float cy, float size, int player, int value, float deg, int i, int t, float shade)
{
    if (t >= DICE_MS) {
        ui_die(cx, cy, size, player, value, deg, 0, 1, shade);
        return 0;
    }
    float turn, lift;
    ui_die_tumble(t < 0 ? 0 : t, i, &turn, &lift);
    ui_die(cx, cy, size, player, tumble_face(i, value), deg + turn, lift * size, 1, 1);
    need_frame = 1;
    return 1;
}

void ui_dice_draw(float cx, float cy, float size, unsigned used)
{
    int s = (int)size;
    if (s < 4 || !dice.n) return;
    int rolling = ui_dice_busy();
    float gap = 0.3f * size, x = cx - (dice.n * size + (dice.n - 1) * gap) / 2 + size / 2;
    for (int i = 0; i < dice.n; i++, x += size + gap) {
        int v = dice.v[i];
        float a = dice.tilt[i], y = cy;
        if (rolling) {                          /* tumbling: the faces flicker, the die turns and bounces */
            float turn, lift;
            ui_die_tumble((int)(ui_ms() - dice.t0), i, &turn, &lift);
            v = tumble_face(i, dice.v[i]);
            a += turn;
            y -= lift * size;
        }
        Uint8 alpha = used >> i & 1 ? 110 : 255;
        SDL_Texture *f = die_face(s, dice.player[i], v);
        ui_shadow_flat(x, cy, size * 1.1f, size * 1.1f, alpha / 255.0f);
        SDL_SetTextureAlphaMod(f, alpha);
        ui_put_rot(f, x, y, size, size, a);
        SDL_SetTextureAlphaMod(f, 255);
    }
    if (rolling) need_frame = 1;
}

void ui_sound_soft(int s, float volume) { snd_play_vol(s, volume); }
void ui_sound_later(int s, int ms) { snd_play_at(s, 1, ms); }

/* digits: white textures per height and weight (a few kept), tinted when drawn, turned round their centre */
static const uint8_t NUM_LIGHT[3] = {246, 238, 220}, NUM_DARK[3] = {40, 34, 30};

static void digits_at(float cx, float cy, float h, int value, const uint8_t rgb[3], Uint8 alpha, int how, double deg)
{
    enum { KEEP = 6 };
    static struct { int h, regular; SDL_Texture *d[10]; } cache[KEEP];
    static int next;
    int ih = (int)(h + 0.5f), k, regular = how & UI_NUM_REGULAR;
    if (ih < 4) return;
    for (k = 0; k < KEEP && (cache[k].h != ih || cache[k].regular != regular); k++) {}
    if (k == KEEP) {
        k = next++ % KEEP;
        for (int i = 0; i < 10; i++) ui_free(&cache[k].d[i]);
        int w = (int)(0.62f * ih + 1);
        static const uint8_t white[3] = {255, 255, 255};
        for (int i = 0; i < 10; i++) {          /* the font's digits (help/glyphs.txt keeps them), tinted */
            uint8_t *b = ui_buf(w, ih);
            memset(b, 0, (size_t)w * ih * 4);
            text_glyph('0' + i, ih * 1.2f, !regular, white, b, w, ih);
            cache[k].d[i] = ui_tex(w, ih, b);
        }
        cache[k].h = ih;
        cache[k].regular = regular;
    }
    char s[12];
    int n = snprintf(s, sizeof s, "%d", value < 0 ? 0 : value);
    float dw = 0.62f * ih, step = (how & UI_NUM_TIGHT ? 0.66f : 0.74f) * ih, x = cx - (step * (n - 1) + dw) / 2;
    float ca = cosf((float)(deg * 3.14159265 / 180)), sa = sinf((float)(deg * 3.14159265 / 180));
    for (int i = 0; i < n; i++, x += step) {
        SDL_Texture *t = cache[k].d[s[i] - '0'];
        SDL_SetTextureColorMod(t, rgb[0], rgb[1], rgb[2]);
        SDL_SetTextureAlphaMod(t, alpha);
        if (deg == 0) ui_put(t, x, cy - h / 2, dw, (float)ih);
        else {                                  /* the digit's centre turned round the number's */
            float ox = x + dw / 2 - cx;
            ui_put_rot(t, cx + ox * ca, cy + ox * sa, dw, (float)ih, deg);
        }
        SDL_SetTextureAlphaMod(t, 255);
    }
}

static void number(float cx, float cy, float h, int value, int light, double deg)
{
    digits_at(cx, cy, h, value, light ? NUM_LIGHT : NUM_DARK, 255, 0, deg);
}

void ui_number(float cx, float cy, float h, int value, int light) { number(cx, cy, h, value, light, 0); }

void ui_number_rot(float cx, float cy, float h, int value, int light, double deg)
{
    number(cx, cy, h, value, light, deg);
}

void ui_number_ex(float cx, float cy, float h, int value, const uint8_t rgb[3], Uint8 alpha, int how)
{
    digits_at(cx, cy, h, value, rgb, alpha, how, 0);
}

float ui_number_w(int value, float h)
{
    int n = 1;
    for (int v = value; v >= 10; v /= 10) n++;
    return (0.74f * (n - 1) + 0.62f) * h;
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

float ui_bar_number(box b, SDL_Texture *piece, int n)
{
    float sz = 0.6f * ui.unit, dh = 0.46f * ui.unit, cy = b.y + b.h / 2, x = b.x;
    if (n < 0) n = 0;
    if (piece) {
        ui_put(piece, x, cy - sz / 2, sz, sz);
        x = x + sz + 0.12f * ui.unit;
    }
    int digits = 1;
    for (int v = n; v >= 10; v /= 10) digits++;
    float w = (0.74f * (digits - 1) + 0.62f) * dh;
    ui_number(x + w / 2, cy, dh, n, 1);
    return x + w;
}

void ui_bar_count(box b, SDL_Texture *piece, int n)
{
    if (n > 0) ui_bar_number(b, piece, n);
}

box ui_bar_countdown(box b, int left)
{
    float u = ui.unit, dh = 0.42f * u, id = 0.5f * u, cy = b.y + b.h / 2;
    left = left < 0 ? 0 : left;
    float nw = ui_number_w(left, dh), x = b.x + b.w - nw;
    ui_number_ex(x + nw / 2, cy, dh, left, NUM_LIGHT, 220, UI_NUM_REGULAR);
    SDL_Texture *t = btn_tex(paint_btn_icon, ICON_HOURGLASS, (int)(id + 0.5f));
    x -= 0.06f * u + id;
    if (t) {
        SDL_SetTextureColorMod(t, NUM_LIGHT[0], NUM_LIGHT[1], NUM_LIGHT[2]);
        SDL_SetTextureAlphaMod(t, 200);
        ui_put(t, x, cy - id / 2, id, id);
        SDL_SetTextureAlphaMod(t, 255);
        SDL_SetTextureColorMod(t, 255, 255, 255);
    }
    b.w = fmaxf(0, x - 0.3f * u - b.x);
    return b;
}

/* the width of the counts at a scale, the first one's picture dropped or not */
static float counts_w(const ui_count *c, int nc, float k, int drop)
{
    float u = ui.unit, w = 0;
    for (int i = 0; i < nc; i++) {
        if (c[i].pic && !(drop && i == 0)) w += 0.72f * u * k;
        w += ui_number_w(c[i].value < 0 ? 0 : c[i].value, 0.46f * u * k) + (i ? 0.35f * u * k : 0);
    }
    return w;
}

float ui_bar_fit(box b, const ui_count *c, int nc, int nbtn)
{
    static const float scale[5] = {1, 1, 0.9f, 0.8f, 0.7f};
    float u = ui.unit, k = 1, d = 0.84f * u;
    int drop = 0;
    for (int pass = 0; pass < 5; pass++) {      /* the first count's picture goes first, then all gets smaller */
        drop = pass >= 1 && nc > 0 && c[0].pic;
        k = scale[pass];
        float room = b.w - counts_w(c, nc, k, drop) - (nc && nbtn ? 0.3f * u : 0) - 0.05f * u;
        d = nbtn ? (room - (nbtn - 1) * 0.3f * u) / nbtn : 0.84f * u;
        if (d >= 0.8f * u * k) break;
        if (pass == 0 && !(nc > 0 && c[0].pic)) pass = 1;   /* no picture to drop */
    }
    d = floorf(clampf(d, 0.5f * u, 0.84f * u));
    float x = b.x, cy = b.y + b.h / 2, dh = 0.46f * u * k, sz = 0.6f * u * k;
    for (int i = 0; i < nc; i++) {
        if (i) x += 0.35f * u * k;
        if (c[i].pic && !(drop && i == 0)) {
            ui_put(c[i].pic, x, cy - sz / 2, sz, sz);
            x += sz + 0.12f * u * k;
        }
        int v = c[i].value < 0 ? 0 : c[i].value;
        float w = ui_number_w(v, dh);
        ui_number(x + w / 2, cy, dh, v, 1);
        x += w;
    }
    return d;
}

void ui_bar_spot(box b, int i, float d, float *cx, float *cy)
{
    *cx = b.x + b.w - d / 2 - 0.05f * ui.unit - i * (d + 0.3f * ui.unit);
    *cy = b.y + b.h / 2;
}

/* ---- what is kept between two runs (save.c): the sound, the menu's tab, each game's choice ---------------------- */
static void game_key(int g, char *k, size_t n) { snprintf(k, n, "shell/game/%s", games[g]->id); }

static int nlevels(int g) { return games[g]->levels < MAX_LEVELS ? games[g]->levels : MAX_LEVELS; }

/* a game's choice: its variant (a game with levels: the last level played) and how many play */
static void remember_game(int g)
{
    char k[96];
    game_key(g, k, sizeof k);
    int v = chosen[g] ? variant[g] + 1 : 0;
    /* a game of levels that also plays at random: the variant picked for it, and which way it was last played */
    uint8_t b[5] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)nplayers[g], (uint8_t)(rvariant[g] + 1),
                    (uint8_t)rnd_mode[g]};
    ui_save_set(k, b, has_random(g) ? 5 : 3);
}

static void remember_mute(void)
{
    uint8_t b = (uint8_t)(snd_muted != 0);
    ui_save_set("shell/mute", &b, 1);
}

/* the kept settings, each checked (a game gone, fewer variants: left as new) */
static void settings_load(void)
{
    uint8_t b[5];
    if (ui_save_get("shell/mute", b, 1) == 1) snd_muted = b[0] == 1;
    if (ui_save_get("shell/tab", b, 1) == 1 && b[0] < SECTIONS) menu.tab = b[0];
    for (int g = 0; g < ngames; g++) {
        char k[96];
        game_key(g, k, sizeof k);
        int len = ui_save_get(k, b, 5);
        if (len < 3) continue;
        int v = (b[0] | b[1] << 8) - 1, most = games[g]->levels ? nlevels(g) : games[g]->variants;
        if (v >= 0 && v < most) {
            variant[g] = v;
            chosen[g] = 1;
        }
        if (b[2] >= 1 && b[2] <= MAX_PLAYERS && player_counts(g) >> b[2] & 1) nplayers[g] = b[2];
        if (len == 5 && has_random(g) && b[3] >= 1 && b[3] <= games[g]->variants) {
            rvariant[g] = b[3] - 1;
            rnd_mode[g] = b[4] == 1;
        }
    }
}

/* ---- the levels (game.levels): the progress, the chapters ------------------------------------------------------- */
static void progress_load(int g)
{
    if (LV.g == g) return;
    LV.g = g;
    memset(LV.best, 0, sizeof LV.best);
    char k[96];
    snprintf(k, sizeof k, "shell/levels/%s", games[g]->id);
    ui_save_get(k, LV.best, nlevels(g));        /* fewer kept (levels added since): the rest not solved */
    for (int l = 0; l < MAX_LEVELS; l++)
        if (LV.best[l] > 4) LV.best[l] = 4;
}

static void progress_save(void)
{
    char k[96];
    snprintf(k, sizeof k, "shell/levels/%s", games[LV.g]->id);
    ui_save_set(k, LV.best, nlevels(LV.g));
}

static int level_chapter(int g, int l)
{
    const game *G = games[g];
    int c;
    if (G->chapter_of) c = G->chapter_of(l);
    else {
        int n = G->chapters > 1 ? G->chapters : 1, per = (nlevels(g) + n - 1) / n;
        c = per ? l / per : 0;
    }
    return c < 0 ? 0 : c >= MAX_CHAPTERS ? MAX_CHAPTERS - 1 : c;
}

static void chapters_make(int g)
{
    int n = nlevels(g);
    LV.nch = 0;
    for (int l = 0; l < n; l++)
        for (int c = level_chapter(g, l); LV.nch <= c;) LV.first[LV.nch++] = l;
    if (!LV.nch) LV.first[LV.nch++] = 0;
    LV.first[LV.nch] = n;
}

/* open: solved, or among the first unsolved ones (levels_open, else 3) */
static int level_open(int g, int l)
{
    progress_load(g);
    if (l < 0 || l >= nlevels(g)) return 0;
    if (LV.best[l]) return 1;
    int ahead = games[g]->levels_open > 0 ? games[g]->levels_open : 3, before = 0;
    for (int k = 0; k < l && before < ahead; k++) before += !LV.best[k];
    return before < ahead;
}

static int first_unsolved(int g)
{
    progress_load(g);
    for (int l = 0; l < nlevels(g); l++)
        if (!LV.best[l]) return l;
    return -1;
}

/* the level screen opens at the chapter of the level last played, else of the first one not solved */
static void levels_enter(int g)
{
    int l = chosen[g] ? variant[g] : first_unsolved(g);
    chapters_make(g);
    LV.chapter = l < 0 ? 0 : level_chapter(g, l);
    LV.grabbing = 0;
}

/* the level in play solved: its stars kept when better than before */
static void level_check(void)
{
    if (screen != SCR_GAME || cur < 0 || !games[cur]->levels || !started[cur] || in_random(cur)) return;
    int l = variant[cur];
    if (l < 0 || l >= nlevels(cur) || games[cur]->status() != GS_WIN0) return;
    int s = games[cur]->level_stars ? games[cur]->level_stars() : 0;
    s = s < 0 ? 0 : s > 3 ? 3 : s;
    progress_load(cur);
    if (LV.best[l] < 1 + s) {
        LV.best[l] = (uint8_t)(1 + s);
        progress_save();
    }
}

int ui_level_best(int level)
{
    if (cur < 0 || !games[cur]->levels || level < 0 || level >= nlevels(cur)) return -1;
    progress_load(cur);
    return LV.best[level] - 1;
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
    if (made_game == cur * 8 + ui.players) return;   /* (ui_tokens_changed() sets it to -1) */
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

/* the game's own round buttons (game.tools): the room the shell gave them (right: packed against its right end, the
 * bottom bar's), their size; where each is, worked out again when the count shown changes */
static struct {
    int n, shown, right, dirty;
    box room;
    float d0, d, x[UI_MAX_TOOLS], y[UI_MAX_TOOLS];
} TL;

static int tools_of(int g)
{
    int n = g >= 0 ? games[g]->tools : 0;
    return n < 0 ? 0 : n > UI_MAX_TOOLS ? UI_MAX_TOOLS : n;
}

/* the tools shown in their room: rows as few as fit, as big as fit (down to 0.5 unit), each row centred (or packed to
 * the right) */
static void tools_place(void)
{
    TL.dirty = 0;
    int n = TL.shown;
    if (n <= 0 || TL.room.w <= 0) return;
    float u = ui.unit, d = TL.d0, gap = 0.3f * u;
    int per = 1, rows = 1;
    for (;;) {
        per = (int)((TL.room.w + gap) / (d + gap));
        per = per < 1 ? 1 : per > n ? n : per;
        rows = (n + per - 1) / per;
        if (rows * (d + gap) - gap <= TL.room.h + 0.5f || d <= 0.5f * u) break;
        d *= 0.92f;
        gap = fmaxf(0.15f * u, gap * 0.92f);
    }
    per = (n + rows - 1) / rows;                /* the rows even */
    TL.d = floorf(d);
    float y0 = TL.room.y + (TL.room.h - (rows * (d + gap) - gap)) / 2 + d / 2;
    for (int i = 0; i < n; i++) {
        int r = i / per, k = r == rows - 1 ? n - r * per : per;
        float w = k * d + (k - 1) * gap, x0 = TL.right ? TL.room.x + TL.room.w - w : TL.room.x + (TL.room.w - w) / 2;
        TL.x[i] = floorf(x0 + (i % per) * (d + gap) + d / 2);
        TL.y[i] = floorf(y0 + r * (d + gap));
    }
}

void ui_tools_shown(int n)
{
    n = n < 0 ? 0 : n > TL.n ? TL.n : n;
    if (n != TL.shown) TL.dirty = 1;
    TL.shown = n;
}

int ui_tool_at(int i, float *cx, float *cy, float *d)
{
    if (TL.dirty) tools_place();
    if (screen != SCR_GAME || i < 0 || i >= TL.shown) return 0;
    if (cx) *cx = TL.x[i];
    if (cy) *cy = TL.y[i];
    if (d) *d = TL.d;
    return 1;
}

void ui_tool(int i, int icon, int how)
{
    float x, y, d;
    if (ui_tool_at(i, &x, &y, &d)) ui_button(x, y, d, icon, how);
}

void ui_tool_paint(int i, ui_painter paint, int kind, float k, int how)
{
    float x, y, d;
    if (ui_tool_at(i, &x, &y, &d)) ui_button_paint(x, y, d, paint, kind, k, how);
}

int ui_tool_hit(float x, float y)
{
    float cx, cy, d;
    for (int i = 0; ui_tool_at(i, &cx, &cy, &d); i++)
        if (ui_button_hit(cx, cy, d, x, y)) return i;
    return -1;
}

/* the screen of a game: its bars, its board, the shell's buttons, the game's own tools.
 * Portrait: the top bar (when a player sits there) at the top of the screen, the bottom bar and the buttons at its
 * bottom, the board in all the room between (a square in its middle; the whole rectangle for VIEW_* games and
 * game.fill). Tools: up to three at the right end of the bottom bar (one bar there), else a row (two) just above it.
 * Landscape: the board on the left (a square, the whole room left for VIEW_* and fill), a column on the right: the
 * bars, the buttons (two rows) and the tools between the top bar and the buttons. */
static void layout_game(void)
{
    float m = fminf(ui.w, ui.h) * 0.025f, u;
    int view = is_view(), fill = view || (cur >= 0 && games[cur]->fill), nt = tools_of(cur);
    /* each player's side (home(): top or bottom), in their order from the left (VIEW_*: in turn order) */
    int side[2][MAX_PLAYERS], n[2] = {0, 0};
    float key[MAX_PLAYERS];
    for (int p = 0; p < ui.players; p++) {
        float fx = 0.5f, fy = 0;
        if (cur >= 0 && games[cur]->home && !view) games[cur]->home(p, &fx, &fy);
        key[p] = ui.flipped ? 1 - fx : fx;
        if (view) key[p] = (float)((p - ui_bottom() + ui.players) % ui.players);
        int s = seat_top(p), k = n[s]++;
        while (k > 0 && key[side[s][k - 1]] > key[p]) { side[s][k] = side[s][k - 1]; k--; }
        side[s][k] = p;
    }
    int nb = 0;
    for (int i = 0; i < NBTN; i++) nb += shown(i);
    box top, bottom;
    TL.n = TL.shown = nt;
    TL.room = (box){0, 0, 0, 0};
    TL.right = 0;
    TL.dirty = 1;
    if (ui.w <= ui.h) {
        int in_bar = nt > 0 && nt <= 3 && n[0] == 1, row = nt > 0 && !in_bar, rows = nt > 7 ? 2 : 1;
        float extra = row ? 1.1f * rows : 0;   /* units */
        u = floorf(fminf((ui.w - 2 * m) / 9, (ui.h - 2 * m) / (12.9f + extra + (row ? 0.1f : 0))));
        float x = floorf((ui.w - 9 * u) / 2);
        L.br = 0.56f * u;
        top = (box){x, floorf(m), 9 * u, u};
        bottom = (box){x, floorf(ui.h - m - 1.35f * u - 2 * L.br), 9 * u, u};
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = ui.w / 2.0f + (k++ - (nb - 1) / 2.0f) * 1.5f * u;
            L.by[i] = bottom.y + bottom.h + 0.35f * u + L.br;
        }
        float r0 = n[1] ? top.y + 1.2f * u : floorf(m), r1 = bottom.y - 0.2f * u;
        TL.d0 = 0.84f * u;
        if (in_bar) {                           /* the tools at the bar's right end, the bar shorter */
            float w = nt * TL.d0 + (nt - 1) * 0.3f * u + 0.1f * u;
            TL.room = (box){bottom.x + bottom.w - w, bottom.y, w, bottom.h};
            TL.right = 1;
            bottom.w -= w + 0.2f * u;
        } else if (row) {
            TL.d0 = 0.9f * u;
            TL.room = (box){x, r1 - extra * u, 9 * u, extra * u};
            r1 -= extra * u + 0.1f * u;
        }
        if (fill) L.board = (box){x, r0, 9 * u, r1 - r0};
        else L.board = (box){x, floorf(r0 + fmaxf(0, r1 - r0 - 9 * u) / 2), 9 * u, 9 * u};
    } else if (view) {                          /* the table on the left; others' bars, buttons, the viewer's bar */
        u = floorf(fminf((ui.h - 2 * m) / 9, (ui.w - 2 * m) / 13.45f));
        float y = floorf((ui.h - 9 * u) / 2), cx = floorf(ui.w - m - 4 * u);
        L.board = (box){floorf(m), y, cx - 0.45f * u - floorf(m), 9 * u};
        top = (box){cx, y, 4 * u, u};
        bottom = (box){cx, y + 8 * u, 4 * u, u};
        L.br = 0.55f * u;
        int per = (nb + 1) / 2;
        for (int i = 0, k = 0; i < NBTN; i++) {
            if (!shown(i)) continue;
            L.bx[i] = cx + 2 * u + (k % per - (per - 1) / 2.0f) * 1.35f * u;
            L.by[i] = y + (k / per ? 7.2f : 5.95f) * u;
            k++;
        }
        float t0 = n[1] ? y + fminf(n[1] * 1.1f * u, 5.2f * u) : y;   /* below the others' bars */
        TL.room = (box){cx, t0, 4 * u, y + 5.3f * u - t0};
        TL.d0 = 0.84f * u;
    } else {                                    /* the board, then a column: bar, tools, buttons, bar */
        u = floorf(fminf((ui.h - 2 * m) / 9, (ui.w - 2 * m) / 13.45f));
        float x = floorf((ui.w - 13.45f * u) / 2), y = floorf((ui.h - 9 * u) / 2), cx = x + 9.45f * u;
        L.board = (box){x, y, 9 * u, 9 * u};
        if (fill) {                             /* all the width left of the column, the column at the right */
            cx = floorf(ui.w - m - 4 * u);
            L.board = (box){floorf(m), y, cx - 0.45f * u - floorf(m), 9 * u};
        }
        top = (box){cx, y, 4 * u, u};
        bottom = (box){cx, y + 8 * u, 4 * u, u};
        L.br = 0.55f * u;
        int per = (nb + 1) / 2;
        for (int i = 0, k = 0; i < NBTN; i++) {     /* two rows (of three) */
            if (!shown(i)) continue;
            L.bx[i] = cx + 2 * u + (k % per - (per - 1) / 2.0f) * 1.35f * u;
            L.by[i] = y + 4.5f * u + (k / per ? 0.7f : -0.7f) * u;
            k++;
        }
        float t0 = y + (n[1] ? 1.2f : 0.1f) * u;
        TL.room = (box){cx, t0, 4 * u, y + 3.15f * u - t0};
        TL.d0 = 0.84f * u;
    }
    ui.unit = u;
    /* each player's bar on their side: side by side sharing the bar's width (portrait), or stacked (landscape: the
     * column is narrow) */
    for (int s = 0; s < 2; s++) {
        box b = s ? top : bottom;
        for (int k = 0; k < n[s]; k++) {
            box c = b;
            if (ui.w <= ui.h && n[s] >= 5) {    /* five bars on a side: the screen's whole width, closer */
                float gap = 0.08f * u, x0 = floorf(fminf(b.x, 0.012f * ui.w));
                c.w = floorf((ui.w - 2 * x0 - (n[s] - 1) * gap) / n[s]);
                c.x = x0 + k * (c.w + gap);
            } else if (ui.w <= ui.h) {
                float gap = 0.25f * u;
                c.w = floorf((b.w - (n[s] - 1) * gap) / n[s]);
                c.x = b.x + k * (c.w + gap);
            } else if (view) {                  /* the others' bars from the top, above the buttons */
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

/* ---- a grid of square cells fitted into a box (game.fill's room): turned a quarter when that makes them bigger --- */
ui_grid ui_grid_fit(box b, int cols, int rows, float frame)
{
    ui_grid g = {0, 0, 0, cols, rows, 0};
    if (cols < 1 || rows < 1) return g;
    float a = fminf(b.w / (cols + 2 * frame), b.h / (rows + 2 * frame)),
          t = fminf(b.w / (rows + 2 * frame), b.h / (cols + 2 * frame));
    g.turned = t > 1.04f * a;                   /* turned only when the cells grow by more than a little */
    g.cell = floorf(g.turned ? t : a);
    g.cols = g.turned ? rows : cols;
    g.rows = g.turned ? cols : rows;
    g.x = floorf(b.x + (b.w - g.cols * g.cell) / 2);
    g.y = floorf(b.y + (b.h - g.rows * g.cell) / 2);
    return g;
}

void ui_grid_cell(const ui_grid *g, int col, int row, float *x, float *y)
{
    *x = g->x + (g->turned ? row : col) * g->cell;
    *y = g->y + (g->turned ? col : row) * g->cell;
}

int ui_grid_hit(const ui_grid *g, float x, float y, int *col, int *row)
{
    if (g->cell <= 0 || x < g->x || y < g->y) return 0;
    int sc = (int)((x - g->x) / g->cell), sr = (int)((y - g->y) / g->cell);
    if (sc >= g->cols || sr >= g->rows) return 0;
    *col = g->turned ? sr : sc;
    *row = g->turned ? sc : sr;
    return 1;
}

/* a tab's picture: the pieces of its section (a draughts man, a knight, a die, a card, a question, a marble, a jigsaw
 * piece) */
static void tab_icon(int s, int d, uint8_t *o)
{
    memset(o, 0, (size_t)d * d * 4);
    if (s == SEC_STRATEGY) art_counter(COUNTER_MAN, 0, d, o);
    else if (s == SEC_CHESS) art_piece(KNIGHT, WHITE, d, o);
    else if (s == SEC_GUESS) art_icon(ICON_HELP, d, o);
    else if (s == SEC_PUZZLE) art_icon(ICON_PUZZLE, d, o);
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
    SDL_SetTextureColorMod(t_tabicon[SEC_PUZZLE], 246, 238, 220);
}

static float level_tabs(float head);
static void draw_level_tabs(int on);

/* ---- the picker on two axes (game.axes: a size, then a level) --------------------------------------------------- */
static int pick_size[MAX_GAMES];                /* the size chosen on the picker */

static int axes_on(int g)
{
    if (g < 0) return 0;
    const game *G = games[g];
    return G->axes[0] > 0 && G->axes[1] > 0 && G->axes[0] * G->axes[1] <= G->variants &&
           G->axes[0] + G->axes[1] <= MAX_VARIANTS;
}

/* the variant the picker shows as chosen last (a game of levels: its random play's); -1 none */
static int picked(int g)
{
    if (games[g]->levels) return in_random(g) ? rvariant[g] : -1;
    return chosen[g] ? variant[g] : -1;
}

/* the picker opens on the size chosen last (else the first) */
static void axes_enter(int g)
{
    int v = picked(g);
    if (axes_on(g)) pick_size[g] = v >= 0 ? v / games[g]->axes[1] : 0;
}

/* the cards: the sizes in rows (as many a row as makes them biggest), the levels in one row under them, smaller.
 * L.cards[0..sizes-1], then the levels'; their tops from 0; returns the block's height, *card the sizes' side */
static float axes_layout(float H, float g, float *card)
{
    int ns = games[cur]->axes[0], nl = games[cur]->axes[1], bc = 1;
    float W = ui.w - 2 * fminf(ui.w, ui.h) * 0.05f, best = 0;
    for (int cols = 1; cols <= ns; cols++) {
        int rows = (ns + cols - 1) / cols;
        float c = fminf((W - (cols - 1) * g) / cols, (H - (rows - 1) * g - 1.4f * g) / (rows + 0.55f));
        c = fminf(c, fminf(ui.w, ui.h) * 0.62f);
        if (c > best + 0.5f) { best = c; bc = cols; }
    }
    float c = floorf(best), cl = floorf(fminf(0.55f * c, (W - (nl - 1) * g) / nl));
    int rows = (ns + bc - 1) / bc;
    for (int i = 0; i < ns; i++) {
        int r = i / bc, k = r == rows - 1 ? ns - r * bc : bc;   /* the last row centred */
        float x0 = (ui.w - (k * c + (k - 1) * g)) / 2;
        L.cards[i] = (box){floorf(x0 + (i % bc) * (c + g)), r * (c + g), c, c};
    }
    float ly = rows * c + (rows - 1) * g + 1.4f * g, x0 = (ui.w - (nl * cl + (nl - 1) * g)) / 2;
    for (int j = 0; j < nl; j++) L.cards[ns + j] = (box){floorf(x0 + j * (cl + g)), floorf(ly), cl, cl};
    *card = c;
    return ly + cl;
}

/* a level's picture by default: as many ebony stones as its number (one easy .. three hard), in a row */
static void paint_stones(int k, int size, uint8_t *o)
{
    int n = k + 1, d = (int)(size * fminf(0.24f, 0.9f / (1.2f * n)));
    uint8_t *st = d > 2 ? calloc((size_t)d * d, 4) : NULL;
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i < n; i++)
        art_blend(o, size, size, st, d, d, (int)(size / 2.0f + (i - (n - 1) / 2.0f) * 1.2f * d - d / 2.0f),
                  (size - d) / 2);
    free(st);
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
    float below = top;                          /* where the pills and the cards may start */
    if (screen == SCR_PICK && games[cur]->levels) {   /* random play's picker: the level screen's tabs over it */
        chapters_make(cur);
        head = below = level_tabs(head);
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
    } else if (screen == SCR_PICK && axes_on(cur)) {   /* two axes: the sizes, then the levels */
        tall = axes_layout(ui.h - head - 2 * m, g, &c);
        ch = c;
        n = games[cur]->axes[0] + games[cur]->axes[1];
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
        float ph = L.pills[0].h, start = below + fmaxf(0, (ui.h - below - m - (ph + g + tall)) / 2);
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
        const uint8_t gold[4] = {150, 118, 46, 255};   /* the picker's size chosen (two axes) */
        ui_free(&t_menu_card_on);
        art_roundrect(L.card, h, 0.1f * L.card, gold, ui_buf(L.card, h));
        t_menu_card_on = ui_tex(L.card, h, ui_buf(L.card, h));
        for (int i = 0; i < MAX_GAMES; i++)
            for (int v = 0; v <= MAX_VARIANTS; v++) ui_free(&t_thumb[i][v]);
    }
}

/* the level screen's tabs (the chapters, then a die for random play when the game has it) under the top row's
 * buttons, at head; where what is under them starts (head: no tabs, a single chapter) */
static int level_ntabs(void) { return LV.nch + has_random(cur); }

static float level_tabs(float head)
{
    float s = fminf(ui.w, ui.h), m = s * 0.05f, g = s * 0.04f;
    int n = level_ntabs();
    if (n <= 1) return head;
    float th = floorf(s * 0.12f), tg = floorf(g * 0.4f);
    float tw = floorf(fminf((ui.w - 2 * m - (n - 1) * tg) / n, 2.4f * th));
    float tx = (ui.w - (n * tw + (n - 1) * tg)) / 2;
    for (int k = 0; k < n; k++) LV.tabs[k] = (box){floorf(tx + k * (tw + tg)), floorf(head), tw, th};
    tab_textures((int)tw, (int)th);
    return head + th + g;
}

/* the level screen: home at the top left (the stars won at the right), the chapters' tabs under it (as the menu's),
 * then the chapter's tiles in a grid right under them, as big as the room allows (at most 0.28 of the screen's short
 * side), the same grid for every chapter */
static void layout_levels(void)
{
    float s = fminf(ui.w, ui.h), m = s * 0.05f, head = s * 0.16f;
    chapters_make(cur);
    LV.chapter = LV.chapter >= LV.nch ? LV.nch - 1 : LV.chapter < 0 ? 0 : LV.chapter;
    float top = level_tabs(head);
    int most = 1;
    for (int c = 0; c < LV.nch; c++) {
        int n = LV.first[c + 1] - LV.first[c];
        if (n > most) most = n < CHAPTER_MAX ? n : CHAPTER_MAX;
    }
    float W = ui.w - 2 * m, H = ui.h - m - top, cap = s * 0.28f, bp = 0, bs = 0, ba = 0;
    int cols = 1;
    for (int c = 1; c <= most; c++) {           /* big tiles, few gaps in the last row (25: five by five, not four
                                                 * by seven); even: the grid's shape nearest the room's */
        int r = (most + c - 1) / c;
        float p = floorf(fminf(fminf(W / c, H / r), cap)), sc = p * (1 - 0.04f * (c * r - most));
        float a = fabsf(logf((float)c / r) - logf(W / H));
        if (c == 1 || sc > bs + 0.5f || (fabsf(sc - bs) <= 0.5f && a < ba)) {
            cols = c;
            bp = p;
            bs = sc;
            ba = a;
        }
    }
    float tile = floorf(bp * 0.84f), x0 = (ui.w - cols * bp) / 2, y0 = top;   /* right under the tabs */
    for (int i = 0; i < most; i++)
        LV.tiles[i] = (box){floorf(x0 + (i % cols) * bp + (bp - tile) / 2),
                            floorf(y0 + (i / cols) * bp + (bp - tile) / 2), tile, tile};
    L.snd_r = head * 0.28f;                     /* the top row's buttons, as the menu's and the picker's */
    L.snd_x = ui.w - m - L.snd_r;
    L.snd_y = head * 0.55f;
    ui.unit = s / 12.0f;
    L.br = L.snd_r;
    shared_textures();
}

static void layout(void)
{
    if (screen == SCR_GAME) layout_game();
    else if (screen == SCR_LEVELS) layout_levels();
    else layout_cards(screen == SCR_MENU ? ngames : games[cur]->variants);
}

/* ---- the screens ------------------------------------------------------------------------------------------------- */
/* the picker comes before a new game when there is something to choose: a variant, how many play, a level */
static int needs_picker(int g) { return games[g]->variants > 1 || many_counts(g) || games[g]->levels > 0; }

/* the picker, or a game with levels its level screen (random play: its picker, under the level screen's tabs) */
static void show_picker(int g)
{
    if (in_random(g) && (games[g]->variants > 1 || many_counts(g))) {   /* random play's picker, under the tabs */
        chapters_make(g);
        screen = SCR_PICK;
    } else if (games[g]->levels > 0) {
        levels_enter(g);
        screen = SCR_LEVELS;
    } else {
        screen = SCR_PICK;
    }
    axes_enter(g);
    layout();
}

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

/* a new game of the open one: start(v), given a fresh random state (kept), or the one it was given last (same: a
 * replay, the same deal or puzzle) */
static void start_game(int v, int same)
{
    if (same) rng = start_rng[cur];
    else start_rng[cur] = rng;
    ui.level = games[cur]->levels && !in_random(cur) ? v : -1;
    if (in_random(cur)) games[cur]->start_random(v);
    else games[cur]->start(v);
    started[cur] = 1;
    card_t0 = 0;
}

static void open_game(int i, int fresh_variant)
{
    cur = i;
    ui.players = players_of(i);
    solo_view();
    int fresh = 0;
    if (fresh_variant >= 0) {
        if (in_random(i)) rvariant[i] = fresh_variant;
        else variant[i] = fresh_variant;
        chosen[i] = 1;
        remember_game(i);
        start_game(fresh_variant, 0);
        fresh = 1;
    } else if (!started[i]) {
        if (needs_picker(i)) {
            show_picker(i);
            return;
        }
        start_game(0, 0);
        fresh = 1;
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

/* the game started again at once (its variant, its level): a new deal, or the same one (same: replay) */
static void restart(int same)
{
    armed = 0;
    start_game(in_random(cur) ? rvariant[cur] : variant[cur], same);
    view_start(1);
    over_hidden = 0;
    last_status = games[cur]->status();
    end_seen = 0;
}

static void new_game(void)
{
    armed = 0;
    if (needs_picker(cur)) {
        started[cur] = 0;
        show_picker(cur);
        return;
    }
    restart(0);
}

/* a solved level's next button: the next level, or the level screen after the last one */
static void level_next(void)
{
    int l = variant[cur] + 1;
    if (l < nlevels(cur) && level_open(cur, l)) open_game(cur, l);
    else new_game();
}

/* the solved puzzle's card: replay (the same puzzle, the same level), next (the next level; a new puzzle of the same
 * variant) */
static void card_replay(void) { restart(1); }
static void card_next(void)
{
    if (games[cur]->levels && !in_random(cur)) level_next();
    else restart(0);
}

/* after anything a player did: a new end of the game (or none any more) is noticed; draw_game() shows it */
static void after_input(void)
{
    if (screen != SCR_GAME) return;
    level_check();
    int s = games[cur]->status();
    if (s != last_status) { over_hidden = 0; end_seen = 0; card_t0 = 0; }
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
    int narrow = b.w < 1.9f * ui.unit;          /* five bars side by side: closer, the token smaller */
    float tk = narrow ? 0.6f * ui.unit : L.tk;
    float cy = b.y + b.h / 2, tx = b.x + (narrow ? 0.04f : 0.15f) * ui.unit + tk / 2;
    if (turn || won) {                          /* the gold light: whose turn it is, or who won */
        float k = turn ? 1.8f : 2.3f;
        ui_put_c(t_gold, tx, cy, tk * k, tk * k);
    }
    ui_put_c(t_tok[p], tx, cy, tk, tk);
    float right = b.x + b.w;
    if (shows_pass(p)) {
        float r = 0.42f * ui.unit;
        draw_button(right - r - 0.05f * ui.unit, cy, r, ICON_PASS, NULL);
        right -= 2 * r + 0.25f * ui.unit;
    }
    if (g->bar) {
        float x = tx + tk / 2 + (narrow ? 0.08f : 0.3f) * ui.unit;
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

box ui_bar_box(int player, int *upside)
{
    int ok = cur >= 0 && player >= 0 && player < ui.players;
    if (upside) *upside = ok && ui_upside(player) && t_bartex;
    return ok ? L.bar[player] : (box){0, 0, 0, 0};
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

/* a solved puzzle's card (a game of levels, or one player with stars): its stars popping in one by one (a game of
 * levels without stars: the player's token in gold light), the moves made beside the fewest possible (score(),
 * par(): digits, the par after a flag), then two buttons: replay (the same puzzle) and next in gold light (the next
 * level, after the last one the level screen; a new puzzle of the same variant) */
static void draw_solved(float cx, float cy, float u)
{
    const game *g = games[cur];
    int row = g->score != NULL;
    float top = cy - (row ? 0.95f : 0.72f) * u, by = cy + (row ? 1.12f : 0.85f) * u;
    if (g->level_stars) {
        int s = g->level_stars();
        s = s < 0 ? 0 : s > 3 ? 3 : s;
        if (!card_t0) {                         /* first shown: the stars to come, each with a soft knock */
            card_t0 = instant ? ui_ms() - 100000 : ui_ms();
            for (int k = 0; k < s && !instant; k++) snd_play_at(SND_PLACE, 0.6f, 150 + 170 * k);
        }
        float t = (float)(ui_ms() - card_t0);
        for (int k = 0; k < 3; k++) {
            float x = cx + (k - 1) * 1.25f * u, y = top - (k == 1 ? 0.14f * u : 0), d = (k == 1 ? 1.2f : 1.0f) * u;
            star_at(x, y, d, 2);
            if (k >= s) continue;
            float e = (t - 150 - 170 * k) / 240;   /* 0..1: it swells past its size, then settles */
            if (e <= 0) {
                ui_animate();
                continue;
            }
            float grow = e >= 1 ? 1 : e < 0.7f ? 1.2f * e / 0.7f : 1.2f - 0.2f * (e - 0.7f) / 0.3f;
            if (e < 1) ui_animate();
            star_at(x, y, d * grow, 0);
        }
    } else {
        ui_put_c(t_gold, cx, top + 0.1f * u, 2.3f * u, 2.3f * u);
        ui_put_c(t_big[0], cx, top + 0.1f * u, 1.5f * u, 1.5f * u);
    }
    if (row) {                                  /* the moves; the par after its flag */
        int m = g->score(), p = g->par ? g->par() : 0;
        float h = 0.58f * u, y = cy + 0.1f * u, fd = 0.72f * u, gap = 0.45f * u;
        m = m < 0 ? 0 : m;
        float mw = ui_number_w(m, h), pw = p > 0 ? ui_number_w(p, h) : 0;
        float w = mw + (p > 0 ? gap + fd + 0.06f * u + pw : 0), x = cx - w / 2;
        ui_number(x + mw / 2, y, h, m, 1);
        if (p > 0) {
            SDL_Texture *flag = btn_tex(paint_btn_icon, ICON_PAR, (int)(fd + 0.5f));
            x += mw + gap;
            if (flag) {
                SDL_SetTextureColorMod(flag, 255, 200, 80);
                ui_put_c(flag, x + fd / 2, y, fd, fd);
                SDL_SetTextureColorMod(flag, 255, 255, 255);
            }
            ui_number_ex(x + fd + 0.06f * u + pw / 2, y, h, p, NUM_LIGHT, 170, 0);
        }
    }
    replay_btn.x = cx - 1.05f * u;
    next_btn.x = cx + 1.05f * u;
    replay_btn.y = next_btn.y = by;
    replay_btn.d = next_btn.d = 1.1f * u;
    int last = g->levels && !in_random(cur) && variant[cur] + 1 >= nlevels(cur);
    ui_button(replay_btn.x, replay_btn.y, replay_btn.d, ICON_REPLAY, 0);
    ui_button(next_btn.x, next_btn.y, next_btn.d, last ? ICON_GRID : ICON_ARROW, UI_LIT);
}

static void draw_over(void)
{
    const game *g = games[cur];
    int st = g->status();
    box b = L.board;
    float u = ui.unit, cx = b.x + b.w / 2, cy = b.y + b.h / 2, big = 2.4f * u;
    ui_fill(b.x, b.y, b.w, b.h, 0, 0, 0, 90);
    ui_put_c(t_card, cx, cy, 6.8f * u, 3.8f * u);
    if (st == GS_WIN0 && (g->levels || (ui.players == 1 && g->level_stars))) {   /* a puzzle solved */
        draw_solved(cx, cy, u);
        return;
    }
    int np = ui.players;
    unsigned won = g->winners ? winners_of(st) : 0;
    if (np > 1 && won) {                       /* winners() (partners, a shared win, a tie): they stand together */
        draw_winners(won, st == GS_DRAW);
    } else if (np == 1 && g->score) {           /* alone, a score: the token on the left, the score in big digits */
        float tx = cx - 1.55f * u;
        if (st == GS_WIN0) {
            ui_put_c(t_gold, tx, cy, big * 1.15f, big * 1.15f);
            ui_put_c(t_big[0], tx, cy, 2.1f * u, 2.1f * u);
        } else {
            ui_put_rot(t_big[0], tx, cy + 0.3f * u, 1.8f * u, 1.8f * u, 90);
        }
        int sc = g->score();
        float h = 1.25f * u, w = ui_number_w(sc < 0 ? -sc : sc, h) + (sc < 0 ? 0.56f * h : 0);
        if (w > 3.2f * u) h *= 3.2f * u / w;    /* a long number: smaller, within the card */
        ui_number_signed(cx + 1.3f * u, cy, h, sc, 1, 0);
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
    int key = ((cur * 8 + hand.player) * 4096 + (int)C.td) * 8 + tok_gen % 8;
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
    level_check();
    next_btn.d = replay_btn.d = 0;
    game_textures();
    g->draw();                                  /* under the cover too: ui.viewer -1, no hand shown */
    game_textures();                            /* again: the game may have changed its tokens while drawing */
    for (int p = 0; p < ui.players; p++) draw_bar(p);
    if (armed && SDL_GetTicks() - armed > 2500) armed = 0;
    for (int i = 0; i < NBTN; i++) {
        if (!shown(i)) continue;
        SDL_Texture *disc = i == B_NEW && armed ? t_btn_armed : i == B_F2F && ui.f2f ? t_btn_on : NULL;
        int icon = i == B_NEW && g->levels ? ICON_GRID : BTN_ICON[i];   /* a level: back to the level screen */
        draw_button(L.bx[i], L.by[i], L.br, icon, disc);
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

/* the picker on two axes: the sizes (the one chosen on a gold card, in gold light), the levels (the one played last at
 * that size in gold light) */
static void draw_axes(void)
{
    const game *G = games[cur];
    int ns = G->axes[0], nl = G->axes[1], v = picked(cur), lev = v >= 0 ? v % nl : 0;
    for (int i = 0; i < ns + nl; i++) {
        box c = L.cards[i];
        int level = i >= ns, k = level ? i - ns : i;
        int on = level ? v >= 0 && v / nl == pick_size[cur] && lev == k : k == pick_size[cur];
        if (on) ui_put_c(t_gold, c.x + c.w / 2, c.y + c.h / 2, c.w * 1.4f, c.h * 1.4f);
        ui_put(on && !level ? t_menu_card_on : t_menu_card, c.x, c.y, c.w, c.h);
        float s = c.w * 0.86f;
        SDL_Texture *t;
        if (G->axis_pic[level]) t = btn_tex(G->axis_pic[level], k, (int)s);
        else t = level ? btn_tex(paint_stones, k, (int)s) : thumb(cur, k * nl + lev);
        ui_put_c(t, c.x + c.w / 2, c.y + c.h / 2, s, s);
    }
}

static void draw_cards(void)
{
    int n = screen == SCR_MENU ? ngames : games[cur]->variants;
    if (screen == SCR_PICK && axes_on(cur)) {   /* two axes: their own cards */
        draw_axes();
        n = 0;
    }
    for (int k = 0; k < L.ntabs && screen == SCR_MENU; k++) {   /* the tabs */
        box b = L.tabs[k];
        int s = L.tab_sec[k];
        float d = floorf(b.h * 0.66f);
        ui_put(s == menu.tab ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
        ui_put_c(t_tabicon[s], b.x + b.w / 2, b.y + b.h / 2, d, d);
    }
    if (screen == SCR_PICK && games[cur]->levels) draw_level_tabs(LV.nch);   /* random play's picker */
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
        if ((screen == SCR_MENU && started[i] && games[i]->status() == GS_PLAY) ||   /* a game in progress */
            (screen == SCR_PICK && (games[cur]->levels ? in_random(cur) && rvariant[cur] == i
                                    : chosen[cur] && variant[cur] == i)))   /* the variant chosen last */
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

/* ---- the level screen -------------------------------------------------------------------------------------------- */
/* a level's picture (game.level_thumb) at the size shown, kept for the chapter shown (a texture a tile) */
static SDL_Texture *level_pic(int i, int level, int size)
{
    static SDL_Texture *t[CHAPTER_MAX];
    static int key[CHAPTER_MAX][3];
    if (size < 4 || i < 0 || i >= CHAPTER_MAX) return NULL;
    if (t[i] && key[i][0] == cur && key[i][1] == level && key[i][2] == size) return t[i];
    uint8_t *b = calloc((size_t)size * size, 4);   /* its own buffer: the game may use ui_buf() */
    if (!b) return NULL;
    games[cur]->level_thumb(level, size, b);
    ui_free(&t[i]);
    t[i] = ui_tex(size, size, b);
    free(b);
    key[i][0] = cur;
    key[i][1] = level;
    key[i][2] = size;
    return t[i];
}

/* a level's tile: gold when solved (its stars under its number), wood when open, dim walnut when locked; the next
 * level to play in gold light. With the game's picture: the picture on the tile, its number on a dark disc in the
 * corner, the stars along the bottom. */
static void draw_tile(int l, int i, box t, int next)
{
    static const uint8_t dim[3] = {150, 120, 90};
    const game *G = games[cur];
    int best = LV.best[l], open = level_open(cur, l), n = l + 1, stars = best && G->level_stars;
    float T = t.w, cx = t.x + T / 2, cy = t.y + T / 2;
    if (l == next && open) ui_mark_gold(cx, cy, 1.5f * T);
    ui_shadow_flat(cx + 0.04f * T, cy + 0.06f * T, T, T, 0.55f);
    ui_put(btn_tex(paint_tile, best ? 0 : open ? 1 : 2, (int)T), t.x, t.y, T, T);
    if (G->level_thumb) {
        float ps = floorf(0.8f * T), bd = 0.36f * T, bx = t.x + 0.17f * T, by = t.y + 0.17f * T;
        SDL_Texture *p = level_pic(i, l, (int)ps);
        if (p) {
            SDL_SetTextureAlphaMod(p, open ? 255 : 90);
            ui_put_c(p, cx, cy, ps, ps);
            SDL_SetTextureAlphaMod(p, 255);
        }
        ui_put_c(btn_tex(paint_badge, 0, (int)bd), bx, by, bd, bd);
        float nh = fminf(0.5f * bd, 0.86f * bd / (0.66f * (n >= 100 ? 2 : n >= 10 ? 1 : 0) + 0.62f));   /* fits the disc */
        ui_number_ex(bx, by, nh, n, CREAM, open ? 255 : 140, UI_NUM_TIGHT);
        for (int k = 0; stars && k < 3; k++)
            star_at(cx + (k - 1) * 0.25f * T, t.y + 0.85f * T, 0.25f * T, k < best - 1 ? 0 : 1);
        return;
    }
    ui_number_ex(cx, cy - (stars ? 0.12f * T : 0), 0.4f * T, n, open ? NUM_DARK : dim, open ? 255 : 160, 0);
    for (int k = 0; stars && k < 3; k++)
        star_at(cx + (k - 1) * 0.25f * T, cy + 0.28f * T, 0.24f * T, k < best - 1 ? 0 : 1);
}

/* the level screen's tabs (on: the one shown; LV.nch: random play) */
static void draw_level_tabs(int on)
{
    const game *G = games[cur];
    if (level_ntabs() <= 1) return;
    progress_load(cur);
    if (has_random(cur)) {                      /* random play: a die */
        box b = LV.tabs[LV.nch];
        float d = floorf(b.h * 0.66f);
        ui_put(on == LV.nch ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
        ui_put_c(t_tabicon[SEC_RACE], b.x + b.w / 2, b.y + b.h / 2, d, d);
    }
    for (int k = 0; k < LV.nch; k++) {          /* the chapters: the game's picture, else the number */
        box b = LV.tabs[k];
        int any = 0;
        for (int l = LV.first[k]; l < LV.first[k + 1] && !any; l++) any = level_open(cur, l);
        Uint8 a = any ? 255 : 90;
        float d = floorf(b.h * 0.66f), cy = b.y + b.h / 2, nh = floorf(b.h * 0.4f);
        ui_put(k == on ? t_tab_on : t_tab, b.x, b.y, b.w, b.h);
        SDL_Texture *p = G->chapter_icon ? btn_tex(G->chapter_icon, k, (int)d) : NULL;
        if (!p) {
            ui_number_ex(b.x + b.w / 2, cy, nh, k + 1, CREAM, a, 0);
            continue;
        }
        float nw = b.w >= 1.9f * b.h ? ui_number_w(k + 1, nh) : 0, gap = nw ? 0.12f * b.h : 0;
        float x0 = b.x + (b.w - d - gap - nw) / 2;   /* wide enough: the picture, then the number */
        SDL_SetTextureAlphaMod(p, a);
        ui_put(p, x0, cy - d / 2, d, d);
        SDL_SetTextureAlphaMod(p, 255);
        if (nw) ui_number_ex(x0 + d + gap + nw / 2, cy, nh, k + 1, CREAM, a, 0);
    }
}

static void draw_levels(void)
{
    const game *G = games[cur];
    progress_load(cur);
    draw_level_tabs(LV.chapter);
    int first = LV.first[LV.chapter], n = LV.first[LV.chapter + 1] - first, next = first_unsolved(cur);
    if (n > CHAPTER_MAX) n = CHAPTER_MAX;
    for (int i = 0; i < n; i++) draw_tile(first + i, i, LV.tiles[i], next);
    draw_button(fminf(ui.w, ui.h) * 0.05f + L.snd_r, L.snd_y, L.snd_r, ICON_HOME, NULL);
    if (G->level_stars) {                       /* the stars won, at the top right */
        int total = 0;
        for (int l = 0; l < nlevels(cur); l++) total += LV.best[l] ? LV.best[l] - 1 : 0;
        float h = floorf(L.snd_r * 0.95f), nw = ui_number_w(total, h), right = L.snd_x + L.snd_r, d = 1.5f * h;
        ui_number_ex(right - nw / 2, L.snd_y, h, total, CREAM, 255, 0);
        star_at(right - nw - 0.25f * h - d / 2, L.snd_y, d, 0);
    }
}

/* ---- the rules page ---------------------------------------------------------------------------------------------- */
static char *help_text;                        /* the game's rules, then the app's buttons */

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
    else if (screen == SCR_LEVELS) draw_levels();
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
    uint8_t b = (uint8_t)s;                     /* the menu opens there next time */
    ui_save_set("shell/tab", &b, 1);
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

/* the level screen's last tab: random play. Its variant picker (the level screen's tabs over it), or a random
 * puzzle at once when there is nothing to pick */
static void random_tab(void)
{
    if (games[cur]->variants > 1 || many_counts(cur)) {
        screen = SCR_PICK;
        axes_enter(cur);
        layout();
        return;
    }
    rnd_mode[cur] = 1;
    open_game(cur, 0);
}

static void levels_step(int d)                  /* the level screen's chapter on the left (-1) or the right (1) */
{
    if (LV.chapter + d < 0 || LV.chapter + d >= level_ntabs()) return;
    snd_play(SND_TAP);
    if (LV.chapter + d == LV.nch) {             /* past the last chapter: random play */
        random_tab();
        return;
    }
    LV.chapter += d;
    layout();
}

static int in_box(box b, float x, float y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

/* the level screen: home and the tabs at once; a tile on release, unless the finger swiped (another chapter) */
static void levels_press(float x, float y)
{
    if (in_circle(x, y, fminf(ui.w, ui.h) * 0.05f + L.snd_r, L.snd_y, L.snd_r)) {
        snd_play(SND_TAP);
        go_home();
        return;
    }
    for (int k = 0; k < level_ntabs() && level_ntabs() > 1; k++)
        if (in_box(LV.tabs[k], x, y)) {
            snd_play(SND_TAP);
            if (k == LV.nch) {
                random_tab();
                return;
            }
            LV.chapter = k;
            layout();
            return;
        }
    LV.grabbing = 1;
    LV.moved = 0;
    LV.grab_x = x;
    LV.grab_y = y;
}

static void levels_release(float x, float y)
{
    float s = fminf(ui.w, ui.h);
    LV.grabbing = 0;
    if (LV.moved) {
        if (fabsf(x - LV.grab_x) > 0.12f * s && fabsf(x - LV.grab_x) > fabsf(y - LV.grab_y))
            levels_step(x < LV.grab_x ? 1 : -1);
        return;
    }
    int first = LV.first[LV.chapter], n = LV.first[LV.chapter + 1] - first;
    for (int i = 0; i < n && i < CHAPTER_MAX; i++)
        if (in_box(LV.tiles[i], x, y)) {
            if (!level_open(cur, first + i)) { snd_play(SND_DENY); return; }
            snd_play(SND_TAP);
            rnd_mode[cur] = 0;
            open_game(cur, first + i);
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
    if (screen == SCR_LEVELS) {
        levels_press(x, y);
        return;
    }
    if (screen != SCR_GAME) {
        float home_x = fminf(ui.w, ui.h) * 0.05f + L.snd_r;
        if (screen == SCR_MENU && in_circle(x, y, L.snd_x, L.snd_y, L.snd_r)) {
            snd_muted = !snd_muted;
            remember_mute();
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
            remember_game(cur);
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
        for (int k = 0; k < LV.nch && games[cur]->levels; k++)   /* random play's picker: back to a chapter */
            if (in_box(LV.tabs[k], x, y)) {
                snd_play(SND_TAP);
                LV.chapter = k;
                screen = SCR_LEVELS;
                layout();
                return;
            }
        if (axes_on(cur)) {                     /* two axes: a size chosen, or a level started at that size */
            int ns = games[cur]->axes[0], nl = games[cur]->axes[1];
            for (int i = 0; i < ns + nl; i++) {
                if (!in_box(L.cards[i], x, y)) continue;
                snd_play(SND_TAP);
                if (i < ns) {
                    pick_size[cur] = i;
                    return;
                }
                if (games[cur]->levels) rnd_mode[cur] = 1;   /* random play */
                open_game(cur, pick_size[cur] * nl + i - ns);
                return;
            }
            return;
        }
        int n = games[cur]->variants;
        for (int i = 0; i < n; i++) {
            box c = L.cards[i];
            if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) {
                snd_play(SND_TAP);
                if (games[cur]->levels) rnd_mode[cur] = 1;   /* random play */
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
    if (games[cur]->status() != GS_PLAY && !over_hidden) {
        if (next_btn.d > 0 && ui_button_hit(next_btn.x, next_btn.y, next_btn.d, x, y)) {   /* a solved puzzle's next */
            snd_play(SND_TAP);
            card_next();
            return;
        }
        if (replay_btn.d > 0 && ui_button_hit(replay_btn.x, replay_btn.y, replay_btn.d, x, y)) {   /* the same again */
            snd_play(SND_TAP);
            card_replay();
            return;
        }
        over_hidden = 1;
        return;
    }
    if (games[cur]->status() == GS_WIN0 && (games[cur]->levels || (ui.players == 1 && games[cur]->level_stars))) {
        over_hidden = 0;                        /* a solved puzzle: its card again */
        return;
    }
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
    if (screen == SCR_LEVELS && LV.grabbing && hypotf(x - LV.grab_x, y - LV.grab_y) > fminf(ui.w, ui.h) * 0.02f)
        LV.moved = 1;
    if (screen == SCR_GAME && !(is_view() && hand.up)) games[cur]->motion(x, y);
}

static void release(float x, float y)
{
    if (help.open) {
        help.grabbing = 0;
        return;
    }
    if (screen == SCR_LEVELS) {
        if (LV.grabbing) levels_release(x, y);
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
    if (type == SDL_FINGERDOWN && games[cur]->status() != GS_PLAY && !over_hidden) {   /* as press(): the end card */
        over_hidden = 1;
        return;
    }
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

/* before a gesture: the game's animations over (a deal, a slide, the dice), so the finger is not ignored */
static void shot_wait(void)
{
    const game *g = games[cur];
    for (int k = 0; k < 8 && ((g->busy && g->busy()) || ui_dice_busy()); k++) {
        ms_ahead += 600000;
        draw();
    }
}

/* the picture mode's gestures, in the order given: --taps "fx,fy ...", --drag "fx0,fy0 fx1,fy1" (the finger down,
 * moved, lifted), --hold "fx,fy ms" (the finger down that long; the last gesture: the picture with it still down).
 * Fractions of the board's box (the menu, a picker, the level screen: of the screen). --keys "Up Left": keys pressed
 * (SDL's names), as from the keyboard. */
enum { GE_TAP, GE_DRAG, GE_HOLD, GE_KEY };
static void key(SDL_Keycode k, int repeat);
#define MAX_GESTURES 16
static struct { int kind; const char *s; } gesture[MAX_GESTURES];
static int ngestures;

/* base: the clock before the last gesture; held: the last gesture's hold (ms), the finger still down (else -1) */
static void shot_gestures(int reveal, Uint32 *base, int *held)
{
    int first = 1;
    for (int k = 0; k < ngestures; k++) {
        for (const char *s = gesture[k].s; s && *s; first = 0) {
            float fx, fy, gx = 0, gy = 0;
            int n = 0, ms = 0, kind = gesture[k].kind;
            if (kind == GE_KEY) {               /* a key: its name up to the next space */
                char name[32];
                if (sscanf(s, "%31s%n", name, &n) < 1) break;
                s += n;
                SDL_Keycode kc = SDL_GetKeyFromName(name);
                if (kc == SDLK_UNKNOWN) { fprintf(stderr, "no key %s\n", name); break; }
                if (screen == SCR_GAME) shot_wait();
                *base = ms_ahead;
                key(kc, 0);
                after_input();
                while (*s == ' ') s++;
                continue;
            }
            if (sscanf(s, "%f,%f%n", &fx, &fy, &n) < 2) break;
            s += n;
            if (kind == GE_DRAG && sscanf(s, " %f,%f%n", &gx, &gy, &n) < 2) break;
            if (kind == GE_HOLD && sscanf(s, " %d%n", &ms, &n) < 1) break;
            if (kind != GE_TAP) s += n;
            if (!first && is_view()) shot_settle(reveal, 1);   /* the last tap has landed, the device passed */
            if (screen == SCR_GAME) shot_wait();
            *base = ms_ahead;
            box f = screen == SCR_GAME ? L.board : (box){0, 0, (float)ui.w, (float)ui.h};
            float x = f.x + fx * f.w, y = f.y + fy * f.h;
            press(x, y);
            if (kind == GE_DRAG) {              /* the finger slides there in steps, then lifts */
                float x1 = f.x + gx * f.w, y1 = f.y + gy * f.h;
                for (int i = 1; i <= 8; i++) motion(x + (x1 - x) * i / 8, y + (y1 - y) * i / 8);
                x = x1;
                y = y1;
            } else if (kind == GE_HOLD) {       /* held down: the clock moves on meanwhile */
                ms_ahead += (Uint32)(ms > 0 ? ms : 0);
                draw();
                while (*s == ' ') s++;
                *held = k == ngestures - 1 && !*s ? ms : -1;
                if (*held >= 0) return;         /* the last gesture: the picture with the finger down */
            }
            release(x, y);
            after_input();
            while (*s == ' ') s++;
        }
    }
}

/* --solved "1-12 13-14:2 15:0": levels 1 to 12 solved with 3 stars, 13 and 14 with 2, 15 with none; 0: wrong */
static int shot_solved(const char *s)
{
    if (!games[cur]->levels) return 0;
    progress_load(cur);
    for (;;) {
        int a, b, k = 3, n = 0;
        while (*s == ' ' || *s == ',') s++;
        if (!*s) break;
        if (sscanf(s, "%d%n", &a, &n) < 1) return 0;
        s += n;
        b = a;
        if (*s == '-') {
            if (sscanf(s + 1, "%d%n", &b, &n) < 1) return 0;
            s += 1 + n;
        }
        if (*s == ':') {
            if (sscanf(s + 1, "%d%n", &k, &n) < 1) return 0;
            s += 1 + n;
        }
        if (a < 1 || b > nlevels(cur) || a > b || k < 0 || k > 3) return 0;
        for (int l = a - 1; l < b; l++) LV.best[l] = (uint8_t)(1 + k);
    }
    progress_save();
    return 1;
}

/* --icons: every icon of the shell on a button, in rows of six, as games show them (plain, then in gold light, then
 * on the gold disc) */
static void draw_icons(void)
{
    ui_put(t_bg, 0, 0, (float)ui.w, (float)ui.h);
    float d = fminf(ui.w / 8.0f, ui.h / 18.0f), step = 1.3f * d;
    static const int how[3] = {0, UI_LIT, UI_ON};
    int rows = (ICONS + 5) / 6;
    for (int k = 0; k < 3; k++)
        for (int i = 0; i < ICONS; i++) {
            float x = ui.w / 2.0f + (i % 6 - 2.5f) * step, y = d + (k * (rows + 0.5f) + i / 6) * step;
            ui_button(x, y, d, i, how[k]);
        }
}

static int shot(int argc, char **argv)
{
    const char *out = NULL, *id = NULL, *moves = NULL, *lang = NULL, *save_dir = NULL, *solved = NULL;
    int v = 0, keep = 0, pick = 0, show_help = 0, np = 0, tab = -1, reveal = 0, level = 0, rnd = 0, icons = 0;
    float scroll = 0;
    uint32_t seed = 1;
    int at = -1;                                /* --at ms: the picture that long after the last tap (else at rest) */
    int held = -1;                              /* held: the last gesture's hold (ms), the finger still down */
    Uint32 base = 0;                            /* the clock before the last tap */
    ui.w = 720;
    ui.h = 1280;
    shot_clock = 1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shot") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) sscanf(argv[++i], "%dx%d", &ui.w, &ui.h);
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) id = argv[++i];
        else if (!strcmp(argv[i], "--variant") && i + 1 < argc) v = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--moves") && i + 1 < argc) moves = argv[++i];
        else if ((!strcmp(argv[i], "--taps") || !strcmp(argv[i], "--drag") || !strcmp(argv[i], "--hold") ||
                  !strcmp(argv[i], "--keys")) &&
                 i + 1 < argc && ngestures < MAX_GESTURES) {
            char c = argv[i][2];
            gesture[ngestures].kind = c == 't' ? GE_TAP : c == 'd' ? GE_DRAG : c == 'h' ? GE_HOLD : GE_KEY;
            gesture[ngestures++].s = argv[++i];
        }
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
        else if (!strcmp(argv[i], "--random")) rnd = 1;
        else if (!strcmp(argv[i], "--icons")) icons = 1;
        else if (!strcmp(argv[i], "--save") && i + 1 < argc) save_dir = argv[++i];
        else if (!strcmp(argv[i], "--level") && i + 1 < argc) level = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--solved") && i + 1 < argc) solved = argv[++i];
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
    if (save_dir) {                             /* else nothing kept from a past run: the same picture every time */
        save_open(save_dir);
        settings_load();
    }
    if (id) {
        cur = find_game(id);
        if (cur < 0) { fprintf(stderr, "no game %s\n", id); return 1; }
        if (np) nplayers[cur] = np;             /* --players n: how many play */
        ui.players = players_of(cur);
        if (solved && !shot_solved(solved)) { fprintf(stderr, "bad --solved\n"); return 1; }
        if (pick) {                             /* the picker, or the level screen (--tab n: its chapter n, 0..) */
            show_picker(cur);
            if (screen == SCR_LEVELS && tab >= 0 && tab == LV.nch && has_random(cur)) random_tab();
            else if (screen == SCR_LEVELS && tab >= 0) {
                LV.chapter = tab;
                layout();
            }
        } else {
            if (level > nlevels(cur)) { fprintf(stderr, "no level %d\n", level); return 1; }
            if (rnd && !has_random(cur)) { fprintf(stderr, "no random play\n"); return 1; }
            rnd_mode[cur] = rnd;                /* --random: a game of levels at random (--variant n) */
            open_game(cur, level > 0 ? level - 1 : v);   /* --level n: level n (1..) */
            if (moves && (!games[cur]->script || !games[cur]->script(moves))) {
                fprintf(stderr, "bad moves\n");
                return 1;
            }
            draw();                             /* once before the taps, as the app always has: the bars' layout */
            if (reveal) shot_settle(1, 0);
        }
    } else {
        layout();
        if (tab >= 0) menu_tab(tab);           /* --tab n: that section's tab (0 strategy .. 6 puzzles) */
        if (!show_help) menu_wheel(scroll);     /* --scroll px: further down the menu */
    }
    shot_gestures(reveal, &base, &held);
    if (id) over_hidden = keep;
    /* the gestures' animations: over, or caught --at ms after the last one began (a hold: as long as it was held) */
    ms_ahead = base + (at >= 0 ? (Uint32)at : held >= 0 ? (Uint32)held : 600000);
    if (id && !pick && reveal) shot_settle(1, 0);
    if (show_help) {                            /* the rules page (the game's, or the app's), scrolled by --scroll px */
        open_help(id ? cur : -1);
        help_scroll(scroll);
    }
    if (icons) draw_icons();                    /* --icons: every icon of the shell, on its buttons */
    else draw();
    SDL_RenderPresent(ui.r);
    if (!out || SDL_SaveBMP(surf, out)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    int st = id && started[cur] ? games[cur]->status() : -1;   /* -1: the menu, or a game not started (--pick) */
    printf("%s: %dx%d %s status %d\n", out, ui.w, ui.h, id ? id : "menu", st);
    save_flush();                               /* --save DIR: written back (else nothing to write) */
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

/* repeat: the key held down (only the shell's scrolling and undo repeat; a game gets a key once) */
static void key(SDL_Keycode k, int repeat)
{
    if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
        if (help.open) help.open = 0;
        else if (screen == SCR_MENU) quit = 1;
        else go_home();
        return;
    }
    if (k == SDLK_m) {
        snd_muted = !snd_muted;
        remember_mute();
        return;
    }
    if (k == SDLK_h || k == SDLK_F1) { if (!help.open) open_help(screen == SCR_GAME ? cur : -1); return; }
    if (screen == SCR_MENU && !help.open && (k == SDLK_LEFT || k == SDLK_RIGHT)) {   /* the tabs */
        menu_step(k == SDLK_RIGHT ? 1 : -1);
        return;
    }
    if (screen == SCR_PICK && !help.open && k == SDLK_LEFT && games[cur]->levels) {   /* random play: the chapters */
        LV.chapter = LV.nch - 1;
        screen = SCR_LEVELS;
        layout();
        return;
    }
    if (screen == SCR_LEVELS && !help.open && (k == SDLK_LEFT || k == SDLK_RIGHT)) {   /* the chapters */
        levels_step(k == SDLK_RIGHT ? 1 : -1);
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
    else if (k != SDLK_f && k != SDLK_t && games[cur]->key && !repeat && !(is_view() && hand.up) && !ui_handing())
        games[cur]->key(k);                     /* the game's own keys (the arrows: a push) */
}

int main(int argc, char **argv)
{
    qsort(games, (size_t)ngames, sizeof games[0], by_order);
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--shot")) return shot(argc, argv);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    /* every way round: with a resizable window SDL would otherwise lock Android to portrait (the manifest ignored) */
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    const char *lang = NULL;                    /* --lang fr: another language than the device's */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--lang")) lang = argv[i + 1];
    text_init(lang);
    const char *save_dir = NULL;                /* --save DIR: the settings and progress kept there */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--save")) save_dir = argv[i + 1];
    save_open(save_dir);
    settings_load();
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
                case SDL_APP_WILLENTERBACKGROUND:
                case SDL_APP_TERMINATING: save_flush(); break;
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
                    key(e.key.keysym.sym, e.key.repeat);
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
        save_flush();                           /* what changed, written at once (nothing: no write) */
    }
    save_flush();
    SDL_Quit();
    return 0;
}
