/*
 * Twist Five in RetroStone Board: four maple plates of 3 x 3 holes on a walnut tray, the marbles (the house's ivory
 * and ebony stones), a quarter turned after each marble (the rules and the notation: twistfive_rules.c,
 * twistfive.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A turn: tap an empty hole (the marble drops in), then one of the round arrows on the tray's rim, two by each
 * plate's outer corner, one each way: the plate lifts a little and turns a quarter with its marbles. The plates keep
 * how far they have turned (their grain turns with them); the holes' light stays from the upper left. The row of five
 * lights up in gold.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/twistfive.h"
#include "shell.h"

#define RIM 0.66f                               /* the tray's rim, in holes (the arrows sit on it) */
#define MARGIN 0.09f                            /* a plate's edge round its holes */
#define GAP 0.14f                               /* between the plates */
#define MARBLE 0.8f
#define TURN_MS 420
#define DROP_MS 130

static tvgame G;
static int cell, rim, made = -1;
static float bx, by, plate, inner;              /* the plates' area's top left, a plate's side, the area's side */
static SDL_Texture *t_base, *t_plate, *t_hole, *t_marble[2];
static struct { int on, kind, hole, q, dir; Uint32 t0; } anim;   /* kind: 0 a marble dropping, 1 a quarter turning */
static int pending = -1;                        /* a bot's turn: the twist (q * 2 + dir) once its marble has dropped */

static const tvpos *now(void) { return tv_pos(&G); }

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void calm(void)
{
    anim.on = 0;
    pending = -1;
}

static void start(int variant)
{
    (void)variant;
    tv_new(&G);
    calm();
}

static int status(void)
{
    int w = now()->winner;
    return w < 0 ? GS_PLAY : w == TV_DRAW ? GS_DRAW : w ? GS_WIN1 : GS_WIN0;
}

static int turn(void) { return now()->side; }

