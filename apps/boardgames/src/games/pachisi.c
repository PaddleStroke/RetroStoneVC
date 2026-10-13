/*
 * Pachisi in RetroStone Board: the cross of inlaid squares round the charkoni, its twelve castles, each player's two
 * armies of discs (four players: an army each), six cowries thrown with a tap (they fly, flip and settle, mouths up
 * or down), the pieces hopping round (the rules and the notation: pachisi_rules.c, pachisi.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The armies: ivory and amber (the house's) against ebony and rosewood (the house's ebony reddened); each arm's middle
 * column inlaid in its army's tone, so the arms say whose they are. Two players: player 0 has ivory and amber, 1 ebony
 * and rosewood; the bottom player's arm at the bottom (flipped: the board turned a quarter round, player 1's arm at the
 * bottom); the corners: each player's tray of pieces home on their left, the cowries' tray on their right. Four
 * players: player p has army p (0 ivory, 1 ebony, 2 amber, 3 rosewood: partners opposite), the turn round the table;
 * each has one corner tray, on their right from their arm's end, for its cowries and its pieces home; flipped: a half
 * turn. The pieces waiting sit in the charkoni, each army on the side of its arm.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/lines.h"
#include "games/pachisi.h"
#include "shell.h"

#define N 19                                    /* cells across the board */
#define FRAME 0.45f                             /* the frame, in cells */
#define PIECE 1.02f                             /* a piece's picture, in cells (the disc fills 0.88 of it) */
#define WAIT 0.62f                              /* a piece waiting in the charkoni, smaller */
#define SHELL 2.0f                              /* a cowrie's picture, in cells (the shell 0.92 x 0.62 of it) */
#define HOP_MS 105                              /* a hop, at most (a long move hops faster) */
#define MOVE_MS 1500                            /* a move's hops, at most */
#define GLIDE_MS 360                            /* a piece sent back, or home, gliding to its place */
#define THROW_MS 950                            /* the cowries' flight and settling */

static pgame G;
static int cs, fr, made = -1, made_small = -1;
static float bx, by;                            /* the cells' top left corner */
static box sq;                                  /* the board's square */
static SDL_Texture *t_board, *t_piece[PA_ARMIES], *t_small[PA_ARMIES], *t_shell[2];
static uint8_t shells[PA_MAX_EVENTS];           /* each throw's cowries (bit i: shell i's mouth up), kept through
                                                 * undo: undo never rethrows */
static int nshells;
static struct { int on, k; Uint32 t0; } roll;   /* the throw in the air (k: the throws before it) */
static int pass_snd;                            /* a throw lost: its sound once the cowries lie still */
static struct { int on, piece, moved; float x, y; } drag;
static int pick = -1;                           /* a square chosen whose pieces go two ways: their targets only */
static struct { int on, piece, army, from, to, hops, heard, caught, hop; Uint32 t0; ppos before; } anim;

/* the armies' colours: the house's ivory, ebony and amber, and rosewood: the ebony reddened (a factor per channel) */
static const int ART[PA_ARMIES] = {0, 1, 4, 1};
static const float ROSEWOOD[3] = {1.7f, 0.92f, 0.7f};
/* the arms' middle columns in their armies' tones (factors on the plank: the grain stays) */
static const float TONE[PA_ARMIES][3] = {{1.09f, 1.25f, 1.62f}, {0.42f, 0.42f, 0.5f}, {1.08f, 1.0f, 0.78f},
                                         {0.62f, 0.4f, 0.36f}};
static const float MAPLE[3] = {1.06f, 1.09f, 1.16f}, TRAY[3] = {0.8f, 0.78f, 0.76f};
static const uint8_t PLANK[3] = {222, 184, 128};

/* ---- where things are -------------------------------------------------------------------------------------------- */
static int four(void) { return G.pos.players == 4; }
static int owner(int army) { return four() ? army : army & 1; }   /* the player */

/* board cells (u, v: the board with arm 0 at the bottom) to the screen's cells. Flipped: two players, a quarter turn
 * (player 1's arm at the bottom); four, a half turn (the shell's way for players round a board) */
static void view(float u, float v, float *x, float *y)
{
    if (ui.flipped) {
        float du = u - N / 2.0f, dv = v - N / 2.0f;
        if (four()) u = N / 2.0f - du, v = N / 2.0f - dv;
        else u = N / 2.0f - dv, v = N / 2.0f + du;
    }
    *x = u;
    *y = v;
}

static void px_of(float u, float v, float *x, float *y)   /* screen cells to pixels */
{
    *x = bx + u * cs;
    *y = by + v * cs;
}

/* (dx, dy) turned anticlockwise (on the screen, y down) by r quarters: arm 0's frame to arm r's */
static void turn_by(int r, float *dx, float *dy)
{
    for (int k = 0; k < r; k++) {
        float t = *dx;
        *dx = *dy;
        *dy = -t;
    }
}

/* a four-player game: the corner tray of player p (its cowries and its pieces home), on the board: the corner on
 * their right, seen from their arm's end */
static void corner(int p, float *u, float *v)
{
    float dx = 5.5f, dy = 5.5f;
    turn_by(p, &dx, &dy);
    *u = N / 2.0f + dx;
    *v = N / 2.0f + dy;
}

