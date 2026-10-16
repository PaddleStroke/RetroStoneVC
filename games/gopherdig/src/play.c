/*
 * Gopher Dig: the rules (DESIGN.md "Rules"). No drawing, no sound: integer maths and the world's own hash RNG, so every
 * run is deterministic (the determinism and save-state tests) and the rules are tested alone (tests/test_rules.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/gopherdig/LICENSE.
 *
 * The well: COLS columns, endless downwards; a ring of ROWS rows follows the gopher (rows above the ring are air, rows
 * below it are not dug yet: solid). Each row is generated from the seed and the row before it as generated, so every
 * player of a race digs the very same well.
 * The blocks: colour blocks of the same colour that touch (up, down, left, right) stick together as a group; hard rocks
 * and air bubbles are groups of one. A group stands when one of its blocks rests on a standing block of another group
 * (or on the undug rock below the ring); the gopher holds nothing up. A group that loses its support shakes for
 * S_SHAKE frames, then falls: the falling blocks move one row every S_FALL frames, all of a well's at once (the "fall
 * tick"), so they are drawn as one layer; a group lands when one of its blocks meets something; then a group of
 * POP_MIN or more same-coloured blocks touching a block that just landed pops by itself (chains).
 * A block that moves into the gopher's cell crushes it (an air bubble refills it instead): one hit, out.
 */
#include "game.h"
#include <string.h>

static const int16_t T_PARAM[8][STRATA] = {S_SHAKE, S_FALL, S_DRAIN, S_HARD, S_AIRGAP, S_SAME_H, S_SAME_V, S_HOLE};

int stratum_param(int s, int which)
{
    if (s < 0) s = 0;
    int v = T_PARAM[which][s < STRATA ? s : STRATA - 1];
    if (which == P_DRAIN && s >= STRATA) v += DEEP_DRAIN * (s - (STRATA - 1));
    return v;
}

int stratum_of_row(int row) { return row < 0 ? 0 : row / STRATUM_M; }

static uint32_t hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

/* ---- the ring of rows ------------------------------------------------------------------------------------------------- */
static int ring(int r)
{
    int m = r % ROWS;
    return m < 0 ? m + ROWS : m;
}

static gcell *cellp(well *w, int c, int r)
{
    if (c < 0 || c >= COLS || r < w->top || r >= w->top + ROWS) return NULL;
    return &w->g[ring(r)][c];
}

const gcell *well_cell(const well *w, int col, int row) { return cellp((well *)w, col, row); }

int well_kind(const well *w, int col, int row)
{
    if (col < 0 || col >= COLS || row >= w->top + ROWS) return -1;
    if (row < w->top) return K_EMPTY;
    return w->g[ring(row)][col].kind;
}

int well_passable(const well *w, int col, int row)
{
    int k = well_kind(w, col, row);
    if (k == K_EMPTY) return 1;
    return k == K_AIR && w->g[ring(row)][col].st != C_FALL;
}

int well_fall_px(const well *w)
{
    int n = stratum_param(w->go.stratum, P_FALL);
    return w->fall_t * CELL / n;
}

/* a shaking or falling block (not an air bubble) above this cell, with only air between: it will come down here */
int well_danger(const well *w, int col, int row)
{
    for (int r = row - 1; r >= w->top && r >= row - 7; r--) {
        const gcell *c = &w->g[ring(r)][col];
        if (c->kind == K_EMPTY) continue;
        return c->kind != K_AIR && c->st != C_STILL;
    }
    return 0;
}

/* ---- generation --------------------------------------------------------------------------------------------------------- */
static void gen_row(well *w, int r)
{
    gcell *row = w->g[ring(r)];
    memset(row, 0, sizeof(gcell) * COLS);
    if (r < 0) {
        memset(w->gen_prev, 0, sizeof w->gen_prev);
        return;
    }
    if (w->gen_off) {                               /* the rules' tests: a floor of rock */
        for (int c = 0; c < COLS; c++) row[c].kind = K_HARD;
        return;
    }
    int s = stratum_of_row(r);
    int hole = stratum_param(s, P_HOLE), hard = stratum_param(s, P_HARD);
    int sh = stratum_param(s, P_SAME_H), sv = stratum_param(s, P_SAME_V);
    rs_rng rng;
    rs_rng_seed(&rng, hash32(w->seed ^ ((uint32_t)r * 0x9e3779b1u)) | 1u);
    uint8_t cur[COLS];
    for (int c = 0; c < COLS; c++) {
        int k, roll = rs_rng_range(&rng, 1000), a = rs_rng_range(&rng, 1000);
        if (r >= SAFE_ROWS && roll < hole) k = K_EMPTY;
        else if (r >= SAFE_ROWS && roll < hole + hard) k = K_HARD;
        else if (c > 0 && K_IS_COLOUR(cur[c - 1]) && a < sh) k = cur[c - 1];
        else if (K_IS_COLOUR(w->gen_prev[c]) && a < sh + sv) k = w->gen_prev[c];
        else k = K_RED + rs_rng_range(&rng, N_COLOURS);
        cur[c] = (uint8_t)k;
    }
    if (r == w->next_air) {
        cur[rs_rng_range(&rng, COLS)] = K_AIR;
        w->next_air += stratum_param(s, P_AIRGAP) + rs_rng_range(&rng, 4);
    }
    for (int c = 0; c < COLS; c++) row[c].kind = cur[c];
    memcpy(w->gen_prev, cur, sizeof cur);
}

