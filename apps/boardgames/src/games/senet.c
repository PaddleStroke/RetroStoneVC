/*
 * Senet in RetroStone Board: the board of thirty inlaid squares and its houses, the pieces (the house's discs), four
 * throwing sticks thrown with a tap, the pieces hopping along the snake path (the rules and the notation:
 * senet_rules.c, senet.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board lies across the room the shell gives, square 1 at its top left (seen by player 0), the path running along
 * the top row, back along the middle one and out along the bottom one. Beyond each long side, next to its player:
 * their four sticks and the throw's count on the left, the pieces they have borne off on the right. The board turns
 * round when flipped.
 * The houses are inlaid in darker wood with pale signs: 15 an ankh (rebirth), 26 a nefer (beauty), 27 waves (water),
 * 28, 29 and 30 three, two and one strokes (the exact throw that takes a piece off).
 * The sticks: flat on one side (pale, incised), round on the other (dark walnut): a pale side up counts one.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/senet.h"
#include "shell.h"

#define COLS 10
#define FRAME 0.35f                             /* the board's frame, in cells */
#define WIDE (COLS + 2 * FRAME)
#define PIECE 0.98f                             /* a piece's picture, in cells */
#define RACK 0.8f                               /* the pieces borne off, a little smaller */
#define STICK_MIN 1.0f                          /* a stick's length, in cells */
#define STICK_MAX 1.7f
#define STICK_W 0.2f                            /* its width, a part of its length */
#define HOP_MS 120
#define SWAP_MS 300                             /* a piece swapped slides to the mover's square */
#define WATER_MS 420                            /* a piece in the water slides back to 15 */

static senet_game G;
static int cs, fr, made = -1, made_stick = -1, made_small = -1, sl, sw;   /* sl x sw: a stick's picture */
static float bx, by, row_v = 1.4f, stick_l = STICK_MIN;   /* row_v: the sticks' and the rack's row beyond the side */
static box sq;
static SDL_Texture *t_board, *t_piece[2], *t_small[2], *t_stick[2];
static uint8_t rolls[SENET_MAX_EVENTS];         /* each throw's sticks (bit i: stick i's flat side up), kept through
                                                 * undo (undo never rethrows), a ring as the rules' history */
static int nrolls;
static struct { int on, k; Uint32 t0; } roll;
static int pass_snd;
static struct { int on, from, moved; float x, y; } drag;
static struct { int on, player, from, to, land, hops, dir, heard, swap; Uint32 t0; senet_pos before; } anim;

static const float MAPLE[3] = {1.07f, 1.1f, 1.18f}, HOUSE[3] = {0.66f, 0.5f, 0.4f}, WATER[3] = {0.55f, 0.7f, 0.92f};

/* ---- where things are -------------------------------------------------------------------------------------------- */
/* beyond a player's side of the board, u cells from the left as that player reads it: pixels */
static void side_px(int player, float u, float *x, float *y)
{
    if (ui_upside(player)) u = COLS - u;
    *x = bx + u * cs;
    *y = by + (player == ui_bottom() ? 3 + row_v : -row_v) * cs;
}

/* a square's centre (squares 1..30, turned round when flipped), or slot k of the player's rack (31: borne off, at
 * the right of their side) */
static void place_px(int player, int square, int k, float *x, float *y)
{
    int c, r;
    if (!senet_cell(square, &c, &r)) {
        float step = fminf(0.7f, 4.4f / (G.pos.pieces ? G.pos.pieces : 5));
        side_px(player, 9.5f - step * k, x, y);
        return;
    }
    float u = c + 0.5f, v = r + 0.5f;
    if (ui.flipped) u = COLS - u, v = 3 - v;
    *x = bx + u * cs;
    *y = by + v * cs;
}

/* a player's stick i (4: the count after them), at the left of their side */
static void stick_px(int player, int i, float *x, float *y)
{
    float gap = fmaxf(0.42f, 0.42f * stick_l);
    side_px(player, 0.6f + gap * i + (i == 4 ? 0.25f : 0), x, y);
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void) { anim.on = drag.on = pass_snd = roll.on = 0; }

static void start(int variant)
{
    senet_new(&G, variant);
    nrolls = 0;
    calm();
}