/* a player's tray (screen cells). Two players: home (their pieces home) on their left, else the cowries' on their
 * right; four: their corner, both */
static void tray(int player, int home, float *u, float *v)
{
    if (four()) {
        float cu, cv;
        corner(player, &cu, &cv);
        view(cu, cv, u, v);
        return;
    }
    int bottom = player == ui_bottom();
    *u = bottom != home ? 15 : 4;
    *v = bottom ? 15 : 4;
}

static int tray_top(int player)                 /* the tray in the upper half: turned round */
{
    float u, v;
    tray(player, 0, &u, &v);
    return v < N / 2.0f;
}

/* where each player sits, for the shell's bars (0..1 of the board, not flipped): two, at the bottom and the top;
 * four, at their corner tray */
static void home(int player, float *fx, float *fy)
{
    if (G.pos.players != 4) {
        *fx = 0.5f;
        *fy = player ? 0 : 1;
        return;
    }
    corner(player, fx, fy);
    *fx /= N;
    *fy /= N;
}

/* a piece waiting in the charkoni: slot k in arm 0's triangle (cells from the centre): three, and one inside */
static void wait_at(int k, float *dx, float *dy)
{
    *dx = k == 3 ? 0 : (k - 1) * 0.58f;
    *dy = k == 3 ? 0.5f : 1.08f;
}

/* where a piece is drawn (pixels): k its slot (waiting: among its army's in the charkoni; home: in the tray) */
static void spot(int army, int place, int k, float *x, float *y)
{
    float u, v;
    if (place == 0) {                           /* in the charkoni, on its arm's side */
        float dx, dy;
        wait_at(k, &dx, &dy);
        turn_by(army, &dx, &dy);
        view(N / 2.0f + dx, N / 2.0f + dy, &u, &v);
    } else if (place == PA_HOME) {              /* home: a row per army in the owner's tray; four: a row at the far
                                                 * side of the player's tray */
        int pl = owner(army), top = tray_top(pl);
        float cu, cv, du = (k - 1.5f) * 1.3f, dv = four() ? -3.0f : army >= 2 ? 0.8f : -0.8f;
        tray(pl, 1, &cu, &cv);
        if (top) du = -du, dv = -dv;
        u = cu + du;
        v = cv + dv;
    } else {
        int cx, cy;
        pa_cell(pa_square(army, place), &cx, &cy);
        view(cx + 0.5f, cy + 0.5f, &u, &v);
    }
    px_of(u, v, x, y);
}

/* a piece's slot among its army's pieces waiting or home (by index), or its rank among all the pieces on its
 * square (the stack) */
static int slot_of(const ppos *p, int army, int i)
{
    int at = p->at[army][i], k = 0;
    if (at == 0 || at == PA_HOME) {
        for (int j = 0; j < i; j++) k += p->at[army][j] == at;
        return k;
    }
    int sq_ = pa_square(army, at);
    for (int a = 0; a < PA_ARMIES; a++)
        for (int j = 0; j < PA_PIECES; j++) {
            if (a == army && j == i) return k;
            k += pa_square(a, p->at[a][j]) == sq_;
        }
    return k;
}

static void piece_px(const ppos *p, int army, int i, float *x, float *y)
{
    int at = p->at[army][i], k = slot_of(p, army, i);
    spot(army, at, k, x, y);
    if (at > 0 && at < PA_HOME) {               /* a stack: each one a little up and left */
        *x -= 0.08f * k * cs;
        *y -= 0.13f * k * cs;
    }
}

/* the mover's piece 0..7 <-> army, index */
static int army_of(int piece) { return pa_army(&G.pos, piece); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = drag.on = pass_snd = roll.on = 0;
    pick = -1;
}

static void start(int variant)
{
    (void)variant;
    pa_new(&G, ui.players);
    nshells = 0;
    calm();
}

static int status(void) { return G.pos.winner < 0 ? GS_PLAY : gs_win(G.pos.winner); }
static int turn(void) { return G.pos.turn; }
static int tumbling(void) { return roll.on && ui_ms() - roll.t0 < THROW_MS; }
static int busy(void) { return anim.on || tumbling(); }   /* the end waits for the last piece */

static int mouths(int m)
{
    int n = 0;
    for (int i = 0; i < 6; i++) n += m >> i & 1;
    return n;
}

/* the cowries of a throw given by its value only (a script): some shells mouth up, always the same */
static int mask_of(int k, int up)
{
    int m = (1 << up) - 1, r = (k * 5) % 6;
    return ((m << r) | (m >> (6 - r))) & 63;
}

static void throw_shells(void)
{
    int k = G.pos.throws, m = 0;
    if (G.n >= PA_MAX_EVENTS) return;
    if (k < nshells) m = shells[k];             /* thrown before an undo: the same cowries again */
    else {
        for (int i = 0; i < 6; i++) m |= ui_random(2) << i;
        shells[k] = (uint8_t)m;
        nshells = k + 1;
    }
    int r = pa_throw(&G, mouths(m));
    if (!r) return;
    roll.on = 1;
    roll.k = k;
    roll.t0 = ui_ms();
    pass_snd = r != PT_MOVE;
    pick = -1;
    ui_sound(SND_DICE);
    ui_animate();
}

