/*
 * Bao in RetroStone Board: a plank of four rows of eight carved pits, the grey komwe seeds sown one by one by a hand
 * that travels round the player's two rows, captured seeds lifted across the middle and sown again from a kichwa (the
 * rules, Bao la kujifunza: bao_rules.c, bao.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board lies across the room the shell gives, each player's two rows next to them (ivory's at the bottom unless
 * flipped), a groove between the two halves. Each pit shows its count in the corner of its square. A tap on a pit
 * that can be sown: when it can go both ways, its two neighbours along the ring show dots, and a tap on one (or a
 * drag to it) sows that way; when it can go one way only, it sows at once. The pits that can be played glow; a
 * capture's target is ringed.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/bao.h"
#include "shell.h"

#define FRAME 0.32f                             /* the frame, in cells */
#define PIT_R 0.41f                             /* a pit's radius, in cells */
#define SEED 0.2f                               /* a seed, in cells */
#define TINTS 4
#define SLOTS 32                                /* player * 16 + pit */
#define PICK_MS 220
#define TAKE_MS 340
#define DROP_MS 140
#define LONGEST 7000                            /* a move's animation, at most (longer: all its steps quicker) */

typedef struct { uint8_t n[SLOTS], id[SLOTS][BAO_SEEDS]; } heap;

static bao_game G, scratch;
static heap H;
static bao_trace T;
static int cs, fr, sd, made = -1, made_small = -1;
static float bx, by;
static SDL_Texture *t_board, *t_seed[TINTS], *t_small;
static int sel = -1, dragging;                  /* the pit chosen (both ways open), a finger down on it */
static struct { int on, heard, end; Uint32 t0; heap before; int at[BAO_TRACE + 1]; } anim;

static const uint8_t TINT[TINTS][3] = {{150, 150, 140}, {128, 136, 124}, {168, 160, 146}, {118, 118, 116}};

/* ---- the seeds, one by one --------------------------------------------------------------------------------------- */
static void heap_start(heap *h)
{
    memset(h, 0, sizeof *h);
    for (int k = 0; k < BAO_SEEDS; k++) h->id[k / 2][h->n[k / 2]++] = (uint8_t)k;
}

static void heap_push(heap *h, int slot, int id) { h->id[slot][h->n[slot]++] = (uint8_t)id; }

/* one step of a move on the seeds (the hand: the seeds taken up, the first to go first) */
static void heap_step(heap *h, uint8_t *hand, int *nh, const bao_step *s)
{
    int slot = s->player * BAO_RING + s->pit;
    if (s->kind == BAO_DROP) {
        if (*nh) {
            heap_push(h, slot, hand[0]);
            memmove(hand, hand + 1, (size_t)--*nh);
        }
        return;
    }
    for (int k = 0; k < h->n[slot]; k++) hand[(*nh)++] = h->id[slot][k];
    h->n[slot] = 0;
}

static void heap_move(heap *h, const bao_trace *t)
{
    uint8_t hand[BAO_SEEDS];
    int nh = 0;
    for (int i = 0; i < t->n; i++) heap_step(h, hand, &nh, &t->s[i]);
    for (int k = 0; k < nh && t->n; k++) {      /* a move stopped for ever's sake: the hand stays in its pit */
        const bao_step *s = &t->s[t->n - 1];
        heap_push(h, s->player * BAO_RING + s->pit, hand[k]);
    }
}

