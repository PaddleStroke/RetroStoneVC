/*
 * Hamster Havoc: the rules (no drawing, no sound): the hamsters' ships and their sunflower seeds, the marching
 * formation of space mice and their crumbs, the cheese shelters, the bonus saucer and its power-ups, the mothership,
 * the waves; and the bot. Fixed point and the world's own RNG only: every run is deterministic.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/hamsterhavoc/LICENSE.
 *
 * The mechanics are those of the 1978 arcade classic of the genre (DESIGN.md "Legal and originality"): a formation
 * that steps sideways, drops a step at each edge and speeds up as it thins out; one shot on screen at a time;
 * shelters eroded pixel by pixel by both sides' shots; a bonus saucer with mystery points.
 */
#include "game.h"
#include <limits.h>
#include <string.h>

/* the formation layouts: a bit per column (bit c = column c), a row each, top to bottom */
#define FULL 0x7FF
static const uint16_t LAYOUTS[][ROWS] = {
    {FULL, FULL, FULL, FULL, FULL},             /* the full block: 55 mice */
    {0x555, FULL, FULL, FULL, FULL},            /* the castle (battlements on top): 50 */
    {0x78F, 0x78F, 0x78F, 0x78F, 0x78F},        /* two wings (columns 4-6 empty): 40 */
    {0x070, 0x0F8, 0x1FC, 0x3FE, FULL},         /* the arrowhead: 35 */
    {0x555, 0x2AA, 0x555, 0x2AA, 0x555},        /* the checkerboard: 28 */
};
/* the layouts of the waves between the motherships, in turn */
static const uint8_t WAVE_LAYOUT[] = {0, 1, 2, 0, 3, 1, 4, 2, 0};
#define WAVE_LAYOUTS ((int)(sizeof WAVE_LAYOUT / sizeof WAVE_LAYOUT[0]))

static const int SHELTER_X[SHELTERS] = SHELTER_XS;

static int imin(int a, int b) { return a < b ? a : b; }
static int iabs(int a) { return a < 0 ? -a : a; }

int mouse_x(const world *w, int col) { return w->fx + col * COL_W; }
int mouse_y(const world *w, int row) { return w->fy + row * ROW_H; }
int mouse_points(int row)
{
    static const int pts[ROWS] = ROW_POINTS;
    return pts[row];
}
int world_is_boss(const world *w) { return w->wave % BOSS_EVERY == 0; }

int bomb_speed(const world *w, int kind)
{
    static const int32_t sp[BK_KINDS] = BOMB_SPEEDS;
    return (int)((int64_t)sp[kind] * (100 + BOMB_WAVE * imin(w->wave - 1, 10)) / 100);
}

int boss_y(const world *w)
{
    static const int bob[6] = {0, 1, 2, 3, 2, 1};
    return BOSS_Y + bob[(w->boss_t / 10) % 6];
}

/* ---- the shelters: a cheese wedge, 24x16, its top sloping up to the right, a mouse hole under it ---------------- */
static int wedge(int x, int y)
{
    if (y < 7 - x * 7 / 23) return 0;                           /* the slope */
    int dx = 2 * x + 1 - SHELTER_W, dy = 2 * y + 1 - 2 * SHELTER_H;
    return dx * dx + dy * dy >= 100;                            /* the mouse hole: radius 5 at the bottom middle */
}

static void shelters_reset(world *w)
{
    for (int s = 0; s < SHELTERS; s++)
        for (int y = 0; y < SHELTER_H; y++) {
            uint32_t row = 0;
            for (int x = 0; x < SHELTER_W; x++)
                if (wedge(x, y)) row |= 1u << x;
            w->shelter[s][y] = row;
        }
}

static int shelter_at(int x)
{
    for (int s = 0; s < SHELTERS; s++)
        if (x >= SHELTER_X[s] && x < SHELTER_X[s] + SHELTER_W) return s;
    return -1;
}

int shelter_solid(const world *w, int x, int y)
{
    if (y < SHELTER_Y || y >= SHELTER_Y + SHELTER_H) return 0;
    int s = shelter_at(x);
    return s >= 0 && (w->shelter[s][y - SHELTER_Y] >> (x - SHELTER_X[s]) & 1);
}

static void clear_px(world *w, int x, int y)
{
    if (y < SHELTER_Y || y >= SHELTER_Y + SHELTER_H) return;
    int s = shelter_at(x);
    if (s >= 0) w->shelter[s][y - SHELTER_Y] &= ~(1u << (x - SHELTER_X[s]));
}