static int status(void) { return G.pos.winner < 0 ? GS_PLAY : G.pos.winner ? GS_WIN1 : GS_WIN0; }
static int turn(void) { return G.pos.turn; }
static int tumbling(void) { return roll.on && ui_ms() - roll.t0 < UI_DICE_MS; }
static int busy(void) { return anim.on || tumbling(); }

static int value_of(int m) { int n = __builtin_popcount((unsigned)m); return n ? n : 5; }

/* the sticks of a throw given by its value only (a script): one way to throw it, always the same */
static int mask_of(int k, int value)
{
    static const uint8_t way[6][6] = {{0}, {1, 4, 2, 8}, {5, 10, 3, 12, 6, 9}, {7, 13, 11, 14}, {15}, {0}};
    static const int ways[6] = {1, 4, 6, 4, 1, 1};
    return way[value][k % ways[value]];
}

static void throw_sticks(void)
{
    int k = G.pos.throws, m = 0;
    if (k < nrolls) m = rolls[k % SENET_MAX_EVENTS];
    else {
        for (int i = 0; i < 4; i++) m |= ui_random(2) << i;
        rolls[k % SENET_MAX_EVENTS] = (uint8_t)m;
        nrolls = k + 1;
    }
    int r = senet_throw(&G, value_of(m));
    if (!r) return;
    roll.on = 1;
    roll.k = k;
    roll.t0 = ui_ms();
    pass_snd = r == SENET_T_PASS;
    ui_sound(SND_DICE);
    ui_animate();
}

static void play(int from, int animate)
{
    senet_pos before = G.pos;
    int to = senet_dest(&before, from), r = senet_move(&G, from);
    if (!r) { ui_sound(SND_DENY); return; }
    anim.on = animate;
    anim.player = before.turn;
    anim.from = from;
    anim.to = to;
    anim.land = to == SENET_OFF ? to : senet_landing(&before, to);
    anim.dir = to > from ? 1 : -1;
    anim.hops = abs(to - from);
    anim.heard = 0;
    anim.swap = r > 1;
    anim.t0 = ui_ms();
    anim.before = before;
    drag.on = 0;
    if (!animate) ui_sound(r > 1 ? SND_CAPTURE : to == SENET_OFF ? SND_PLACE : SND_MOVE);
    ui_animate();
}

static int undo(void)
{
    if (!senet_undo(&G)) return 0;
    calm();
    return 1;
}

static int script(const char *s)
{
    int ok = senet_script(&G, s);
    for (int i = 0; i < G.n; i++)
        if (G.ev[i] < SENET_MOVE_EV)
            rolls[G.past[i].throws % SENET_MAX_EVENTS] = (uint8_t)mask_of(G.past[i].throws, G.ev[i]);
    nrolls = G.pos.throws;
    calm();
    return ok;
}

/* ---- the bot (senet_bot.c) --------------------------------------------------------------------------------------- */
const senet_game *senet_live(void) { return &G; }

