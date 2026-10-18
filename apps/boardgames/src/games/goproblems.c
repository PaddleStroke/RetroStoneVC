/*
 * Go Problems in RetroStone Board, for one player: life and death in a corner of the go board, Black to play (the
 * rules and the solver: goproblems_rules.c, with go's rules; the levels: goproblems_levels.h, six chapters of 25 by
 * the depth of their main line). No random tab: a problem with one right first move takes the solver too long to
 * find on a phone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board shows the problem's corner, its two edges at the top and the left (turned on the diagonal when that makes
 * the points bigger), the lines running on past the open sides; the points outside the problem are shaded. The key
 * group glows: red, the white stones to kill; gold, the black stones to save. A touch shows the stone where it would
 * go, lifting the finger plays it; White answers by itself (the solver's longest resistance). A move that loses the
 * problem is a mistake: White's refutation comes, both glow red a moment, then they are taken back. The lamp (the
 * shell's tool) lights a right move. The pass button passes (a life problem may need it). Undo takes back the last
 * move and its answer, all the way to the start. The bar: what to do (the key stone in its glow) and the moves
 * played. Solved: a gold light spreads from the last stone, then the stars (ui_logic_stars: hints and mistakes).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/goproblems.h"
#include "games/grid.h"
#include "games/puzzles.h"
#include "shell.h"

#define FRAME 0.42f                             /* the board's frame, in points */
#define REPLY_MS 380                            /* White's answer comes */
#define FADE_MS 260                             /* stones taken fade out */
#define WRONG_MS 900                            /* a mistake and its refutation held, before they go */
#define PASS_MS 900                             /* White's pass shown */
#define WAVE_STEP 60
#define WAVE_LEN 600
#define WAVE_END (10 * WAVE_STEP + WAVE_LEN)

static gpz_game G;
static ui_grid Gr;
static box room;
static int made_cell = -1, made_w, made_h, made_turned, made_u = -1, made_level = -2;
static SDL_Texture *t_board, *t_stone[2], *t_ring, *t_goal[2], *t_mark[2];   /* t_mark: the key stones' (kill, live) */
static int bw, bh;                              /* the board picture's size */
static Uint32 shown_at[GO_HIST + 1];            /* when each position came on the board */
static struct { int on, pt; } ghost;
static struct { int on; Uint32 t0; } wrong, win, passed;

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    memset(shown_at, 0, sizeof shown_at);
    wrong.on = win.on = passed.on = 0;
    ghost.on = 0;
}

static void start(int level)
{
    if (!gpz_level(&G, level)) gpz_level(&G, 0);   /* never: every level is tested */
    calm();
}

static int status(void) { return G.solved ? GS_WIN0 : GS_PLAY; }
static int turn(void) { return 0; }
static int level_stars(void) { return ui_logic_stars(G.hints, G.mistakes); }
static int score(void) { return (G.g.nmoves + 1) / 2; }

/* the position on the board now: the last one that came */
static int shown(Uint32 now)
{
    int k = G.g.nmoves;
    while (k > 0 && now < shown_at[k]) k--;
    return k;
}

static Uint32 wrong_end(void) { return REPLY_MS + WRONG_MS + FADE_MS; }

static int busy(void)
{
    Uint32 now = ui_ms();
    if (shown(now) < G.g.nmoves) return 1;
    if (G.g.nmoves && now < shown_at[G.g.nmoves] + FADE_MS) return 1;
    if (wrong.on) return 1;
    return win.on && now - win.t0 < WAVE_END;
}

static int undo(void)
{
    if (busy() || !gpz_undo(&G)) return 0;
    calm();
    return 1;
}

static void sound_of(int k)                     /* the sound of the move that made position k */
{
    const gopos *a = &G.g.pos[k - 1], *b = &G.g.pos[k];
    int took = a->caps[0] + a->caps[1] != b->caps[0] + b->caps[1];
    if (G.g.move[k - 1] == GO_PASS) return;
    Uint32 now = ui_ms();
    int delay = shown_at[k] > now ? (int)(shown_at[k] - now) : 0;
    ui_sound_later(took ? SND_CAPTURE : SND_PLACE, delay);
}