/* a shot bites a round crater and two nibbles around it (where, from the frame: deterministic) */
static void crater(world *w, int x, int y)
{
    for (int dy = -CRATER_R; dy <= CRATER_R; dy++)
        for (int dx = -CRATER_R; dx <= CRATER_R; dx++)
            if (dx * dx + dy * dy <= CRATER_R * CRATER_R + 1) clear_px(w, x + dx, y + dy);
    uint32_t k = (uint32_t)(w->t * 7 + x * 13 + y * 5);
    clear_px(w, x + (int)(k % 7) - 3, y - 3 + (int)((k >> 3) & 1) * 6);
    clear_px(w, x - 3 + (int)((k >> 4) & 1) * 6, y + (int)((k >> 5) % 5) - 2);
    w->wevents |= WEV_SHELTER;
}

/* the mice nibble the cheese they march over */
static void nibble(world *w)
{
    if (mouse_y(w, ROWS - 1) + 14 < SHELTER_Y) return;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (!w->alive[r][c]) continue;
            int x0 = mouse_x(w, c) + 2, y0 = mouse_y(w, r) + 3;
            if (y0 + 11 < SHELTER_Y || y0 >= SHELTER_Y + SHELTER_H) continue;
            for (int y = y0; y < y0 + 12; y++)
                for (int x = x0; x < x0 + 12; x++) clear_px(w, x, y);
        }
}

/* ---- effects (pops, splats, points: drawn by draw.c) ---------------------------------------------------------------- */
static void add_fx(world *w, int kind, int x, int y, int value)
{
    effect *e = &w->fxs[w->fxs_next];
    w->fxs_next = (w->fxs_next + 1) % FX_MAX;
    e->kind = (uint8_t)kind;
    e->x = (int16_t)x;
    e->y = (int16_t)y;
    e->value = (int16_t)value;
    e->t = 0;
    e->on = 1;
}

static void step_fx(world *w)
{
    static const int dur[] = {18, 12, 60, 60, 16};
    for (int i = 0; i < FX_MAX; i++)
        if (w->fxs[i].on && ++w->fxs[i].t >= dur[w->fxs[i].kind]) w->fxs[i].on = 0;
}

/* ---- scoring ------------------------------------------------------------------------------------------------------ */
static void add_score(world *w, int p, int pts)
{
    hero *h = &w->h[p];
    h->score += pts;
}

/* ---- the waves ---------------------------------------------------------------------------------------------------- */
static void clear_shots(world *w)
{
    memset(w->seeds, 0, sizeof w->seeds);
    memset(w->bombs, 0, sizeof w->bombs);
}

void world_start_wave(world *w, int wave)
{
    w->wave = wave;
    clear_shots(w);
    memset(w->alive, 0, sizeof w->alive);
    w->alive_n = 0;
    w->saucer_on = 0;
    w->saucer_t = SAUCER_FIRST;
    w->power_on = 0;
    w->boss_on = 0;
    w->fdir = 1;
    w->fstep_t = 0;
    w->fframe = 0;
    w->beat = 0;
    w->fx = FORM_X0;
    w->fy = FORM_Y0 + imin((wave - 1) * WAVE_LOWER, WAVE_LOWER_MAX);
    w->bomb_t = BOMB_RELOAD;
    w->bomb_k = 0;
    shelters_reset(w);
    if (world_is_boss(w)) {
        w->boss_on = 1;
        w->boss_hp0 = w->boss_hp = BOSS_HP + BOSS_HP_PLAYER * (w->players - 1);
        w->boss_x = 160 << 16;
        w->boss_dir = 1;
        w->boss_fire_t = BOSS_FIRE;
        w->boss_flash = 0;
        w->boss_t = 0;
        w->boss_volley = 0;
        w->layout = -1;
    } else {
        int normal = wave - 1 - (wave - 1) / BOSS_EVERY;     /* the waves before this one that had a formation */
        w->layout = WAVE_LAYOUT[normal % WAVE_LAYOUTS];
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (LAYOUTS[w->layout][r] >> c & 1) {
                    w->alive[r][c] = 1;
                    w->alive_n++;
                }
    }
    w->count0 = w->alive_n;
    w->phase = PH_INTRO;
    w->phase_t = 0;
    w->wevents |= WEV_WAVE;
}

void world_init(world *w, int players, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed;
    rs_rng_seed(&w->rng, seed ? seed : 1);
    w->players = players < 1 ? 1 : players > MAX_PLAYERS ? MAX_PLAYERS : players;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        hero *h = &w->h[p];
        h->state = p < w->players ? SH_READY : SH_OFF;
        h->x = (int32_t)(160 + (2 * p - (w->players - 1)) * SHIP_GAP / 2) << 16;
        h->face = 1;
        h->fire_t = 99;
    }
    world_start_wave(w, 1);
    w->phase = PH_READY;
    w->wevents = 0;
}