static void place(int h)
{
    if (!tv_play_place(&G, h)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.kind = 0;
    anim.hole = h;
    anim.t0 = ui_ms();
    ui_sound(SND_PLACE);
}

static void twist(int q, int dir)
{
    if (!tv_play_twist(&G, q, dir)) { ui_sound(SND_DENY); return; }
    anim.on = 1;
    anim.kind = 1;
    anim.q = q;
    anim.dir = dir;
    anim.t0 = ui_ms();
    ui_sound(SND_MOVE);
}

static int undo(void)
{
    calm();
    return tv_undo(&G);
}

static int script(const char *s)
{
    calm();
    return tv_script(&G, s);
}

static int busy(void) { return anim.on || pending >= 0; }

/* the bot's side (twistfive_bot.c): the game, read-only; its turn played as taps play it: "c4/2+" drops the marble,
 * then turns the quarter once it is down; "c4" or "/2+" a step alone */
const tvgame *twistfive_live(void) { return &G; }

int twistfive_show(const char *s)
{
    const tvpos *p = now();
    if (p->winner >= 0 || busy()) return 0;
    int h = -1, q = -1, dir = 0;
    if (*s != '/') {
        if ((h = tv_square(s)) < 0) return 0;
        s += 2;
    }
    if (*s == '/') {
        if (s[1] < '1' || s[1] > '4' || (s[2] != '+' && s[2] != '-')) return 0;
        q = s[1] - '1';
        dir = s[2] == '+' ? TV_CW : TV_CCW;
        s += 3;
    }
    if (*s || (h < 0 && q < 0)) return 0;
    tvgame t;                                   /* the whole turn checked before anything moves */
    t.ply = 0;
    t.pos[0] = *p;
    if (h >= 0 && !tv_play_place(&t, h)) return 0;
    if (q >= 0 && !tv_play_twist(&t, q, dir)) return 0;
    if (h >= 0) {
        place(h);
        if (q >= 0) pending = q * 2 + dir;
    } else twist(q, dir);
    return 1;
}

/* ---- the geometry ------------------------------------------------------------------------------------------------ */
static float mid_x(void) { return bx + inner / 2; }
static float mid_y(void) { return by + inner / 2; }

static void flip_xy(float *x, float *y)         /* the board turned half round: player 1 at the bottom */
{
    if (!ui.flipped) return;
    *x = 2 * mid_x() - *x;
    *y = 2 * mid_y() - *y;
}

/* a plate's middle on the screen (rules' quarter q) */
static void plate_c(int q, float *x, float *y)
{
    *x = bx + (q & 1) * (plate + GAP * cell) + plate / 2;
    *y = by + (q & 2 ? 1 : 0) * (plate + GAP * cell) + plate / 2;
    flip_xy(x, y);
}

/* a hole's middle on the screen */
static void hole_c(int h, float *x, float *y)
{
    int q = tv_quarter_of(h), i = h % TV_N - (q & 1) * 3, j = h / TV_N - (q & 2 ? 0 : 3);
    *x = bx + (q & 1) * (plate + GAP * cell) + MARGIN * cell + (i + 0.5f) * cell;
    *y = by + (q & 2 ? 1 : 0) * (plate + GAP * cell) + MARGIN * cell + (2 - j + 0.5f) * cell;
    flip_xy(x, y);
}

static int hole_at(float x, float y)
{
    for (int h = 0; h < TV_CELLS; h++) {
        float hx, hy;
        hole_c(h, &hx, &hy);
        if (fabsf(x - hx) < 0.5f * cell && fabsf(y - hy) < 0.5f * cell) return h;
    }
    return -1;
}

/* the arrows: by each plate's outer corner, one on the rim across (k 0), one on the rim down the side (k 1); the
 * one across turns the plate's edge towards the tray's middle, the one down the side the other way. Its place,
 * which way it turns (as seen), and its picture (painter kind: the rim's side, ART_UP..., * 2 + the way). */
enum { OUT_UP, OUT_DOWN, OUT_LEFT, OUT_RIGHT };
static void arrow_at(int sq, int k, float *x, float *y, int *dir, int *kind)
{
    int col = sq & 1, row = sq >> 1;            /* the plate as seen on the screen */
    float corner = 0.78f * cell, r = rim / 2.0f;
    int out;
    if (k == 0) {
        *x = col ? bx + inner - corner : bx + corner;
        *y = row ? by + inner + r : by - r;
        *dir = col == row ? TV_CW : TV_CCW;
        out = row ? OUT_DOWN : OUT_UP;
    } else {
        *x = col ? bx + inner + r : bx - r;
        *y = row ? by + inner - corner : by + corner;
        *dir = col == row ? TV_CCW : TV_CW;
        out = col ? OUT_RIGHT : OUT_LEFT;
    }
    *kind = out * 2 + *dir;
}

static int rules_quarter(int sq) { return ui.flipped ? 3 - sq : sq; }

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the tray: a walnut-framed plank of side n with its middle sunk into shade, f its rim */
static void paint_base(uint8_t *o, int n, int f)
{
    int w = n + 2 * f;
    art_plank(n, n, f, o);
    for (int y = f; y < w - f; y++)
        for (int x = f; x < w - f; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * 0.42f);
        }
}

/* a hole carved in the plate, as a shade over whatever wood is under it (dark in, light on the far rim) */
static void paint_hole(int size, uint8_t *o)
{
    float r = size / 2.0f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float k = art_recess(x + 0.5f - r, r - y - 0.5f, 0.72f * r, 0.14f * r, 0.5f, 0.24f, 0.22f);
            uint8_t *p = o + 4 * (y * size + x);
            if (k < 1) p[0] = 20, p[1] = 12, p[2] = 6, p[3] = (uint8_t)clampf((1 - k) * 230, 0, 255);
            else p[0] = 255, p[1] = 238, p[2] = 210, p[3] = (uint8_t)clampf((k - 1) * 255, 0, 255);
        }
}

/* a plate: plain maple in a thin walnut edge, side s */
static void paint_plate(int s, uint8_t *o)
{
    int f = (int)fmaxf(2, 0.035f * s);
    art_plank(s - 2 * f, s - 2 * f, f, o);
}

static SDL_Texture *tex_of(void (*paint)(int, uint8_t *), int size)
{
    paint(size, ui_buf(size, size));
    return ui_tex(size, size, ui_buf(size, size));
}