/* the ring follows the gopher: rows far above it are dropped, new rows are dug out of the rock below */
static void well_follow(well *w)
{
    while (w->go.row - ROWS_ABOVE > w->top) {
        gen_row(w, w->top + ROWS);                  /* the same slot as the row dropped */
        w->top++;
        w->dirty = 1;
    }
}

/* ---- groups ------------------------------------------------------------------------------------------------------------- */
typedef struct groups {
    int16_t lab[ROWS][COLS];    /* by ring slot: the group of each cell, -1 none */
    int n;
    int16_t first[ROWS * COLS + 1];     /* the group's cells: pos[first[g] .. first[g + 1]) */
    int16_t pos[ROWS * COLS];           /* ring slot * COLS + col, top row of the ring first */
} groups;

/* label the blocks: colour blocks join their same-coloured neighbours of the same kind of motion (falling ones with
 * falling ones, standing and shaking ones together); hard rocks and air bubbles are alone. moving_only: only the
 * falling blocks and the shaking ones */
static void label(const well *w, groups *G, int skip_fall)
{
    int16_t stack[ROWS * COLS];
    memset(G->lab, 0xff, sizeof G->lab);
    G->n = 0;
    for (int r = w->top; r < w->top + ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            int s = ring(r);
            const gcell *x = &w->g[s][c];
            if (x->kind == K_EMPTY || G->lab[s][c] >= 0 || (skip_fall && x->st == C_FALL)) continue;
            int id = G->n++, sp = 0, fall = x->st == C_FALL;
            G->lab[s][c] = (int16_t)id;
            stack[sp++] = (int16_t)(r - w->top) * COLS + c;
            if (!K_IS_COLOUR(x->kind)) continue;
            while (sp) {
                int q = stack[--sp], qr = q / COLS + w->top, qc = q % COLS;
                static const int8_t d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (int i = 0; i < 4; i++) {
                    int nr = qr + d[i][1], nc = qc + d[i][0];
                    if (nc < 0 || nc >= COLS || nr < w->top || nr >= w->top + ROWS) continue;
                    int ns = ring(nr);
                    const gcell *y = &w->g[ns][nc];
                    if (y->kind != x->kind || G->lab[ns][nc] >= 0 || (y->st == C_FALL) != fall) continue;
                    G->lab[ns][nc] = (int16_t)id;
                    stack[sp++] = (int16_t)((nr - w->top) * COLS + nc);
                }
            }
        }
    /* the cells of each group, in rows from the bottom up (a counting sort) */
    memset(G->first, 0, sizeof(int16_t) * (size_t)(G->n + 1));
    for (int r = w->top; r < w->top + ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (G->lab[ring(r)][c] >= 0) G->first[G->lab[ring(r)][c] + 1]++;
    for (int g = 0; g < G->n; g++) G->first[g + 1] += G->first[g];
    int16_t fill[ROWS * COLS];
    memcpy(fill, G->first, sizeof(int16_t) * (size_t)G->n);
    for (int r = w->top + ROWS - 1; r >= w->top; r--)
        for (int c = 0; c < COLS; c++) {
            int g = G->lab[ring(r)][c];
            if (g >= 0) G->pos[fill[g]++] = (int16_t)(ring(r) * COLS + c);
        }
}

static int slot_row(const well *w, int slot)     /* the row of a ring slot */
{
    int r = w->top + ((slot - ring(w->top)) % ROWS + ROWS) % ROWS;
    return r;
}

/* which groups stand and which have lost their support: shakes start and stop */
void well_settle(well *w)
{
    static groups G;            /* scratch (state_audit.txt) */
    uint8_t stable[ROWS * COLS];
    label(w, &G, 1);
    memset(stable, 0, (size_t)G.n);
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int r = w->top + ROWS - 1; r >= w->top; r--)
            for (int c = 0; c < COLS; c++) {
                int g = G.lab[ring(r)][c];
                if (g < 0 || stable[g]) continue;
                int below = r + 1;
                if (below >= w->top + ROWS) { stable[g] = 1; changed = 1; continue; }
                int gb = G.lab[ring(below)][c];
                if (gb >= 0 && gb != g && stable[gb]) { stable[g] = 1; changed = 1; }
            }
    }
    int shake = stratum_param(w->go.stratum, P_SHAKE);
    w->moving = 0;
    for (int g = 0; g < G.n; g++) {
        int t = 255;
        for (int i = G.first[g]; i < G.first[g + 1]; i++) {
            gcell *x = &w->g[G.pos[i] / COLS][G.pos[i] % COLS];
            if (x->st == C_SHAKE && x->timer < t) t = x->timer;
        }
        if (t == 255) t = shake;
        for (int i = G.first[g]; i < G.first[g + 1]; i++) {
            gcell *x = &w->g[G.pos[i] / COLS][G.pos[i] % COLS];
            if (stable[g]) {
                x->st = C_STILL;
                x->timer = 0;
            } else {
                if (x->st == C_STILL) w->ev |= EV_SHAKE;
                x->st = C_SHAKE;
                x->timer = (uint8_t)t;
                w->moving++;
            }
        }
    }
    for (int s = 0; s < ROWS; s++)
        for (int c = 0; c < COLS; c++)
            if (w->g[s][c].kind != K_EMPTY && w->g[s][c].st == C_FALL) w->moving++;
    w->dirty = 0;
}

