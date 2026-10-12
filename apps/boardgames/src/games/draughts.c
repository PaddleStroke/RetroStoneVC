/*
 * Draughts in RetroStone Board: international draughts (10 x 10), English checkers, Russian, Brazilian and Turkish
 * draughts (8 x 8), Frisian (10 x 10), Canadian (12 x 12), bashni (8 x 8, columns drawn as lasca's: column.h). The
 * board, the men and kings, tap or drag, a capture tapped landing square by landing square (the rules:
 * draughts_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The script's notation (the screenshots, the tests): the standard square numbers (draughts.h), "32-28" a move,
 * "19x30" a capture from 19 ending on 30, "26x37x48" a capture through its landing squares (needed only when two
 * routes end on the same square taking different pieces); Russian, Brazilian, Turkish draughts and bashni: a1-h8,
 * "c3-d4", "c3:e5:g3" (or "c3xe5xg3").
 */
#include <math.h>
#include <string.h>

#include "games/column.h"
#include "games/draughts.h"
#include "shell.h"

#define PIECE 0.94f                             /* bashni: a column's pieces, in squares (as lasca's) */

#define FRAME 0.32f                             /* the board's frame, in squares */
#define STEP_MS 150                             /* the slide: a plain move, or each jump of a capture */
#define STEP (-2)

static dgame G;
static dmove legal[DR_MAX_MOVES];
static int nlegal, state = GS_PLAY;
static int n = 10, sq, fr, made = -1, made_n = -1, made_plain = -1, made_small = -1;
static float bx, by;                            /* the top left square's corner */
static SDL_Texture *t_man[2][2], *t_small[2], *t_board;   /* [colour][king] */
static int sel = -1, steps;                     /* the selected piece; the landing squares tapped so far */
static uint8_t path[DR_MAX_STEPS];
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, first; dmove m; Uint32 t0; } anim;

/* a player's colour: English, dark moves first: player 0 ebony; the others, player 0 ivory (white) */
static int colour(int player) { return G.variant == DR_ENGLISH ? !player : player; }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void refresh(void)
{
    nlegal = dr_legal(dr_pos(&G), G.variant, legal);
    switch (dr_status(&G)) {
    case DR_PLAY: state = GS_PLAY; break;
    case DR_WON0: state = GS_WIN0; break;
    case DR_WON1: state = GS_WIN1; break;
    default: state = GS_DRAW;
    }
    sel = -1;
    steps = 0;
    drag.on = 0;
}

static void start(int variant)
{
    dr_new(&G, variant >= 0 && variant < DR_VARIANTS ? variant : DR_INTERNATIONAL);
    n = dr_size(G.variant);
    anim.on = 0;
    refresh();
}

static int status(void) { return state; }
static int turn(void) { return dr_pos(&G)->side; }

static int movable(int s)
{
    for (int i = 0; i < nlegal; i++)
        if (legal[i].from == s) return 1;
    return 0;
}

/* a move still possible for the selected piece, after the landing squares tapped so far */
static int fits(const dmove *m)
{
    return m->from == sel && m->n > steps && !memcmp(m->path, path, (size_t)steps);
}

/* what a tap on square s does to the selected piece: the move it ends (its index), STEP (the next landing square of a
 * capture with more to come), or -1: nothing (or routes taking different pieces end there: tap them step by step) */
static int resolve(int s)
{
    int step = 0, end = -1, mixed = 0;
    for (int i = 0; i < nlegal; i++) {
        const dmove *m = &legal[i];
        if (!fits(m)) continue;
        if (m->path[steps] == s) {
            if (m->n == steps + 1) return i;
            step = 1;
        } else if (m->path[m->n - 1] == s) {
            if (end < 0) end = i;
            else if (!dr_same(&legal[end], m)) mixed = 1;
        }
    }
    return step ? STEP : mixed ? -1 : end;
}

static void play(int i, int animate)
{
    dmove m = legal[i];
    anim.on = animate;
    anim.first = steps;                         /* the piece slides on from where it stands */
    anim.m = m;
    anim.t0 = ui_ms();
    dr_play(&G, &m);
    refresh();
    ui_sound(m.ncap ? SND_CAPTURE : SND_MOVE);
}

/* the selected piece to square s (a tap, or dropped there); 0: it cannot go there */
static int go(int s, int animate)
{
    int r = resolve(s);
    if (r == STEP) {
        path[steps++] = (uint8_t)s;
        ui_sound(SND_TAP);
        return 1;
    }
    if (r >= 0) play(r, animate);
    return r >= 0;
}