int march_interval(const world *w)
{
    if (w->alive_n <= 1) return 1;
    int full = MARCH_FULL * (100 - MARCH_WAVE * imin(w->wave - 1, MARCH_WAVE_MAX)) / 100;
    int n = w->count0 > 1 ? w->count0 - 1 : 1;
    return MARCH_MIN + (full - MARCH_MIN) * (w->alive_n - 1) / n;
}

int world_running(const world *w) { return w->phase != PH_OVER; }

int world_all_down(const world *w)
{
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state != SH_DOWN) return 0;
    return 1;
}

/* ---- the hamsters ------------------------------------------------------------------------------------------------- */
static int seeds_out(const world *w, int p)
{
    int n = 0;
    for (int i = 0; i < MAX_SEEDS; i++) n += w->seeds[p][i].on;
    return n;
}

static void spawn_seed(world *w, int p, int x, int32_t vy)
{
    for (int i = 0; i < MAX_SEEDS; i++) {
        seed *s = &w->seeds[p][i];
        if (s->on) continue;
        s->on = 1;
        s->x = (int32_t)x << 16;
        s->y = (int32_t)(SHIP_Y + 2) << 16;
        s->vy = vy;
        return;
    }
}

static void try_fire(world *w, int p)
{
    hero *h = &w->h[p];
    int n = seeds_out(w, p), x = h->x >> 16;
    if (h->power == PW_DOUBLE) {
        if (n) return;
        spawn_seed(w, p, x - DOUBLE_DX, SEED_SPEED);
        spawn_seed(w, p, x + DOUBLE_DX, SEED_SPEED);
    } else if (h->power == PW_RAPID) {
        if (n >= MAX_SEEDS || h->gap) return;
        spawn_seed(w, p, x, RAPID_SPEED);
        h->gap = RAPID_GAP;
    } else {
        if (n) return;
        spawn_seed(w, p, x, SEED_SPEED);
    }
    h->shots++;
    h->fire_t = 0;
    w->events[p] |= EV_FIRE;
}

/* one hit and the hamster is out (the house rule); the others play on, nothing waits for it */
static void hit(world *w, int p)
{
    hero *h = &w->h[p];
    h->state = SH_POP;
    h->t = 0;
    h->power = PW_NONE;
    h->power_t = 0;
    h->shield_t = 0;
    memset(w->seeds[p], 0, sizeof w->seeds[p]);
    w->events[p] |= EV_HIT | EV_DOWN;
    int left = 0;
    for (int q = 0; q < w->players; q++) left += w->h[q].state == SH_PLAY;
    if (!left) w->freeze = HIT_FREEZE;                         /* the house hit-stop, when the run ends */
}

static void give_power(world *w, int p, int kind)
{
    hero *h = &w->h[p];
    if (kind == PW_SHIELD) {
        h->shield_t = SHIELD_FRAMES;
    } else {
        h->power = kind;
        h->power_t = POWER_FRAMES;
    }
    w->events[p] |= EV_POWER;
    w->msg = kind == PW_DOUBLE ? MSG_DOUBLE : kind == PW_RAPID ? MSG_RAPID : MSG_SHIELD;
    w->msg_t = 0;
    w->msg_player = p;
}

static void step_heroes(world *w, const pin in[MAX_PLAYERS])
{
    for (int p = 0; p < w->players; p++) {
        hero *h = &w->h[p];
        h->t++;
        h->move = 0;
        if (h->fire_t < 999) h->fire_t++;
        switch (h->state) {
        case SH_PLAY: {
            int32_t d;
            if (in[p].target >= 0) {                            /* a touch: slide toward the finger at full speed */
                d = ((int32_t)in[p].target << 16) - h->x;
                if (d > SHIP_SPEED) d = SHIP_SPEED;
                if (d < -SHIP_SPEED) d = -SHIP_SPEED;
            } else {
                d = in[p].dir < 0 ? -SHIP_SPEED : in[p].dir > 0 ? SHIP_SPEED : 0;
            }
            h->x += d;
            if (h->x < (SHIP_MIN_X << 16)) h->x = SHIP_MIN_X << 16;
            if (h->x > (SHIP_MAX_X << 16)) h->x = SHIP_MAX_X << 16;
            h->move = d > 0 ? 1 : d < 0 ? -1 : 0;
            if (h->move) h->face = h->move;
            if (h->power_t && --h->power_t == 0) h->power = PW_NONE;
            if (h->shield_t) h->shield_t--;
            if (h->gap) h->gap--;
            if (in[p].fire && w->phase == PH_PLAY) try_fire(w, p);
            break;
        }
        case SH_POP:
            if (h->t >= POP_FRAMES) {
                h->state = SH_DOWN;
                h->t = 0;
            }
            break;
        default:
            break;
        }
    }
}

