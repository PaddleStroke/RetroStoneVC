/*
 * Mancala in RetroStone Board: a long plank with two rows of carved pits and a store at each end, the seeds sown one
 * by one, the captured seeds flying to the store (the rules, Oware and Kalah: mancala_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board lies across the room the shell gives, each player's row of pits next to them: 8 columns (a store, 6 pits,
 * a store) by 3 squares of the house plank, each row of pits 1.5 squares high, as wide as the room allows (landscape).
 * Where the width is short (portrait) the stores leave the plank's ends for planks of their own beyond its long sides,
 * each next to its player at the corner where the sowing turns (ivory's below the plank's right end, ebony's above
 * its left end, the board turning round when flipped): the six columns of pits take the whole width, the stores the
 * height left over. Every seed is someone: it keeps its pebble and its tint from pit to pit.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "games/mancala.h"
#include "shell.h"

#define FRAME 0.32f                             /* the board's frame, in columns */
#define COLS 8
#define ROWS 3                                  /* the plank's height, in columns */
#define PIT_RX 0.40f                            /* a pit's half width and half height, in columns */
#define PIT_RY 0.56f
#define STORE_RY 1.32f
#define SEED 0.34f                              /* a seed, in columns */
#define NUM 0.34f                               /* the counts' digits, in columns */
#define NUM_V 0.42f                             /* a count's centre beyond the plank's frame */
#define STORE_W 3.0f                            /* a store on its own plank (portrait): its length, in columns... */
#define STORE_H0 1.1f                           /* ...its height, at least and at most */
#define STORE_H1 1.8f
#define STORE_RX 1.3f                           /* its hollow's half length */
#define STORE_GAP 0.72f                         /* from the board's frame to the store's (the pits' counts between) */
#define TINTS 4
#define SHAPES 4
#define LIFT_MS 140                             /* the seeds picked up */
#define TAKE_MS 380                             /* a capture flying to the store */
#define SWEEP_MS 620                            /* the end: the seeds left flying to their stores */
#define PAUSE_MS 140

typedef struct { uint8_t n[MC_SLOTS], id[MC_SLOTS][MC_SEEDS]; } heap;   /* which seeds lie where, bottom first */

static mc_game G, scratch;
static heap H;                                  /* the seeds after the last move */
static int variant, sq, fr, sd, made = -1, made_out = -1, made_small = -1;   /* sd: a seed, pixels */
static int out, ncol = COLS;                    /* out: the stores on their own planks, the plank 6 columns long */
static float bx, by;                            /* the plank's top left corner */
static float store_h, made_h;                   /* out: a store's height, in columns */
static SDL_Texture *t_board, *t_store, *t_seed[TINTS][SHAPES], *t_small[TINTS], *t_num;
static int num_w, num_h;
static struct { int on, step, heard; Uint32 t0; heap before; mc_move m; } anim;
typedef struct { int id; float x0, y0, h0, x1, y1, h1, t, arc; } flight;   /* a seed in the air */

static const uint8_t TINT[TINTS][3] = {{205, 172, 124}, {170, 104, 66}, {118, 96, 78}, {196, 146, 82}};