static void rebuild(void)
{
    bao_new(&scratch);
    heap_start(&H);
    for (int i = 0; i < G.n; i++) {
        bao_play(&scratch, G.move[i], &T);
        heap_move(&H, &T);
    }
    anim.on = 0;
    sel = -1;
    dragging = 0;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
static const bao_pos *now(void) { return bao_now(&G); }

static void start(int variant)
{
    (void)variant;
    bao_new(&G);
    rebuild();
}

static int status(void)
{
    int w = now()->winner;
    return w < 0 ? GS_PLAY : w == 2 ? GS_DRAW : w ? GS_WIN1 : GS_WIN0;
}

static int turn(void) { return now()->turn; }

static void play(int move, int animate)
{
    anim.before = H;
    if (!bao_play(&G, move, &T)) { ui_sound(SND_DENY); return; }
    heap_move(&H, &T);
    sel = -1;
    dragging = 0;
    if (!animate) { ui_sound(T.captures ? SND_CAPTURE : SND_PLACE); return; }
    int t = 0;                                  /* each step's start: drops quick, picks and captures slower */
    for (int i = 0; i < T.n; i++) {
        anim.at[i] = t;
        t += T.s[i].kind == BAO_DROP ? DROP_MS : T.s[i].kind == BAO_TAKE ? TAKE_MS : PICK_MS;
    }
    anim.at[T.n] = t;
    if (t > LONGEST)
        for (int i = 0; i <= T.n; i++) anim.at[i] = (int)((long)anim.at[i] * LONGEST / t);
    anim.end = anim.at[T.n] + 120;
    anim.on = 1;
    anim.heard = 0;
    anim.t0 = ui_ms();
    ui_animate();
}

static int undo(void)
{
    if (!bao_undo(&G)) return 0;
    rebuild();
    return 1;
}

static int script(const char *s)
{
    int ok = 1;
    while (*s && ok) {
        if (*s == ' ') { s++; continue; }
        int m, len = bao_parse(now(), s, &m);
        ok = len && bao_legal(now(), m);
        if (ok) play(m, 0);
        s += len ? len : 1;
    }
    anim.on = 0;
    return ok;
}

const bao_game *bao_live(void) { return &G; }

int bao_show(const char *move)
{
    int m;
    if (anim.on || bao_parse(now(), move, &m) != 3 || move[3] || !bao_legal(now(), m)) return 0;
    play(m, 1);
    return 1;
}

static int busy(void) { return anim.on; }

/* ---- where things are -------------------------------------------------------------------------------------------- */
/* a cell's centre on the screen (col from ivory's left, row from ivory's side), turned round when flipped */
static void cell_px(int col, int row, float *x, float *y)
{
    float u = col + 0.5f, v = 3 - row + 0.5f;
    if (ui.flipped) u = 8 - u, v = 4 - v;
    *x = bx + u * cs;
    *y = by + v * cs;
}

static void pit_px(int player, int pit, float *x, float *y)
{
    int c, r;
    bao_cell(player, pit, &c, &r);
    cell_px(c, r, x, y);
}

static int pit_at(float x, float y, int *player)
{
    float u = (x - bx) / cs, v = (y - by) / cs;
    if (ui.flipped) u = 8 - u, v = 4 - v;
    if (u < 0 || v < 0 || u >= 8 || v >= 4) return -1;
    return bao_at((int)u, 3 - (int)v, player);
}

/* the k-th seed of a pit, from its centre (pixels): a sunflower of up to 9 a layer, the layers piling up */
static void seed_off(int salt, int k, float *dx, float *dy)
{
    int cap = 9, layer = k / cap < 3 ? k / cap : 3, j = k % cap;
    float R = (PIT_R - 0.6f * SEED) * cs * (1 - 0.15f * layer);
    float r = sqrtf((j + 0.4f) / cap) * R, a = j * 2.39996f + layer * 1.1f + salt * 0.7f;
    *dx = cosf(a) * r + (art_hash(salt * 64 + k, 1) - 0.5f) * 0.04f * cs;
    *dy = sinf(a) * r + (art_hash(salt * 64 + k, 2) - 0.5f) * 0.04f * cs - layer * 0.03f * cs;
}

static void seed_px(int slot, int k, float *x, float *y)
{
    float dx, dy;
    pit_px(slot / BAO_RING, slot % BAO_RING, x, y);
    seed_off(slot, k, &dx, &dy);
    *x += dx;
    *y += dy;
}

/* ---- the pictures ------------------------------------------------------------------------------------------------ */
/* the plank for cells of c pixels in a frame of f: 32 round pits carved (the house's recess), a groove across the
 * middle between the two players' halves */
static void paint_board(uint8_t *o, int c, int f)
{
    int w = 8 * c + 2 * f, h = 4 * c + 2 * f;
    art_board_n(8, 4, c, f, 0, o);
    float r = PIT_R * c, rim = fmaxf(1.5f, 0.07f * c);
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 8; col++) {
            float cx = f + (col + 0.5f) * c, cy = f + (row + 0.5f) * c;
            for (int y = (int)(cy - r - 2); y <= (int)(cy + r + 2); y++)
                for (int x = (int)(cx - r - 2); x <= (int)(cx + r + 2); x++) {
                    if (x < 0 || y < 0 || x >= w || y >= h) continue;
                    float k = art_recess(x + 0.5f - cx, y + 0.5f - cy, r, rim, 0.42f, 0.24f, 0.2f);
                    uint8_t *p = o + 4 * ((size_t)y * w + x);
                    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
                }
        }
    float gy = f + 2.0f * c, gw = fmaxf(1.5f, 0.05f * c);
    art_tint_rect(o, w, h, (float)f, gy - gw / 2, (float)(f + 8 * c), gy + gw / 2, ART_INK, 0.55f);
}