/* ---- the seeds ------------------------------------------------------------------------------------------------------ */
static int pop_mouse(world *w, int p, int x, int ytop, int ybot)
{
    if (!w->alive_n) return 0;
    int rel = x - w->fx;
    if (rel < 0) return 0;
    int c = rel / COL_W, off = rel % COL_W;
    if (c >= COLS || off < 2 || off > 13) return 0;
    for (int r = ROWS - 1; r >= 0; r--) {
        if (!w->alive[r][c]) continue;
        int my = mouse_y(w, r);
        if (ybot >= my + 3 && ytop <= my + 14) {
            w->alive[r][c] = 0;
            w->alive_n--;
            add_score(w, p, mouse_points(r));
            add_fx(w, FX_POP, mouse_x(w, c), my, r);
            w->events[p] |= EV_POP;
            w->h[p].pops++;
            return 1;
        }
    }
    return 0;
}

static void shoot_saucer(world *w, int p)
{
    static const int pts[4] = SAUCER_POINTS, odds[4] = SAUCER_ODDS;
    int r = rs_rng_range(&w->rng, 100), k = 0;
    while (k < 3 && r >= odds[k]) r -= odds[k++];
    int sx = w->saucer_x >> 16;
    add_score(w, p, pts[k]);
    add_fx(w, FX_POINTS, sx, SAUCER_Y + 4, pts[k]);
    add_fx(w, FX_BIGPOP, sx, SAUCER_Y + 8, 0);
    w->events[p] |= EV_SAUCER;
    w->saucer_on = 0;
    w->saucer_t = SAUCER_EVERY;
    if (!w->power_on && rs_rng_range(&w->rng, 100) < POWER_CHANCE) {
        w->power_on = 1;
        w->power_kind = rs_rng_range(&w->rng, 100) < SHIELD_ODDS ? PW_SHIELD : 1 + rs_rng_range(&w->rng, 2);
        w->power_x = (int32_t)sx << 16;
        w->power_y = (int32_t)(SAUCER_Y + 4) << 16;
    }
}

static void shoot_boss(world *w, int p, int x, int y)
{
    w->boss_hp--;
    w->boss_flash = 6;
    add_score(w, p, BOSS_HIT_POINTS);
    w->events[p] |= EV_BOSS_HIT;
    add_fx(w, FX_SPLAT, x, y, 1);
    if (w->boss_hp <= 0) {
        w->boss_on = 0;
        add_score(w, p, BOSS_POINTS);
        add_fx(w, FX_BIGPOP, w->boss_x >> 16, boss_y(w) + 16, 1);
        add_fx(w, FX_POINTS, w->boss_x >> 16, boss_y(w) + 4, BOSS_POINTS);
        w->events[p] |= EV_BOSS_DOWN;
    }
}

static void step_seeds(world *w)
{
    for (int p = 0; p < w->players; p++)
        for (int i = 0; i < MAX_SEEDS; i++) {
            seed *s = &w->seeds[p][i];
            if (!s->on) continue;
            int x = s->x >> 16, y0 = s->y >> 16;
            s->y -= s->vy;
            int y1 = s->y >> 16;
            for (int y = y0; y >= y1; y--)                      /* the cheese, from below */
                if (shelter_solid(w, x, y)) {
                    crater(w, x, y);
                    s->on = 0;
                    break;
                }
            if (!s->on) continue;
            for (int b = 0; b < BOMB_MAX; b++) {                /* a seed meets a crumb: both pop */
                bomb *k = &w->bombs[b];
                int bx = k->x >> 16, by = k->y >> 16;
                if (k->on && iabs(bx - x) <= 2 && by <= y0 + 6 && by + 7 >= y1) {
                    k->on = 0;
                    s->on = 0;
                    add_fx(w, FX_SPLAT, x, by + 3, 0);
                    break;
                }
            }
            if (!s->on) continue;
            if (pop_mouse(w, p, x, y1, y0 + 6)) {
                s->on = 0;
                continue;
            }
            if (w->saucer_on && iabs((w->saucer_x >> 16) - x) <= 12 && y1 <= SAUCER_Y + 13 && y0 + 6 >= SAUCER_Y + 3) {
                shoot_saucer(w, p);
                s->on = 0;
                continue;
            }
            if (w->boss_on) {
                int by = boss_y(w);
                if (iabs((w->boss_x >> 16) - x) <= 28 && y1 <= by + 26 && y0 + 6 >= by + 4) {
                    shoot_boss(w, p, x, y1);
                    s->on = 0;
                    continue;
                }
            }
            if (y1 < SEED_TOP) s->on = 0;
        }
}

/* ---- the mice's shots ----------------------------------------------------------------------------------------------- */
static void spawn_bomb(world *w, int kind, int x, int y, int32_t vx, int32_t vy)
{
    for (int i = 0; i < BOMB_MAX; i++) {
        bomb *b = &w->bombs[i];
        if (b->on) continue;
        b->on = 1;
        b->kind = kind;
        b->x = (int32_t)x << 16;
        b->y = (int32_t)y << 16;
        b->vx = vx;
        b->vy = vy;
        w->wevents |= WEV_BOMB;
        return;
    }
}

