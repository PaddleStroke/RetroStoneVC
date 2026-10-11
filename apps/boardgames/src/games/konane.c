/*
 * Konane in RetroStone Board: the checkered board (6 x 6, 8 x 8, 10 x 10) full of stones, the two opening removals,
 * then jumps, tap or drag (the rules and the notation: konane_rules.c, konane.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The opening: the stones that may be lifted have a ring; a tap lifts one. Then a stone selected shows every square it
 * can jump to (a dot each: one jump, two, more, straight on); a tap there, or a drag, plays.
 */
#include <math.h>

#include "games/konane.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in squares */
#define STONE 0.9f                              /* a stone's size, in squares */
#define JUMP_MS 210                             /* each jump of a move */
#define LIFT_MS 260                             /* a stone lifted off (an opening removal) */

static kngame G;
static knmove legal[KN_MAX_MOVES];
static int nlegal, n = 8, sq, fr, made = -1, made_n = -1, made_small = -1;
static float bx, by;                            /* the top left square's corner */
static SDL_Texture *t_stone[2], *t_small[2], *t_board;
static int sel = -1;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, who, k; knmove m; int8_t over[KN_MAX_N]; Uint32 t0; } anim;

/* player 0 plays ebony (black moves first, by tradition): the house's second colour */
static int colour(int player) { return !player; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = kn_legal(kn_pos(&G), legal);
    sel = -1;
    drag.on = 0;
}

static void start(int variant)
{
    n = kn_size(variant);
    kn_new(&G, n);
    anim.on = 0;
    refresh();
}

static int status(void)
{
    int w = kn_winner(&G);
    return w < 0 ? GS_PLAY : w ? GS_WIN1 : GS_WIN0;   /* no draws in konane */
}

static int turn(void) { return kn_pos(&G)->side; }
static int opening(void) { return kn_pos(&G)->opening < 2; }

static int find(int from, int to)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from && legal[i].to == to) return i;
    return -1;
}

static int can_move(int from)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == from) return 1;
    return 0;
}

static void play(knmove m, int animate)
{
    anim.who = kn_pos(&G)->side;
    anim.k = kn_jumped(kn_pos(&G), m, anim.over);
    if (!kn_play(&G, m)) return;
    anim.on = animate;
    anim.m = m;
    anim.t0 = ui_ms();
    refresh();
    ui_sound(m.from == m.to ? SND_MOVE : SND_CAPTURE);
}