/* a komwe seed: a smooth grey sphere, lit from the upper left */
static void paint_seed(int tint, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float r = size / 2.0f - 0.5f;
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            float dx = (x + 0.5f - size / 2.0f) / r, dy = (y + 0.5f - size / 2.0f) / r, d = dx * dx + dy * dy;
            if (d >= 1.0f + 2.0f / r) continue;
            float a = clampf((1 - sqrtf(d)) * r + 0.5f, 0, 1), z = sqrtf(fmaxf(0, 1 - d));
            float lit = 0.55f + 0.5f * fmaxf(0, -0.5f * dx - 0.6f * dy + 0.62f * z);
            float spec = powf(fmaxf(0, -0.45f * dx - 0.55f * dy + 0.7f * z), 24) * 0.55f;
            lit += art_noise(x * 7 + tint * 31, y * 7) * 0.05f;
            uint8_t *p = o + 4 * ((size_t)y * size + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(TINT[tint][i] * lit + 255 * spec, 0, 255);
            p[3] = (uint8_t)(255 * a);
        }
}

static void layout(box b)
{
    float c = fminf(b.w / (8 + 2 * FRAME), b.h / (4 + 2 * FRAME));
    cs = (int)floorf(c);
    fr = (int)(cs * FRAME);
    bx = floorf(b.x + (b.w - 8.0f * cs) / 2);
    by = floorf(b.y + (b.h - 4.0f * cs) / 2);
    if (made != cs) {
        made = cs;
        int w = 8 * cs + 2 * fr, h = 4 * cs + 2 * fr;
        uint8_t *o = malloc((size_t)w * h * 4);
        if (o) {
            paint_board(o, cs, fr);
            ui_free(&t_board);
            t_board = ui_tex(w, h, o);
            free(o);
        }
        sd = (int)(SEED * cs + 0.5f);
        for (int t = 0; t < TINTS; t++) {
            ui_free(&t_seed[t]);
            paint_seed(t, sd, ui_buf(sd, sd));
            t_seed[t] = ui_tex(sd, sd, ui_buf(sd, sd));
        }
    }
    int small = (int)(0.45f * ui.unit);
    if (made_small != small) {
        made_small = small;
        ui_free(&t_small);
        paint_seed(0, small, ui_buf(small, small));
        t_small = ui_tex(small, small, ui_buf(small, small));
    }
}

/* ---- drawing ----------------------------------------------------------------------------------------------------- */
static void seed(int id, float x, float y, float lift)
{
    SDL_Texture *t = t_seed[id % TINTS];
    float d = sd * (1 + 0.5f * lift / cs);
    ui_shadow_flat(x + 0.2f * lift + 0.06f * sd, y + 0.12f * lift + 0.1f * sd, d, d, 0.55f - 0.4f * lift / cs);
    ui_put_c(t, x, y - lift, d, d);
}

/* the hand: the seeds held, a small cluster above a point */
static void hand_at(const uint8_t *hand, int nh, int from, float x, float y)
{
    float lift = 0.38f * cs;
    for (int j = from; j < nh; j++) {
        int i = nh - 1 - j;
        float r = 0.075f * cs * sqrtf((float)i), a = i * 2.39996f;
        seed(hand[j], x + r * cosf(a), y + r * sinf(a), lift);
    }
}

static void step_px(const bao_step *s, float *x, float *y) { pit_px(s->player, s->pit, x, y); }