static void play(int piece, int animate)
{
    ppos before = G.pos;
    int a = army_of(piece), from = before.at[a][piece % PA_PIECES], to = pa_dest(&before, piece);
    int r = pa_move(&G, piece);
    if (!r) { ui_sound(SND_DENY); return; }
    anim.on = animate;
    anim.piece = piece;
    anim.army = a;
    anim.from = from;
    anim.to = to;
    anim.hops = from == 0 ? to : to - from;
    anim.hop = MOVE_MS / anim.hops < HOP_MS ? MOVE_MS / anim.hops : HOP_MS;
    anim.heard = 0;
    anim.caught = r - 1;
    anim.t0 = ui_ms();
    anim.before = before;
    drag.on = 0;
    pick = -1;
    if (!animate) ui_sound(r > 1 ? SND_CAPTURE : to == PA_HOME ? SND_PLACE : SND_MOVE);
    ui_animate();
}

static int undo(void)
{
    if (!pa_undo(&G)) return 0;
    calm();
    return 1;
}

static int can_pass(void) { return G.pos.phase == PA_MOVE && !busy(); }

static void pass(void)                          /* declining the throw */
{
    if (!can_pass() || !pa_decline(&G)) return;
    drag.on = 0;
    pick = -1;
}

/* "10 a 25 e 3 - 6 b": throws, the pieces moved, '-' a throw declined (pachisi.h) */
static int script(const char *s)
{
    int ok = pa_script(&G, s);
    for (int i = 0; i < G.n; i++)               /* its throws are the game's from now on */
        if (G.ev[i] < PA_DECLINE_EV) shells[G.past[i].throws] = (uint8_t)mask_of(G.past[i].throws, G.ev[i]);
    nshells = G.pos.throws;
    calm();
    return ok;
}

/* ---- the bot (pachisi_bot.c) ------------------------------------------------------------------------------------- */
const pgame *pachisi_live(void) { return &G; }

int pachisi_show(const char *move)
{
    if (busy()) return 0;
    if (!strcmp(move, "roll")) {
        if (G.pos.phase != PA_THROW) return 0;
        throw_shells();
        return 1;
    }
    if (!strcmp(move, "-")) {
        if (!can_pass()) return 0;
        pass();
        return 1;
    }
    int i = move[0] - 'a';
    if (move[0] < 'a' || move[1] || G.pos.phase != PA_MOVE || pa_dest(&G.pos, i) < 0) return 0;
    play(i, 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
static void tone_rgb(const float tone[3], uint8_t rgb[3])
{
    for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)clampf(PLANK[k] * tone[k], 0, 255);
}

/* a castle's mark: a cross from corner to corner, ink (pale on a dark square), over the square at (x, y), w wide */
static void paint_cross(uint8_t *o, int n, int x, int y, int w, int pale)
{
    art_fig *f = art_fig_new();
    for (int s = 0; s < 2; s++) {
        float a = 0.2f, b = 0.8f, h = 0.06f;    /* a band from (a, a) to (b, b), flared at its ends */
        float band[] = {a - h, a + h, a + h, a - h, 0.5f, 0.5f - 0.6f * h, b + h, b - h, b - h, b + h,
                        0.5f, 0.5f + 0.6f * h};
        if (s)
            for (int i = 0; i < 6; i++) band[2 * i] = 1 - band[2 * i];
        art_poly(f, ART_PART, band, 6);
    }
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.09f, 0.09f);
    art_style st = art_flat(pale ? ART_PALE : ART_INK, ART_INK, 0, 0);
    art_fig_blend(o, n, n, f, &st, x, y, w, pale ? 0.8f : 0.5f);
    art_fig_free(f);
}

/* the board: cells of c pixels in a frame of f, (19 c + 2 f) squared, arm 0 at the bottom. The house plank; the
 * squares inlaid (maple, each arm's middle column in its army's tone, the castles crossed); the charkoni a square of
 * four triangles in the armies' tones; the four corners sunk into trays. */