static int undo(void)
{
    if (!kn_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "e4 e5 c4-e4": the screenshots' and the tests' moves (konane.h) */
static int script(const char *s)
{
    int ok = kn_script(&G, s);
    anim.on = 0;
    refresh();
    return ok;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
static void layout(box b)
{
    sq = (int)floorf(b.w / (n + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = (float)n * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != sq || made_n != n) {
        made = sq;
        made_n = n;
        int d = (int)(STONE * sq), w = n * sq + 2 * fr;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_stone[c]);
            art_counter(COUNTER_STONE, c, d, ui_buf(d, d));
            t_stone[c] = ui_tex(d, d, ui_buf(d, d));
        }
        ui_free(&t_board);
        art_board_n(n, n, sq, fr, 1, ui_buf(w, w));
        t_board = ui_tex(w, w, ui_buf(w, w));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            art_counter(COUNTER_STONE, c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

static void sq_xy(int s, float *x, float *y)    /* a square's centre */
{
    int f = s % n, r = s / n;
    if (ui.flipped) { f = n - 1 - f; r = n - 1 - r; }
    *x = bx + (f + 0.5f) * sq;
    *y = by + (n - 1 - r + 0.5f) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = n - 1 - (int)floorf((y - by) / sq);
    if (f < 0 || f >= n || r < 0 || r >= n) return -1;
    return ui.flipped ? (n - 1 - r) * n + n - 1 - f : r * n + f;
}

static void mark(int s, void (*m)(float, float, float, float))
{
    float x, y;
    sq_xy(s, &x, &y);
    m(x - sq / 2.0f, y - sq / 2.0f, (float)sq, (float)sq);
}

static void stone_at(int player, float cx, float cy, float d, float alpha)
{
    ui_flat_at(t_stone[colour(player)], cx, cy, d, alpha, 0);
}

static void draw(void)
{
    const knpos *p = kn_pos(&G);
    float w = (float)n * sq + 2 * fr, d = STONE * sq, x, y;
    ui_put(t_board, bx - fr, by - fr, w, w);
    if (G.ply) {                                /* the last move: where it started, where it landed */
        mark(G.move[G.ply - 1].from, ui_mark_last);
        mark(G.move[G.ply - 1].to, ui_mark_last);
    }
    if (sel >= 0) mark(sel, ui_mark_select);
    float t = 1, total = anim.m.from == anim.m.to ? LIFT_MS : (float)JUMP_MS * (anim.k > 0 ? anim.k : 1);
    if (anim.on) {
        t = (ui_ms() - anim.t0) / total;
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    int st = status(), hop = anim.on && anim.m.from != anim.m.to;
    if (st != GS_PLAY && G.ply > 2 && !anim.on) {   /* the winner's last jump: its stone in a gold light */
        sq_xy(G.move[G.ply - 1].to, &x, &y);
        ui_mark_gold(x, y, 1.5f * sq);
    }
    for (int s = 0; s < n * n; s++) {
        if (p->at[s] < 0 || (drag.on && drag.moved && s == drag.from) || (hop && s == anim.m.to)) continue;
        sq_xy(s, &x, &y);
        stone_at(p->at[s], x, y, d, 1);
    }
    if (st == GS_PLAY)
        for (int i = 0; i < nlegal; i++) {      /* the stones that may be lifted; where the selected one can go */
            if (legal[i].from == legal[i].to) {
                sq_xy(legal[i].from, &x, &y);
                ui_mark_ring(x, y, 1.0f * sq);
            } else if (legal[i].from == sel) {
                sq_xy(legal[i].to, &x, &y);
                ui_mark_dot(x, y, 0.32f * sq);
            }
        }
    if (anim.on && !hop) {                      /* an opening removal: the stone lifted, fading away */
        sq_xy(anim.m.from, &x, &y);
        stone_at(anim.who, x, y, d * (1 + 0.2f * t), 1 - t);
    }
    if (hop) {                                  /* jump by jump, up and over: each stone jumped fades as it is passed */
        float seg = t * anim.k, sx, sy, ex, ey;
        int j = seg >= anim.k ? anim.k - 1 : (int)seg;
        float f = seg - j, e = f * f * (3 - 2 * f);
        for (int i = j; i < anim.k; i++) {      /* the stones still on the way; the one under the jumper fading */
            sq_xy(anim.over[i], &x, &y);
            stone_at(!anim.who, x, y, d, i > j || f < 0.5f ? 1 : 2 * (1 - f));
        }
        sq_xy(anim.m.from, &sx, &sy);
        sq_xy(anim.m.to, &ex, &ey);
        float ux = (ex - sx) / anim.k, uy = (ey - sy) / anim.k;   /* one jump: two squares */
        stone_at(anim.who, sx + ux * (j + e), sy + uy * (j + e), d * (1 + 0.16f * sinf(3.14159265f * f)), 1);
    }
    if (drag.on && drag.moved) {                /* the dragged stone, a little bigger, beside the finger */
        float s = 1.18f * d, off = ui_upside(p->side) ? -0.55f : 0.55f;
        stone_at(p->side, drag.x, drag.y - s * off, s, 1);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (status() != GS_PLAY) return;
    int s = xy_sq(x, y), i;
    if (s < 0) { sel = -1; return; }
    if (opening()) {                            /* a tap on a ringed stone lifts it */
        if ((i = find(s, s)) >= 0) play(legal[i], 1);
        else ui_sound(SND_DENY);
        return;
    }
    if (sel >= 0 && (i = find(sel, s)) >= 0) {
        play(legal[i], 1);
        return;
    }
    const knpos *p = kn_pos(&G);
    if (p->at[s] == p->side && can_move(s)) {
        sel = s;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    ui_sound(SND_DENY);
    sel = -1;
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > sq * 0.12f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) return;                    /* a tap: the stone stays selected */
    int s = xy_sq(x, y), i = s >= 0 ? find(drag.from, s) : -1;
    if (i >= 0) play(legal[i], 0);
    else if (s >= 0 && s != drag.from) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the stones the player took */
{
    ui_bar_count(b, t_small[colour(!player)], kn_pos(&G)->took[player]);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_STONE, colour(player), size, rgba); }

/* the menu's card (-1): a corner of a board full of stones but for the opening's two holes; the picker's: the whole
 * board full at the start (6 x 6, 8 x 8, 10 x 10) */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int k = variant < 0 ? 4 : kn_size(variant);
    int s = (int)(size / (k + 2 * FRAME)), f = (int)(s * FRAME), w = k * s + 2 * f, o = (size - w) / 2;
    int d = (int)(STONE * s), in = (s - d) / 2;
    uint8_t *board = ui_buf(w, w);
    art_board_n(k, k, s, f, 1, board);
    art_blend(rgba, size, size, board, w, w, o, o);
    o += f;
    for (int c = 0; c < 2; c++) {
        uint8_t *one = ui_buf(d, d);
        for (int layer = 0; layer < 2; layer++) {   /* the shadows, then the stones */
            if (layer) art_counter(COUNTER_STONE, colour(c), d, one);
            else art_flat_shadow(d, one);
            for (int r = 0; r < k; r++)         /* r: rows from the top */
                for (int x = 0; x < k; x++) {
                    int y = k - 1 - r;
                    if (kn_owner(k, x + k * y) != c) continue;
                    if (variant < 0 && ((x == 1 && r == 1) || (x == 2 && r == 1))) continue;   /* two holes */
                    int px = o + x * s + in, py = o + r * s + in;
                    if (layer) art_blend(rgba, size, size, one, d, d, px, py);
                    else art_blend(rgba, size, size, one, d, d, px + (int)(0.06f * d), py + (int)(0.1f * d));
                }
        }
    }
}

static int busy(void) { return anim.on; }     /* the end waits for the last jump to land */

static const game konane_game = {
    .id = "konane", .order = 205, .variants = KN_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(konane_game)