/* ---- effects ------------------------------------------------------------------------------------------------------------- */
static void add_fx(world *W, int p, int kind, int col, int row, int colour, int n)
{
    if (W->nfx >= FX_MAX) return;
    fx *f = &W->fx[W->nfx++];
    f->x = (int16_t)(col * CELL);
    f->y = (int16_t)(row * CELL);
    f->kind = (uint8_t)kind;
    f->colour = (uint8_t)colour;
    f->player = (uint8_t)p;
    f->n = (uint8_t)n;
}

static void add_air(gopher *g, int32_t a)
{
    g->air += a;
    if (g->air > AIR_FULL) g->air = AIR_FULL;
    if (g->air < 0) g->air = 0;
}

/* pop a colour group (drilled, or 4+ after a fall): its blocks are gone */
static int pop_group(world *W, int p, int col, int row, int kind)
{
    well *w = &W->w[p];
    int16_t stack[ROWS * COLS];
    int sp = 0, n = 0;
    gcell *x = cellp(w, col, row);
    if (!x || x->kind != kind || x->st == C_FALL) return 0;
    x->kind = K_EMPTY;
    stack[sp++] = (int16_t)((row - w->top) * COLS + col);
    while (sp) {
        int q = stack[--sp], qr = q / COLS + w->top, qc = q % COLS;
        n++;
        add_fx(W, p, w->chain ? FX_CHAIN : FX_POP, qc, qr, kind, n);
        static const int8_t d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int i = 0; i < 4; i++) {
            gcell *y = cellp(w, qc + d[i][0], qr + d[i][1]);
            if (!y || y->kind != kind || y->st == C_FALL) continue;
            y->kind = K_EMPTY;
            stack[sp++] = (int16_t)((qr + d[i][1] - w->top) * COLS + qc + d[i][0]);
        }
    }
    for (int s = 0; s < ROWS; s++)
        for (int c = 0; c < COLS; c++)
            if (w->g[s][c].kind == K_EMPTY) memset(&w->g[s][c], 0, sizeof(gcell));
    w->dirty = 1;
    return n;
}

static void gopher_out(world *W, int p, int how)
{
    gopher *g = &W->w[p].go;
    if (!g->alive) return;
    g->alive = 0;
    g->out = how;
    g->out_t = 0;
    g->act = ACT_IDLE;
    g->walk_to = -1;
    W->w[p].ev |= how == OUT_CRUSH ? EV_CRUSH : EV_NOAIR;
    if (how == OUT_CRUSH) add_fx(W, p, FX_CRUSH, g->col, g->row, 0, 0);
}

static void collect_air(world *W, int p, int col, int row)
{
    well *w = &W->w[p];
    gcell *x = cellp(w, col, row);
    if (!x || x->kind != K_AIR) return;
    memset(x, 0, sizeof *x);
    add_air(&w->go, AIR_ITEM);
    w->go.airs++;
    w->ev |= EV_AIR;
    w->dirty = 1;
    add_fx(W, p, FX_AIR, col, row, 0, 0);
}

/* ---- the fall tick: every falling group moves down a row, or lands; shaken groups start falling ------------------- */
static int gopher_cell(const gopher *g, int col, int row)
{
    if (!g->alive) return 0;
    if (col == g->col && row == g->row) return 1;
    /* walking or climbing out of a cell: still there for the first half */
    if ((g->act == ACT_WALK || g->act == ACT_CLIMB) && g->act_t * 2 < g->act_n && col == g->from_col && row == g->from_row)
        return 1;
    return 0;
}