/* the seeds in the pits; during a move, the steps done on the seeds before it, the step under way in the air */
static void draw_seeds(int *count)
{
    heap d = H;
    uint8_t hand[BAO_SEEDS];
    int nh = 0, i = 0, t = anim.on ? (int)(ui_ms() - anim.t0) : 0;
    if (anim.on) {
        d = anim.before;
        for (; i < T.n && anim.at[i + 1] <= t; i++) heap_step(&d, hand, &nh, &T.s[i]);
        for (; anim.heard < i; anim.heard++) {   /* the sounds of the steps done */
            const bao_step *s = &T.s[anim.heard];
            if (s->kind == BAO_DROP) ui_sound_soft(SND_PLACE, 0.45f);
            else if (s->kind == BAO_TAKE) ui_sound(SND_CAPTURE);
        }
    }
    for (int s = 0; s < SLOTS; s++) {
        count[s] = d.n[s];
        for (int k = 0; k < d.n[s]; k++) {
            float x, y;
            seed_px(s, k, &x, &y);
            seed(d.id[s][k], x, y, 0);
        }
    }
    if (!anim.on || i >= T.n) return;
    const bao_step *s = &T.s[i];
    float u = clampf((t - anim.at[i]) / (float)fmaxf(1, anim.at[i + 1] - anim.at[i]), 0, 1), e = ui_ease(u);
    float x0, y0, x1, y1, lift = 0.38f * cs;
    step_px(s, &x1, &y1);
    if (s->kind == BAO_DROP) {                  /* the hand moves on; its first seed falls into the pit */
        if (i) step_px(&T.s[i - 1], &x0, &y0);
        else x0 = x1, y0 = y1;
        float hx = x0 + (x1 - x0) * e, hy = y0 + (y1 - y0) * e, sx, sy;
        int slot = s->player * BAO_RING + s->pit;
        seed_px(slot, d.n[slot], &sx, &sy);
        hand_at(hand, nh, 1, hx, hy);
        if (nh) seed(hand[0], hx + (sx - hx) * e, hy + (sy - hy) * e, lift * (1 - e));
    } else {                                    /* the seeds of a pit taken up into the hand above it */
        int slot = s->player * BAO_RING + s->pit;
        hand_at(hand, nh, 0, x1, y1);
        if (s->kind == BAO_TAKE && i) {         /* a capture: the hand comes from where the sowing stopped */
            step_px(&T.s[i - 1], &x0, &y0);
            x1 = x0 + (x1 - x0) * fminf(1, 2 * u);
            y1 = y0 + (y1 - y0) * fminf(1, 2 * u);
        }
        for (int k = 0; k < d.n[slot]; k++) {
            float sx, sy;
            seed_px(slot, k, &sx, &sy);
            float r = 0.075f * cs * sqrtf((float)k), a = k * 2.39996f;
            seed(d.id[slot][k], sx + (x1 + r * cosf(a) - sx) * e, sy + (y1 + r * sinf(a) - sy) * e, lift * e);
        }
        count[slot] = (int)((1 - e) * d.n[slot] + 0.5f);
    }
}

static int legal_from(int pit, int *cw, int *ccw)
{
    *cw = bao_legal(now(), bao_mv(pit, 1));
    *ccw = bao_legal(now(), bao_mv(pit, 0));
    return *cw || *ccw;
}

static void draw(void)
{
    const bao_pos *p = now();
    float w = 8.0f * cs + 2 * fr, h = 4.0f * cs + 2 * fr, x, y;
    ui_put(t_board, bx - fr, by - fr, w, h);
    if (anim.on && (int)(ui_ms() - anim.t0) >= anim.end) anim.on = 0;
    if (anim.on) ui_animate();
    int me = p->turn, playing = p->winner < 0 && !anim.on;
    if (G.n && !anim.on) {                      /* the last pit sown */
        const bao_pos *b = &G.pos[G.n - 1];
        pit_px(b->turn, bao_mv_pit(G.move[G.n - 1]), &x, &y);
        ui_mark_last_round(x, y, 2 * PIT_R * cs);
    }
    if (playing)
        for (int i = 0; i < BAO_RING; i++) {
            int cw, ccw;
            if (!legal_from(i, &cw, &ccw)) continue;
            pit_px(me, i, &x, &y);
            if (i == sel) ui_mark_select_round(x, y, 2.1f * PIT_R * cs);
            else ui_mark_gold(x, y, 2.2f * PIT_R * cs);
        }
    int count[SLOTS];
    draw_seeds(count);
    if (playing && sel >= 0) {                  /* the two ways: dots on the neighbours, rings on what they take */
        for (int cw = 0; cw < 2; cw++) {
            int m = bao_mv(sel, cw);
            if (!bao_legal(p, m)) continue;
            pit_px(me, (sel + (cw ? 1 : -1)) & 15, &x, &y);
            ui_mark_dot(x, y, 0.42f * cs);
            if (bao_capturing(p, m)) {
                int n = p->pit[me][sel], last = (sel + (cw ? n : -n)) & 15;
                pit_px(!me, 7 - last, &x, &y);
                ui_mark_ring(x, y, 2.1f * PIT_R * cs);
            }
        }
    }
    static const uint8_t ink[3] = {58, 38, 24};
    for (int s = 0; s < SLOTS; s++) {           /* the counts in the squares' corners, towards the board's edge */
        if (!count[s]) continue;
        int pl = s / BAO_RING, c, r;
        bao_cell(pl, s % BAO_RING, &c, &r);
        cell_px(c, r, &x, &y);
        float off = 0.4f * cs, side = (r < 2) == !ui.flipped ? 1 : -1;
        ui_number_ex(x + off * side, y + off * side, 0.2f * cs, count[s], ink, 210, UI_NUM_TIGHT);
    }
}