static void step_bombs(world *w)
{
    for (int i = 0; i < BOMB_MAX; i++) {
        bomb *b = &w->bombs[i];
        if (!b->on) continue;
        int y0 = (b->y >> 16) + 7;
        b->y += b->vy;
        b->x += b->vx;
        int x = b->x >> 16, top = b->y >> 16, y1 = top + 7;
        if (x < -8 || x > 328) {
            b->on = 0;
            continue;
        }
        for (int y = y0 + 1; y <= y1; y++)                      /* the cheese, from above */
            if (shelter_solid(w, x, y)) {
                crater(w, x, y);
                b->on = 0;
                break;
            }
        if (!b->on) continue;
        for (int p = 0; p < w->players; p++) {
            hero *h = &w->h[p];
            int hx = h->x >> 16;
            if (h->state != SH_PLAY) continue;
            if (x + 1 < hx - SHIP_HALF || x - 2 > hx + SHIP_HALF || y1 < SHIP_Y + 6 || top > SHIP_Y + 21) continue;
            if (h->shield_t) {                                  /* the bubble takes it */
                h->shield_t = 0;
                b->on = 0;
                w->events[p] |= EV_SHIELD;
                add_fx(w, FX_BUBBLE, hx, SHIP_Y + 12, 0);
            } else {
                hit(w, p);
            }
            break;
        }
        if (!b->on) continue;
        if (y1 >= MOON_Y) {
            b->on = 0;
            add_fx(w, FX_SPLAT, x, MOON_Y - 3, 0);
            w->wevents |= WEV_SPLAT;
        }
    }
}

/* the bottom mouse of a column (-1: empty) */
static int column_bottom(const world *w, int c)
{
    for (int r = ROWS - 1; r >= 0; r--)
        if (w->alive[r][c]) return r;
    return -1;
}

/* the column nearest c that still has mice */
static int nearest_column(const world *w, int c)
{
    if (c < 0) c = 0;
    if (c >= COLS) c = COLS - 1;
    for (int d = 0; d < COLS; d++) {
        if (c - d >= 0 && column_bottom(w, c - d) >= 0) return c - d;
        if (c + d < COLS && column_bottom(w, c + d) >= 0) return c + d;
    }
    return -1;
}

static int bombs_out(const world *w)
{
    int n = 0;
    for (int i = 0; i < BOMB_MAX; i++) n += w->bombs[i].on;
    return n;
}

/* a ship to aim at (in turn), -1: none */
static int aim_player(const world *w, int k)
{
    int live[MAX_PLAYERS], n = 0;
    for (int p = 0; p < w->players; p++)
        if (w->h[p].state == SH_PLAY) live[n++] = p;
    return n ? live[k % n] : -1;
}

static void drop_bombs(world *w)
{
    if (--w->bomb_t > 0) return;
    int reload = BOMB_RELOAD - BOMB_RELOAD_WAVE * (w->wave - 1);
    w->bomb_t = reload < BOMB_RELOAD_MIN ? BOMB_RELOAD_MIN : reload;
    if (bombs_out(w) >= BOMB_LIMIT + w->players - 1) return;
    int kind = w->bomb_k % BK_KINDS, c;
    w->bomb_k++;
    if (kind == BK_CRUMB) {                                     /* the crumb: from above a ship */
        int p = aim_player(w, w->bomb_k / BK_KINDS);
        int rel = p < 0 ? 0 : (w->h[p].x >> 16) - w->fx - 8 + COL_W / 2;
        c = nearest_column(w, p < 0 ? COLS / 2 : rel < 0 ? 0 : rel / COL_W);
    } else if (kind == BK_PEA) {                                /* the pea: any column */
        c = nearest_column(w, rs_rng_range(&w->rng, COLS));
    } else {                                                    /* the jelly: the columns in a fixed round */
        w->jelly_col = (w->jelly_col + 7) % COLS;
        c = nearest_column(w, w->jelly_col);
    }
    if (c < 0) return;
    int r = column_bottom(w, c);
    spawn_bomb(w, kind, mouse_x(w, c) + 8, mouse_y(w, r) + 12, 0, bomb_speed(w, kind));
}

/* ---- the formation -------------------------------------------------------------------------------------------------- */
static void landed(world *w)
{
    w->landed = 1;
    w->phase = PH_OVER;
    w->wevents |= WEV_LANDED;
    for (int p = 0; p < w->players; p++) {
        hero *h = &w->h[p];
        if (h->state == SH_DOWN || h->state == SH_OFF) continue;
        if (h->state != SH_POP) {
            h->state = SH_POP;
            h->t = 0;
        }
        w->events[p] |= EV_HIT | EV_DOWN;
    }
    clear_shots(w);
}