/* the player's move (a cell or a pass) */
static void attempt(int cell)
{
    int before = G.g.nmoves, r = gpz_try(&G, cell);
    Uint32 now = ui_ms();
    if (r == GPZ_ILLEGAL) { ui_sound(SND_DENY); return; }
    shown_at[before + 1] = now;
    sound_of(before + 1);
    if (G.g.nmoves > before + 1) {             /* White's answer */
        shown_at[before + 2] = now + REPLY_MS;
        if (G.g.move[before + 1] == GO_PASS) { passed.on = 1; passed.t0 = now + REPLY_MS; }
        else sound_of(before + 2);
    }
    if (r == GPZ_WRONG) {
        wrong.on = 1;
        wrong.t0 = now;
        ui_sound_later(SND_DENY, G.g.nmoves > before + 1 ? REPLY_MS + 120 : 120);
    }
    if (r == GPZ_SOLVED) {
        win.on = 1;
        win.t0 = now + (G.g.nmoves > before + 1 ? REPLY_MS : 0) + FADE_MS;
    }
    ui_animate();
}

static void pass(void) { if (!busy()) attempt(GPZ_PASS); }
static int can_pass(void) { return gpz_to_move(&G) && !busy(); }

static void hint(void)
{
    if (busy() || gpz_hint(&G) < 0) { ui_sound(SND_DENY); return; }
    ui_sound(SND_TAP);
    ui_animate();
}

