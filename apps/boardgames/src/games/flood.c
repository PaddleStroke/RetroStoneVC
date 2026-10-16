/*
 * Flood in RetroStone Board, for one player: the house plank in its walnut frame, its cells stained in six colours a
 * colourblind eye tells apart (the Okabe-Ito set), each colour with its own small sign, in all the room the shell
 * gives (the rules: flood_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The shell's tools are the colours (a disc each, its sign on it) and the lamp; a tap on a cell of the board chooses
 * its colour too (or a digit key, 1..6). The region from the top left corner (a gold mark on the frame) takes the
 * colour, the cells changing in a ripple from the corner; patches of one colour show as one, a groove only between
 * different colours. A colour that floods nothing is dimmed. The bar: the moves, and the moves left (the hourglass);
 * none left and the board not flooded: the corner glows red, the colours dim (undo). The lamp plays the next move of
 * the best way the solver finds, the cells it takes in gold light. Undo takes back one move, all the way to the start.
 * Flooded: a wave of gold light from the corner; the stars rate the moves against the par (ui_par_stars). The shell's
 * level screen chooses a level (150 in six chapters, made by tools/flood_gen.c), or its die tab a board at random.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/bench_ui.h"
#include "games/flood.h"
#include "shell.h"

#define FRAME 0.3f                              /* the frame, in cells */
#define RIPPLE 22                               /* ms a cell further from the corner */
#define TAKE_MS 160                             /* a cell taking its colour */
#define HINT_MS 1300

static const uint8_t FL_RGB[FLD_COLOURS][3] = {
    {230, 159, 0}, {86, 180, 233}, {0, 158, 115}, {240, 228, 66}, {0, 114, 178}, {204, 121, 167}};
static const uint8_t INK[3] = {36, 26, 20}, CREAM[3] = {250, 244, 230}, GOLD[3] = {236, 194, 96};

static int light_colour(int c) { return 0.3f * FL_RGB[c][0] + 0.59f * FL_RGB[c][1] + 0.11f * FL_RGB[c][2] > 140; }

static fld_game G;
static ui_grid grid;
static box last_box;
static int fr, made_key = -1, made_small = -1;
static SDL_Texture *t_board, *t_cell[FLD_COLOURS], *t_small;

static uint8_t was[FLD_CELLS];                  /* a cell's colour before the last move */
static Uint32 change_at[FLD_CELLS];             /* when it takes the new one */
static uint8_t hinted[FLD_CELLS];               /* the cells the hint took */
static Uint32 hint_t0, move_end;
static int hint_on;
static bench_wave win;

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a colour's sign, in the unit square: a disc, a square, a triangle, a diamond, a plus, a star */
static void sign_fig(art_fig *f, int c, float r)
{
    switch (c) {
    case 0: art_ellipse(f, ART_PART, 0.5f, 0.5f, r, r); break;
    case 1: art_rect(f, ART_PART, 0.5f - 0.85f * r, 0.5f - 0.85f * r, 0.5f + 0.85f * r, 0.5f + 0.85f * r); break;
    case 2: {
        const float q[] = {0.5f, 0.5f - 1.05f * r, 0.5f + 1.1f * r, 0.5f + 0.85f * r,
                           0.5f - 1.1f * r, 0.5f + 0.85f * r};
        art_poly(f, ART_PART, q, 3);
        break;
    }
    case 3: {
        const float q[] = {0.5f, 0.5f - 1.15f * r, 0.5f + 1.15f * r, 0.5f,
                           0.5f, 0.5f + 1.15f * r, 0.5f - 1.15f * r, 0.5f};
        art_poly(f, ART_PART, q, 4);
        break;
    }
    case 4:
        art_rect(f, ART_PART, 0.5f - 0.33f * r, 0.5f - 1.05f * r, 0.5f + 0.33f * r, 0.5f + 1.05f * r);
        art_rect(f, ART_PART, 0.5f - 1.05f * r, 0.5f - 0.33f * r, 0.5f + 1.05f * r, 0.5f + 0.33f * r);
        break;
    default: art_star(f, ART_PART, 0.5f, 0.53f, 1.2f * r); break;
    }
}