static void fall_tick(world *W, int p)
{
    well *w = &W->w[p];
    static groups G;            /* scratch (state_audit.txt) */
    uint8_t moved[ROWS * COLS], movable[ROWS * COLS], landed[ROWS][COLS];
    label(w, &G, 0);
    memset(landed, 0, sizeof landed);
    for (int g = 0; g < G.n; g++) {
        const gcell *x = &w->g[G.pos[G.first[g]] / COLS][G.pos[G.first[g]] % COLS];
        movable[g] = x->st == C_FALL || (x->st == C_SHAKE && x->timer == 0);
        moved[g] = 0;
    }
    int any = 1, any_moved = 0;
    while (any) {               /* every group that can move moves once (the lower ones first make room) */
        any = 0;
        for (int g = 0; g < G.n; g++) {
            if (!movable[g] || moved[g]) continue;
            int ok = 1;
            for (int i = G.first[g]; i < G.first[g + 1] && ok; i++) {
                int s = G.pos[i] / COLS, c = G.pos[i] % COLS, r = slot_row(w, s);
                if (r + 1 >= w->top + ROWS) { ok = 0; break; }
                int sb = ring(r + 1);
                if (w->g[sb][c].kind != K_EMPTY && G.lab[sb][c] != g) ok = 0;
            }
            if (!ok) continue;
            /* move it down, its lowest blocks first */
            for (int i = G.first[g]; i < G.first[g + 1]; i++) {
                int s = G.pos[i] / COLS, c = G.pos[i] % COLS, r = slot_row(w, s), sb = ring(r + 1);
                gcell x = w->g[s][c];
                memset(&w->g[s][c], 0, sizeof(gcell));
                G.lab[s][c] = -1;
                x.st = C_FALL;
                x.timer = 0;
                if (gopher_cell(&w->go, c, r + 1)) {
                    if (x.kind == K_AIR) {          /* an air bubble falls on the gopher: breathe */
                        add_air(&w->go, AIR_ITEM);
                        w->go.airs++;
                        w->ev |= EV_AIR;
                        add_fx(W, p, FX_AIR, c, r + 1, 0, 0);
                        G.pos[i] = (int16_t)(sb * COLS + c);
                        continue;
                    }
                    gopher_out(W, p, OUT_CRUSH);
                }
                w->g[sb][c] = x;
                G.lab[sb][c] = (int16_t)g;
                G.pos[i] = (int16_t)(sb * COLS + c);
            }
            moved[g] = 1;
            any = any_moved = 1;
        }
    }
    /* the falling groups that could not move land */
    int any_landed = 0;
    for (int g = 0; g < G.n; g++) {
        if (moved[g]) continue;
        for (int i = G.first[g]; i < G.first[g + 1]; i++) {
            int s = G.pos[i] / COLS, c = G.pos[i] % COLS;
            gcell *x = &w->g[s][c];
            if (x->st != C_FALL) break;
            x->st = C_STILL;
            landed[s][c] = 1;
            any_landed = 1;
        }
    }
    if (any_landed) {
        w->ev |= EV_LAND;
        /* 4+ of a colour touching a block that just landed: they pop by themselves (a chain) */
        label(w, &G, 1);
        int popped = 0;
        for (int g = 0; g < G.n; g++) {
            int n = G.first[g + 1] - G.first[g], hit = 0, s0 = G.pos[G.first[g]] / COLS, c0 = G.pos[G.first[g]] % COLS;
            if (n < POP_MIN || !K_IS_COLOUR(w->g[s0][c0].kind)) continue;
            for (int i = G.first[g]; i < G.first[g + 1]; i++) hit |= landed[G.pos[i] / COLS][G.pos[i] % COLS];
            if (!hit) continue;
            if (!popped) w->chain++;
            popped = 1;
            pop_group(W, p, c0, slot_row(w, s0), w->g[s0][c0].kind);
        }
        if (popped) {
            w->ev |= EV_CHAIN;
            if (w->chain > w->go.best_chain) w->go.best_chain = w->chain;
        }
    }
    if (any_moved || any_landed) w->dirty = 1;
}

/* ---- the gopher ---------------------------------------------------------------------------------------------------------- */
static void act_start(gopher *g, int act, int n, int col, int row)
{
    g->from_col = g->col;
    g->from_row = g->row;
    g->col = col;
    g->row = row;
    g->act = act;
    g->act_t = 0;
    g->act_n = n;
}