static void paint_board(uint8_t *o, int c, int f)
{
    int n = N * c + 2 * f;
    float m = fmaxf(1, 0.05f * c), seam = fmaxf(1, 0.07f * c);
    art_board_n(N, N, c, f, 0, o);
    for (int q = 0; q < 4; q++) {               /* the trays */
        float in = 0.35f * c, x0 = f + (q & 1 ? 11 : 0) * c + in, y0 = f + (q & 2 ? 11 : 0) * c + in;
        art_inlay(o, n, n, x0, y0, 8 * c - 2 * in, 8 * c - 2 * in, 0.9f * c, TRAY, fmaxf(1, 0.12f * c));
    }
    for (int s = 0; s < PA_SQUARES; s++) {
        int x, y, army = s >= PA_LAP ? (s - PA_LAP) / 7 : s % 17 == 0 ? s / 17 : -1;
        pa_cell(s, &x, &y);
        const float *tone = army >= 0 ? TONE[army] : MAPLE;
        art_inlay(o, n, n, f + x * c + m, f + y * c + m, c - 2 * m, c - 2 * m, 0.1f * c, tone, seam);
        if (pa_castle(s)) paint_cross(o, n, f + x * c, f + y * c, c, army == 1 || army == 3);
    }
    float k = f + 8 * c + m, w = 3 * c - 2 * m;  /* the charkoni */
    art_inlay(o, n, n, k, k, w, w, 0.12f * c, MAPLE, seam);
    for (int a = 0; a < PA_ARMIES; a++) {
        art_fig *fig = art_fig_new();
        float tri[6] = {-0.5f, 0.5f, 0.5f, 0.5f, 0, 0};   /* arm 0's: the bottom, from the centre */
        for (int i = 0; i < 3; i++)
            for (int r = 0; r < a; r++) {
                float t = tri[2 * i];
                tri[2 * i] = tri[2 * i + 1];
                tri[2 * i + 1] = -t;
            }
        for (int i = 0; i < 6; i++) tri[i] = 0.5f + tri[i] * 0.94f;
        art_poly(fig, ART_PART, tri, 3);
        uint8_t rgb[3];
        tone_rgb(TONE[a], rgb);
        art_style st = art_flat(rgb, ART_INK, fmaxf(0.006f, 1.0f / w), 0.15f);
        art_fig_blend(o, n, n, fig, &st, (int)k, (int)k, (int)w, 0.92f);
        art_fig_free(fig);
    }
    lines_dot(o, n, f + 9.5f * c, f + 9.5f * c, 0.17f * c, ART_INK);
    lines_dot(o, n, f + 9.5f * c, f + 9.5f * c, 0.11f * c, ART_PALE);
}

/* a piece of an army: the house's disc in its player's colour, warmed for the second army */
static void paint_piece(int army, int size, uint8_t *o)
{
    art_counter(COUNTER_DISC, ART[army], size, o);
    if (army != 3) return;
    for (int i = 0; i < size * size; i++)
        for (int k = 0; k < 3; k++) o[4 * i + k] = (uint8_t)clampf(o[4 * i + k] * ROSEWOOD[k], 0, 255);
}

/* a cowrie seen from above, its long side across: mouth up (the toothed slit), or its glossy back */
static void paint_shell(int up, int size, uint8_t *o)
{
    static const uint8_t shell[3] = {240, 230, 208}, back[3] = {226, 196, 138}, slit[3] = {66, 44, 30},
                         spot_c[3] = {184, 136, 84}, gloss[3] = {255, 250, 236};
    art_fig *f = art_fig_new();
    float line = fmaxf(0.02f, 1.2f / size);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.46f, up ? 0.3f : 0.31f);
    if (up) {                                   /* the slit along the middle, a little curved */
        float pt[2 * 30];
        int k = 0;
        for (int i = 0; i <= 14; i++) {
            float t = i / 14.0f, x = 0.12f + 0.76f * t, h = 0.045f * sinf(ART_PI * t) + 0.008f;
            pt[k++] = x;
            pt[k++] = 0.5f + 0.03f * sinf(ART_PI * t) - h;
        }
        for (int i = 14; i >= 0; i--) {
            float t = i / 14.0f, x = 0.12f + 0.76f * t, h = 0.045f * sinf(ART_PI * t) + 0.008f;
            pt[k++] = x;
            pt[k++] = 0.5f + 0.03f * sinf(ART_PI * t) + h;
        }
        art_poly(f, ART_MARK, pt, k / 2);
    } else {                                    /* the back's spots */
        for (int i = 0; i < 6; i++)
            art_ellipse(f, ART_MARK, 0.3f + 0.08f * i + 0.02f * (i & 1), 0.42f + 0.1f * (i % 3 == 1) + 0.04f * (i & 1),
                        0.025f, 0.02f);
    }
    art_style st = art_flat(up ? shell : back, ART_INK, line, up ? 0.3f : 0.55f);
    memcpy(st.mark, (float[3]){up ? slit[0] : spot_c[0], up ? slit[1] : spot_c[1], up ? slit[2] : spot_c[2]},
           sizeof st.mark);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
    art_fig *g = art_fig_new();
    if (up)                                     /* the teeth across the slit */
        for (int i = 0; i < 11; i++) {
            float x = 0.2f + 0.06f * i, y = 0.5f + 0.03f * sinf(ART_PI * (x - 0.12f) / 0.76f);
            art_rect(g, ART_PART, x - 0.011f, y - 0.06f, x + 0.011f, y + 0.06f);
        }
    else                                        /* the gloss */
        art_ellipse(g, ART_PART, 0.4f, 0.36f, 0.16f, 0.06f);
    art_style ts = art_flat(up ? shell : gloss, ART_INK, 0, 0);
    art_fig_blend(o, size, size, g, &ts, 0, 0, size, up ? 1 : 0.55f);
    art_fig_free(g);
}