static float hash(int a, int b)
{
    unsigned h = (unsigned)a * 374761393u + (unsigned)b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

/* ---- the seeds, one by one --------------------------------------------------------------------------------------- */
static void heap_start(heap *h)
{
    memset(h, 0, sizeof *h);
    for (int k = 0; k < MC_SEEDS; k++) h->id[k / 4][h->n[k / 4]++] = (uint8_t)k;
}

static void push(heap *h, int slot, int id) { h->id[slot][h->n[slot]++] = (uint8_t)id; }

/* a move on the seeds: the pit's seeds sown in order, then the captures and the sweep, whole pits to a store */
static void heap_apply(heap *h, const mc_move *m)
{
    uint8_t hand[MC_SEEDS];
    memcpy(hand, h->id[m->pit], (size_t)m->n);
    h->n[m->pit] = 0;
    for (int i = 0; i < m->n; i++) push(h, m->to[i], hand[i]);
    for (int i = 0; i < m->ntake; i++) {
        const mc_take *t = &m->take[i];
        for (int k = 0; k < h->n[t->pit]; k++) push(h, t->store, h->id[t->pit][k]);
        h->n[t->pit] = 0;
    }
}

/* the seeds after the moves played (a new game, an undo) */
static void rebuild(void)
{
    mc_move m;
    mc_new(&scratch, variant);
    heap_start(&H);
    for (int i = 0; i < G.n; i++) {
        mc_play(&scratch, G.move[i], &m);
        heap_apply(&H, &m);
    }
    anim.on = 0;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static void start(int v)
{
    variant = v ? MC_KALAH : MC_OWARE;
    mc_new(&G, variant);
    rebuild();
}

static int status(void)
{
    switch (mc_status(&G)) {
    case MC_WIN0: return GS_WIN0;
    case MC_WIN1: return GS_WIN1;
    case MC_DRAW: return GS_DRAW;
    default: return GS_PLAY;
    }
}

static int turn(void) { return mc_turn(&G); }

/* the sowing's pace: a few seeds slowly, many quicker */
static int step_ms(int n) { return n <= 6 ? 170 : n >= 16 ? 90 : 170 - (n - 6) * 8; }

static void play(int pit, int animate)
{
    anim.before = H;
    if (!mc_play(&G, pit, &anim.m)) return;
    heap_apply(&H, &anim.m);
    anim.on = animate;
    anim.step = step_ms(anim.m.n);
    anim.heard = 0;
    anim.t0 = ui_ms();
    if (animate) ui_animate();
}

static int undo(void)
{
    if (!mc_undo(&G)) return 0;
    rebuild();
    return 1;
}

/* "3 9 4 12": the pits sown, 1 to 12 counter-clockwise from ivory's left (ivory 1-6, ebony 7-12 from their left) */
static int script(const char *s)
{
    for (char *end; *s; s = end) {
        long p = strtol(s, &end, 10);
        if (end == s) break;
        if (!mc_legal(&G, (int)p - 1)) return 0;
        play((int)p - 1, 0);
    }
    return 1;
}

/* the bot's side (mancala_bot.c): the game to copy, and its move played as a tap plays it */
const mc_game *mancala_live(void) { return &G; }

int mancala_show(const char *move)
{
    char *end;
    long p = strtol(move, &end, 10);
    if (end == move || *end || !mc_legal(&G, (int)p - 1)) return 0;
    play((int)p - 1, 1);
    return 1;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* a pit carved into the plank: darker inside, its wall in shade at the top left and lit at the bottom right */
static void carve(uint8_t *o, int w, int h, float cx, float cy, float rx, float ry)
{
    for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
        for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float nx = (x + 0.5f - cx) / rx, ny = (y + 0.5f - cy) / ry, e = sqrtf(nx * nx + ny * ny);
            float a = clampf((1 - e) * fminf(rx, ry) + 0.5f, 0, 1);
            if (a <= 0) continue;
            float wall = e * e * sqrtf(e), k = 0.74f * (1 + 0.6f * (nx + ny) * 0.7071f * wall);
            if (e > 0.94f) k *= 0.86f;          /* the sharp edge of the cut */
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * (1 - a + a * k), 0, 255);
        }
}

/* the plank of ncol columns in its frame, a store carved in the columns marked, two pits in the others */
static void make_board(int ncol, unsigned stores, int s, int f, uint8_t *o)
{
    int w = ncol * s + 2 * f, h = ROWS * s + 2 * f;
    art_board_n(ncol, ROWS, s, f, 0, o);
    for (int c = 0; c < ncol; c++) {
        float cx = f + (c + 0.5f) * s;
        if (stores >> c & 1) carve(o, w, h, cx, f + 1.5f * s, PIT_RX * s, STORE_RY * s);
        else for (int r = 0; r < 2; r++) carve(o, w, h, cx, f + (0.75f + 1.5f * r) * s, PIT_RX * s, PIT_RY * s);
    }
}

/* a pebble: a rounded irregular outline, a warm natural tint, lit from the upper left */
static void make_seed(int tint, int shape, int size, uint8_t *rgba)
{
    art_style st;
    memset(&st, 0, sizeof st);
    for (int i = 0; i < 3; i++) {
        st.fill[i] = TINT[tint][i];
        st.outer[i] = TINT[tint][i] * 0.5f;
        st.inner[i] = TINT[tint][i] * 0.75f;
        st.mark[i] = fminf(255, TINT[tint][i] * 1.18f + 20);
    }
    st.light = 0.55f;
    st.line = fmaxf(0.025f, 0.9f / size);
    float xy[2 * 20], p1 = 6.2832f * hash(shape, 5), p2 = 6.2832f * hash(shape, 6), tilt = 0.6f * hash(shape, 8);
    for (int i = 0; i < 20; i++) {              /* an oval, bumped smoothly, turned a little */
        float a = i * 6.2832f / 20, r = 0.42f * (1 + 0.06f * sinf(2 * a + p1) + 0.035f * sinf(3 * a + p2));
        float ex = cosf(a) * r, ey = sinf(a) * r * 0.84f;
        xy[2 * i] = 0.5f + ex * cosf(tilt) - ey * sinf(tilt);
        xy[2 * i + 1] = 0.5f + ex * sinf(tilt) + ey * cosf(tilt);
    }
    art_fig *f = art_fig_new();
    art_poly(f, ART_PART, xy, 20);
    art_ellipse(f, ART_MARK, 0.38f, 0.34f, 0.09f, 0.055f);
    memset(rgba, 0, (size_t)size * size * 4);
    art_render(f, &st, size, size, (float)size, 0, 0, rgba);
    art_fig_free(f);
}

/* a store on its own plank: w x h pixels inside a frame of f, its hollow along it */
static void make_store(int w, int h, int s, int f, uint8_t *o)
{
    art_plank(w, h, f, o);
    carve(o, w + 2 * f, h + 2 * f, f + w / 2.0f, f + h / 2.0f, STORE_RX * s, h / 2.0f - 0.12f * s);
}

/* the room: the plank with its stores at its ends and the counts along it (as wide as it may be: landscape), or the
 * plank of pits alone with a store on a plank beyond each long side (the narrow room of portrait), whichever gives
 * the bigger pits */
static void layout(box b)
{
    float h_in = ROWS + 2 * FRAME + 2 * (NUM_V + 0.2f);
    float h_out = ROWS + 2 * FRAME + 2 * (STORE_GAP + STORE_H0 + 2 * FRAME);
    float s_in = fminf(b.w / (COLS + 2 * FRAME), b.h / h_in), s_out = fminf(b.w / (6 + 2 * FRAME), b.h / h_out);
    out = s_out > s_in;
    ncol = out ? 6 : COLS;
    sq = (int)floorf(out ? s_out : s_in);
    fr = (int)(sq * FRAME);
    store_h = clampf((b.h / sq - ROWS - 2 * FRAME) / 2 - STORE_GAP - 2 * FRAME, STORE_H0, STORE_H1);
    float w = (float)ncol * sq + 2 * fr, h = (float)ROWS * sq + 2 * fr;
    bx = floorf(b.x + (b.w - w) / 2) + fr;
    by = floorf(b.y + (b.h - h) / 2) + fr;
    if (made != sq || made_out != out || (out && made_h != store_h)) {
        made = sq;
        made_out = out;
        made_h = store_h;
        int bw = (int)w, bh = (int)h, d = sd = (int)(SEED * sq + 0.5f);
        ui_free(&t_board);
        make_board(ncol, out ? 0 : 1u | 1u << (COLS - 1), sq, fr, ui_buf(bw, bh));
        t_board = ui_tex(bw, bh, ui_buf(bw, bh));
        ui_free(&t_store);
        if (out) {
            int sw = (int)(STORE_W * sq), sh = (int)(store_h * sq);
            make_store(sw, sh, sq, fr, ui_buf(sw + 2 * fr, sh + 2 * fr));
            t_store = ui_tex(sw + 2 * fr, sh + 2 * fr, ui_buf(sw + 2 * fr, sh + 2 * fr));
        }
        for (int t = 0; t < TINTS; t++)
            for (int s = 0; s < SHAPES; s++) {
                ui_free(&t_seed[t][s]);
                make_seed(t, s, d, ui_buf(d, d));
                t_seed[t][s] = ui_tex(d, d, ui_buf(d, d));
            }
        ui_free(&t_num);                        /* the top player's counts, drawn here and turned round */
        num_h = (int)(NUM * sq + 2);
        num_w = 3 * num_h;
        t_num = SDL_CreateTexture(ui.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, num_w, num_h);
        if (t_num) SDL_SetTextureBlendMode(t_num, SDL_BLENDMODE_BLEND);
    }
    int small = (int)(0.36f * ui.unit);
    if (made_small != small) {
        made_small = small;
        for (int t = 0; t < TINTS; t++) {
            ui_free(&t_small[t]);
            make_seed(t, t % SHAPES, small, ui_buf(small, small));
            t_small[t] = ui_tex(small, small, ui_buf(small, small));
        }
    }
}

/* ---- where things are -------------------------------------------------------------------------------------------- */
/* board units (columns from the plank's top left, ivory's row at the bottom) to pixels; flipped: turned round */
static void at(float u, float v, float *x, float *y)
{
    if (ui.flipped) { u = ncol - u; v = ROWS - v; }
    *x = bx + u * sq;
    *y = by + v * sq;
}

/* out: a store's centre beyond the plank's long side (board units from the plank's edge) */
static float store_v(void) { return FRAME + STORE_GAP + FRAME + store_h / 2; }

static void slot_uv(int s, float *u, float *v)  /* a pit's or a store's centre, board units */
{
    float p0 = out ? 0.5f : 1.5f;               /* the first pit's column */
    if (out && s == MC_STORE) { *u = ncol - STORE_W / 2; *v = ROWS + store_v(); }
    else if (out && s == MC_STORE + 1) { *u = STORE_W / 2; *v = -store_v(); }
    else if (s == MC_STORE) { *u = COLS - 0.5f; *v = 1.5f; }
    else if (s == MC_STORE + 1) { *u = 0.5f; *v = 1.5f; }
    else if (s < 6) { *u = p0 + s; *v = 2.25f; }
    else { *u = p0 + (11 - s); *v = 0.75f; }
}

static void centre(int s, float *x, float *y)
{
    float u, v;
    slot_uv(s, &u, &v);
    at(u, v, x, y);
}

/* the k-th seed of a pit or a store, from its centre (board units): a sunflower spiral, a little jostled (salt: the
 * pit's own jostle); more than a layer makes a heap */
static void spot_in(float hx, float hy, int cap, int salt, int k, float *du, float *dv)
{
    int layer = k / cap < 4 ? k / cap : 4, j = k % cap;
    float rx = hx - 0.56f * SEED, ry = hy - 0.56f * SEED;
    float r = sqrtf((j + 0.5f) / cap) * (1 - 0.16f * layer), a = j * 2.39996f + k / cap * 1.3f + salt;
    *du = cosf(a) * r * rx + 0.05f * (hash(salt * 64 + k, 1) - 0.5f);
    *dv = sinf(a) * r * ry + 0.05f * (hash(salt * 64 + k, 2) - 0.5f) - 0.03f * layer;
}

static void spot(int store, int salt, int k, float *du, float *dv)
{
    spot_in(PIT_RX, store ? STORE_RY : PIT_RY, store ? 16 : 6, salt, k, du, dv);
}

static void seed_xy(int s, int k, float *x, float *y)
{
    float u, v, du, dv;
    slot_uv(s, &u, &v);
    if (out && s >= MC_STORE) {                 /* a store on its plank: a layer as big as its hollow's room */
        float hy = store_h / 2 - 0.12f, room = (STORE_RX - 0.56f * SEED) * (hy - 0.56f * SEED);
        int cap = (int)clampf(16 * room / ((PIT_RX - 0.56f * SEED) * (STORE_RY - 0.56f * SEED)), 16, 40);
        spot_in(STORE_RX, hy, cap, s, k, &du, &dv);
    } else {
        spot(s >= MC_STORE, s, k, &du, &dv);
    }
    at(u + du, v + dv, x, y);
}

static int slot_at(float x, float y)            /* the pit or store under a point, or -1 */
{
    if (ui.flipped) { x = 2 * bx + ncol * sq - x; y = 2 * by + ROWS * sq - y; }
    float u = (x - bx) / sq, v = (y - by) / sq;
    if (out) {                                  /* the stores on their planks, the pits */
        for (int s = MC_STORE; s < MC_SLOTS; s++) {
            float su, sv;
            slot_uv(s, &su, &sv);
            if (fabsf(u - su) < STORE_W / 2 + FRAME && fabsf(v - sv) < store_h / 2 + FRAME) return s;
        }
        if (u < 0 || u >= ncol || v < 0 || v >= ROWS) return -1;
        return v >= 1.5f ? (int)u : 11 - (int)u;
    }
    if (u < 0 || u >= COLS || v < 0 || v >= ROWS) return -1;
    int c = (int)u;
    if (c == 0) return MC_STORE + 1;
    if (c == COLS - 1) return MC_STORE;
    return v >= 1.5f ? c - 1 : 12 - c;
}

/* ---- the board on the screen ------------------------------------------------------------------------------------- */
/* a seed lying at (x, y), or lifted by h pixels (bigger, its shadow further) */
static void seed(int id, float x, float y, float h)
{
    SDL_Texture *t = t_seed[id % TINTS][id / TINTS % SHAPES];    /* not turned: the light stays at the top left */
    if (h <= 0) {                               /* lying: its own size, on whole pixels (sharp) */
        x = floorf(x - sd / 2.0f);
        y = floorf(y - sd / 2.0f);
        ui_shadow_flat(x + sd / 2.0f, y + sd / 2.0f, (float)sd, (float)sd, 0.5f);
        ui_put(t, x, y, (float)sd, (float)sd);
        return;
    }
    float d = sd * (1 + 0.6f * h / sq);
    ui_shadow_flat(x + 0.25f * h, y + 0.15f * h, d, d, 0.5f - 0.5f * h / sq);
    ui_put_c(t, x, y - h, d, d);
}

/* a seed on its way: eased from one place (and height) to another, in an arc */
static void fly(int id, float x0, float y0, float h0, float x1, float y1, float h1, float t, float arc)
{
    float e = t * t * (3 - 2 * t);
    seed(id, x0 + (x1 - x0) * e, y0 + (y1 - y0) * e, h0 + (h1 - h0) * e + arc * sinf(3.14159f * t));
}

static void number(float x, float y, int n, int upside)   /* a count beside a pit, turned for the top player */
{
    float h = NUM * sq;
    if (!upside || !t_num) { ui_number(x, y, h, n, 1); return; }
    SDL_Texture *old = SDL_GetRenderTarget(ui.r);
    SDL_SetRenderTarget(ui.r, t_num);
    SDL_SetRenderDrawColor(ui.r, 0, 0, 0, 0);
    SDL_RenderClear(ui.r);
    ui_number(num_w / 2.0f, num_h / 2.0f, h, n, 1);
    SDL_SetRenderTarget(ui.r, old);
    ui_put_rot(t_num, x, y, (float)num_w, (float)num_h, 180);
}

static int elapsed(void) { return (int)(ui_ms() - anim.t0); }

/* the animation's times: the seeds picked up, one sown each step, then each capture, then the sweep together */
static int sow_end(void) { return LIFT_MS + anim.m.n * anim.step; }

static void take_times(int i, int *t0, int *t1)
{
    int c = 0, ncap = 0;
    for (int k = 0; k < anim.m.ntake; k++) ncap += !anim.m.take[k].sweep;
    for (int k = 0; k < i; k++) c += !anim.m.take[k].sweep;
    int caps_end = sow_end() + (ncap ? PAUSE_MS + ncap * TAKE_MS : 0);
    if (anim.m.take[i].sweep) { *t0 = caps_end + 2 * PAUSE_MS; *t1 = *t0 + SWEEP_MS; }
    else { *t0 = sow_end() + PAUSE_MS + c * TAKE_MS; *t1 = *t0 + TAKE_MS; }
}

static int anim_end(void)
{
    int t0, t1 = sow_end();
    for (int i = 0; i < anim.m.ntake; i++) take_times(i, &t0, &t1);
    return t1;
}

/* the sounds of what has landed by now: a seed sown, a capture, the sweep (once) */
static void sounds(int t)
{
    int n = 0;
    for (int i = 0; i < anim.m.n; i++)
        if (t >= LIFT_MS + (i + 1) * anim.step && n++ >= anim.heard) ui_sound(SND_PLACE);
    for (int i = 0, t0, t1; i < anim.m.ntake; i++) {
        take_times(i, &t0, &t1);
        if (t >= t1 && n++ >= anim.heard && !(anim.m.take[i].sweep && i && anim.m.take[i - 1].sweep))
            ui_sound(SND_CAPTURE);
    }
    anim.heard = n;
}

/* the seeds in the pits and the stores, and during a move the ones on their way */
static void draw_seeds(void)
{
    if (!anim.on) {
        for (int s = 0; s < MC_SLOTS; s++)
            for (int k = 0; k < H.n[s]; k++) {
                float x, y;
                seed_xy(s, k, &x, &y);
                seed(H.id[s][k], x, y, 0);
            }
        return;
    }
    int t = elapsed();
    if (t >= anim_end()) {                      /* landed: the next frame shows the pits to play */
        anim.on = 0;
        sounds(t);
        ui_animate();
        draw_seeds();
        return;
    }
    ui_animate();
    sounds(t);
    const mc_move *m = &anim.m;
    heap d = anim.before;
    uint8_t hand[MC_SEEDS];
    memcpy(hand, d.id[m->pit], (size_t)m->n);
    d.n[m->pit] = 0;
    int landed = t < LIFT_MS ? 0 : (t - LIFT_MS) / anim.step;
    if (landed > m->n) landed = m->n;
    for (int j = 0; j < landed; j++) push(&d, m->to[j], hand[j]);
    flight air[MC_SEEDS];
    int nair = 0;
    float lift = 0.42f * sq, cx0, cy0, cx1, cy1, x, y;
    /* the hand: the seeds not sown yet in a small cluster above the pit it is over (the next to go on its rim) */
    if (landed < m->n) {
        float u = t < LIFT_MS ? 0 : (t - LIFT_MS - landed * anim.step) / (float)anim.step, e = u * u * (3 - 2 * u);
        centre(landed ? m->to[landed - 1] : m->pit, &cx0, &cy0);
        centre(m->to[landed], &cx1, &cy1);
        float hx = cx0 + (cx1 - cx0) * e, hy = cy0 + (cy1 - cy0) * e;
        for (int j = landed; j < m->n; j++) {
            int i = m->n - 1 - j;               /* the last to go in the middle */
            float r = 0.11f * sq * sqrtf((float)i), a = i * 2.39996f, ox = r * cosf(a), oy = r * sinf(a);
            if (t < LIFT_MS) {                  /* picked up from the pit */
                seed_xy(m->pit, j, &x, &y);
                air[nair++] = (flight){hand[j], x, y, 0, cx0 + ox, cy0 + oy, lift, t / (float)LIFT_MS, 0};
            } else if (j == landed) {           /* dropped into the next pit, to its place there */
                float sx = cx0 + ox, sy = cy0 + oy;
                seed_xy(m->to[j], d.n[m->to[j]], &x, &y);
                air[nair++] = (flight){hand[j], sx, sy, lift, x, y, 0, u, 0.12f * sq};
            } else {
                air[nair++] = (flight){hand[j], hx + ox, hy + oy, lift, hx + ox, hy + oy, lift, 0, 0};
            }
        }
    }
    /* the captures and the sweep: whole pits fly to a store, each seed to its place there */
    int pending[MC_SLOTS] = {0};
    for (int i = 0, t0, t1; i < m->ntake && t >= sow_end(); i++) {
        const mc_take *tk = &m->take[i];
        take_times(i, &t0, &t1);
        if (t < t0) continue;
        int n = d.n[tk->pit];
        if (t >= t1) {
            for (int k = 0; k < n; k++) push(&d, tk->store, d.id[tk->pit][k]);
            d.n[tk->pit] = 0;
            continue;
        }
        float u = (t - t0) / (float)(t1 - t0);
        for (int k = 0; k < n; k++) {
            float x1, y1;
            seed_xy(tk->pit, k, &x, &y);
            seed_xy(tk->store, d.n[tk->store] + pending[tk->store] + k, &x1, &y1);
            air[nair++] = (flight){d.id[tk->pit][k], x, y, 0, x1, y1, 0, u, 0.5f * sq};
        }
        pending[tk->store] += n;
        d.n[tk->pit] = 0;
    }
    for (int s = 0; s < MC_SLOTS; s++)
        for (int k = 0; k < d.n[s]; k++) {
            seed_xy(s, k, &x, &y);
            seed(d.id[s][k], x, y, 0);
        }
    for (int i = 0; i < nair; i++)
        fly(air[i].id, air[i].x0, air[i].y0, air[i].h0, air[i].x1, air[i].y1, air[i].h1, air[i].t, air[i].arc);
}

/* the count of every pit and store, as shown (during a move: the seeds already there) */
static int shown(int s)
{
    if (!anim.on) return H.n[s];
    const mc_move *m = &anim.m;
    int t = elapsed(), landed = t < LIFT_MS ? 0 : (t - LIFT_MS) / anim.step, n = anim.before.n[s];
    if (s == m->pit) n = 0;
    for (int j = 0; j < landed && j < m->n; j++) n += m->to[j] == s;
    for (int i = 0, t0, t1; i < m->ntake; i++) {
        take_times(i, &t0, &t1);
        if (t >= t0 && m->take[i].pit == s) n = 0;
        if (t >= t1 && m->take[i].store == s) n += m->take[i].n;
    }
    return n;
}

static void draw(void)
{
    float x, y, x1, y1;
    ui_put(t_board, bx - fr, by - fr, (float)ncol * sq + 2 * fr, (float)ROWS * sq + 2 * fr);
    if (out && t_store)                         /* the stores on their own planks */
        for (int s = MC_STORE; s < MC_SLOTS; s++) {
            centre(s, &x, &y);
            float w = floorf(STORE_W * sq) + 2 * fr, h = floorf(store_h * sq) + 2 * fr;
            ui_put(t_store, floorf(x - w / 2), floorf(y - h / 2), w, h);
        }
    int playing = status() == GS_PLAY;
    if (G.n && !anim.on) {                      /* the last pit sown */
        float u, v;
        slot_uv(G.move[G.n - 1], &u, &v);
        at(u - 0.5f, v - 0.75f, &x, &y);
        at(u + 0.5f, v + 0.75f, &x1, &y1);
        ui_mark_last(fminf(x, x1), fminf(y, y1), (float)sq, 1.5f * sq);
    }
    if (playing && !anim.on)                    /* the pits the player may sow: a warm light in them */
        for (int s = 0; s < MC_PITS; s++)
            if (mc_legal(&G, s)) {
                centre(s, &x, &y);
                ui_mark_gold(x, y, 1.1f * sq);
                ui_mark_gold(x, y, 0.8f * sq);
            }
    draw_seeds();
    for (int s = 0; s < MC_SLOTS; s++) {        /* the counts outside the board, along each player's edge */
        int owner = s >= MC_STORE ? s - MC_STORE : MC_OWNER(s);
        float u, v;
        slot_uv(s, &u, &v);
        if (out && s >= MC_STORE)               /* a store on its plank: its count beside it, towards the middle */
            at(owner ? STORE_W + FRAME + 0.5f : ncol - STORE_W - FRAME - 0.5f, v, &x, &y);
        else
            at(u, owner ? -FRAME - NUM_V : ROWS + FRAME + NUM_V, &x, &y);
        number(x, y, shown(s), ui_upside(owner));
    }
}

/* ---- the pointer: a tap on one of the player's pits sows it ------------------------------------------------------ */
static void press(float x, float y)
{
    if (status() != GS_PLAY || anim.on) return;
    int s = slot_at(x, y);
    if (s < 0) return;
    if (s < MC_PITS && mc_legal(&G, s)) play(s, 1);
    else ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }
static void release(float x, float y) { (void)x; (void)y; }

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
/* the seeds in the player's store: a few pebbles, then their number */
static void bar(int player, box b)
{
    float u = ui.unit, s = 0.36f * u, cy = b.y + b.h / 2, x = b.x + s / 2;
    ui_put_c(t_small[0], x, cy + 0.08f * u, s, s);
    ui_put_c(t_small[1], x + 0.26f * u, cy + 0.1f * u, s, s);
    ui_put_c(t_small[3], x + 0.13f * u, cy - 0.12f * u, s, s);
    ui_number(x + 0.26f * u + s / 2 + 0.5f * u, cy, 0.46f * u, shown(MC_STORE + player), 1);
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's and the picker's cards: a piece of the board with seeds (Kalah: its store at the left) */
static void thumb(int v, int size, uint8_t *rgba)
{
    enum { NC = 4 };
    static const int pits[2][2][NC] = {{{3, 5, 0, 2}, {4, 1, 6, 2}}, {{0, 2, 5, 1}, {0, 4, 0, 3}}};
    int kalah = v == 1, s = (int)(size / (NC + 2 * FRAME)), f = (int)(s * FRAME), d = (int)(SEED * s + 0.5f);
    int w = NC * s + 2 * f, h = ROWS * s + 2 * f, ox = (size - w) / 2, oy = (size - h) / 2, id = 0;
    make_board(NC, kalah ? 1u : 0u, s, f, ui_buf(w, h));
    art_blend(rgba, size, size, ui_buf(w, h), w, h, ox, oy);
    uint8_t *one = calloc((size_t)d * d, 4), *sh = calloc((size_t)d * d, 4);
    if (!one || !sh) { free(one); free(sh); return; }
    art_flat_shadow(d, sh);
    for (int c = 0; c < NC; c++)
        for (int r = 0; r < 2; r++) {
            int store = kalah && c == 0, n = store ? (r ? 0 : 9) : pits[kalah][r][c];
            for (int k = 0; k < n; k++, id++) {
                float du, dv, px, py;
                spot(store, c * 2 + r, k, &du, &dv);
                px = ox + f + (c + 0.5f + du) * s - d / 2.0f;
                py = oy + f + ((store ? 1.5f : 0.75f + 1.5f * r) + dv) * s - d / 2.0f;
                art_blend(rgba, size, size, sh, d, d, (int)(px + 0.05f * d), (int)(py + 0.08f * d));
                make_seed(id % TINTS, id / TINTS % SHAPES, d, one);
                art_blend(rgba, size, size, one, d, d, (int)px, (int)py);
            }
        }
    free(one);
    free(sh);
}

static int busy(void) { return anim.on; }     /* the end waits for the last seeds to land */

static const game mancala_game = {
    .id = "mancala", .order = 90, .variants = 2,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(mancala_game)