static void layout(box b)
{
    float side = fminf(b.w, b.h), units = 6 + 4 * MARGIN + GAP + 2 * RIM;
    cell = (int)floorf(side / units);
    rim = (int)floorf((side - (6 + 4 * MARGIN + GAP) * cell) / 2);
    plate = 3 * cell + 2 * MARGIN * cell;
    inner = 2 * plate + GAP * cell;
    int n = (int)ceilf(inner), w = n + 2 * rim;
    bx = floorf(b.x + (b.w - w) / 2) + rim;
    by = floorf(b.y + (b.h - w) / 2) + rim;
    if (made == cell) return;
    made = cell;
    ui_free(&t_base);
    paint_base(ui_buf(w, w), n, rim);
    t_base = ui_tex(w, w, ui_buf(w, w));
    ui_free(&t_plate);
    t_plate = tex_of(paint_plate, (int)lroundf(plate));
    ui_free(&t_hole);
    t_hole = tex_of(paint_hole, cell);
    int d = (int)(MARBLE * cell);
    for (int c = 0; c < 2; c++) {
        ui_free(&t_marble[c]);
        art_counter(COUNTER_STONE, c, d, ui_buf(d, d));
        t_marble[c] = ui_tex(d, d, ui_buf(d, d));
    }
}

/* the curved arrow on a button: white (tinted by the shell), kind = the rim's side (OUT_*) * 2 + the way (TV_CW,
 * TV_CCW). An arc of 140 degrees bulging out over the rim, as the plate's edge there moves, a round tail and a
 * filled head along its tangent at the end; the coverage worked out exactly at each pixel (no blur, any size) */
static float seg_side(float px, float py, float ax, float ay, float bx_, float by_)   /* signed, inside > 0 */
{
    float ex = bx_ - ax, ey = by_ - ay, l = hypotf(ex, ey);
    return ((px - ax) * ey - (py - ay) * ex) / (l > 0 ? l : 1);
}

static void paint_turn(int kind, int size, uint8_t *o)
{
    static const float OUT[4] = {-90, 90, 180, 0};   /* screen degrees, y down: OUT_UP, DOWN, LEFT, RIGHT */
    int dir = kind & 1;
    float s = (float)size, r = 0.3f * s, w = 0.09f * s, d2r = ART_PI / 180;
    float mid = OUT[(kind >> 1) & 3], sgn = dir == TV_CW ? 1 : -1;
    /* the circle's middle set back from the bulge, so that the arrow sits in the middle of the disc */
    float cx = s / 2 - 0.14f * s * cosf(mid * d2r), cy = s / 2 - 0.14f * s * sinf(mid * d2r);
    float a0 = (mid - sgn * 62) * d2r, a1 = (mid + sgn * 38) * d2r;   /* the head adds about 40 degrees */
    /* the head: its tip ahead of the arc's end along the tangent, its base across the arc */
    float tx = cosf(a1 + sgn * ART_PI / 2), ty = sinf(a1 + sgn * ART_PI / 2);   /* the tangent, the way it turns */
    float hx = cx + r * cosf(a1), hy = cy + r * sinf(a1), hl = 0.2f * s, hw = 0.13f * s;
    float bx0 = hx - tx * 0.02f * s, by0 = hy - ty * 0.02f * s;
    float tipx = bx0 + tx * hl, tipy = by0 + ty * hl;
    float px1 = bx0 - ty * hw, py1 = by0 + tx * hw, px2 = bx0 + ty * hw, py2 = by0 - tx * hw;
    float lo = fminf(a0, a1), span = fabsf(a1 - a0);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float px = x + 0.5f, py = y + 0.5f, dx = px - cx, dy = py - cy;
            float a = atan2f(dy, dx);
            while (a < lo) a += 2 * ART_PI;
            while (a > lo + 2 * ART_PI) a -= 2 * ART_PI;
            float db;                           /* the band's distance (round ends) */
            if (a - lo <= span) db = fabsf(hypotf(dx, dy) - r) - w / 2;
            else {
                float e0 = hypotf(px - (cx + r * cosf(a0)), py - (cy + r * sinf(a0)));
                float e1 = hypotf(px - (cx + r * cosf(a1)), py - (cy + r * sinf(a1)));
                db = fminf(e0, e1) - w / 2;
            }
            /* the head's triangle: inside where all three sides agree (same winding either way) */
            float s1 = seg_side(px, py, px1, py1, tipx, tipy), s2 = seg_side(px, py, tipx, tipy, px2, py2),
                  s3 = seg_side(px, py, px2, py2, px1, py1);
            float wind = seg_side(px2, py2, px1, py1, tipx, tipy) >= 0 ? 1 : -1;
            float dh = -fminf(fminf(s1 * wind, s2 * wind), s3 * wind);
            float cov = clampf(0.5f - fminf(db, dh), 0, 1);
            uint8_t *p = o + 4 * (y * size + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * cov + 0.5f);
        }
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* how far each plate has turned so far (quarter turns, clockwise), over the steps before `ply` */
static int turned(int q, int ply)
{
    int t = 0;
    for (int i = 0; i < ply; i++)
        if (G.step[i] >= 64 && (G.step[i] - 64) / 2 == q) t += (G.step[i] - 64) % 2 == TV_CW ? 1 : -1;
    return t;
}