static void act_pos(gopher *g)
{
    int x0 = g->from_col * CELL, y0 = g->from_row * CELL, x1 = g->col * CELL, y1 = g->row * CELL;
    int t = g->act_t, n = g->act_n;
    if (g->act == ACT_CLIMB) {                      /* up first, then over the edge */
        int h = n * 6 / 10;
        if (t < h) { g->x = x0; g->y = y0 + (y1 - y0) * t / h; }
        else { g->x = x0 + (x1 - x0) * (t - h) / (n - h); g->y = y1; }
    } else if (g->act == ACT_WALK || g->act == ACT_FALL) {
        g->x = x0 + (x1 - x0) * t / n;
        g->y = y0 + (y1 - y0) * t / n;
    } else {
        g->x = x1;
        g->y = y1;
    }
}

static void drill(world *W, int p)
{
    well *w = &W->w[p];
    gopher *g = &w->go;
    int tc = g->aim == AIM_DOWN ? g->col : g->col + g->face, tr = g->aim == AIM_DOWN ? g->row + 1 : g->row;
    g->drill_dir = g->aim == AIM_DOWN ? 2 : g->face < 0 ? 0 : 1;
    act_start(g, ACT_DRILL, DRILL_FRAMES, g->col, g->row);
    g->drills++;
    gcell *x = cellp(w, tc, tr);
    if (!x || x->kind == K_EMPTY || x->st == C_FALL) {
        w->ev |= EV_WHIFF;
        g->act_n = DRILL_FRAMES / 2;
        return;
    }
    w->ev |= EV_DRILL;
    if (K_IS_COLOUR(x->kind)) {
        w->chain = 0;
        g->pops += pop_group(W, p, tc, tr, x->kind);
        w->ev |= EV_POP;
    } else if (x->kind == K_HARD) {
        x->hits++;
        if (x->hits >= HARD_HITS) {
            memset(x, 0, sizeof *x);
            add_air(g, -HARD_AIR);
            g->hards++;
            w->ev |= EV_HARD_BREAK;
            w->dirty = 1;
            add_fx(W, p, FX_HARD, tc, tr, 0, 0);
        } else {
            w->ev |= EV_HARD_HIT;
            add_fx(W, p, FX_CRACK, tc, tr, 0, x->hits);
        }
    } else if (x->kind == K_AIR) {
        collect_air(W, p, tc, tr);
    }
}

static int can_climb(const well *w, const gopher *g, int d)
{
    int nc = g->col + d, k = well_kind(w, nc, g->row);
    if (k <= K_EMPTY || k == K_AIR) return 0;
    if (w->g[ring(g->row)][nc].st == C_FALL) return 0;
    return well_passable(w, nc, g->row - 1) && well_passable(w, g->col, g->row - 1);
}

static void gopher_step(world *W, int p, const pin *in)
{
    well *w = &W->w[p];
    gopher *g = &w->go;
    if (!g->alive) {
        g->out_t++;
        return;
    }
    add_air(g, -stratum_param(g->stratum, P_DRAIN));
    if (g->air <= 0) {
        gopher_out(W, p, OUT_AIR);
        return;
    }
    if (g->air < AIR_LOW) {
        if (g->low_t % 60 == 0) w->ev |= EV_LOW;
        g->low_t++;
    } else {
        g->low_t = 0;
    }
    if (in->drill_hold) g->drill_held++;
    else g->drill_held = 0;
    if (in->walk_to >= 0) g->walk_to = in->walk_to < COLS ? in->walk_to : COLS - 1;
    if ((in->drill || in->tap_drill) && g->act != ACT_IDLE) {     /* asked during another action: kept a moment */
        g->buf_drill = 12;
        g->buf_tap = in->tap_drill;
    } else if (g->buf_drill > 0) {
        g->buf_drill--;
    }
    if (g->act != ACT_IDLE) {
        g->act_t++;
        act_pos(g);
        if (g->act_t < g->act_n) return;
        g->act = ACT_IDLE;
        g->x = g->col * CELL;
        g->y = g->row * CELL;
    }
    /* the deepest point, the strata */
    if (g->row + 1 > g->depth) g->depth = g->row + 1;
    int s = g->depth / STRATUM_M;
    if (s > g->stratum) {
        g->stratum = s;
        add_air(g, CHECK_AIR);
        w->freeze = CHECK_FREEZE;
        g->cheer_t = CHECK_FREEZE;
        w->ev |= EV_CHECKPOINT;
        return;
    }
    /* nothing under its feet: it falls (into an air bubble: it breathes) */
    if (well_passable(w, g->col, g->row + 1)) {
        if (well_kind(w, g->col, g->row + 1) == K_AIR) collect_air(W, p, g->col, g->row + 1);
        act_start(g, ACT_FALL, CELL / GOPHER_FALL, g->col, g->row + 1);
        well_follow(w);
        return;
    }
    if (in->down) g->aim = AIM_DOWN;
    if (in->dir) {
        g->face = in->dir;
        g->aim = in->down ? AIM_DOWN : AIM_SIDE;
    }
    int tap = in->tap_drill ? in->tap_drill : g->buf_drill > 0 ? g->buf_tap : 0;
    if (tap) {                                      /* touch: the block tapped */
        g->aim = tap == 3 ? AIM_DOWN : AIM_SIDE;
        if (tap < 3) g->face = tap == 1 ? -1 : 1;
        g->walk_to = -1;
        g->buf_drill = g->buf_tap = 0;
        drill(W, p);
        return;
    }
    if (in->drill || g->buf_drill > 0 ||
        (in->drill_hold && g->drill_held >= DRILL_REPEAT && g->drill_held % DRILL_REPEAT == 0)) {
        g->buf_drill = 0;
        drill(W, p);
        return;
    }
    int d = in->dir, touch = 0;
    if (!d && g->walk_to >= 0) {
        if (g->walk_to == g->col) g->walk_to = -1;
        else { d = g->walk_to > g->col ? 1 : -1; touch = 1; }
    }
    if (!d) {
        g->hold_t = 0;
        return;
    }
    g->face = d;
    g->aim = AIM_SIDE;
    int nc = g->col + d;
    if (nc < 0 || nc >= COLS) {
        g->hold_t = 0;
        g->walk_to = -1;
    } else if (well_passable(w, nc, g->row)) {
        if (well_kind(w, nc, g->row) == K_AIR) collect_air(W, p, nc, g->row);
        act_start(g, ACT_WALK, CELL / WALK_SPEED, nc, g->row);
        g->hold_t = 0;
        w->ev |= EV_STEP;
    } else {
        g->hold_t++;
        if ((touch || g->hold_t >= CLIMB_HOLD) && can_climb(w, g, d)) {
            if (well_kind(w, nc, g->row - 1) == K_AIR) collect_air(W, p, nc, g->row - 1);
            act_start(g, ACT_CLIMB, CLIMB_FRAMES, nc, g->row - 1);
            g->hold_t = 0;
            w->ev |= EV_CLIMB;
        } else if (touch) {
            g->walk_to = -1;                        /* blocked: the touch walk stops */
        }
    }
}

