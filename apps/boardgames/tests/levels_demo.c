/*
 * The level screen's test (make ... DEMO=1, never shipped): three tiny games for one player. Each level is
 * a row of discs to turn gold, a tap each; a tap on a gold one turns it back (a move wasted).
 * - "levelsdemo": 40 levels in 4 even chapters (`chapters`), the tabs' pictures (`chapter_icon`), stars (3 with no
 *   move wasted, 2 with one or two, else 1);
 * - "levelsdemo2": 30 levels in 3 uneven chapters (`chapter_of`: 12, 10, 8), the tiles' pictures (`level_thumb`), no
 *   stars, all the levels open (`levels_open`).
 * - "scoredemo": no levels, the game's keys (`key`) and a score on its end card (`score`).
 * The notation (script): "t3" taps disc 3 (from 1), "=" taps every dark one.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "shell.h"

#define MOST 9

static int level, n, moves;
static uint8_t lit[MOST];
static box B;
static SDL_Texture *t_board, *t_disc[2];
static int made = -1;

static int discs_of(int l) { return 2 + l % 7; }

static void start(int l)
{
    level = l;
    n = discs_of(l);
    moves = 0;
    memset(lit, 0, sizeof lit);
}

static int solved(void)
{
    for (int i = 0; i < n; i++)
        if (!lit[i]) return 0;
    return 1;
}

static int status(void) { return solved() ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int stars(void) { return moves <= n ? 3 : moves <= n + 2 ? 2 : 1; }

static void tap(int i)
{
    if (i < 0 || i >= n || solved()) return;
    lit[i] = !lit[i];
    moves++;
}

static int undo(void) { return 0; }

static int script(const char *s)
{
    for (; *s; s++) {
        if (*s == ' ') continue;
        if (*s == '=') {
            for (int i = 0; i < n; i++)
                if (!lit[i]) tap(i);
        } else if (*s == 't') {
            int i = atoi(s + 1);
            if (i < 1 || i > n) return 0;
            tap(i - 1);
            while (s[1] >= '0' && s[1] <= '9') s++;
        } else {
            return 0;
        }
    }
    return 1;
}

/* the discs in rows of up to three, across the plank */
static void spot(int i, float *cx, float *cy, float *d)
{
    int rows = (n + 2) / 3, r = i / 3, in_row = r == rows - 1 ? n - 3 * r : 3;
    *d = B.w / 4.2f;
    *cx = B.x + B.w / 2 + (i % 3 - (in_row - 1) / 2.0f) * B.w / 3.4f;
    *cy = B.y + B.h / 2 + (r - (rows - 1) / 2.0f) * B.w / 3.4f;
}

static void layout(box b)
{
    B = b;
    int s = (int)b.w, d = (int)(b.w / 4.2f);
    if (made == s) return;
    made = s;
    uint8_t *p = calloc((size_t)s * s, 4);
    if (!p) return;
    art_board_n(1, 1, s - 2 * (s / 24), s / 24, 0, p);
    ui_free(&t_board);
    t_board = ui_tex(s, s, p);
    for (int k = 0; k < 2; k++) {
        memset(p, 0, (size_t)d * d * 4);
        art_counter(COUNTER_DISC, k ? 4 : 1, d, p);
        ui_free(&t_disc[k]);
        t_disc[k] = ui_tex(d, d, p);
    }
    free(p);
}

static void draw(void)
{
    ui_put(t_board, B.x, B.y, B.w, B.h);
    for (int i = 0; i < n; i++) {
        float cx, cy, d;
        spot(i, &cx, &cy, &d);
        if (lit[i]) ui_mark_gold(cx, cy, 1.5f * d);
        ui_flat_at(t_disc[lit[i]], cx, cy, d, 1, 0);
    }
}

static void press(float x, float y)
{
    for (int i = 0; i < n; i++) {
        float cx, cy, d;
        spot(i, &cx, &cy, &d);
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < d * d / 4) {
            tap(i);
            ui_sound(SND_PLACE);
            return;
        }
    }
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

static void bar(int player, box b)
{
    (void)player;
    ui_bar_number(b, NULL, moves);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player ? 1 : 4, size, rgba); }

static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    art_counter(COUNTER_DISC, 4, size, rgba);
}

/* a chapter's picture: as many discs as its number */
static void chapter_icon(int c, int size, uint8_t *rgba)
{
    int d = size / 2;
    uint8_t *p = calloc((size_t)d * d, 4);
    if (!p) return;
    art_counter(COUNTER_DISC, 4, d, p);
    for (int k = 0; k <= c; k++)
        art_blend(rgba, size, size, p, d, d, k % 2 * (size - d), k / 2 * (size - d));
    free(p);
}

/* a level's picture: its discs, small */
static void level_thumb(int l, int size, uint8_t *rgba)
{
    int k = discs_of(l), d = size / 4;
    uint8_t *p = calloc((size_t)d * d, 4);
    if (!p) return;
    art_counter(COUNTER_DISC, 1, d, p);
    for (int i = 0; i < k; i++)
        art_blend(rgba, size, size, p, d, d, size / 8 + i % 3 * (size * 3 / 10), size / 8 + i / 3 * (size * 3 / 10));
    free(p);
}

static int chapter_of(int l) { return l < 12 ? 0 : l < 22 ? 1 : 2; }

static const game demo_game = {
    .id = "levelsdemo", .order = 9000, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .players = 1u << 1,
    .levels = 40, .chapters = 4, .chapter_icon = chapter_icon, .level_stars = stars,
};
REGISTER_GAME(demo_game)

static const game demo2_game = {
    .id = "levelsdemo2", .order = 9001, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .players = 1u << 1,
    .levels = 30, .chapter_of = chapter_of, .level_thumb = level_thumb, .levels_open = 30,
};
REGISTER_GAME(demo2_game)

/* "scoredemo": no levels; the keys (1..9 tap a disc, the right arrow the first dark one) and a score on the end card
 * (ten a move) */
static void demo_key(SDL_Keycode k)
{
    if (k >= SDLK_1 && k <= SDLK_9) tap((int)(k - SDLK_1));
    for (int i = 0; k == SDLK_RIGHT && i < n; i++)
        if (!lit[i]) { tap(i); break; }
}

static int score(void) { return 10 * moves; }

static void start_score(int variant) { (void)variant, start(3); }

static const game score_game = {
    .id = "scoredemo", .order = 9002, .variants = 1,
    .thumb = thumb, .token = token, .start = start_score, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .players = 1u << 1, .key = demo_key, .score = score,
};
REGISTER_GAME(score_game)