static void paint_sign(uint8_t *o, int size, int c, float r, float alpha)
{
    uint8_t *s = calloc((size_t)size * size, 4);
    if (!s) return;
    art_fig *f = art_fig_new();
    sign_fig(f, c, r);
    const uint8_t *ink = light_colour(c) ? INK : CREAM;
    art_fig_paint(s, size, f, ink, ink, 0, 0);
    for (int i = 0; i < size * size; i++) s[4 * i + 3] = (uint8_t)(s[4 * i + 3] * alpha);
    art_blend(o, size, size, s, size, size, 0, 0);
    free(s);
}

/* the wood stained in colour c, its grain showing: w x h of the plank at (x, y) of picture o (pw wide), from a plank
 * picture p of the same size */
static void stain(uint8_t *o, int pw, const uint8_t *p, int x0, int y0, int w, int h, int c)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++) {
            uint8_t *d = o + 4 * ((size_t)y * pw + x);
            const uint8_t *s = p + 4 * ((size_t)y * pw + x);
            float l = (0.3f * s[0] + 0.59f * s[1] + 0.11f * s[2]) / 255.0f;
            float k = clampf(0.8f + 0.55f * (l - 0.45f), 0.6f, 1.1f);
            for (int i = 0; i < 3; i++) d[i] = (uint8_t)clampf(FL_RGB[c][i] * k + 12, 0, 255);
            d[3] = 255;
        }
}

/* a cell of colour c, q x q: stained wood and its sign (sign: its part of the cell; 0 none) */
static void paint_cell(uint8_t *o, int q, int c, float sign)
{
    uint8_t *p = malloc((size_t)q * q * 4);
    if (!p) return;
    art_board_n(1, 1, q, 0, 0, p);
    stain(o, q, p, 0, 0, q, q, c);
    free(p);
    if (sign > 0) paint_sign(o, q, c, sign / 2, 0.8f);
}