/* ---- the world ------------------------------------------------------------------------------------------------------------ */
static void well_init(well *w, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->top = START_ROW - ROWS_ABOVE + 1;
    w->next_air = 7;
    for (int r = w->top; r < w->top + ROWS; r++) gen_row(w, r);
    gopher *g = &w->go;
    g->col = g->from_col = START_COL;
    g->row = g->from_row = START_ROW;
    g->x = g->col * CELL;
    g->y = g->row * CELL;
    g->face = 1;
    g->air = AIR_FULL;
    g->alive = 1;
    g->walk_to = -1;
    w->bot.target_col = START_COL;
    w->bot.target_row = -100;
    w->bot.rng = seed * 2654435761u + 77u;
    w->dirty = 1;
    well_settle(w);
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        well_init(&w->w[p], seed);
        w->w[p].bot.rng ^= (uint32_t)p * 0x51ed27u;
    }
}

static void well_step(world *W, int p, const pin *in)
{
    well *w = &W->w[p];
    if (w->freeze > 0) {                            /* the checkpoint: the well stands still, the gopher cheers */
        w->freeze--;
        if (w->go.cheer_t > 0) w->go.cheer_t--;
        return;
    }
    if (w->dirty) well_settle(w);
    gopher_step(W, p, in);
    if (w->freeze > 0) return;
    if (w->dirty) well_settle(w);
    for (int s = 0; s < ROWS; s++)
        for (int c = 0; c < COLS; c++)
            if (w->g[s][c].st == C_SHAKE && w->g[s][c].timer > 0) w->g[s][c].timer--;
    if (++w->fall_t >= stratum_param(w->go.stratum, P_FALL)) {
        w->fall_t = 0;
        fall_tick(W, p);
        if (w->dirty) well_settle(w);
    }
}

void world_step(world *W, const pin in[MAX_PLAYERS])
{
    W->t++;
    W->nfx = 0;
    for (int p = 0; p < W->players; p++) {
        W->w[p].ev = 0;
        well_step(W, p, &in[p]);
        W->events[p] = W->w[p].ev;
    }
}

int world_running(const world *W)
{
    for (int p = 0; p < W->players; p++)
        if (W->w[p].go.alive) return 1;
    return 0;
}

int world_out_t(const world *W)
{
    int t = 1 << 30;
    for (int p = 0; p < W->players; p++)
        if (!W->w[p].go.alive && W->w[p].go.out_t < t) t = W->w[p].go.out_t;
    return t;
}

/* ---- test hooks --------------------------------------------------------------------------------------------------------- */
void well_set(well *w, int col, int row, int kind)
{
    gcell *x = cellp(w, col, row);
    if (!x) return;
    memset(x, 0, sizeof *x);
    x->kind = (uint8_t)kind;
    w->dirty = 1;
}