/* "c2 b1 pass" (the replies by themselves, a mistake taken back); "#n" first: level n + 1 */
static int script(const char *moves)
{
    int ok = gpz_script(&G, moves);
    calm();
    if (G.solved) { win.on = 1; win.t0 = ui_ms(); }
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* a cell's place on the screen: its column and row (the corner at the top left; turned: on the diagonal) */
static void cell_xy(int cell, float *cx, float *cy)
{
    float x, y;
    ui_grid_cell(&Gr, GPZ_X(cell), GPZ_Y(cell), &x, &y);
    *cx = x + Gr.cell / 2;
    *cy = y + Gr.cell / 2;
}

static int cell_at(float x, float y)
{
    int c, r;
    return ui_grid_hit(&Gr, x, y, &c, &r) ? GPZ_CELL(c, r) : -1;
}

/* the corner's board: cols x rows points of s pixels in a frame of f, the edges at the top and the left, the lines
 * running to the frame on the other sides, the star point; the points outside the problem shaded (out: by screen
 * column and row) */
static void paint_window(uint8_t *o, int cols, int rows, int s, int f, const uint8_t *out, int hoshi_c, int hoshi_r)
{
    static const uint8_t INK[3] = {34, 24, 16}, SHADE[3] = {28, 18, 10};
    int w = cols * s + 2 * f, h = rows * s + 2 * f, side = w > h ? w : h;
    art_board_n(cols, rows, s, f, 0, o);
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (out[r * cols + c])
                art_tint_rect(o, w, h, (float)(f + c * s), (float)(f + r * s), (float)(f + (c + 1) * s),
                              (float)(f + (r + 1) * s), SHADE, 0.34f);
    float *m = calloc((size_t)side * side, sizeof *m);
    if (!m) return;
    float lw = 0.9f + 0.02f * s, edge = 1.8f * lw, c0 = f + s / 2.0f;
    for (int c = 0; c < cols; c++) {
        float x = c0 + c * s, t = c == 0 ? edge : lw;
        grid_cover_rect(m, side, x - t / 2, c0 - edge / 2, x + t / 2, (float)(f + rows * s));
    }
    for (int r = 0; r < rows; r++) {
        float y = c0 + r * s, t = r == 0 ? edge : lw;
        grid_cover_rect(m, side, c0 - edge / 2, y - t / 2, (float)(f + cols * s), y + t / 2);
    }
    if (hoshi_c < cols && hoshi_r < rows)
        grid_cover_disc(m, side, c0 + hoshi_c * s, c0 + hoshi_r * s, fmaxf(1.6f * lw, 0.11f * s));
    grid_ink(o, w, h, m, side, INK, 0.9f);
    free(m);
}

/* the key group's mark, d x d: an inlaid dot of red lacquer (kind 0: to kill) or gold (1: to save), domed, rimmed */
static void paint_mark(int kind, int d, uint8_t *o)
{
    static const float TONE[2][3] = {{200, 50, 38}, {250, 198, 78}}, RIM[2] = {0.55f, 0.3f};
    const float *t = TONE[kind != 0];
    float R = 0.47f * d, m = d / 2.0f;
    for (int py = 0; py < d; py++)
        for (int px = 0; px < d; px++) {
            float col[3] = {0, 0, 0}, cov = 0;
            for (int s = 0; s < 9; s++) {           /* 3 x 3 samples a pixel */
                float x = (px + (s % 3 + 0.5f) / 3 - m) / R, y = (py + (s / 3 + 0.5f) / 3 - m) / R, r2 = x * x + y * y;
                if (r2 >= 1) continue;
                float z = sqrtf(1 - r2), lit = (-0.45f * x - 0.55f * y + 0.7f * z) / 1.0f;   /* up to the left */
                float k = 0.62f + 0.5f * fmaxf(0, lit), rim = clampf((sqrtf(r2) - 1) * R / 1.3f + 1, 0, 1);
                float hl = 0.55f * powf(fmaxf(0, lit), 24);
                for (int j = 0; j < 3; j++) {
                    float v = clampf(t[j] * k, 0, 255);
                    v += (255 - v) * hl;
                    col[j] += v * (1 - RIM[kind != 0] * rim);
                }
                cov += 1;
            }
            uint8_t *p = o + 4 * ((size_t)py * d + px);
            for (int j = 0; j < 3; j++) p[j] = (uint8_t)(cov > 0 ? col[j] / cov : 0);
            p[3] = (uint8_t)(255 * cov / 9 + 0.5f);
        }
}

/* the goal's picture: the key stone (White to kill, Black to save) in its glow */
static void paint_goal(int kind, int size, uint8_t *o)
{
    static const uint8_t RED[3] = {235, 52, 36}, GOLD[3] = {255, 200, 70};
    memset(o, 0, (size_t)size * size * 4);
    art_disc(size, kind == GPZ_KILL ? RED : GOLD, 0.85f, 1.2f, o);
    int d = (int)(0.62f * size);
    uint8_t *st = malloc((size_t)d * d * 4);
    if (!st) return;
    grid_token(kind == GPZ_KILL ? 1 : 0, d, st);
    art_blend(o, size, size, st, d, d, (size - d) / 2, (size - d) / 2);
    int md = d / 3;                             /* its dot, as on the board */
    paint_mark(kind == GPZ_KILL ? 0 : 1, md, st);
    art_blend(o, size, size, st, md, md, (size - md) / 2, (size - md) / 2);
    free(st);
}

static void layout(box b)
{
    room = b;
    Gr = ui_grid_fit(b, G.pz.w, G.pz.h, FRAME);
    int s = (int)Gr.cell, f = (int)(FRAME * s);
    bw = Gr.cols * s + 2 * f;
    bh = Gr.rows * s + 2 * f;
    if (made_cell != s || made_w != G.pz.w || made_h != G.pz.h || made_turned != Gr.turned || made_level != G.level ||
        !t_board) {
        made_cell = s, made_w = G.pz.w, made_h = G.pz.h, made_turned = Gr.turned, made_level = G.level;
        uint8_t out[GPZ_MAXW * GPZ_MAXW] = {0};
        for (int y = 0; y < G.pz.h; y++)
            for (int x = 0; x < G.pz.w; x++) {
                int c = Gr.turned ? y : x, r = Gr.turned ? x : y;
                out[r * Gr.cols + c] = G.pz.start.c[GPZ_CELL(x, y)] == GPZ_FREE;
            }
        ui_free(&t_board);
        paint_window(ui_buf(bw, bh), Gr.cols, Gr.rows, s, f, out, 3, 3);
        t_board = ui_tex(bw, bh, ui_buf(bw, bh));
        for (int p = 0; p < 2; p++) {
            ui_free(&t_stone[p]);
            t_stone[p] = grid_stone_tex(p, s);
        }
        int rs = (int)fmaxf(8, 0.46f * s);
        static const uint8_t WHITE[3] = {255, 255, 255};
        ui_free(&t_ring);
        int md = (int)fmaxf(6, 0.24f * s);
        for (int k = 0; k < 2; k++) {
            ui_free(&t_mark[k]);
            paint_mark(k, md, ui_buf(md, md));
            t_mark[k] = ui_tex(md, md, ui_buf(md, md));
        }
        art_ring(rs, 0.70f, WHITE, 1, ui_buf(rs, rs));
        t_ring = ui_tex(rs, rs, ui_buf(rs, rs));
    }
    int u = (int)(0.7f * ui.unit);
    if (made_u != u) {
        made_u = u;
        for (int k = 0; k < 2; k++) {
            ui_free(&t_goal[k]);
            paint_goal(k, u, ui_buf(u, u));
            t_goal[k] = ui_tex(u, u, ui_buf(u, u));
        }
    }
}

static void stone(int player, float cx, float cy, float a)
{
    SDL_Texture *t = t_stone[player];
    ui_shadow_flat(cx, cy, Gr.cell, Gr.cell, a);
    SDL_SetTextureAlphaMod(t, (Uint8)(255 * a));
    ui_put_c(t, cx, cy, Gr.cell, Gr.cell);
    SDL_SetTextureAlphaMod(t, 255);
}

static int colour_of(int v) { return v == GO_EBONY ? 0 : 1; }

static void draw(void)
{
    if (!t_board || made_level != G.level) layout(room);
    Uint32 now = ui_ms();
    if (wrong.on && now - wrong.t0 >= wrong_end()) {   /* the mistake and its refutation go */
        wrong.on = 0;
        gpz_undo(&G);
        memset(shown_at, 0, sizeof shown_at);
    }
    int k = shown(now);
    const gopos *p = &G.g.pos[k];
    float x0 = Gr.x - (bw - Gr.cols * Gr.cell) / 2, y0 = Gr.y - (bh - Gr.rows * Gr.cell) / 2, cx, cy;
    ui_put(t_board, x0, y0, (float)bw, (float)bh);
    /* the key group's glow */
    uint8_t key[GPZ_CELLS];
    if (gpz_key_group(&G, key) && !win.on)
        for (int c = 0; c < GPZ_CELLS; c++) {
            if (!key[c] || !p->pt[gpz_pt(c)]) continue;
            cell_xy(c, &cx, &cy);
            if (G.pz.kind == GPZ_KILL) ui_mark_glow(cx, cy, 1.6f * Gr.cell);
            else ui_mark_gold_a(cx, cy, 1.6f * Gr.cell, 0.8f);
        }
    /* the mistake: the two stones in red */
    float fade = 1;
    if (wrong.on) {
        float e = (float)(now - wrong.t0);
        if (e > REPLY_MS + WRONG_MS) fade = 1 - (e - REPLY_MS - WRONG_MS) / FADE_MS;
        for (int i = G.g.nmoves - 1; i >= 0 && i >= G.g.nmoves - 2; i--) {
            int m = G.g.move[i];
            if (m == GO_PASS || i + 1 > k) continue;
            cell_xy(gpz_cell(m), &cx, &cy);
            ui_mark_glow(cx, cy, 1.5f * Gr.cell * fmaxf(fade, 0));
        }
        ui_animate();
    }
    /* the hint */
    if (G.hint_ply == G.g.nmoves && gpz_to_move(&G) && k == G.g.nmoves && G.hint != GPZ_PASS) {
        cell_xy(G.hint, &cx, &cy);
        ui_mark_gold(cx, cy, 1.5f * Gr.cell);
        ui_mark_gold(cx, cy, 1.1f * Gr.cell);
    }
    /* the stones; those just taken fade out */
    int base = wrong.on ? G.g.nmoves - 2 : G.g.nmoves;   /* a mistake's stones fade with it */
    for (int y = 0; y < G.pz.h; y++)
        for (int x = 0; x < G.pz.w; x++) {
            int c = GPZ_CELL(x, y), pt = x + 19 * y, v = p->pt[pt];
            cell_xy(c, &cx, &cy);
            if (v) {
                float a = 1;
                for (int j = base; wrong.on && j < k; j++)
                    if (j >= 0 && G.g.move[j] == pt) a = fmaxf(fade, 0);
                if (k && pt == G.g.move[k - 1] && now < shown_at[k] + 120)   /* set down */
                    a *= (now - shown_at[k]) / 120.0f;
                stone(colour_of(v), cx, cy, a);
            } else if (k && G.g.pos[k - 1].pt[pt] && now < shown_at[k] + FADE_MS) {
                float t = (now - shown_at[k]) / (float)FADE_MS;
                stone(colour_of(G.g.pos[k - 1].pt[pt]), cx, cy, (1 - t) * (1 - t));
                ui_animate();
            }
        }
    if (k < G.g.nmoves || (k && now < shown_at[k] + FADE_MS)) ui_animate();
    if (gpz_key_group(&G, key) && !win.on)     /* the key stones: a dot on each */
        for (int c = 0; c < GPZ_CELLS; c++) {
            if (!key[c] || !p->pt[gpz_pt(c)]) continue;
            float md = fmaxf(6, 0.24f * Gr.cell);
            cell_xy(c, &cx, &cy);
            ui_put_c(t_mark[G.pz.kind == GPZ_KILL ? 0 : 1], cx, cy, md, md);
        }
    /* the last stone's ring */
    if (k && G.g.move[k - 1] != GO_PASS && p->pt[G.g.move[k - 1]]) {
        int pt = G.g.move[k - 1], who = colour_of(p->pt[pt]);
        float rs = fmaxf(8, 0.46f * Gr.cell);
        cell_xy(gpz_cell(pt), &cx, &cy);
        if (who == 0) SDL_SetTextureColorMod(t_ring, 236, 228, 212);
        else SDL_SetTextureColorMod(t_ring, 58, 50, 44);
        ui_put_c(t_ring, cx, cy, rs, rs);
    }
    if (ghost.on && gpz_to_move(&G) && !busy()) {
        cell_xy(ghost.pt, &cx, &cy);
        SDL_SetTextureAlphaMod(t_stone[0], GRID_GHOST_A);
        ui_put_c(t_stone[0], cx, cy, Gr.cell, Gr.cell);
        SDL_SetTextureAlphaMod(t_stone[0], 255);
    }
    if (passed.on) {                            /* White passed */
        float e = (float)now - (float)passed.t0;
        if (e >= PASS_MS) passed.on = 0;
        else {
            if (e >= 0) {
                float a = fminf(1, fminf(e / 150, (PASS_MS - e) / 200));
                float bx = Gr.x + Gr.cols * Gr.cell / 2, by = Gr.y + Gr.rows * Gr.cell / 2;
                ui_button(bx, by, 1.6f * ui.unit * (0.8f + 0.2f * a),
                          ICON_PASS, a > 0.5f ? 0 : UI_DIM);
            }
            ui_animate();
        }
    }
    if (win.on) {                               /* the gold light, from the last stone */
        float e = (float)now - (float)win.t0;
        int last = G.g.nmoves - 1;
        while (last >= 0 && G.g.move[last] == GO_PASS) last--;
        int from = last >= 0 ? gpz_cell(G.g.move[last]) : G.pz.first;
        for (int y = 0; y < G.pz.h && e >= 0; y++)
            for (int x = 0; x < G.pz.w; x++) {
                if (G.pz.start.c[GPZ_CELL(x, y)] == GPZ_FREE) continue;
                int d = abs(x - GPZ_X(from)) + abs(y - GPZ_Y(from));
                float q = puz_pulse(e, d * WAVE_STEP, WAVE_LEN);
                if (q <= 0) continue;
                cell_xy(GPZ_CELL(x, y), &cx, &cy);
                ui_mark_gold_a(cx, cy, 1.3f * Gr.cell, 0.8f * q);
            }
        if (e < WAVE_END) ui_animate();
        else win.on = 0;
    }
    ui_tool(0, ICON_HINT, (G.hint_ply == G.g.nmoves ? UI_ON : 0) | (!gpz_to_move(&G) ? UI_DIM : 0));
}

/* ---- the pointer: a touch shows the stone, lifting the finger plays it ------------------------------------------ */
static int playable(int cell)
{
    return cell > 0 && gpz_cell_at(&G, cell) == GPZ_EMPTY;
}

static void press(float x, float y)
{
    ghost.on = 0;
    if (ui_tool_hit(x, y) == 0) { hint(); return; }
    if (busy() || !gpz_to_move(&G)) return;
    int c = cell_at(x, y);
    if (c < 0) return;
    if (!playable(c)) { ui_sound(SND_DENY); return; }
    ghost.on = 1;
    ghost.pt = c;
    ui_animate();
}

static void motion(float x, float y)
{
    if (!ghost.on) return;
    int c = cell_at(x, y);
    if (c >= 0 && c != ghost.pt && playable(c)) { ghost.pt = c; ui_animate(); }
}

static void release(float x, float y)
{
    (void)x;
    (void)y;
    if (!ghost.on) return;
    ghost.on = 0;
    attempt(ghost.pt);
}

/* ---- the bar: what to do, the moves played ---------------------------------------------------------------------- */
static void bar(int player, box b)
{
    (void)player;
    ui_count c[1] = {{t_goal[G.pz.kind == GPZ_KILL ? 0 : 1], (G.g.nmoves + 1) / 2}};
    ui_bar_fit(b, c, 1, 0);
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the menu's card: a corner, a white group in its red glow, the vital point in gold */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    static const uint8_t RED[3] = {235, 52, 36}, GOLD[3] = {255, 196, 64};
    grid_pic pic = grid_thumb(5, 1, size, rgba);
    static const int white[][2] = {{0, 2}, {1, 2}, {2, 2}, {3, 1}, {3, 0}}, black[][2] = {{0, 3}, {1, 3}, {2, 3},
                                                                                        {3, 3}, {4, 2}, {4, 1}};
    int s = pic.s, g = (int)(1.5f * s);
    uint8_t *glow = malloc((size_t)g * g * 4);
    if (glow) {
        art_disc(g, RED, 0.7f, 1.3f, glow);
        for (int i = 0; i < 5; i++)
            art_blend(rgba, size, size, glow, g, g, pic.x0 + white[i][0] * s + s / 2 - g / 2,
                      pic.y0 + white[i][1] * s + s / 2 - g / 2);
        art_disc(g, GOLD, 0.95f, 1.1f, glow);
        art_blend(rgba, size, size, glow, g, g, pic.x0 + 1 * s + s / 2 - g / 2, pic.y0 + 1 * s + s / 2 - g / 2);
        free(glow);
    }
    for (int i = 0; i < 5; i++) grid_thumb_stone(pic, white[i][0], white[i][1], 1, size, rgba);
    for (int i = 0; i < 6; i++) grid_thumb_stone(pic, black[i][0], black[i][1], 0, size, rgba);
    int md = (int)fmaxf(3, 0.26f * s);            /* the white group's dots */
    uint8_t *dot = malloc((size_t)md * md * 4);
    if (!dot) return;
    paint_mark(0, md, dot);
    for (int i = 0; i < 5; i++)
        art_blend(rgba, size, size, dot, md, md, pic.x0 + white[i][0] * s + (s - md) / 2,
                  pic.y0 + white[i][1] * s + (s - md) / 2);
    free(dot);
}

/* a chapter's tab: as many black stones as Black's moves in its main line (one to six) */
static void chapter_icon(int chapter, int size, uint8_t *rgba)
{
    int n = chapter + 1, d = (int)(size * (n == 1 ? 0.6f : n == 2 ? 0.46f : 0.32f)), cols = n <= 3 ? n : 3;
    int rows = (n + 2) / 3;
    uint8_t *st = malloc((size_t)d * d * 4);
    if (!st) return;
    grid_token(0, d, st);
    for (int i = 0; i < n; i++) {
        int r = i / 3, in_row = r == rows - 1 ? n - 3 * r : cols, c = i % 3;
        float cx = size / 2.0f + (c - (in_row - 1) / 2.0f) * d * 1.02f;
        float cy = size / 2.0f + (r - (rows - 1) / 2.0f) * d * 1.02f;
        art_flat_put(rgba, size, size, st, d, cx, cy);
    }
    free(st);
}

static const game goproblems_game = {
    .id = "goproblems", .order = 431, .variants = 1,
    .thumb = thumb, .token = grid_token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy, .players = 1u << 1, .fill = 1,
    .tools = 1, .levels = GPZ_LEVELS, .chapters = GPZ_CHAPTERS, .chapter_icon = chapter_icon,
    .level_stars = level_stars, .score = score,
};
REGISTER_GAME(goproblems_game)