/* the plank in its frame (w x w), the corner's gold mark */
static void paint_plank(uint8_t *o, int w, int f)
{
    art_board_n(1, 1, w - 2 * f, f, 0, o);
    const float corner[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    art_fig *g = art_fig_new();
    art_poly(g, ART_PART, corner, 3);
    uint8_t *t = calloc((size_t)f * f, 4);
    if (!t) return;
    art_fig_paint(t, f, g, GOLD, INK, 0.06f, 0.5f);
    art_blend(o, w, w, t, f, f, 0, 0);
    free(t);
}

/* a board in a picture (size x size, its cells at their colours; signs when the cells are big enough) */
static void board_pic(const fld_board *b, int size, uint8_t *rgba)
{
    int n = b->n, f = (int)fmaxf(1, 0.04f * size), q = (size - 2 * f) / n, w = n * q + 2 * f, o = (size - w) / 2;
    uint8_t *pic = malloc((size_t)w * w * 4), *plank = malloc((size_t)w * w * 4);
    if (!pic || !plank) {
        free(pic), free(plank);
        return;
    }
    art_board_n(1, 1, w - 2 * f, f, 0, pic);
    memcpy(plank, pic, (size_t)w * w * 4);
    for (int i = 0; i < n * n; i++) stain(pic, w, plank, f + i % n * q, f + i / n * q, q, q, b->c[i]);
    float t = fmaxf(1, 0.06f * q);
    for (int i = 0; i < n * n; i++) {          /* the grooves between colours */
        int x = i % n, y = i / n;
        float px = (float)(f + x * q), py = (float)(f + y * q);
        if (x < n - 1 && b->c[i] != b->c[i + 1])
            puz_ink_rect(pic, w, w, px + q - t / 2, py, px + q + t / 2, py + q, INK, 0.7f);
        if (y < n - 1 && b->c[i] != b->c[i + n])
            puz_ink_rect(pic, w, w, px, py + q - t / 2, px + q, py + q + t / 2, INK, 0.7f);
    }
    if (q >= 20) {                              /* the signs, where they read (the menu's card) */
        uint8_t *s = calloc((size_t)q * q, 4);
        for (int i = 0; s && i < n * n; i++) {
            memset(s, 0, (size_t)q * q * 4);
            paint_sign(s, q, b->c[i], 0.17f, 0.8f);
            art_blend(pic, w, w, s, q, q, f + i % n * q, f + i / n * q);
        }
        free(s);
    }
    art_blend(rgba, size, size, pic, w, w, o, o);
    free(pic), free(plank);
}

/* a colour's tool: a lacquered disc of it, its sign */
static void paint_tool(int c, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_fig_paint(o, size, f, FL_RGB[c], INK, fmaxf(0.03f, 1.3f / size), 0.45f);
    paint_sign(o, size, c, 0.2f, 1);
}

/* the token: a small board, its corner's region flooded */
static void token(int player, int size, uint8_t *rgba)
{
    (void)player;
    static const char cells[] = "2220"
                                "2213"
                                "0223"
                                "1302";
    fld_board b;
    memset(&b, 0, sizeof b);
    b.n = 4;
    b.k = 4;
    for (int i = 0; i < 16; i++) b.c[i] = (uint8_t)(cells[i] - '0');
    board_pic(&b, size, rgba);
}

static void layout(box b)
{
    last_box = b;
    int n = G.b.n ? G.b.n : 6;
    grid = ui_grid_fit(b, n, n, FRAME);
    int q = (int)grid.cell;
    fr = (int)fmaxf(2, FRAME * q);
    int key = q * 64 + n;
    if (made_key != key) {
        made_key = key;
        int w = n * q + 2 * fr;
        ui_free(&t_board);
        paint_plank(ui_buf(w, w), w, fr);
        t_board = ui_tex(w, w, ui_buf(w, w));
        for (int c = 0; c < FLD_COLOURS; c++) {
            ui_free(&t_cell[c]);
            paint_cell(ui_buf(q, q), q, c, 0.36f);
            t_cell[c] = ui_tex(q, q, ui_buf(q, q));
        }
    }
    bench_small(&t_small, &made_small, token);
    ui_tools_shown(G.b.k + 1);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(change_at, 0, sizeof change_at);
    memcpy(was, G.b.c, sizeof was);
    hint_on = 0;
    win.on = 0;
    move_end = 0;
}

static void begin(void)
{
    calm();
    if (last_box.w > 0) {
        made_key = -1;
        layout(last_box);
    }
}

static void start(int level)                    /* a designed level */
{
    if (!fld_level(&G, level)) fld_new(&G, FLD_SMALL, 1);   /* never: every level is tested */
    begin();
}

static void start_random(int variant)           /* a board dealt at random */
{
    fld_new(&G, variant, (uint32_t)ui_random(0x7fffffff) + 1u);
    begin();
}

static int status(void) { return fld_done(&G) ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int score(void) { return G.nmoves; }
static int par(void) { return G.par; }
static int level_stars(void) { return ui_par_stars(G.nmoves, G.par); }
static int busy(void) { return ui_ms() < move_end || bench_wave_busy(&win); }

/* the move made: the region's cells take the colour in a ripple from the corner (their distance in the region) */
static void ripple(const fld_board *before, int hint)
{
    int n = G.b.n, dist[FLD_CELLS], q[FLD_CELLS], h = 0, t = 0, far = 0;
    Uint32 now = ui_ms();
    for (int i = 0; i < n * n; i++) dist[i] = -1;
    dist[0] = 0;
    q[t++] = 0;
    while (h < t) {
        int i = q[h++], x = i % n, y = i / n;
        int nb[4] = {x > 0 ? i - 1 : -1, x < n - 1 ? i + 1 : -1, y > 0 ? i - n : -1, y < n - 1 ? i + n : -1};
        for (int d = 0; d < 4; d++)
            if (nb[d] >= 0 && G.in[nb[d]] && dist[nb[d]] < 0) dist[nb[d]] = dist[i] + 1, q[t++] = nb[d];
    }
    hint_on = hint;
    hint_t0 = now;
    for (int i = 0; i < n * n; i++) {
        was[i] = before->c[i];
        change_at[i] = G.in[i] ? now + (Uint32)(dist[i] * RIPPLE) : 0;
        hinted[i] = hint && G.in[i] && before->c[i] != G.b.c[0] && dist[i] > 0;
        if (dist[i] > far) far = dist[i];
    }
    move_end = now + (Uint32)(far * RIPPLE + TAKE_MS);
    if (fld_done(&G)) {
        bench_wave_start(&win, 0.5f, 0.5f, n, n);
        win.t0 = move_end;
    }
    ui_animate();
}

static void choose(int c, int hint)
{
    fld_board before = G.b;
    if (!fld_play(&G, c)) {
        ui_sound(SND_DENY);
        return;
    }
    if (hint) G.hints++;
    ui_sound(SND_PLACE);
    ripple(&before, hint);
}

static int undo(void)
{
    if (!fld_undo(&G)) return 0;
    calm();
    return 1;
}

static int script(const char *moves)
{
    int ok = fld_script(&G, moves);
    calm();
    if (fld_done(&G)) bench_wave_start(&win, 0.5f, 0.5f, G.b.n, G.b.n);
    return ok;
}

static void ask_hint(void)
{
    int c = busy() ? -1 : fld_hint(&G);
    if (c < 0) {
        ui_sound(SND_DENY);
        return;
    }
    choose(c, 1);
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static int shown(int i, Uint32 now) { return change_at[i] && now < change_at[i] ? was[i] : G.b.c[i]; }

static void draw(void)
{
    if (made_key < 0 || grid.cell < 1) layout(last_box);
    int n = G.b.n, k = G.b.k, q = (int)grid.cell, done = fld_done(&G), out = fld_out(&G);
    Uint32 now = ui_ms();
    float bx = grid.x - fr, by = grid.y - fr, w = (float)(n * q + 2 * fr);
    ui_shadow_flat(bx + w / 2, by + w / 2 + 0.02f * w, w * 1.02f, w * 1.02f, 0.4f);
    ui_put(t_board, bx, by, w, w);
    uint8_t col[FLD_CELLS];
    for (int i = 0; i < n * n; i++) col[i] = (uint8_t)shown(i, now);
    for (int i = 0; i < n * n; i++) {
        float x, y;
        ui_grid_cell(&grid, i % n, i / n, &x, &y);
        float s = 1;
        if (change_at[i] && now >= change_at[i] && now < change_at[i] + TAKE_MS && was[i] != G.b.c[i])
            s = 1 + 0.12f * sinf(PUZ_PI * (now - change_at[i]) / TAKE_MS);   /* a cell taking its colour swells */
        ui_put_c(t_cell[col[i]], x + q / 2.0f, y + q / 2.0f, q * s, q * s);
    }
    float t = fmaxf(1, 0.07f * q), tg = fmaxf(2, 0.09f * q);   /* the grooves between colours; the region's gold */
    for (int i = 0; i < n * n; i++) {
        int x = i % n, y = i / n;
        float px, py;
        ui_grid_cell(&grid, x, y, &px, &py);
        for (int way = 0; way < 2; way++) {
            if (way ? y == n - 1 : x == n - 1) continue;
            int j = way ? i + n : i + 1, edge = !done && G.in[i] != G.in[j];
            if (col[i] == col[j] && !edge) continue;
            float d = edge ? tg : t;
            const uint8_t *ink = edge ? GOLD : INK;
            Uint8 a = edge ? 235 : 170;
            if (way) ui_fill(px, py + q - d / 2, (float)q, d, ink[0], ink[1], ink[2], a);
            else ui_fill(px + q - d / 2, py, d, (float)q, ink[0], ink[1], ink[2], a);
        }
    }
    if (hint_on) {                              /* the cells the hint took, in gold light */
        float p = puz_pulse((float)(now - hint_t0), 0, HINT_MS);
        for (int i = 0; p > 0 && i < n * n; i++)
            if (hinted[i]) ui_mark_gold_a(grid.x + (i % n + 0.5f) * q, grid.y + (i / n + 0.5f) * q, 1.6f * q, p);
        if (p <= 0) hint_on = 0;
        else ui_animate();
    }
    if (out) ui_mark_glow(grid.x + q / 2.0f, grid.y + q / 2.0f, 2.4f * q);   /* no moves left */
    if (win.on && now >= win.t0) {
        for (int i = 0; i < n * n; i++)
            bench_wave_at(&win, i % n + 0.5f, i / n + 0.5f, grid.x + (i % n + 0.5f) * q, grid.y + (i / n + 0.5f) * q,
                          2.2f * q);
        bench_wave_tick(&win);
    } else if (win.on) ui_animate();
    if (now < move_end) ui_animate();
    for (int c = 0; c < k; c++) ui_tool_paint(c, paint_tool, c, 0.86f, fld_can(&G, c) ? 0 : UI_DIM);
    ui_tool(k, ICON_HINT, done || out ? UI_DIM : 0);
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    int t = ui_tool_hit(x, y);
    if (fld_done(&G)) return;
    if (t >= 0) {
        if (t == G.b.k) ask_hint();
        else choose(t, 0);
        return;
    }
    int cx, cy;
    if (!ui_grid_hit(&grid, x, y, &cx, &cy)) return;
    int c = G.b.c[cy * G.b.n + cx];
    if (fld_can(&G, c)) choose(c, 0);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x, (void)y; }
static void release(float x, float y) { (void)x, (void)y; }

static void key(SDL_Keycode k)
{
    if (k >= SDLK_1 && k < SDLK_1 + G.b.k && !fld_done(&G)) choose((int)(k - SDLK_1), 0);
}

/* the bar: the moves; the moves left at its right end */
static void bar(int player, box b)
{
    (void)player;
    box r = ui_bar_countdown(b, G.limit - G.nmoves);
    bench_bar_moves(r, t_small, G.nmoves);
}

/* ---- the pictures for the shell ---------------------------------------------------------------------------------- */
/* the menu's card: a board of the middle size, its corner flooded a few moves; a variant's: a board of its size */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int n, k;
    fld_variant(variant < 0 ? FLD_MEDIUM : variant, &n, &k);
    if (variant < 0) n = 8;
    fld_board b;
    fld_deal(&b, n, k, 2026u + (uint32_t)(variant + 1));
    uint8_t in[FLD_CELLS];
    fld_region(&b, in);
    if (variant < 0) {
        static const int way[] = {1, 2, 0, 3, 1};
        for (int i = 0; i < 5; i++) fld_fill(&b, in, way[i]);
    }
    board_pic(&b, size, rgba);
}

static void level_thumb(int level, int size, uint8_t *rgba)
{
    fld_board b;
    uint8_t way[FLD_MOVES];
    int len;
    if (level >= 0 && level < FLD_LEVELS && fld_decode(fld_levels[level], &b, way, &len)) board_pic(&b, size, rgba);
}

/* a chapter's tab: its last level's board */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int ch = chapter < 0 ? 0 : chapter >= FLD_CHAPTERS ? FLD_CHAPTERS - 1 : chapter;
    level_thumb((ch + 1) * FLD_LEVELS / FLD_CHAPTERS - 1, size, rgba);
}

static const game flood_game = {
    .id = "flood", .order = 460, .variants = FLD_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .players = 1u << 1, .key = key,
    .levels = FLD_LEVELS, .chapters = FLD_CHAPTERS, .start_random = start_random, .chapter_icon = chapter_icon,
    .level_thumb = level_thumb, .level_stars = level_stars, .score = score, .par = par,
    .fill = 1, .tools = FLD_COLOURS + 1,
};
REGISTER_GAME(flood_game)