void well_clear(well *w)
{
    memset(w->g, 0, sizeof w->g);
    w->gen_off = 1;
    w->fall_t = 0;
    w->dirty = 1;
}


/* ---- the bot ----------------------------------------------------------------------------------------------------------- */
/* The test hook (bot=1), the screenshots and the medal tuning (DESIGN.md "Medals"). Two layers: a rule of thumb (dig
 * down where the rock is soft, fetch air when it runs low, step out from under anything shaking or falling), and a
 * look-ahead that plays each possible move forward on a copy of the well (the rule of thumb playing on) and keeps the
 * one that lives and digs deepest. The human-like bot (skill 1..10) thinks every few frames, holds the pad in between,
 * misjudges the columns and now and then forgets the danger or the look-ahead. */
static int bot_rand(bot_mem *b)
{
    b->rng = b->rng * 1103515245u + 12345u;
    return (int)((b->rng >> 16) & 0x7fff);
}

static int colourish(int k) { return K_IS_COLOUR(k) || k == K_AIR; }

/* the cost of digging down column c from row r: hard rocks below cost, air bubbles and holes help */
static int column_cost(const well *w, int c, int r, int air_low)
{
    static const int wgt[6] = {12, 8, 5, 3, 2, 2};
    int cost = 0;
    for (int i = 0; i < 6; i++) {
        int k = well_kind(w, c, r + 1 + i);
        if (k == K_HARD) cost += wgt[i] * 4;
        if (k == K_AIR) cost -= wgt[i] * (air_low ? 10 : 2);
        if (k == K_EMPTY) cost -= wgt[i] / 2;
    }
    if (well_danger(w, c, r)) cost += 60;
    return cost;
}

static void press(pin *out, int dir, int down, int drill)
{
    out->dir = (int8_t)dir;
    out->down = (uint8_t)down;
    out->drill = (uint8_t)drill;
}

/* the rule of thumb (the gopher is idle) */
static void bot_rule(well *w, int skill, int sloppy, pin *out)
{
    gopher *g = &w->go;
    bot_mem *b = &w->bot;
    int c = g->col, r = g->row;
    int pref = b->target_col > c ? 1 : b->target_col < c ? -1 : ((bot_rand(b) & 1) ? 1 : -1);
    press(out, 0, 0, 0);
    if (well_danger(w, c, r) && !sloppy) {
        /* something is coming down: step aside, else open a side, else dig away down */
        for (int k = 0; k < 2; k++) {
            int d = k ? -pref : pref;
            if (well_passable(w, c + d, r) && !well_danger(w, c + d, r)) { press(out, d, 0, 0); return; }
        }
        for (int k = 0; k < 2; k++) {
            int d = k ? -pref : pref;
            const gcell *x = well_cell(w, c + d, r);
            if (x && K_IS_COLOUR(x->kind) && x->st != C_FALL && !well_danger(w, c + d, r)) { press(out, d, 0, 1); return; }
        }
        press(out, 0, 1, 1);
        return;
    }
    int air_low = g->air < 60000 && !(skill > 0 && bot_rand(b) % 100 < skill * 5);
    if (air_low && (b->target_row != r || b->target_col < 0)) {
        /* short of air: the nearest bubble below */
        int best = -1, bc = 1 << 20;
        for (int rr = r; rr <= r + 10; rr++)
            for (int cc = 0; cc < COLS; cc++)
                if (well_kind(w, cc, rr) == K_AIR) {
                    int cost = (rr - r) * 2 + (cc > c ? cc - c : c - cc) * 3;
                    if (cost < bc) { bc = cost; best = cc; }
                }
        if (best >= 0) {
            b->target_col = best;
            b->target_row = r;
        }
    }
    if (b->target_row != r || b->target_col < 0) {
        int best = c, bc = 1 << 20;
        for (int d = -3; d <= 3; d++) {
            int nc = c + d, ad = d < 0 ? -d : d;
            if (nc < 0 || nc >= COLS) continue;
            int cost = column_cost(w, nc, r, air_low) + ad * (air_low ? 2 : 5);
            for (int k = 1; k <= ad; k++)                  /* rocks on the way there */
                if (well_kind(w, c + (d < 0 ? -k : k), r) == K_HARD) cost += 30;
            if (skill > 0) cost += bot_rand(b) % (skill * 6 + 1);
            if (cost < bc) { bc = cost; best = nc; }
        }
        b->target_col = best;
        b->target_row = r;
    }
    int tc = b->target_col;
    if (tc != c) {
        int d = tc > c ? 1 : -1, nc = c + d;
        const gcell *x = well_cell(w, nc, r);
        if (well_passable(w, nc, r) && !well_danger(w, nc, r)) { press(out, d, 0, 0); return; }
        if (x && colourish(x->kind) && x->st != C_FALL && !well_danger(w, nc, r)) { press(out, d, 0, 1); return; }
        b->target_col = c;                             /* the way is shut: dig here */
    }
    if (well_kind(w, c, r + 1) == K_HARD) {
        /* a rock below: a side with a softer way down? */
        for (int k = 0; k < 2; k++) {
            int d = k ? -pref : pref, nc = c + d, below = well_kind(w, nc, r + 1);
            if (nc < 0 || nc >= COLS || well_danger(w, nc, r) || below == K_HARD || below < 0) continue;
            if (well_passable(w, nc, r) || colourish(well_kind(w, nc, r))) {
                b->target_col = nc;
                press(out, d, 0, !well_passable(w, nc, r));
                return;
            }
        }
    }
    press(out, 0, 1, 1);
}