static int undo(void)
{
    if (!dr_undo(&G)) return 0;
    anim.on = 0;
    refresh();
    return 1;
}

/* "32-28 19x23 28x19 14x23", "c3-d4 f6-e5": the screenshots' and the tests' moves (the variant's notation) */
static int script(const char *s)
{
    while (*s == ' ') s++;
    while (*s) {
        int used, k = dr_parse(legal, nlegal, G.variant, s, &used);
        if (k < 0) return 0;
        play(k, 0);
        s += used;
        while (*s == ' ') s++;
    }
    return 1;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* the board's picture, k x k squares of s pixels in a frame of f: the checkered wood, or (plain: Turkish draughts,
 * every square used) one plank ruled into squares */
static void paint_board(uint8_t *o, int k, int s, int f, int plain)
{
    static const float INK[3] = {52, 34, 22};
    art_board_n(k, k, s, f, !plain, o);
    if (!plain) return;
    int w = k * s + 2 * f;
    float lw = 0.8f + 0.028f * s;               /* the lines' width, pixels */
    for (int y = f; y < f + k * s; y++)
        for (int x = f; x < f + k * s; x++) {
            float dx = fmodf(x - f + 0.5f, (float)s), dy = fmodf(y - f + 0.5f, (float)s);
            float d = fminf(fminf(dx, s - dx), fminf(dy, s - dy));   /* to the nearest line */
            float a = 0.72f * fminf(fmaxf(lw / 2 - d + 0.5f, 0), 1);
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * (1 - a) + INK[i] * a + 0.5f);
        }
}