static void march(world *w)
{
    if (!w->alive_n || ++w->fstep_t < march_interval(w)) return;
    w->fstep_t = 0;
    int dx = w->alive_n == 1 ? MARCH_LAST_DX : MARCH_DX, cmin = COLS, cmax = -1, rmax = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (w->alive[r][c]) {
                if (c < cmin) cmin = c;
                if (c > cmax) cmax = c;
                rmax = r;
            }
    int lx = mouse_x(w, cmin), rx = mouse_x(w, cmax) + 16;
    if ((w->fdir > 0 && rx + dx > BAND_R) || (w->fdir < 0 && lx - dx < BAND_L)) {
        w->fy += MARCH_DY;
        w->fdir = -w->fdir;
    } else {
        w->fx += w->fdir * dx;
    }
    w->fframe ^= 1;
    w->beat = (w->beat + 1) & 3;
    w->wevents |= WEV_BEAT;
    nibble(w);
    if (mouse_y(w, rmax) + 14 >= INVADE_Y) landed(w);
}

/* ---- the saucer, its capsule ---------------------------------------------------------------------------------------- */
static void step_saucer(world *w)
{
    if (world_is_boss(w)) return;
    if (!w->saucer_on) {
        if (w->alive_n < SAUCER_MIN_MICE || --w->saucer_t > 0) return;
        w->saucer_on = 1;
        w->saucer_dir = (w->saucer_n & 1) ? -1 : 1;
        w->saucer_n++;
        w->saucer_x = (int32_t)(w->saucer_dir > 0 ? -24 : 344) << 16;
        w->wevents |= WEV_SAUCER;
        return;
    }
    w->saucer_x += w->saucer_dir * SAUCER_SPEED;
    int x = w->saucer_x >> 16;
    if ((w->saucer_dir > 0 && x > 344) || (w->saucer_dir < 0 && x < -24)) {
        w->saucer_on = 0;
        w->saucer_t = SAUCER_EVERY;
    }
}

static void step_power(world *w)
{
    if (!w->power_on) return;
    w->power_y += POWER_SPEED;
    int x = w->power_x >> 16, y = w->power_y >> 16;
    for (int p = 0; p < w->players; p++) {
        const hero *h = &w->h[p];
        if (h->state != SH_PLAY) continue;
        if (iabs((h->x >> 16) - x) <= 14 && y + 14 >= SHIP_Y + 6 && y <= SHIP_Y + 21) {
            give_power(w, p, w->power_kind);
            w->power_on = 0;
            return;
        }
    }
    if (y > MOON_Y) w->power_on = 0;
}

/* ---- the mothership ---------------------------------------------------------------------------------------------- */
static void step_boss(world *w)
{
    if (!w->boss_on) return;
    if (w->boss_flash) w->boss_flash--;
    w->boss_t++;
    w->boss_x += w->boss_dir * BOSS_SPEED;
    if (w->boss_x > (BOSS_MAX_X << 16)) w->boss_dir = -1;
    if (w->boss_x < (BOSS_MIN_X << 16)) w->boss_dir = 1;
    if (--w->boss_fire_t > 0) return;
    int angry = w->boss_hp * 2 < w->boss_hp0, x = w->boss_x >> 16, y = boss_y(w) + 26;
    w->boss_fire_t = angry ? BOSS_FIRE_ANGRY : BOSS_FIRE;
    for (int k = -1; k <= 1; k++) spawn_bomb(w, BK_PEA, x + 14 * k, y, k * BOSS_SPREAD, BOSS_BOMB_VY);
    if (angry && (w->boss_volley & 1)) {                        /* an aimed crumb at a ship, in turn */
        int p = aim_player(w, w->boss_volley / 2);
        if (p >= 0) {
            int32_t vy = bomb_speed(w, BK_CRUMB), frames = (SHIP_Y + 12 - y) * Q16_ONE / vy;
            int32_t vx = (int32_t)(((int64_t)((w->h[p].x >> 16) - x) << 16) / (frames > 0 ? frames : 1));
            if (vx > 49152) vx = 49152;
            if (vx < -49152) vx = -49152;
            spawn_bomb(w, BK_CRUMB, x, y, vx, vy);
        }
    }
    w->boss_volley++;
    w->wevents |= WEV_BOSS_FIRE;
}