static void put_plate(int q, double deg, float k)
{
    float x, y;
    plate_c(q, &x, &y);
    float s = plate * k;
    if (k > 1) ui_fill(x - s / 2 + 0.06f * cell, y - s / 2 + 0.1f * cell, s, s, 0, 0, 0, 70);   /* lifted */
    ui_put_rot(t_plate, x, y, s, s, deg);
}

static void put_marble(int player, float x, float y, float k)
{
    ui_flat_at(t_marble[player], x, y, MARBLE * cell * k, 1, 0);
}

static void draw(void)
{
    if (pending >= 0 && !anim.on) {             /* a bot's marble is down: its twist */
        int t = pending;
        pending = -1;
        twist(t / 2, t % 2);
    }
    const tvpos *p = now();
    float w = inner + 2 * rim, x, y;
    ui_put(t_base, bx - rim, by - rim, w, w);
    float t = 1;
    if (anim.on) {
        t = (ui_ms() - anim.t0) / (float)(anim.kind ? TURN_MS : DROP_MS);
        if (t >= 1) t = 1, anim.on = 0;
        else ui_animate();
    }
    if (pending >= 0) ui_animate();
    int spinning = anim.on && anim.kind == 1;
    const tvpos *shown = spinning ? &G.pos[G.ply - 1] : p;   /* the turning plate's marbles: as they were */
    float e = ui_ease(t), lift = spinning ? 1 + 0.07f * sinf(ART_PI * t) : 1;
    double sign = spinning && anim.dir == TV_CCW ? -1 : 1;
    int marked = -1;                            /* the last turn's marble, where it is now (under it: a warm tint) */
    if (!anim.on && G.ply >= 2 && p->stage == TV_PLACE && p->winner < 0) {
        int h = G.step[G.ply - 2], s = G.step[G.ply - 1];
        if (h >= 0 && h < TV_CELLS && s >= 64) marked = tv_turned(h, (s - 64) / 2, (s - 64) % 2);
    }
    for (int pass = 0; pass < 2; pass++)        /* the still plates, then the turning one over them */
        for (int q = 0; q < 4; q++) {
            int turning = spinning && q == anim.q;
            if (turning != pass) continue;
            double deg = 90.0 * turned(q, turning ? G.ply - 1 : G.ply) + (turning ? sign * 90 * e : 0);
            put_plate(q, deg, turning ? lift : 1);
            float cx, cy;
            plate_c(q, &cx, &cy);
            double a = turning ? sign * 90 * e * ART_PI / 180 : 0;
            float ca = (float)cos(a), sa = (float)sin(a);
            for (int h = 0; h < TV_CELLS; h++) {
                if (tv_quarter_of(h) != q) continue;
                hole_c(h, &x, &y);
                float dx = (x - cx) * lift, dy = (y - cy) * lift;
                x = cx + dx * ca - dy * sa;
                y = cy + dx * sa + dy * ca;
                ui_put_c(t_hole, x, y, cell * lift, cell * lift);
                int v = shown->cell[h];
                if (!v) continue;
                int dropping = anim.on && anim.kind == 0 && h == anim.hole;
                if (h == marked) ui_mark_last_round(x, y, 0.98f * cell);
                if (h == p->last && p->stage == TV_TWIST && !dropping) ui_mark_select_round(x, y, 0.98f * cell);
                put_marble(v - 1, x, y, dropping ? 1.25f - 0.25f * e : lift);
            }
        }
    if (!anim.on && p->winner >= 0) {           /* the rows of five in gold */
        uint8_t mark[TV_CELLS];
        if (tv_marks(p->cell, mark))
            for (int h = 0; h < TV_CELLS; h++)
                if (mark[h]) {
                    hole_c(h, &x, &y);
                    ui_mark_gold(x, y, 1.5f * cell);
                    put_marble(p->cell[h] - 1, x, y, 1);
                }
    }
    if (!anim.on && pending < 0 && p->winner < 0 && p->stage == TV_TWIST && !(ui_bot_seats() >> p->side & 1))
        for (int sq = 0; sq < 4; sq++)           /* the arrows */
            for (int k = 0; k < 2; k++) {
                int dir, kind;
                arrow_at(sq, k, &x, &y, &dir, &kind);
                ui_button_paint(x, y, 0.92f * cell, paint_turn, kind, 0.9f, UI_LIT | UI_TINT);
            }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static void press(float x, float y)
{
    const tvpos *p = now();
    if (p->winner >= 0 || busy()) return;
    if (p->stage == TV_TWIST) {
        for (int sq = 0; sq < 4; sq++)
            for (int k = 0; k < 2; k++) {
                int dir, kind;
                float ax, ay;
                arrow_at(sq, k, &ax, &ay, &dir, &kind);
                if (ui_button_hit(ax, ay, 0.92f * cell, x, y)) { twist(rules_quarter(sq), dir); return; }
            }
        ui_sound(SND_DENY);
        return;
    }
    int h = hole_at(x, y);
    if (h >= 0 && !p->cell[h]) place(h);
    else if (h >= 0) ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_STONE, player, size, rgba); }