static void layout(box b)
{
    sq = (int)floorf(b.w / (n + 2 * FRAME));
    fr = (int)(sq * FRAME);
    float bw = (float)n * sq + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    int plain = dr_all(G.variant);
    if (made != sq || made_n != n || made_plain != plain) {
        made = sq;
        made_n = n;
        made_plain = plain;
        for (int c = 0; c < 2; c++)
            for (int k = 0; k < 2; k++) {
                ui_free(&t_man[c][k]);
                art_counter(k ? COUNTER_KING : COUNTER_MAN, c, sq, ui_buf(sq, sq));
                t_man[c][k] = ui_tex(sq, sq, ui_buf(sq, sq));
            }
        int w = n * sq + 2 * fr;
        ui_free(&t_board);
        paint_board(ui_buf(w, w), n, sq, fr, plain);
        t_board = ui_tex(w, w, ui_buf(w, w));
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int c = 0; c < 2; c++) {
            ui_free(&t_small[c]);
            art_counter(COUNTER_MAN, c, small, ui_buf(small, small));
            t_small[c] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

static void sq_xy(int s, float *x, float *y)
{
    int f = s % n, r = s / n;
    if (ui.flipped) { f = n - 1 - f; r = n - 1 - r; }
    *x = bx + (float)f * sq;
    *y = by + (float)(n - 1 - r) * sq;
}

static int xy_sq(float x, float y)
{
    int f = (int)floorf((x - bx) / sq), r = n - 1 - (int)floorf((y - by) / sq);
    if (f < 0 || f >= n || r < 0 || r >= n) return -1;   /* off the board */
    if (!dr_all(G.variant) && ((f + r) & 1))            /* a light square (bashni: the column below, rising into it) */
        return dr_columns(G.variant) && y + 0.5f * sq < by + n * sq ? xy_sq(x, y + 0.5f * sq) : -1;
    return ui.flipped ? (n - 1 - r) * n + n - 1 - f : r * n + f;
}

/* a piece (alpha: below 255, faded), turned round for the top player face to face: the king's crown faces them */
static void piece_at(int pc, float x, float y, float size, Uint8 alpha)
{
    int pl = DR_OWNER(pc);                      /* a man lies flat, like the discs */
    ui_flat_at(t_man[colour(pl)][DR_IS_KING(pc)], x + size / 2, y + size / 2, size, alpha / 255.0f, ui_upside(pl));
}

/* the piece on square s of a position, at (x, y) (its square's corner): bashni's whole column */
static void stack_at(const dpos *p, int s, float x, float y, float size, Uint8 alpha)
{
    if (!dr_columns(G.variant)) { piece_at(p->sq[s], x, y, size, alpha); return; }
    column_at(dr_column(p, s), dr_height(p, s), t_man, x + size / 2, y + size / 2, (float)sq, PIECE * size, 0,
              alpha / 255.0f);
}

static void draw(void)
{
    const dpos *p = dr_pos(&G), *shown = p;
    float w = (float)n * sq + 2 * fr, x, y, h = sq / 2.0f;
    ui_put(t_board, bx - fr, by - fr, w, w);
    if (G.ply) {                                /* the last move: where it started and every square it landed on */
        const dmove *m = &G.move[G.ply - 1];
        sq_xy(m->from, &x, &y);
        ui_mark_last(x, y, sq, sq);
        for (int i = 0; i < m->n; i++) {
            sq_xy(m->path[i], &x, &y);
            ui_mark_last(x, y, sq, sq);
        }
    }
    int at = sel < 0 ? -1 : steps ? path[steps - 1] : sel;   /* where the selected piece stands now */
    if (at >= 0) {
        sq_xy(at, &x, &y);
        ui_mark_select(x, y, sq, sq);
    }
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(STEP_MS * (anim.m.n - anim.first));
        if (t >= 1) t = 1, anim.on = 0;
        else {
            ui_animate();
            shown = &G.pos[G.ply - 1];          /* the pieces taken go when the move ends */
        }
        t = 1 - (1 - t) * (1 - t) * (1 - t);
    }
    uint8_t ring[DR_MAX_SQ] = {0}, dot[DR_MAX_SQ] = {0};   /* the pieces the selected one takes; where it can go */
    for (int i = 0; sel >= 0 && i < nlegal; i++) {
        const dmove *m = &legal[i];
        if (!fits(m)) continue;
        for (int k = 0; k < m->ncap; k++) ring[m->cap[k]] = 1;
        dot[m->path[steps]] = dot[m->path[m->n - 1]] = 1;
    }
    int cols = dr_columns(G.variant);
    for (int i = 0; i < n * n; i++) {           /* bashni: the rows from the top of the screen (a column rises over
                                                 * the one behind it) */
        int s = !cols ? i : i % n + n * (ui.flipped ? i / n : n - 1 - i / n);
        if (!shown->sq[s] || s == sel || (anim.on && s == anim.m.from)) continue;
        sq_xy(s, &x, &y);
        stack_at(shown, s, x, y, (float)sq, ring[s] ? 140 : 255);   /* the pieces taken: faded, ringed */
        if (ring[s]) ui_mark_ring(x + h, y + h, 1.12f * sq);
    }
    for (int s = 0; s < n * n; s++)
        if (dot[s] && resolve(s) != -1) {
            sq_xy(s, &x, &y);
            ui_mark_dot(x + h, y + h, 0.32f * sq);
        }
    if (sel >= 0 && !(drag.on && drag.moved)) {
        sq_xy(at, &x, &y);
        if (cols) column_at(dr_column(p, sel), dr_height(p, sel), t_man, x + h, y + h, (float)sq, PIECE * sq, 0, 1);
        else piece_at(p->sq[sel], x, y, (float)sq, 255);
    }
    if (anim.on) {                              /* the slide, square to square along the path */
        int segs = anim.m.n - anim.first, k = (int)(t * segs);
        if (k >= segs) k = segs - 1;
        int a = anim.first + k ? anim.m.path[anim.first + k - 1] : anim.m.from, b = anim.m.path[anim.first + k];
        float x0, y0, x1, y1, f = t * segs - k;
        sq_xy(a, &x0, &y0);
        sq_xy(b, &x1, &y1);
        stack_at(shown, anim.m.from, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, (float)sq, 255);
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, beside the finger */
        float s = 1.18f * sq, off = ui_upside(p->side) ? -0.25f : 0.75f;
        stack_at(p, drag.from, drag.x - s / 2, drag.y - s * off, s, 255);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    if (state != GS_PLAY) return;
    int s = xy_sq(x, y);
    if (sel >= 0 && steps && s == path[steps - 1]) return;   /* the piece where it stands in its capture */
    if (sel >= 0 && s >= 0 && go(s, 1)) return;
    if (s >= 0 && movable(s)) {
        sel = s;
        steps = 0;
        drag.on = 1;
        drag.from = s;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    const dpos *p = dr_pos(&G);
    if (sel >= 0 || (s >= 0 && p->sq[s] && DR_OWNER(p->sq[s]) == p->side)) ui_sound(SND_DENY);
    sel = -1;
    steps = 0;
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
    if (!drag.moved) return;                    /* a tap: the piece stays selected */
    int s = xy_sq(x, y);
    if (s >= 0 && s != drag.from && !go(s, 0)) ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the pieces a player took: a man of the other colour, and how many */
static void bar(int player, box b)
{
    const dpos *p = dr_pos(&G);
    int left = 0;
    if (dr_columns(G.variant)) {                /* bashni: the other's pieces held in the player's columns */
        ui_bar_count(b, t_small[colour(!player)], dr_prisoners(p, player));
        return;
    }
    for (int s = 0; s < n * n; s++) left += p->sq[s] && DR_OWNER(p->sq[s]) != player;
    ui_bar_count(b, t_small[colour(!player)], dr_pieces(G.variant) - left);
}

/* a man of the player's colour (the variant's: English player 0 is ebony) */
static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_MAN, colour(player), size, rgba); }

/* pieces on a picture: one counter drawn once, put on every square listed (column, row from the top) */
static void put_all(uint8_t *rgba, int size, int kind, int c, int s, int ox, int oy, const int (*place)[2], int count)
{
    uint8_t *one = ui_buf(s, s);
    art_counter(kind, c, s, one);
    for (int i = 0; i < count; i++) art_blend(rgba, size, size, one, s, s, ox + place[i][0] * s, oy + place[i][1] * s);
}

/* Frisian's card: a game going on, a man taking along a row: the man taken faded in its ring, a dot beyond it */
static void thumb_frisian(uint8_t *rgba, int size, int s, int o)
{
    static const uint8_t shade[3] = {24, 34, 26}, dark[3] = {40, 34, 30}, cream[3] = {246, 238, 220},
                         gold[3] = {255, 196, 64};
    int g = 3 * s;                              /* the row taken along, in a gold light */
    art_disc(g, gold, 0.6f, 1.3f, ui_buf(g, g));
    art_blend(rgba, size, size, ui_buf(g, g), g, g, o + 4 * s + s / 2 - g / 2, o + 5 * s + s / 2 - g / 2);
    static const int ivory[][3] = {{0, 9, 0}, {4, 9, 0}, {8, 9, 0}, {1, 8, 0}, {5, 8, 0}, {7, 6, 0}, {2, 5, 0},
                                   {6, 3, 1}},
                     ebony[][3] = {{1, 0, 0}, {5, 0, 0}, {9, 0, 0}, {2, 1, 0}, {6, 1, 0}, {3, 2, 0}, {0, 3, 0}};
    uint8_t *one = ui_buf(s, s);
    for (int pl = 0; pl < 2; pl++) {
        const int (*list)[3] = pl ? ebony : ivory;
        int count = pl ? 7 : 8;
        for (int i = 0; i < count; i++) {
            art_counter(list[i][2] ? COUNTER_KING : COUNTER_MAN, pl, s, one);
            art_blend(rgba, size, size, one, s, s, o + list[i][0] * s, o + list[i][1] * s);
        }
    }
    art_counter(COUNTER_MAN, 1, s, one);        /* the man taken, on the row of the taker, two squares on */
    for (int i = 0; i < s * s; i++) one[4 * i + 3] = (uint8_t)(one[4 * i + 3] * 0.55f);
    art_blend(rgba, size, size, one, s, s, o + 4 * s, o + 5 * s);
    int d = (int)(1.12f * s);
    art_ring(d, 0.82f, shade, 0.30f, ui_buf(d, d));
    art_blend(rgba, size, size, ui_buf(d, d), d, d, o + 4 * s + (s - d) / 2, o + 5 * s + (s - d) / 2);
    int dot = (int)(0.5f * s), in = (int)(0.4f * s);   /* the landing square beyond: a dark dot in a cream rim */
    art_disc(dot, cream, 1, 0, ui_buf(dot, dot));
    art_blend(rgba, size, size, ui_buf(dot, dot), dot, dot, o + 6 * s + (s - dot) / 2, o + 5 * s + (s - dot) / 2);
    art_disc(in, dark, 1, 0, ui_buf(in, in));
    art_blend(rgba, size, size, ui_buf(in, in), in, in, o + 6 * s + (s - in) / 2, o + 5 * s + (s - in) / 2);
}

/* bashni's card: a game going on, columns of every height (the back rows first) */
static void thumb_bashni(uint8_t *rgba, int size, int k, int s, int o)
{
    dpos p;
    dr_setup(&p, DR_BASHNI, 0, "a1 c1 b2 d2 c3 g3 d4 e5", "h8 f8 e7 a5 g5 f6 Kd6");
    static const char *under[][2] = {{"d2", "b"}, {"c3", "bw"}, {"d4", "bbw"}, {"f6", "w"}, {"g5", "wb"}};
    for (unsigned i = 0; i < sizeof under / sizeof under[0]; i++) {
        int used;
        dr_stack(&p, dr_read(DR_BASHNI, under[i][0], &used), under[i][1]);
    }
    int d = (int)(PIECE * s);
    for (int r = 0; r < k; r++)
        for (int c = 0; c < k; c++) {
            int q = c + k * (k - 1 - r);
            if (p.sq[q])
                column_thumb(rgba, size, dr_column(&p, q), dr_height(&p, q), o + c * s + s / 2.0f,
                             o + r * s + s / 2.0f, s, d);
        }
}

/* the menu's card (-1): a corner of a board, men and a king; the picker's: the whole board at the start, 10 x 10
 * (ivory at the bottom) or 8 x 8 (English: ebony at the bottom, it moves first; Russian: ivory at the bottom),
 * Brazilian a game going on with a king each, Turkish its men in two rows on a plank ruled into squares, Frisian a
 * capture along a row, Canadian the 12 x 12 board at the start, bashni its columns */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int k = variant < 0 ? 4 : dr_size(variant);
    int s = (int)(size / (k + 2 * FRAME)), f = (int)(s * FRAME), w = k * s + 2 * f, o = (size - w) / 2;
    uint8_t *board = ui_buf(w, w);
    paint_board(board, k, s, f, variant >= 0 && dr_all(variant));
    art_blend(rgba, size, size, board, w, w, o, o);
    o += f;
    if (variant == DR_FRISIAN) { thumb_frisian(rgba, size, s, o); return; }
    if (variant == DR_BASHNI) { thumb_bashni(rgba, size, k, s, o); return; }
    if (variant >= DR_RUSSIAN) {                /* the pieces of a position, ivory (player 0) at the bottom */
        dpos p;
        if (variant == DR_BRAZILIAN) dr_setup(&p, variant, 0, "a1 c1 b2 f2 e3 g3 d4 Kf6", "b8 d8 h8 c7 a5 e5 h6 Kb4");
        else dr_start(&p, variant);
        int place[2][2][30][2], count[2][2] = {{0}};   /* [player][king]: column, row from the top */
        for (int q = 0; q < k * k; q++) {
            int pc = p.sq[q], pl = DR_OWNER(pc), kg = DR_IS_KING(pc), *c = count[pl] + kg;
            if (!pc || *c >= 30) continue;
            place[pl][kg][*c][0] = q % k;
            place[pl][kg][(*c)++][1] = k - 1 - q / k;
        }
        for (int pl = 0; pl < 2; pl++)
            for (int kg = 0; kg < 2; kg++)
                put_all(rgba, size, kg ? COUNTER_KING : COUNTER_MAN, pl, s, o, o, (const int (*)[2])place[pl][kg],
                        count[pl][kg]);
        return;
    }
    if (variant < 0) {
        static const int ebony[3][2] = {{1, 0}, {3, 0}, {2, 1}}, ivory[2][2] = {{0, 3}, {2, 3}}, king[1][2] = {{1, 2}};
        put_all(rgba, size, COUNTER_MAN, 1, s, o, o, ebony, 3);
        put_all(rgba, size, COUNTER_MAN, 0, s, o, o, ivory, 2);
        put_all(rgba, size, COUNTER_KING, 0, s, o, o, king, 1);
        return;
    }
    int rows = variant == 1 ? 3 : 4, top[20][2], bottom[20][2], nt = 0, nb = 0;
    for (int r = 0; r < k; r++)
        for (int c = 0; c < k; c++) {
            if (!((c + r) & 1)) continue;       /* the dark squares: the bottom left one is dark */
            if (r < rows) top[nt][0] = c, top[nt++][1] = r;
            else if (r >= k - rows) bottom[nb][0] = c, bottom[nb++][1] = r;
        }
    put_all(rgba, size, COUNTER_MAN, variant == 1 ? 0 : 1, s, o, o, (const int (*)[2])top, nt);
    put_all(rgba, size, COUNTER_MAN, variant == 1 ? 1 : 0, s, o, o, (const int (*)[2])bottom, nb);
}

static int busy(void) { return anim.on; }     /* the end waits for the last move to finish */

static const game draughts_game = {
    .id = "draughts", .order = 20, .variants = DR_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy,
};
REGISTER_GAME(draughts_game)