/* ---- a frame ---------------------------------------------------------------------------------------------------------- */
void world_step(world *w, const pin in[MAX_PLAYERS])
{
    for (int p = 0; p < MAX_PLAYERS; p++) w->events[p] = 0;
    w->wevents = 0;
    if (!w->started) {                                          /* the first press (fire) starts the run */
        int go = 0;
        for (int p = 0; p < w->players; p++) go |= in[p].fire;
        if (!go) return;
        w->started = 1;
        w->phase = PH_INTRO;
        w->phase_t = 0;
        w->wevents |= WEV_WAVE;
        for (int p = 0; p < w->players; p++) {
            w->h[p].state = SH_PLAY;
            w->h[p].t = 0;
            w->events[p] |= EV_START;
        }
    }
    if (w->freeze > 0) {                                        /* the hit-stop */
        w->freeze--;
        return;
    }
    w->t++;
    w->phase_t++;
    if (w->msg && ++w->msg_t >= MSG_FRAMES) w->msg = MSG_NONE;
    step_fx(w);
    step_heroes(w, in);
    switch (w->phase) {
    case PH_INTRO:
        if (w->phase_t >= INTRO_FRAMES) {
            w->phase = PH_PLAY;
            w->phase_t = 0;
        }
        break;
    case PH_PLAY:
        step_seeds(w);
        step_bombs(w);
        if (w->phase != PH_PLAY) break;
        step_power(w);
        step_boss(w);
        step_saucer(w);
        march(w);
        if (w->phase != PH_PLAY) break;
        if (w->alive_n) drop_bombs(w);
        if (world_is_boss(w) ? !w->boss_on : !w->alive_n) {     /* the wave is cleared */
            w->phase = PH_CLEAR;
            w->phase_t = 0;
            w->saucer_on = 0;
            w->power_on = 0;
            clear_shots(w);
            w->wevents |= WEV_CLEAR;
        }
        break;
    case PH_CLEAR:
        if (w->phase_t >= CLEAR_FRAMES) world_start_wave(w, w->wave + 1);
        break;
    default:
        break;
    }
    int playing = 0;                                            /* the last hamster popped: the run is over */
    for (int p = 0; p < w->players; p++) playing += w->h[p].state == SH_PLAY;
    if (w->phase != PH_OVER && !playing) w->phase = PH_OVER;
}

/* ---- the bot ---------------------------------------------------------------------------------------------------------
 * It plays from the world: for each place it could slide to (every 3 px), it plays the next frames ahead (its slide,
 * every crumb's fall, the shelters stopping them) and keeps the safe places; of those it picks the one where a seed
 * would hit something (the lowest mice first, the saucer, the mothership, a power-up to catch), not too far. It fires
 * when a seed from where it stands would hit. skill 1..10 (botskill=, a human-like player): it looks less far ahead,
 * dodges tighter and misjudges its aim by up to skill px. */
static uint32_t bhash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 2654435761u ^ (b + 0x9e3779b9u) * 40503u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

/* the line where a falling shot stops: a shelter it hits first, else the moon */
static int bomb_stop(const world *w, const bomb *b)
{
    if (b->vx) return MOON_Y;
    int x = b->x >> 16;
    for (int y = (b->y >> 16) + 7; y < SHELTER_Y + SHELTER_H; y++)
        if (shelter_solid(w, x, y)) return y;
    return MOON_Y;
}

static int path_safe(const world *w, int p, int cx, int look, int margin, const int stop[BOMB_MAX])
{
    int32_t sx = w->h[p].x, target = (int32_t)cx << 16;
    for (int t = 1; t <= look; t++) {
        int32_t d = target - sx;
        sx += d > SHIP_SPEED ? SHIP_SPEED : d < -SHIP_SPEED ? -SHIP_SPEED : d;
        int hx = sx >> 16;
        for (int i = 0; i < BOMB_MAX; i++) {
            const bomb *b = &w->bombs[i];
            if (!b->on || stop[i] < 0) continue;
            int top = (b->y + b->vy * t) >> 16, bx = (b->x + b->vx * t) >> 16;
            if (top + 7 > stop[i]) continue;
            if (bx + 1 + margin >= hx - SHIP_HALF && bx - 2 - margin <= hx + SHIP_HALF && top + 7 >= SHIP_Y + 6 &&
                top <= SHIP_Y + 21)
                return 0;
        }
    }
    return 1;
}