/* the menu's card: the tray, three plates still and the fourth turning, marbles on all of them, a row of five
 * coming */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    float units = 6 + 4 * MARGIN + GAP + 2 * 0.5f;
    int c = (int)(size / units), f = (int)((size - (6 + 4 * MARGIN + GAP) * c) / 2), n = size - 2 * f;
    uint8_t *base = ui_buf(size, size);
    paint_base(base, n, f);
    memcpy(rgba, base, (size_t)size * size * 4);
    int ps = (int)lroundf(3 * c + 2 * MARGIN * c), d = (int)(MARBLE * c);
    uint8_t *pl = malloc((size_t)ps * ps * 4), *hole = malloc((size_t)c * c * 4), *mb[2];
    mb[0] = malloc((size_t)d * d * 4);
    mb[1] = malloc((size_t)d * d * 4);
    if (!pl || !hole || !mb[0] || !mb[1]) { free(pl); free(hole); free(mb[0]); free(mb[1]); return; }
    paint_plate(ps, pl);
    paint_hole(c, hole);
    art_counter(COUNTER_STONE, 0, d, mb[0]);
    art_counter(COUNTER_STONE, 1, d, mb[1]);
    static const char *const MARBLES[2] = {"a1 b2 c3 d4 b5 e2", "c1 a3 c5 e5 f4 d2 a6"};
    for (int q = 0; q < 4; q++) {
        float deg = q == 1 ? 15 : 0, gap = GAP * c, a = deg * ART_PI / 180;
        float cx = f + (q & 1) * (ps + gap) + ps / 2.0f, cy = f + (q & 2 ? 1 : 0) * (ps + gap) + ps / 2.0f;
        art_blend_rot(rgba, size, size, pl, ps, ps, cx, cy, deg);
        for (int h = 0; h < TV_CELLS; h++) {
            if (tv_quarter_of(h) != q) continue;
            int i = h % TV_N - (q & 1) * 3, j = h / TV_N - (q & 2 ? 0 : 3);
            float dx = (i - 1) * (float)c, dy = (1 - j) * (float)c;
            float x = cx + dx * cosf(a) - dy * sinf(a), y = cy + dx * sinf(a) + dy * cosf(a);
            art_blend(rgba, size, size, hole, c, c, (int)lroundf(x - c / 2.0f), (int)lroundf(y - c / 2.0f));
            for (int pl2 = 0; pl2 < 2; pl2++)
                for (const char *s = MARBLES[pl2]; *s; s += s[2] ? 3 : 2)
                    if (tv_square(s) == h) art_flat_put(rgba, size, size, mb[pl2], d, x, y);
        }
    }
    free(pl);
    free(hole);
    free(mb[0]);
    free(mb[1]);
}

static const game twistfive_game = {
    .id = "twistfive", .order = 72, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(twistfive_game)