/* ---- the pointer ------------------------------------------------------------------------------------------------- */
static int try_way(int pl, int pit)             /* a tap on a neighbour of the pit chosen: that way; 1 played */
{
    if (sel < 0 || pl != now()->turn) return 0;
    for (int cw = 0; cw < 2; cw++)
        if (pit == ((sel + (cw ? 1 : -1)) & 15) && bao_legal(now(), bao_mv(sel, cw))) {
            play(bao_mv(sel, cw), 1);
            return 1;
        }
    return 0;
}

static void press(float x, float y)
{
    if (status() != GS_PLAY || anim.on) return;
    int pl, pit = pit_at(x, y, &pl), cw, ccw;
    if (pit < 0) { sel = -1; return; }
    if (try_way(pl, pit)) return;
    if (pl == now()->turn && legal_from(pit, &cw, &ccw)) {
        if (cw != ccw) { play(bao_mv(pit, cw), 1); return; }
        sel = pit == sel ? -1 : pit;
        dragging = sel >= 0;
        ui_sound(SND_TAP);
        return;
    }
    sel = -1;
    ui_sound(SND_DENY);
}

static void motion(float x, float y) { (void)x; (void)y; }

static void release(float x, float y)
{
    if (!dragging) return;
    dragging = 0;
    int pl, pit = pit_at(x, y, &pl);
    if (pit >= 0 && pit != sel) try_way(pl, pit);   /* dragged from the pit to a neighbour */
}

/* ---- the shell's pictures ---------------------------------------------------------------------------------------- */
static void bar(int player, box b)              /* the seeds the player holds */
{
    ui_bar_number(b, t_small, bao_seeds(now(), player));
}

static void token(int player, int size, uint8_t *rgba) { art_counter(COUNTER_DISC, player, size, rgba); }

/* the menu's card: the board, seeds in its pits as in a game under way */
static void thumb(int variant, int size, uint8_t *rgba)
{
    (void)variant;
    static const uint8_t n[4][8] = {{2, 0, 3, 1, 0, 2, 4, 1}, {1, 5, 0, 2, 3, 0, 2, 6}, {0, 2, 4, 0, 1, 3, 0, 2},
                                    {3, 1, 0, 2, 2, 0, 1, 3}};
    int c = (int)(size / (8 + 2 * FRAME + 0.3f)), f = (int)(c * FRAME), w = 8 * c + 2 * f, h = 4 * c + 2 * f;
    int ox = (size - w) / 2, oy = (size - h) / 2, d = (int)(SEED * c * 1.15f + 0.5f), id = 0;
    uint8_t *o = malloc((size_t)w * h * 4), *one = malloc((size_t)d * d * 4);
    if (!o || !one) { free(o); free(one); return; }
    paint_board(o, c, f);
    art_blend(rgba, size, size, o, w, h, ox, oy);
    for (int r = 0; r < 4; r++)
        for (int col = 0; col < 8; col++)
            for (int k = 0; k < n[r][col]; k++, id++) {
                int cap = 6, j = k % cap;
                float R = (PIT_R - 0.7f * SEED) * c, rr = sqrtf((j + 0.4f) / cap) * R, a = j * 2.39996f + r + col;
                paint_seed(id % TINTS, d, one);
                art_flat_put(rgba, size, size, one, d, ox + f + (col + 0.5f) * c + rr * cosf(a),
                             oy + f + (r + 0.5f) * c + rr * sinf(a));
            }
    free(o);
    free(one);
}

static const game bao_game_def = {
    .id = "bao", .order = 91, .variants = 1,
    .thumb = thumb, .token = token, .start = start, .layout = layout, .draw = draw,
    .press = press, .motion = motion, .release = release, .undo = undo, .turn = turn, .status = status,
    .bar = bar, .script = script, .busy = busy, .fill = 1,
};
REGISTER_GAME(bao_game_def)