/* what a seed fired from x in ta frames would hit (a value; 0 nothing, -1 our own cheese) */
static int aim_value(const world *w, int p, int x, int ta)
{
    for (int y = SHELTER_Y; y < SHELTER_Y + SHELTER_H; y++)
        if (shelter_solid(w, x, y)) return -1;
    int speed = (w->h[p].power == PW_RAPID ? RAPID_SPEED : SEED_SPEED) >> 16, best = 0;
    if (w->alive_n && w->phase == PH_PLAY) {
        int rmax = 0;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (w->alive[r][c]) rmax = r;
        int flight = (SHIP_Y - mouse_y(w, rmax) - 14) / speed, iv = march_interval(w);
        int steps = (w->fstep_t + ta + flight) / iv;
        int dx = w->alive_n == 1 ? MARCH_LAST_DX : MARCH_DX, fx = w->fx + w->fdir * dx * steps;
        int lx = fx, rx = fx + (COLS - 1) * COL_W + 16, cmin = COLS, cmax = -1;
        for (int c = 0; c < COLS; c++)
            if (column_bottom(w, c) >= 0) {
                if (c < cmin) cmin = c;
                if (c > cmax) cmax = c;
            }
        lx = fx + cmin * COL_W;
        rx = fx + cmax * COL_W + 16;
        if (lx < BAND_L || rx > BAND_R) fx = w->fx;            /* it turns meanwhile: aim where it is */
        int rel = x - fx;
        if (rel >= 0) {
            int c = rel / COL_W, off = rel % COL_W;
            if (c < COLS && off >= 4 && off <= 11) {
                int r = column_bottom(w, c);
                if (r >= 0) best = mouse_points(r) + 6 * r;
            }
        }
    }
    if (w->saucer_on) {
        int flight = (SHIP_Y - SAUCER_Y - 8) / speed;
        int sx = (w->saucer_x + w->saucer_dir * SAUCER_SPEED * (ta + flight)) >> 16;
        if (iabs(sx - x) <= 8 && best < 80) best = 80;
    }
    if (w->boss_on && w->phase == PH_PLAY) {
        int flight = (SHIP_Y - boss_y(w) - 20) / speed;
        int32_t bx = w->boss_x + w->boss_dir * BOSS_SPEED * (ta + flight);
        if (bx > (BOSS_MAX_X << 16) || bx < (BOSS_MIN_X << 16)) bx = w->boss_x;
        if (iabs((bx >> 16) - x) <= 20 && best < 40) best = 40;
    }
    return best;
}

pin bot_decide(const world *w, int p, int skill)
{
    pin o = {0, 0, -1};
    const hero *h = &w->h[p];
    if (!w->started) {
        o.fire = 1;
        return o;
    }
    if (h->state != SH_PLAY) return o;
    int x = h->x >> 16, look = 72 - skill * 5, margin = skill ? 1 : 3, stop[BOMB_MAX];
    for (int i = 0; i < BOMB_MAX; i++) {
        stop[i] = w->bombs[i].on ? bomb_stop(w, &w->bombs[i]) : -1;
        /* a human-like player misses a shot now and then (each shot, by its slot and the frame it was seen) */
        if (skill && stop[i] >= 0 && bhash((uint32_t)i * 977u + (uint32_t)(w->bombs[i].x >> 12), w->seed) % 100 <
                                         (uint32_t)(skill * 2))
            stop[i] = -1;
    }
    int err = 0;
    if (skill) err = (int)(bhash((uint32_t)(w->t / 40), (uint32_t)p * 7919u + w->seed) % (uint32_t)(2 * skill + 1)) - skill;
    int lane = 160 + (2 * p - (w->players - 1)) * 40, best = x, bestv = INT_MIN;
    for (int k = -1; k <= (SHIP_MAX_X - SHIP_MIN_X) / 3; k++) {
        int cx = k < 0 ? x : SHIP_MIN_X + 3 * k;
        if (!path_safe(w, p, cx, look, margin, stop)) continue;
        int ta = iabs(cx - x) * 2 / 3, v = aim_value(w, p, cx + err, ta);
        int score = (v > 0 ? v * 4 : v < 0 ? -20 : 0) - iabs(cx - x) / 3;
        if (w->players > 1) score -= iabs(cx - lane);          /* 2-4 bots: each keeps to its own lane */
        if (w->power_on) {
            int fall = ((SHIP_Y + 6 - 14) - (w->power_y >> 16)) * 4 / 3;
            if (ta <= fall && iabs(cx - (w->power_x >> 16)) <= 8) score += 200;
        }
        if (score > bestv) {
            bestv = score;
            best = cx;
        }
    }
    if (bestv == INT_MIN) {                                     /* nowhere safe: away from the nearest shot */
        int nd = 9999;
        for (int i = 0; i < BOMB_MAX; i++)
            if (w->bombs[i].on && iabs((w->bombs[i].x >> 16) - x) < nd) {
                nd = iabs((w->bombs[i].x >> 16) - x);
                o.dir = (w->bombs[i].x >> 16) <= x ? 1 : -1;
            }
        if (x <= SHIP_MIN_X + 2) o.dir = 1;
        if (x >= SHIP_MAX_X - 2) o.dir = -1;
    } else {
        o.target = best;
    }
    if (aim_value(w, p, x + err, 0) > 0 &&
        (!skill || bhash((uint32_t)w->t, (uint32_t)p + 31u * w->seed) % 100 >= (uint32_t)(skill * 3)))
        o.fire = 1;
    return o;
}