int senet_show(const char *move)
{
    if (busy()) return 0;
    if (!strcmp(move, "roll")) {
        if (G.pos.phase != SENET_THROW) return 0;
        throw_sticks();
        return 1;
    }
    char *end;
    long from = move[0] == 's' ? strtol(move + 1, &end, 10) : -1;
    if (from < 1 || *end || senet_dest(&G.pos, (int)from) < 0) return 0;
    play((int)from, 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a house's sign, pale ink, in a square w wide at (x, y) of the picture */
static void paint_sign(uint8_t *o, int pw, int ph, int square, int x, int y, int w)
{
    art_fig *f = art_fig_new();
    float lw = 0.075f;
    switch (square) {
    case SENET_REBIRTH:                         /* the ankh: a loop over a crossbar and a stem */
        art_arc(f, ART_PART, 0.5f, 0.3f, 0.1f, 0.1f + lw, 0, 360);
        art_band(f, ART_PART, 0.27f, 0.46f, 0.73f, 0.46f, lw);
        art_band(f, ART_PART, 0.5f, 0.42f, 0.5f, 0.84f, lw * 1.15f);
        break;
    case SENET_BEAUTY: {                        /* the nefer: a heart, a tall windpipe, its crossbar */
        float heart[] = {0.5f, 0.86f, 0.33f, 0.72f, 0.3f, 0.6f, 0.36f, 0.5f, 0.5f, 0.47f, 0.64f, 0.5f, 0.7f, 0.6f,
                         0.67f, 0.72f};
        art_poly(f, ART_PART, heart, 8);
        art_band(f, ART_PART, 0.5f, 0.14f, 0.5f, 0.5f, lw);
        art_band(f, ART_PART, 0.38f, 0.27f, 0.62f, 0.27f, lw);
        break;
    }
    case SENET_WATER:                           /* three rows of waves */
        for (int r = 0; r < 3; r++)
            for (int i = 0; i < 6; i++) {
                float x0 = 0.17f + i * 0.11f, y0 = 0.3f + r * 0.2f + (i & 1 ? -0.05f : 0.05f);
                art_band(f, ART_PART, x0, y0, x0 + 0.11f, 0.3f + r * 0.2f + (i & 1 ? 0.05f : -0.05f), lw * 0.8f);
            }
        break;
    default: {                                  /* 28, 29, 30: three, two, one strokes */
        int n = SENET_OFF - square;
        for (int i = 0; i < n; i++) {
            float cx = 0.5f + (i - (n - 1) / 2.0f) * 0.2f;
            art_band(f, ART_PART, cx, 0.24f, cx, 0.76f, lw * 1.2f);
        }
    }
    }
    art_style st = art_flat(ART_PALE, ART_PALE, 0, 0);
    art_fig_blend(o, pw, ph, f, &st, x, y, w, 0.85f);
    art_fig_free(f);
}

static int house(int square) { return square == SENET_REBIRTH || square >= SENET_BEAUTY; }

/* the board for cells of c pixels in a frame of f: (10 c + 2 f) x (3 c + 2 f): the house plank and frame, the thirty
 * squares inlaid, the houses in darker wood (the water bluish) with their signs; a faint dot in each plain square's
 * middle, the path's arrows at the row ends in the frame */
static void paint_board(uint8_t *o, int c, int f)
{
    int pw = COLS * c + 2 * f, ph = 3 * c + 2 * f;
    art_board_n(COLS, 3, c, f, 0, o);
    float m = fmaxf(1, 0.045f * c), seam = fmaxf(1, 0.05f * c);
    for (int s = 1; s <= SENET_SQ; s++) {
        int col, row;
        senet_cell(s, &col, &row);
        float x = f + col * c + m, y = f + row * c + m, w = c - 2 * m;
        const float *tone = s == SENET_WATER ? WATER : house(s) ? HOUSE : MAPLE;
        art_inlay(o, pw, ph, x, y, w, w, 0.08f * c, tone, seam);
        if (house(s)) paint_sign(o, pw, ph, s, (int)x, (int)y, (int)w);
    }
    art_fig *a = art_fig_new();                 /* the path's way: small heads in the frame (in, the turns, out) */
    float W = (float)pw, hl = 0.55f * f / W, xr = (f + COLS * c + f / 2.0f) / W, xl = f / 2.0f / W;
    art_head(a, ART_PART, xl, (f + 0.5f * c) / W, 0, hl);
    art_head(a, ART_PART, xr, (f + 1.0f * c) / W, 90, hl);
    art_head(a, ART_PART, xl, (f + 2.0f * c) / W, 90, hl);
    art_head(a, ART_PART, xr, (f + 2.5f * c) / W, 0, hl);
    art_style st = art_flat(ART_PALE, ART_PALE, 0, 0);
    art_fig_blend(o, pw, ph, a, &st, 0, 0, pw, 0.55f);
    art_fig_free(a);
}

/* a stick seen from above, lying along its length (vertical in the picture, w x h): its flat pale side up (incised
 * chevrons at its ends, a groove along it) or its round dark side (lit across like a rod) */
static void paint_stick(int flat, int w, int h, uint8_t *o)
{
    static const float pale[3] = {236, 214, 172}, dark[3] = {96, 62, 40};
    memset(o, 0, (size_t)w * h * 4);
    float r = w / 2.0f;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float dx = x + 0.5f - r, ey = y + 0.5f < r ? r - (y + 0.5f) : y + 0.5f > h - r ? y + 0.5f - (h - r) : 0;
            float d = hypotf(dx, ey), a = clampf(r - d + 0.5f, 0, 1);
            if (a <= 0) continue;
            float t = dx / r, k;
            if (flat) {
                k = 1 - 0.18f * (d / r) * (d / r);
                float g = art_noise(x * 3, y / 2) * 0.1f;
                k += g;
                float yy = (float)y / h, cv = fabsf(t) * 0.12f, cut = 0.012f + 0.4f / h;
                if (fabsf(fabsf(yy - 0.12f) - cv) < cut || fabsf(fabsf(yy - 0.88f) - cv) < cut)
                    k *= 0.55f;                 /* the chevrons */
                if (fabsf(t) < 0.1f && yy > 0.25f && yy < 0.75f) k *= 0.8f;   /* the groove */
            } else {
                k = 0.75f + 0.7f * expf(-(t + 0.35f) * (t + 0.35f) * 6) - 0.3f * t * t;
                k += art_noise(x, y / 3) * 0.12f;
            }
            if (d > r - 1.2f) k *= 0.6f;        /* the edge */
            const float *c = flat ? pale : dark;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(c[i] * k, 0, 255);
            p[3] = (uint8_t)(255 * a);
        }
}

static void layout(box b)
{
    sq = b;
    /* the board as wide as it may be, a row beyond each long side for the sticks and the rack */
    float c = fminf(b.w / WIDE, b.h / (3 + 2 * (FRAME + 0.3f + STICK_MIN + 0.1f)));
    cs = (int)floorf(c);
    float side = (b.h / cs - 3) / 2;           /* the room beyond each long side, cells */
    stick_l = clampf(side - FRAME - 0.5f, STICK_MIN, STICK_MAX);
    row_v = FRAME + 0.3f + stick_l / 2 + fminf(0.4f, (side - FRAME - 0.5f - stick_l) / 3);
    fr = (int)(cs * FRAME);
    bx = floorf(b.x + (b.w - (float)COLS * cs) / 2);
    by = floorf(b.y + (b.h - 3.0f * cs) / 2);
    int l = (int)(stick_l * cs), w = (int)(STICK_W * l);
    if (made_stick != l) {
        made_stick = l;
        sl = l;
        sw = w < 4 ? 4 : w;
        for (int k = 0; k < 2; k++) {
            ui_free(&t_stick[k]);
            paint_stick(k, sw, sl, ui_buf(sw, sl));
            t_stick[k] = ui_tex(sw, sl, ui_buf(sw, sl));
        }
    }
    if (made != cs) {
        made = cs;
        int pw = COLS * cs + 2 * fr, ph = 3 * cs + 2 * fr;
        uint8_t *o = malloc((size_t)pw * ph * 4);
        if (o) {
            paint_board(o, cs, fr);
            ui_free(&t_board);
            t_board = ui_tex(pw, ph, o);
            free(o);
        }
        int ps = (int)(PIECE * cs);
        for (int p = 0; p < 2; p++) {
            ui_free(&t_piece[p]);
            art_counter(COUNTER_DISC, p, ps, ui_buf(ps, ps));
            t_piece[p] = ui_tex(ps, ps, ui_buf(ps, ps));
        }
    }
    int small = (int)(0.6f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int p = 0; p < 2; p++) {
            ui_free(&t_small[p]);
            art_counter(COUNTER_DISC, p, small, ui_buf(small, small));
            t_small[p] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void piece_at(int player, float x, float y, float scale)
{
    ui_flat_at(t_piece[player], x, y, PIECE * cs * scale, 1, ui_upside(player));
}

static int last_throw(int player)
{
    for (int i = G.n - 1; i >= 0; i--)
        if (G.ev[i] < SENET_MOVE_EV && G.past[i].turn == player) return i;
    return -1;
}

/* a player's four sticks: waiting to be thrown (in gold light), tumbling, the throw to play, or the last one
 * (dimmed; in a red glow when it was lost); the count after them */
static void draw_sticks(int q)
{
    const senet_pos *p = &G.pos;
    int e = last_throw(q), k = e >= 0 ? G.past[e].throws : -1, rolling = tumbling() && roll.k == k && k >= 0;
    int active = p->phase != SENET_OVER && p->turn == q, invite = active && p->phase == SENET_THROW && !tumbling();
    if (e < 0 && !invite) return;
    int mask = e >= 0 ? rolls[k % SENET_MAX_EVENTS] : 0, lost = e == G.n - 1 && p->turn != q && !rolling;
    float alpha = active || rolling ? 1 : 0.45f, x, y, x2, y2;
    if (invite && e >= 0) alpha = 0.8f;
    float rot = ui_upside(q) ? 180 : 0;
    if (lost) {
        stick_px(q, 0, &x, &y);
        stick_px(q, 3, &x2, &y2);
        ui_mark_glow((x + x2) / 2, y, 1.9f * stick_l * cs);
    }
    if (invite) {
        stick_px(q, 0, &x, &y);
        stick_px(q, 3, &x2, &y2);
        ui_mark_gold((x + x2) / 2, y, 1.7f * stick_l * cs);
    }
    Uint32 t = ui_ms() - roll.t0;
    for (int i = 0; i < 4; i++) {
        stick_px(q, i, &x, &y);
        int flat = mask >> i & 1;
        float tilt = (art_hash(k, i) - 0.5f) * 14, turn_by = 0, lift = 0;
        if (rolling) {
            ui_die_tumble((int)t, i, &turn_by, &lift);
            turn_by *= 0.25f;
            flat = (int)((t / 60 + (unsigned)i * 3 + (unsigned)mask) % 2);
        }
        ui_shadow_flat(x, y + 0.04f * sl, (float)sw * 1.2f, (float)sl, alpha);
        SDL_Texture *s = t_stick[flat];
        SDL_SetTextureAlphaMod(s, (Uint8)(255 * alpha));
        ui_put_rot(s, x, y - lift * sl * 0.5f, (float)sw, (float)sl, tilt + turn_by + rot);
        SDL_SetTextureAlphaMod(s, 255);
    }
    if (rolling) ui_animate();
    else if (e >= 0) {
        stick_px(q, 4, &x, &y);
        ui_number_rot(x, y, 0.62f * cs, G.ev[e], 1, rot);
    }
}

static void mark_last(void)
{
    for (int i = G.n - 1; i >= 0; i--) {
        if (G.ev[i] < SENET_MOVE_EV) continue;
        const senet_pos *b = &G.past[i];
        int from = G.ev[i] - SENET_MOVE_EV, to = senet_dest(b, from), ends[2] = {from, to};
        for (int k = 0; k < 2; k++) {
            if (ends[k] < 1 || ends[k] > SENET_SQ) continue;
            float x, y;
            place_px(b->turn, ends[k], 0, &x, &y);
            ui_mark_last(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
        }
        return;
    }
}

static void draw_targets(uint32_t moves, int rings)
{
    const senet_pos *p = &G.pos;
    for (int f = 1; f <= SENET_SQ; f++) {
        if (!(moves >> f & 1)) continue;
        if (drag.on && !drag.moved && drag.from != f) continue;
        int to = senet_dest(p, f), hit = senet_takes(p, f);
        float x, y;
        place_px(p->turn, to, p->off[p->turn], &x, &y);
        if (hit && rings) ui_mark_ring(x, y, 1.18f * cs);
        else if (!hit && !rings) ui_mark_dot(x, y, 0.36f * cs);
    }
}

/* the pieces of a position; skip: a square of the mover's not drawn (moving); hide: one of the other's (swapped) */
static void draw_pieces(const senet_pos *p, int mover, int skip, int hide)
{
    for (int pl = 0; pl < 2; pl++)
        for (int k = 0; k < p->off[pl]; k++) {
            float x, y;
            place_px(pl, SENET_OFF, k, &x, &y);
            piece_at(pl, x, y, RACK);
        }
    for (int s = 1; s <= SENET_SQ; s++) {
        int pl = p->sq[s] - 1;
        if (pl < 0 || (pl == mover && s == skip) || (pl != mover && s == hide)) continue;
        float x, y;
        place_px(pl, s, 0, &x, &y);
        piece_at(pl, x, y, 1);
    }
}

static void draw(void)
{
    const senet_pos *p = &G.pos;
    float pw = (float)COLS * cs + 2 * fr, ph = 3.0f * cs + 2 * fr, x, y, t = 0;
    ui_put_rot(t_board, bx - fr + pw / 2, by - fr + ph / 2, pw, ph, ui.flipped ? 180 : 0);
    float water = anim.on && anim.land != anim.to ? (float)WATER_MS / HOP_MS : 0;
    float swap = anim.on && anim.swap ? (float)SWAP_MS / HOP_MS : 0;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)HOP_MS;
        int landed = t >= anim.hops ? anim.hops : (int)t;
        while (anim.heard < landed) {
            int last = ++anim.heard == anim.hops;
            ui_sound(last && anim.swap ? SND_CAPTURE : last && anim.to == SENET_OFF ? SND_PLACE : SND_MOVE);
        }
        if (t >= anim.hops + fmaxf(water, swap)) anim.on = 0;
        else ui_animate();
    }
    mark_last();
    draw_sticks(0);
    draw_sticks(1);
    if (pass_snd && !tumbling()) {
        pass_snd = 0;
        ui_sound(SND_DENY);
    }
    uint32_t moves = p->phase == SENET_MOVE && !busy() ? senet_movable(p) : 0;
    int me = p->turn, one = __builtin_popcount(moves) == 1;
    int sel = drag.on && !drag.moved ? drag.from : one ? __builtin_ctz(moves) : -1;
    if (sel >= 1 && moves) {
        place_px(me, sel, 0, &x, &y);
        ui_mark_select(x - cs / 2.0f, y - cs / 2.0f, (float)cs, (float)cs);
    }
    draw_targets(moves, 0);
    if (anim.on) draw_pieces(&anim.before, anim.player, anim.from, anim.swap ? anim.to : -1);
    else draw_pieces(p, me, drag.on && drag.moved ? drag.from : -1, -1);
    draw_targets(moves, 1);
    if (anim.on && anim.swap) {                 /* the piece swapped: still, then sliding to the mover's square */
        float f = t < anim.hops ? 0 : ui_ease(clampf((t - anim.hops) * HOP_MS / SWAP_MS, 0, 1)), x1, y1;
        int other = !anim.player;
        place_px(other, anim.to, 0, &x, &y);
        place_px(other, anim.from, 0, &x1, &y1);
        piece_at(other, x + (x1 - x) * f, y + (y1 - y) * f, 1);
    }
    if (anim.on) {                              /* the piece hopping square by square (then from the water to 15) */
        int h = (int)t;
        float f = t - h, x1, y1;
        if (h >= anim.hops) {
            place_px(anim.player, anim.to, anim.before.off[anim.player], &x, &y);
            if (anim.land != anim.to) {
                float e = ui_ease(clampf((t - anim.hops) * HOP_MS / WATER_MS, 0, 1));
                place_px(anim.player, anim.land, 0, &x1, &y1);
                x += (x1 - x) * e;
                y += (y1 - y) * e;
            }
            piece_at(anim.player, x, y, anim.to == SENET_OFF ? RACK : 1);
        } else {
            int a = anim.from + anim.dir * h, b2 = a + anim.dir, k = anim.before.off[anim.player];
            place_px(anim.player, a, k, &x, &y);
            place_px(anim.player, b2, k, &x1, &y1);
            float e = ui_ease(f), up = (ui_upside(anim.player) ? -1 : 1) * ui_hop(f) * cs;
            float sc = b2 == SENET_OFF ? 1 - (1 - RACK) * e : 1;
            piece_at(anim.player, x + (x1 - x) * e, y + (y1 - y) * e - up, sc * (1 + 0.1f * sinf(ART_PI * f)));
        }
    }
    if (drag.on && drag.moved) {
        float off = (ui_upside(me) ? -0.5f : 0.5f) * cs;
        piece_at(me, drag.x, drag.y - off, 1.15f);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
/* the square of the player to move's piece that can go, under (x, y): 1, or its target there: 2; else 0 */
static int hit(float x, float y, int *from)
{
    const senet_pos *p = &G.pos;
    uint32_t moves = senet_movable(p);
    int best = -1;
    float near = 0.62f * cs, px, py, d;
    for (int f = 1; f <= SENET_SQ; f++) {
        if (!(moves >> f & 1)) continue;
        place_px(p->turn, f, 0, &px, &py);
        d = hypotf(x - px, y - py);
        if (d < near) near = d, best = f;
    }
    if (best > 0) {
        *from = best;
        return 1;
    }
    for (int f = 1; f <= SENET_SQ; f++) {
        if (!(moves >> f & 1)) continue;
        place_px(p->turn, senet_dest(p, f), p->off[p->turn], &px, &py);
        d = hypotf(x - px, y - py);
        if (d < near) near = d, best = f;
    }
    if (best < 0) return 0;
    *from = best;
    return 2;
}

static void press(float x, float y)
{
    const senet_pos *p = &G.pos;
    if (p->phase == SENET_OVER || busy()) return;
    if (p->phase == SENET_THROW) {              /* a tap anywhere in the room throws */
        if (x >= sq.x && y >= sq.y && x < sq.x + sq.w && y < sq.y + sq.h) throw_sticks();
        return;
    }
    int from, h = hit(x, y, &from);
    if (h == 1) {
        drag.on = 1;
        drag.from = from;
        drag.moved = 0;
        drag.x = x;
        drag.y = y;
    } else if (h == 2) {
        play(from, 1);
    } else {
        ui_sound(SND_DENY);
    }
}

static void motion(float x, float y)
{
    if (!drag.on) return;
    if (hypotf(x - drag.x, y - drag.y) > cs * 0.25f) drag.moved = 1;
    drag.x = x;
    drag.y = y;
    ui_animate();
}

static void release(float x, float y)
{
    if (!drag.on) return;
    drag.on = 0;
    if (!drag.moved) { play(drag.from, 1); return; }
    const senet_pos *p = &G.pos;
    float px, py;
    place_px(p->turn, senet_dest(p, drag.from), p->off[p->turn], &px, &py);
    if (hypotf(x - px, y - py) < 0.8f * cs) play(drag.from, 0);
    else ui_sound(SND_DENY);
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the pieces the player has borne off */
{
    int n = G.pos.off[player];
    float sz = 0.6f * ui.unit, step = fminf(0.42f * ui.unit, (b.w - sz) / (SENET_MAX_PIECES - 1)), cy = b.y + b.h / 2;
    for (int i = 0; i < n; i++) ui_put(t_small[player], b.x + i * step, cy - sz / 2, sz, sz);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card (the game under way) and the picker's (the start, five or seven pieces each): the board, the
 * pieces, four sticks under it */
static void thumb(int variant, int size, uint8_t *rgba)
{
    int c = (int)(size / (WIDE + 0.2f)), f = (int)(c * FRAME), pw = COLS * c + 2 * f, ph = 3 * c + 2 * f;
    int sl2 = (int)(1.9f * c), sw2 = (int)(STICK_W * sl2) < 3 ? 3 : (int)(STICK_W * sl2);
    int ox = (size - pw) / 2, oy = (size - ph - sl2 - c / 2) / 2;
    uint8_t *o = malloc((size_t)pw * ph * 4);
    if (!o) return;
    paint_board(o, c, f);
    art_blend(rgba, size, size, o, pw, ph, ox, oy);
    free(o);
    static const uint8_t mid[] = {3, 1, 6, 0, 7, 1, 8, 0, 12, 1, 17, 0, 18, 0, 22, 1, 24, 1, 26, 0};
    int s = (int)(PIECE * c);
    uint8_t *one = malloc((size_t)(s > sl2 ? s : sl2) * (s > sl2 ? s : sl2) * 4);
    if (!one) return;
    int n = variant < 0 ? (int)sizeof mid / 2 : 2 * (variant == SENET_SEVEN ? 7 : 5);
    for (int i = 0; i < n; i++) {
        int sqr = variant < 0 ? mid[2 * i] : i + 1, pl = variant < 0 ? mid[2 * i + 1] : !((i + 1) & 1), col, row;
        senet_cell(sqr, &col, &row);
        art_counter(COUNTER_DISC, pl, s, one);
        art_flat_put(rgba, size, size, one, s, ox + f + (col + 0.5f) * c, oy + f + (row + 0.5f) * c);
    }
    for (int i = 0; i < 4; i++) {
        paint_stick(i == 1 || i == 2 || (variant < 0 && i == 0), sw2, sl2, one);
        art_blend(rgba, size, size, one, sw2, sl2, size / 2 - sw2 / 2 + (int)((i - 1.5f) * 0.75f * c),
                  oy + ph + c / 4);
    }
    free(one);
}

static const game senet_game_def = {
    .id = "senet", .order = 136, .variants = SENET_VARIANTS,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(senet_game_def)