static void layout(box b)
{
    sq = b;
    cs = (int)floorf(b.w / (N + 2 * FRAME));
    fr = (int)(cs * FRAME);
    float bw = (float)N * cs + 2 * fr;
    bx = floorf(b.x + (b.w - bw) / 2) + fr;
    by = floorf(b.y + (b.h - bw) / 2) + fr;
    if (made != cs) {
        made = cs;
        int n = N * cs + 2 * fr;
        uint8_t *o = malloc((size_t)n * n * 4);
        if (o) {
            paint_board(o, cs, fr);
            ui_free(&t_board);
            t_board = ui_tex(n, n, o);
            free(o);
        }
        int ps = (int)(PIECE * cs), ss = (int)(SHELL * cs);
        for (int a = 0; a < PA_ARMIES; a++) {
            ui_free(&t_piece[a]);
            paint_piece(a, ps, ui_buf(ps, ps));
            t_piece[a] = ui_tex(ps, ps, ui_buf(ps, ps));
        }
        for (int u = 0; u < 2; u++) {
            ui_free(&t_shell[u]);
            paint_shell(u, ss, ui_buf(ss, ss));
            t_shell[u] = ui_tex(ss, ss, ui_buf(ss, ss));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int a = 0; a < PA_ARMIES; a++) {
            ui_free(&t_small[a]);
            paint_piece(a, small, ui_buf(small, small));
            t_small[a] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void piece_at(int army, float x, float y, float scale)
{
    ui_flat_at(t_piece[army], x, y, PIECE * cs * scale, 1, ui_upside(owner(army)));
}

static float scale_of(int place) { return place == 0 ? WAIT / PIECE : 1; }

/* the pieces of a position, but those in skip (bit army * 4 + index) */
static void draw_pieces(const ppos *p, int skip)
{
    for (int a = 0; a < PA_ARMIES; a++)
        for (int i = 0; i < PA_PIECES; i++) {
            if (skip >> (a * PA_PIECES + i) & 1) continue;
            float x, y;
            piece_px(p, a, i, &x, &y);
            piece_at(a, x, y, scale_of(p->at[a][i]));
        }
}

/* the latest throw of a player: its event (-1: none yet) */
static int last_throw(int player)
{
    for (int i = G.n - 1; i >= 0; i--)
        if (G.ev[i] < PA_DECLINE_EV && G.past[i].turn == player) return i;
    return -1;
}

/* where shell i of throw k rests in a tray (screen cells from its centre), and how it lies (degrees) */
static void shell_rest(int k, int i, int top, float *du, float *dv, float *deg)
{
    *du = ((i % 3) - 1) * 2.25f + (art_hash(k, 2 * i) - 0.5f) * 0.8f;
    *dv = (i / 3 ? 1.05f : -1.05f) + (art_hash(k, 2 * i + 1) - 0.5f) * 0.7f + (i % 3 == 1 ? 0.25f : 0);
    *deg = art_hash(k + 7, i) * 180;
    if (top) *du = -*du, *dv = -*dv;
}

/* a player's cowries in their tray: waiting to be thrown (in gold light), in the air, the throw to play, or the
 * last one (dimmed; in a red glow when it was lost); the throw's value at the tray's edge (in gold: a grace) */
static void draw_shells(int q)
{
    const ppos *p = &G.pos;
    int e = last_throw(q), k = e >= 0 ? G.past[e].throws : -1, flying = tumbling() && roll.k == k && k >= 0;
    int active = p->phase != PA_OVER && p->turn == q, invite = active && p->phase == PA_THROW && !tumbling();
    int top = tray_top(q), mask = e >= 0 ? shells[k] : 0;
    int lost = e >= 0 && e == G.n - 1 && p->turn != q && !flying;
    if (e < 0 && !invite) return;
    float cu, cv, x, y, s = SHELL * cs, alpha = active || flying ? 1 : 0.45f, t = 1;
    tray(q, 0, &cu, &cv);
    px_of(cu, cv, &x, &y);
    if (invite) ui_mark_gold(x, y, 7.2f * cs);
    if (lost) ui_mark_glow(x, y, 6.5f * cs);
    if (invite && e >= 0) alpha = 0.75f;
    if (flying) {
        t = (ui_ms() - roll.t0) / (float)THROW_MS;
        ui_animate();
    }
    float fly = clampf(t / 0.7f, 0, 1), ease = 1 - (1 - fly) * (1 - fly) * (1 - fly);
    for (int i = 0; i < 6; i++) {
        float du, dv, deg, su = cu, sv = cv + (top ? -4.6f : 4.6f) * (1 - 0.2f * (i % 3 - 1) * (i % 3 - 1));
        shell_rest(k, i, top, &du, &dv, &deg);
        su += (i % 3 - 1) * 0.5f;
        float u = su + (cu + du - su) * ease, v = sv + (cv + dv - sv) * ease;
        float hgt = flying ? sinf(ART_PI * fly) * (1 - 0.35f * fly) : 0, sc = 1 + 0.5f * hgt;
        float phi = (1 - ease) * (3 + i % 3) * ART_PI, squash = flying ? fabsf(cosf(phi)) : 1;
        int up = mask >> i & 1;
        if (flying && cosf(phi) < 0) up = !up;
        float a = deg + (flying ? (1 - ease) * (i & 1 ? 420 : -420) : 0);
        if (flying && t > 0.7f) a += sinf((t - 0.7f) / 0.3f * ART_PI * 3) * (1 - (t - 0.7f) / 0.3f) * 9;
        px_of(u, v, &x, &y);
        float w = s * sc, h = s * sc * fmaxf(squash, 0.08f);
        ui_shadow_flat(x + hgt * 0.5f * cs, y + hgt * 0.8f * cs, 0.9f * w, 0.62f * w * fmaxf(squash, 0.3f),
                       alpha * (1 - 0.5f * hgt));
        SDL_Texture *tx = t_shell[up];
        SDL_SetTextureAlphaMod(tx, (Uint8)(255 * alpha));
        ui_put_rot(tx, x, y, w, h, a);
        SDL_SetTextureAlphaMod(tx, 255);
    }
    if (e < 0 || flying) return;
    int value = pa_value(G.ev[e]);
    px_of(cu, cv + (top ? -3.15f : 3.15f), &x, &y);
    if (pa_grace(value)) ui_mark_gold(x, y, 2.6f * cs);
    ui_number_rot(x, y, 1.15f * cs, value, 1, ui_upside(q) ? 180 : 0);
}

/* the last move: the squares it left and reached */
static void mark_last(void)
{
    for (int i = G.n - 1; i >= 0; i--) {
        if (G.ev[i] == PA_DECLINE_EV) return;
        if (G.ev[i] < PA_MOVE_EV) continue;
        const ppos *b = &G.past[i];
        int piece = G.ev[i] - PA_MOVE_EV, a = pa_army(b, piece), from = b->at[a][piece % PA_PIECES];
        int ends[2] = {from, pa_dest(b, piece)};
        for (int k = 0; k < 2; k++) {
            if (ends[k] <= 0 || ends[k] >= PA_HOME) continue;
            int cx, cy;
            float u, v, x, y;
            pa_cell(pa_square(a, ends[k]), &cx, &cy);
            view(cx + 0.5f, cy + 0.5f, &u, &v);
            px_of(u, v, &x, &y);
            ui_mark_last(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
        }
        return;
    }
}

/* the first of the mover's movable pieces (moves) of piece i's army on its place: they all move alike */
static int first_alike(int moves, int i)
{
    int a = army_of(i), base = i - i % PA_PIECES;
    for (int j = base; j < i; j++)
        if ((moves >> j & 1) && G.pos.at[a][j - base] == G.pos.at[a][i - base]) return j;
    return i;
}

/* the mover's movable pieces, one for each different move: the first of those alike (bit i) */
static int choices(void)
{
    const ppos *p = &G.pos;
    int moves = p->phase == PA_MOVE && !busy() ? pa_movable(p) : 0, out = 0;
    for (int i = 0; i < 2 * PA_PIECES; i++) {
        if (!(moves >> i & 1)) continue;
        if (first_alike(moves, i) == i) out |= 1 << i;
    }
    return out;
}

static int square_of(int piece)
{
    int a = army_of(piece);
    return pa_square(a, G.pos.at[a][piece % PA_PIECES]);
}

/* where piece's move ends (pixels) */
static void target_px(int piece, float *x, float *y)
{
    const ppos *p = &G.pos;
    int a = army_of(piece), to = pa_dest(p, piece);
    if (to == PA_HOME) {
        int k = 0;
        for (int j = 0; j < PA_PIECES; j++) k += p->at[a][j] == PA_HOME;
        spot(a, PA_HOME, k, x, y);
        return;
    }
    spot(a, to, 0, x, y);
}

/* where the pieces that can move go: dots (under the pieces), or rings round pieces sent back (over them) */
static void draw_targets(int ch, int rings)
{
    for (int i = 0; i < 2 * PA_PIECES; i++) {
        if (!(ch >> i & 1)) continue;
        if (pick >= 0 && square_of(i) != pick) continue;
        if (drag.on && !drag.moved && i != drag.piece) continue;
        float x, y;
        target_px(i, &x, &y);
        int hit = pa_takes(&G.pos, i) > 0;
        if (hit && rings) ui_mark_ring(x, y, 1.3f * cs);
        else if (!hit && !rings) ui_mark_dot(x, y, 0.42f * cs);
    }
}

static void draw(void)
{
    const ppos *p = &G.pos;
    float n = (float)N * cs + 2 * fr, x, y, t = 0;
    ui_put_rot(t_board, bx - fr + n / 2, by - fr + n / 2, n, n, ui.flipped ? (four() ? 180 : 90) : 0);
    if (anim.on) {                              /* a hop a beat, then the glides */
        t = (ui_ms() - anim.t0) / (float)anim.hop;
        int landed = t >= anim.hops ? anim.hops : (int)t;
        while (anim.heard < landed) {
            int last = ++anim.heard == anim.hops;
            if (last) ui_sound(anim.caught ? SND_CAPTURE : anim.to == PA_HOME ? SND_PLACE : SND_MOVE);
            else ui_sound_soft(SND_MOVE, 0.45f);
        }
        float glide = anim.caught || anim.to == PA_HOME ? (float)GLIDE_MS / anim.hop : 0;
        if (t >= anim.hops + glide) anim.on = 0;
        else ui_animate();
    }
    mark_last();
    for (int q = 0; q < G.pos.players; q++) draw_shells(q);
    if (pass_snd && !tumbling()) {
        pass_snd = 0;
        ui_sound(SND_DENY);
    }
    int ch = choices(), me = p->turn;
    int sel = drag.on && !drag.moved ? drag.piece : (ch & (ch - 1)) == 0 && ch ? __builtin_ctz((unsigned)ch) : -1;
    if (pick >= 0) {                            /* a square chosen: its pieces lit */
        int cx, cy;
        float u, v;
        pa_cell(pick, &cx, &cy);
        view(cx + 0.5f, cy + 0.5f, &u, &v);
        px_of(u, v, &x, &y);
        ui_mark_select(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
    } else if (sel >= 0 && ch) {                /* the piece pressed, or the only one that can move */
        int a = army_of(sel), at = p->at[a][sel % PA_PIECES];
        spot(a, at, 0, &x, &y);
        if (at == 0) {
            piece_px(p, a, sel % PA_PIECES, &x, &y);
            ui_mark_select_round(x, y, WAIT * cs * 1.15f);
        } else {
            ui_mark_select(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
        }
    }
    draw_targets(ch, 0);
    int skip = 0, gliding = anim.on && t >= anim.hops;
    if (anim.on) {                              /* the moving piece; the pieces caught, once it has landed */
        skip = 1 << (anim.army * PA_PIECES + anim.piece % PA_PIECES);
        if (gliding)
            for (int a = !owner(anim.army); a < PA_ARMIES; a += 2)
                for (int i = 0; i < PA_PIECES; i++)
                    if (anim.before.at[a][i] != p->at[a][i]) skip |= 1 << (a * PA_PIECES + i);
        draw_pieces(gliding ? p : &anim.before, skip);
    } else {
        if (drag.on && drag.moved) skip = 1 << (army_of(drag.piece) * PA_PIECES + drag.piece % PA_PIECES);
        draw_pieces(p, skip);
    }
    draw_targets(ch, 1);
    if (anim.on) {
        int a = anim.army, i = anim.piece % PA_PIECES;
        if (!gliding) {                         /* square by square */
            int h = (int)t, k0 = slot_of(&anim.before, a, i);
            float f = t - h, x1, y1;
            int p0 = anim.from + h, p1 = p0 + 1;
            if (p1 >= PA_HOME) {             /* into the charkoni's middle */
                x1 = bx + N / 2.0f * cs;
                y1 = by + N / 2.0f * cs;
            } else {
                spot(a, p1, 0, &x1, &y1);
            }
            if (anim.from == 0 && h == 0) spot(a, 0, k0, &x, &y);
            else spot(a, p0, 0, &x, &y);
            float e = ui_ease(f), up = (ui_upside(owner(a)) ? -1 : 1) * ui_hop(f) * cs;
            float sc = anim.from == 0 && h == 0 ? WAIT / PIECE + (1 - WAIT / PIECE) * e : 1;
            piece_at(a, x + (x1 - x) * e, y + (y1 - y) * e - up, sc * (1 + 0.1f * sinf(ART_PI * f)));
        } else {                                /* landed: the pieces caught glide home to the charkoni; a piece
                                                 * home glides from the charkoni to its tray */
            float g = ui_ease(clampf((t - anim.hops) * anim.hop / GLIDE_MS, 0, 1)), x1, y1;
            if (anim.to == PA_HOME) {
                piece_px(p, a, i, &x1, &y1);
                x = bx + N / 2.0f * cs;
                y = by + N / 2.0f * cs;
                piece_at(a, x + (x1 - x) * g, y + (y1 - y) * g, 1);
            } else {
                piece_px(p, a, i, &x, &y);
                piece_at(a, x, y, 1);
                for (int e = !owner(a); e < PA_ARMIES; e += 2)
                    for (int j = 0; j < PA_PIECES; j++) {
                        if (anim.before.at[e][j] == p->at[e][j]) continue;
                        piece_px(&anim.before, e, j, &x, &y);
                        piece_px(p, e, j, &x1, &y1);
                        piece_at(e, x + (x1 - x) * g, y + (y1 - y) * g, 1 - (1 - WAIT / PIECE) * g);
                    }
            }
        }
    }
    if (drag.on && drag.moved) {                /* the dragged piece, a little bigger, above the finger */
        float off = (ui_upside(me) ? -0.6f : 0.6f) * cs;
        piece_at(army_of(drag.piece), drag.x, drag.y - off, 1.2f);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the mover's piece that can go, nearest (x, y): its first alike (choices()); -1 */
static int piece_near(float x, float y)
{
    const ppos *p = &G.pos;
    int moves = pa_movable(p), best = -1;
    float near = 0.85f * cs;
    for (int i = 0; i < 2 * PA_PIECES; i++) {
        if (!(moves >> i & 1)) continue;
        int a = army_of(i);
        float px, py;
        piece_px(p, a, i % PA_PIECES, &px, &py);
        float d = hypotf(x - px, y - py) / (p->at[a][i % PA_PIECES] ? 1 : 0.8f);
        if (d >= near) continue;
        near = d;
        best = first_alike(moves, i);
    }
    return best;
}

static int target_near(float x, float y)
{
    int ch = choices(), best = -1;
    float near = 0.75f * cs;
    for (int i = 0; i < 2 * PA_PIECES; i++) {
        if (!(ch >> i & 1) || (pick >= 0 && square_of(i) != pick)) continue;
        float px, py;
        target_px(i, &px, &py);
        float d = hypotf(x - px, y - py);
        if (d < near) near = d, best = i;
    }
    return best;
}

static void press(float x, float y)
{
    const ppos *p = &G.pos;
    if (p->phase == PA_OVER || busy()) return;
    if (p->phase == PA_THROW) {                 /* a tap anywhere on the board's square throws */
        if (x >= sq.x && y >= sq.y && x < sq.x + sq.w && y < sq.y + sq.h) throw_shells();
        return;
    }
    int t = pick >= 0 ? target_near(x, y) : -1;
    if (t >= 0) { play(t, 1); return; }
    int i = piece_near(x, y);
    if (i >= 0) {
        int sq_ = square_of(i), ways = 0, ch = choices();
        for (int j = 0; j < 2 * PA_PIECES; j++) ways += (ch >> j & 1) && sq_ >= 0 && square_of(j) == sq_;
        if (ways > 1) {                         /* both armies there, going different ways: choose a target */
            pick = sq_;
            ui_sound(SND_TAP);
            return;
        }
        pick = -1;
        drag.on = 1;
        drag.piece = i;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
        return;
    }
    t = target_near(x, y);
    if (t >= 0) play(t, 1);
    else {
        pick = -1;
        ui_sound(SND_DENY);
    }
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > cs * 0.3f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) { play(drag.piece, 1); return; }   /* a tap: the piece goes, hopping */
    float px, py;
    target_px(drag.piece, &px, &py);
    if (hypotf(x - px, y - py) < 1.2f * cs) play(drag.piece, 0);
    else ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)               /* the player's pieces home, each in its army's colour */
{
    float sz = 0.6f * ui.unit, step = fminf(0.42f * ui.unit, (b.w - sz) / 7), cy = b.y + b.h / 2;
    int k = 0;
    for (int a = player; a < PA_ARMIES; a += four() ? PA_ARMIES : 2)   /* four: its own army */
        for (int i = 0; i < PA_PIECES; i++)
            if (G.pos.at[a][i] == PA_HOME) ui_put(t_small[a], b.x + k++ * step, cy - sz / 2, sz, sz);
}

/* the players who share the win: four, both partners; two, the winner */
static unsigned winners(void)
{
    int w = G.pos.winner;
    if (w < 0) return 0;
    return four() ? 1u << w | 1u << (w + 2) % PA_ARMIES : 1u << w;
}

/* a player's token: two players, ivory and ebony; four, its army's piece (ivory, ebony, amber, rosewood) */
static void token(int player, int size, uint8_t *rgba) { paint_piece(player, size, rgba); }

/* the menu's card: the charkoni and the corner of two arms, a few pieces, cowries in the tray */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    int c = (int)(size / 11.4f), f = (int)(c * FRAME), n = N * c + 2 * f, o0 = f + (int)(7.5f * c);
    uint8_t *o = malloc((size_t)n * n * 4);
    if (!o) return;
    paint_board(o, c, f);
    int w = n - o0 < size ? n - o0 : size;
    for (int y = 0; y < w; y++) memmove(o + 4 * (size_t)y * w, o + 4 * ((size_t)(y + o0) * n + o0), 4 * (size_t)w);
    art_blend(rgba, size, size, o, w, w, 0, 0);
    free(o);
    int s = (int)(PIECE * c), ss = (int)(SHELL * c);
    uint8_t *one = malloc((size_t)ss * ss * 4 + (size_t)s * s * 4);
    if (!one) return;
    static const int put[][3] = {{0, 3, 0}, {0, 10, 0}, {1, 4, 0}, {3, 37, 0}, {2, 0, 0}, {2, 0, 2}, {0, 0, 1}};
    for (unsigned i = 0; i < sizeof put / sizeof put[0]; i++) {
        int cx, cy, a = put[i][0], at = put[i][1];
        float px, py;
        if (at) {
            pa_cell(pa_square(a, at), &cx, &cy);
            px = f + (cx + 0.5f) * c - o0, py = f + (cy + 0.5f) * c - o0;
        } else {
            float dx, dy;
            wait_at(put[i][2], &dx, &dy);
            px = f + (9.5f + dx) * c - o0, py = f + (9.5f + dy) * c - o0;
        }
        int d = at ? s : (int)(WAIT * c);
        art_flat_shadow(d, one);
        art_blend(rgba, size, size, one, d, d, (int)(px - d / 2.0f) + d / 20, (int)(py - d / 2.0f) + d * 8 / 100);
        paint_piece(a, d, one);
        art_blend(rgba, size, size, one, d, d, (int)(px - d / 2.0f), (int)(py - d / 2.0f));
    }
    for (int i = 0; i < 5; i++) {               /* cowries in the tray, mouths up and down */
        float du, dv, deg;
        shell_rest(3, i, 0, &du, &dv, &deg);
        paint_shell(i == 1 || i == 3, ss, one);
        int cx = (int)(f + (15 + 0.8f * du) * c - o0), cy = (int)(f + (15 + 0.8f * dv) * c - o0);
        art_blend(rgba, size, size, one, ss, ss, cx - ss / 2, cy - ss / 2);
    }
    free(one);
}

static const game pachisi_game = {
    .id = "pachisi", .order = 145, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .can_pass = can_pass, .pass = pass, .script = script, .busy = busy,
    .players = 1u << 2 | 1u << 4, .home = home, .winners = winners,
};
REGISTER_GAME(pachisi_game)