/* the look-ahead: a copy of the well (scratch, state_audit.txt), this move, then the rule of thumb for `frames`:
 * a score (alive and deep is best) */
static world SIMW;

static int rollout(const world *W, int p, const pin *first, int frames)
{
    SIMW.players = 1;
    SIMW.t = W->t;
    SIMW.seed = W->seed;
    SIMW.w[0] = W->w[p];
    well *w = &SIMW.w[0];
    int depth0 = w->go.row;
    pin in[MAX_PLAYERS];
    memset(in, 0, sizeof in);
    in[0] = *first;
    in[0].walk_to = -1;
    for (int f = 0; f < frames; f++) {
        world_step(&SIMW, in);
        if (!w->go.alive) return f - 100000;
        memset(&in[0], 0, sizeof in[0]);
        in[0].walk_to = -1;
        if (w->go.act == ACT_IDLE && !w->freeze) bot_rule(w, 0, 0, &in[0]);
    }
    int score = (w->go.row - depth0) * 40 + w->go.air / 400;
    if (well_danger(w, w->go.col, w->go.row)) score -= 200;
    return score;
}

void bot_think(world *W, int p, int skill, pin *out)
{
    well *w = &W->w[p];
    gopher *g = &w->go;
    bot_mem *b = &w->bot;
    memset(out, 0, sizeof *out);
    out->walk_to = -1;
    out->any = 1;
    if (!g->alive || w->freeze) return;
    if (skill > 0) {                                /* a human: thinks every few frames, holds the pad between */
        if (b->think_t > 0) {
            b->think_t--;
            out->dir = (int8_t)((b->plan & 3) - 1);
            out->down = (uint8_t)((b->plan >> 2) & 1);
            return;
        }
        b->think_t = skill * 2 + bot_rand(b) % (skill * 3 + 1);
        b->plan = 1;
    }
    if (g->depth > b->last_depth) {
        b->last_depth = g->depth;
        b->stall = 0;
    } else {
        b->stall++;
    }
    if (g->act != ACT_IDLE) return;
    int sloppy = skill > 0 && bot_rand(b) % 100 < skill * 6;
    bot_rule(w, skill, sloppy, out);
    if (!skill || bot_rand(b) % 100 >= skill * 12) {
        /* every move it could make, played forward: the best (the rule's own on a tie) */
        pin cand[6];
        int n = 0, c = g->col, r = g->row;
        cand[n++] = *out;
        for (int d = -1; d <= 1; d += 2) {
            const gcell *x = well_cell(w, c + d, r);
            if (well_passable(w, c + d, r)) { cand[n] = *out; press(&cand[n++], d, 0, 0); }
            else if (x && colourish(x->kind) && x->st != C_FALL) { cand[n] = *out; press(&cand[n++], d, 0, 1); }
        }
        if (well_kind(w, c, r + 1) > K_EMPTY) { cand[n] = *out; press(&cand[n++], 0, 1, 1); }
        int horizon = stratum_param(g->stratum, P_SHAKE) + 6 * stratum_param(g->stratum, P_FALL);
        int best = 0, bs = 0;
        for (int i = 0; i < n; i++) {
            if (i && cand[i].dir == cand[0].dir && cand[i].down == cand[0].down && cand[i].drill == cand[0].drill) continue;
            int s = rollout(W, p, &cand[i], horizon);
            /* stuck on one row for a while (the look-ahead keeps finding the dig too risky): dig, unless it is deadly
             * at once */
            if (b->stall > 150 && cand[i].down && cand[i].drill && s > 40 - 100000) s += 200000;
            if (i == 0 || s > bs) { bs = s; best = i; }
        }
        *out = cand[best];
    }
    if (skill > 0) {
        b->plan = (out->dir + 1) | (out->down << 2);
        if (out->drill) b->plan = 1 | (out->down << 2);    /* a press, not a walk after it */
    }
}
