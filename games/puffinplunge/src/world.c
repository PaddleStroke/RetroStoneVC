/*
 * Puffin Plunge: the rules. The shaft (assembled from hand-made chunks), the puffin (run, jump, the bubble boots: in the
 * air A fires down, slows the fall, uses a bubble; landing refills them), the critters of the four zones and the boss,
 * the gems, combos, the side rooms and the shops' offers, the bot. No SDK call (no drawing, no sound): deterministic,
 * its own RNG, the same at every screen size; linked alone by tests/test_world.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/puffinplunge/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <string.h>

/* ---- tables -------------------------------------------------------------------------------------------------------- */
const weapon_info WEAPONS[W_COUNT] = {
    /* period ammo dmg hover */
    {7, 1, 1, -160},                /* bubbles: one straight down */
    {11, 1, 1, -60},                /* spread: three in a fan, a little less lift */
    {16, 2, 3, -420},               /* beam: a piercing ray, two bubbles, a big lift */
    {4, 1, 1, -110},                /* burst: rapid bubbles */
};

const enemy_info EN_INFO[ET_COUNT] = {
    /* beh       hp soft hw hh gems speed */
    {B_WALK,     1, 1,  6, 5, 2,  96},     /* snail */
    {B_FLY,      1, 1,  6, 5, 2,  150},    /* bat */
    {B_STATIC,   2, 0,  6, 6, 3,  0},      /* urchin (spiky) */
    {B_WALK,     2, 1,  7, 5, 3,  176},    /* crab */
    {B_SWIM,     1, 1,  6, 4, 2,  200},    /* fish */
    {B_FLOAT,    2, 0,  6, 6, 3,  64},     /* jellyfish (stings) */
    {B_PUFF,     2, 1,  6, 6, 3,  80},     /* pufferfish: soft, spiky when puffed up */
    {B_WALK,     3, 1,  7, 5, 4,  128},    /* beetle */
    {B_FLY,      1, 1,  6, 5, 2,  190},    /* moth */
    {B_TURRET,   2, 1,  7, 6, 4,  0},      /* puffcap: shoots spores up */
    {B_BULLET,   1, 1,  3, 3, 0,  0},      /* a spore */
    {B_WALK,     3, 1,  7, 5, 4,  150},    /* woolly grub */
    {B_FLY,      2, 1,  6, 5, 3,  230},    /* ice bat */
    {B_DROP,     1, 0,  4, 7, 2,  0},      /* icicle: drops when the puffin passes under */
    {B_WISP,     2, 0,  6, 6, 4,  72},     /* frost wisp: drifts through the rock to the puffin */
    {B_BOSS,     32, 1, 20, 12, 40, 160},  /* the Grumpy Walrus */
    {B_BULLET,   1, 1,  4, 4, 0,  0},      /* a snowball */
};

/* the chunks' critter letters, per zone: w walker, f flyer, x spiky, j floater, t special */
static const uint8_t ROLE[ZONES][5] = {
    {ET_SNAIL, ET_BAT, ET_URCHIN, ET_BAT, ET_SNAIL},
    {ET_CRAB, ET_FISH, ET_URCHIN, ET_JELLY, ET_PUFFER},
    {ET_BEETLE, ET_MOTH, ET_URCHIN, ET_JELLY, ET_PUFFCAP},
    {ET_GRUB, ET_ICEBAT, ET_URCHIN, ET_WISP, ET_ICICLE},
};

/* the shops' prices: base + per level */
static const int16_t PRICE[UP_COUNT][2] = {
    {24, 5}, {12, 3}, {16, 4}, {30, 5}, {36, 6}, {30, 5}, {24, 4}, {24, 4}, {24, 4}, {28, 4}, {24, 4}};

/* ---- small helpers ------------------------------------------------------------------------------------------------- */
static uint32_t rnd(world *w)
{
    uint32_t x = w->rng ? w->rng : 0x9e3779b9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    w->rng = x;
    return x;
}
static int rndn(world *w, int n) { return n > 0 ? (int)(rnd(w) % (uint32_t)n) : 0; }
static int iabs(int v) { return v < 0 ? -v : v; }
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

int zone_of(int level) { return clampi(level, 0, LEVELS - 1) / LEVELS_PER_ZONE; }
int world_level_rows(int level) { return 9 + clampi(level, 0, LEVELS - 1); }    /* chunks: 9 .. 20 */

static void fx(world *w, int kind, int x, int y, int a)
{
    if (w->n_fx >= MAX_FXEV) return;
    w->fx[w->n_fx++] = (fxev){(int16_t)x, (int16_t)y, (uint8_t)kind, (uint8_t)a};
}
static void popn(world *w, int kind, int x, int y, int n)
{
    if (w->n_pops >= MAX_POPS) return;
    w->pops[w->n_pops++] = (pop){(int16_t)x, (int16_t)y, (int16_t)n, (uint8_t)kind, 0};
}
static void dirty_row(world *w, int r)
{
    for (int i = 0; i < w->n_dirty; i++) if (w->dirty[i] == r) return;
    if (w->n_dirty < MAX_DIRTY) w->dirty[w->n_dirty++] = r;
}

static int cell_at(const world *w, int px, int py)
{
    if (px < 0 || px >= SHAFT_W) return C_ROCK;
    int r = py >> 4;
    if (py < 0) return C_ROCK;
    if (r >= w->rows) return C_FLOOR;
    return w->cell[r][px >> 4];
}
static int solid_px(const world *w, int px, int py) { return SOLID(cell_at(w, px, py)); }

/* a box (left, top, right and bottom exclusive, px) touches the rock */
static int box_solid(const world *w, int x0, int y0, int x1, int y1)
{
    for (int y = y0 >> 4; y <= (y1 - 1) >> 4; y++)
        for (int x = x0 >> 4; x <= (x1 - 1) >> 4; x++)
            if (SOLID(cell_at(w, x * 16, y * 16))) return 1;
    return 0;
}
static int hero_solid(const world *w, int cx, int feet)
{
    return box_solid(w, cx - HB_W / 2, feet - HB_H, cx - HB_W / 2 + HB_W, feet);
}

/* ---- chunks ------------------------------------------------------------------------------------------------------- */
int chunks_parse(const char *s, chunk *out, int max, char *err, int errn)
{
    int n = 0, row = -1, line = 0;
    while (s && *s) {
        const char *e = strchr(s, '\n');
        int len = (int)(e ? e - s : (int)strlen(s));
        char buf[128];
        if (len > 127) len = 127;
        memcpy(buf, s, (size_t)len);
        buf[len] = 0;
        while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == ' ')) buf[--len] = 0;
        line++;
        s = e ? e + 1 : NULL;
        if (!len || (buf[0] == '#' && (strchr(buf, ' ') || len != CHUNK_W))) continue;   /* (a rock row starts with # too) */
        if (!strncmp(buf, "chunk ", 6)) {
            if (row >= 0 && row < CHUNK_H) { snprintf(err, (size_t)errn, "line %d: chunk %s has %d rows", line, out[n - 1].name, row); return -1; }
            if (n >= max) { snprintf(err, (size_t)errn, "line %d: too many chunks", line); return -1; }
            chunk *c = &out[n++];
            memset(c, 0, sizeof *c);
            char zones[32] = "";
            int wgt = 1;
            if (sscanf(buf + 6, "%15s %31s %d", c->name, zones, &wgt) < 2) { snprintf(err, (size_t)errn, "line %d: chunk NAME ZONES [WEIGHT]", line); return -1; }
            for (char *p = zones; *p; p++) {
                if (*p == '*') c->zones = 15;
                else if (*p >= '1' && *p <= '4') c->zones |= (uint8_t)(1 << (*p - '1'));
            }
            c->weight = (uint8_t)clampi(wgt, 1, 9);
            row = 0;
            continue;
        }
        if (row < 0 || row >= CHUNK_H || len != CHUNK_W) {
            snprintf(err, (size_t)errn, "line %d: a row of %d characters (10 expected) or outside a chunk", line, len);
            return -1;
        }
        memcpy(out[n - 1].c[row++], buf, CHUNK_W);
    }
    if (row >= 0 && row < CHUNK_H) { snprintf(err, (size_t)errn, "the last chunk has %d rows", row); return -1; }
    return n;
}

static int chunk_solid(char ch) { return ch == '#' || ch == 'c' || ch == '?'; }

/* from top cell (0, sx): falling (with a little air control), walking along the rock and hopping up to 2 cells (the
 * jump: 2.4 cells), never through rock, crystals or '?' cells: does the puffin reach the bottom row? */
static int chunk_reach(const chunk *c, int sx)
{
    uint8_t seen[CHUNK_H][CHUNK_W];
    memset(seen, 0, sizeof seen);
    seen[0][sx] = 1;
#define FREE(y, x) ((y) >= 0 && (y) < CHUNK_H && (x) >= 0 && (x) < CHUNK_W && !chunk_solid(c->c[y][x]))
#define MARK(y, x) do { if (FREE(y, x) && !seen[y][x]) { seen[y][x] = 1; changed = 1; } } while (0)
    for (int it = 0; it < 200; it++) {
        int changed = 0;
        for (int y = 0; y < CHUNK_H; y++)
            for (int x = 0; x < CHUNK_W; x++) {
                if (!seen[y][x]) continue;
                if (FREE(y + 1, x) || y + 1 >= CHUNK_H) {
                    MARK(y + 1, x);
                    for (int d = -1; d <= 1; d += 2) if (FREE(y, x + d)) MARK(y + 1, x + d);
                } else {
                    for (int d = -1; d <= 1; d += 2) {
                        MARK(y, x + d);
                        if (FREE(y - 1, x)) { MARK(y - 1, x + d); if (FREE(y - 2, x)) MARK(y - 2, x + d); }
                    }
                }
            }
        if (!changed) break;
    }
#undef FREE
#undef MARK
    for (int x = 0; x < CHUNK_W; x++)
        if (seen[CHUNK_H - 1][x]) return 1;
    return 0;
}

/* every chunk passable from every column of its top row (chunk_reach; no shot: crystals count as rock); the top and
 * bottom rows are clear */
int chunk_check(const chunk *c, char *err, int errn)
{
    for (int x = 0; x < CHUNK_W; x++) {
        if (chunk_solid(c->c[0][x]) || chunk_solid(c->c[CHUNK_H - 1][x])) {
            snprintf(err, (size_t)errn, "chunk %s: its top and bottom rows must be clear", c->name);
            return 0;
        }
    }
    for (int y = 0; y < CHUNK_H; y++)
        for (int x = 0; x < CHUNK_W; x++) {
            char ch = c->c[y][x];
            if (!strchr(".#c?gGwfxjtD", ch)) { snprintf(err, (size_t)errn, "chunk %s: unknown '%c'", c->name, ch); return 0; }
            if (ch == 'D' && x != 0 && x != CHUNK_W - 1) { snprintf(err, (size_t)errn, "chunk %s: a door D must be against a wall", c->name); return 0; }
            if (ch == 'D' && (y + 1 >= CHUNK_H || !chunk_solid(c->c[y + 1][x]) || c->c[y + 1][x] == '?')) {
                snprintf(err, (size_t)errn, "chunk %s: a door D needs rock '#' under it", c->name);
                return 0;
            }
        }
    for (int x = 0; x < CHUNK_W; x++)
        if (!chunk_reach(c, x)) {
            snprintf(err, (size_t)errn, "chunk %s: no way down from its top's column %d", c->name, x + 1);
            return 0;
        }
    return 1;
}

/* ---- the run and the levels ------------------------------------------------------------------------------------------ */
void world_new_run(world *w, uint32_t seed)
{
    memset(w, 0, sizeof *w);
    w->seed = seed ? seed : 1;
    w->hp = w->hpmax = START_HP;
    w->ammo = w->ammo_max = START_AMMO;
    w->weapon = W_BUBBLE;
    w->boss = -1;
}

static int spawn_enemy(world *w, int type, int x, int y)
{
    int k = w->n_en;
    if (k >= MAX_EN) {                              /* full: a gone one's place (nothing moves: the indices stay) */
        for (k = 0; k < MAX_EN && w->en[k].alive; k++) {}
        if (k >= MAX_EN) return -1;
    } else {
        w->n_en++;
    }
    enemy *e = &w->en[k];
    memset(e, 0, sizeof *e);
    e->type = (uint8_t)type;
    e->x = x * SUB;
    e->y = y * SUB;
    e->hp = EN_INFO[type].hp;
    e->alive = 1;
    e->home_y = (int16_t)y;
    e->face = (uint8_t)(rnd(w) & 1);
    e->t = (uint16_t)rndn(w, 120);
    return k;
}

static void spawn_gem(world *w, int x, int y, int big, int burst)
{
    int k = -1;
    for (int i = 0; i < MAX_GEM; i++) if (!w->gm[i].alive) { k = i; break; }
    if (k < 0) {                                    /* full: the oldest goes */
        int best = 0;
        for (int i = 1; i < MAX_GEM; i++) if (w->gm[i].t > w->gm[best].t) best = i;
        k = best;
    }
    gem *g = &w->gm[k];
    memset(g, 0, sizeof *g);
    g->x = x * SUB;
    g->y = y * SUB;
    g->big = (uint8_t)big;
    g->alive = 1;
    if (burst) {
        g->vx = (int16_t)(rndn(w, 7) * 64 - 192);
        g->vy = (int16_t)(-200 - rndn(w, 360));
    }
}

static void drop_gems(world *w, int x, int y, int n)
{
    if (w->passives & P_LUCKY) n += (n + 1) / 2;
    while (n >= 5) { spawn_gem(w, x, y, 1, 1); n -= 5; }
    while (n-- > 0) spawn_gem(w, x, y, 0, 1);
}

void world_start_level(world *w, int level, const chunk *defs, int ndefs)
{
    level = clampi(level, 0, LEVELS - 1);
    w->level = level;
    w->rng = w->seed * 2654435761u + (uint32_t)level * 40503u + 17u;
    for (int i = 0; i < 4; i++) rnd(w);
    memset(w->cell, 0, sizeof w->cell);
    memset(w->cell_hp, 0, sizeof w->cell_hp);
    w->n_en = 0;
    memset(w->sh, 0, sizeof w->sh);
    memset(w->gm, 0, sizeof w->gm);
    w->ndoors = 0;
    w->boss = -1;
    w->boss_level = level == LEVELS - 1;
    w->t = 0;
    w->combo = w->combo_t = 0;
    w->beam_t = 0;
    w->hitstop = 0;
    w->cleared = w->room = w->won = 0;
    w->n_offers = 0;
    int z = zone_of(level), li = level % LEVELS_PER_ZONE;
    /* the top: a ceiling, a ledge to land on (the puffin starts there), a short drop */
    for (int r = 0; r < TOP_ROWS; r++) {
        w->cell[r][0] = w->cell[r][COLS - 1] = C_ROCK;
        if (r < 2) for (int c = 1; c < COLS - 1; c++) w->cell[r][c] = C_ROCK;
    }
    for (int c = 1; c <= 3; c++) w->cell[5][c] = C_ROCK;
    /* the chunks: weighted picks among the zone's, never the same twice in a row */
    int n = world_level_rows(level), row = TOP_ROWS, last = -1;
    int total = 0;
    for (int i = 0; i < ndefs; i++) if (defs[i].zones & (1 << z)) total += defs[i].weight;
    w->nchunks = 0;
    int spawn_p = 58 + level * 3 + li * 2;          /* % of the critter letters that bring one */
    if (spawn_p > 92) spawn_p = 92;
    for (int k = 0; k < n && k < MAX_CHUNKS; k++) {
        int pick = -1;
        for (int tries = 0; tries < 8 && total > 0; tries++) {
            int v = rndn(w, total);
            for (int i = 0; i < ndefs; i++) {
                if (!(defs[i].zones & (1 << z))) continue;
                v -= defs[i].weight;
                if (v < 0) { pick = i; break; }
            }
            if (pick != last) break;
        }
        if (pick < 0) break;
        last = pick;
        w->chunk_of[w->nchunks++] = (uint8_t)pick;
        const chunk *c = &defs[pick];
        int mirror = rnd(w) & 1;                    /* a chunk may come mirrored: more variety */
        int doors_here = 0;
        for (int y = 0; y < CHUNK_H; y++) {
            int r = row + y;
            w->cell[r][0] = w->cell[r][COLS - 1] = C_ROCK;
            for (int x = 0; x < CHUNK_W; x++) {
                char ch = c->c[y][mirror ? CHUNK_W - 1 - x : x];
                int col = x + 1, px = col * CELL + CELL / 2, py = r * CELL + CELL / 2;
                switch (ch) {
                case '#': w->cell[r][col] = C_ROCK; break;
                case '?': if (rnd(w) % 100 < 55) w->cell[r][col] = C_ROCK; break;
                case 'c': w->cell[r][col] = C_CRYSTAL; w->cell_hp[r][col] = 2; break;
                case 'g': spawn_gem(w, px, py, 0, 0); break;
                case 'G': spawn_gem(w, px, py, 1, 0); break;
                case 'D':
                    if (w->ndoors < 4 && !doors_here && rnd(w) % 100 < 70) {
                        int wc = col == 1 ? 0 : COLS - 1;
                        w->cell[r][wc] = C_DOOR;
                        w->door_row[w->ndoors] = r;
                        w->door_col[w->ndoors] = wc;
                        w->door_kind[w->ndoors] = rnd(w) % 100 < 60 ? 0 : 1;
                        w->ndoors++;
                        doors_here = 1;
                    }
                    break;
                case 'w': case 'f': case 'x': case 'j': case 't': {
                    if ((int)(rnd(w) % 100) >= spawn_p) break;
                    int role = ch == 'w' ? 0 : ch == 'f' ? 1 : ch == 'x' ? 2 : ch == 'j' ? 3 : 4;
                    int type = ROLE[z][role];
                    if (level == 0 && type == ET_URCHIN && rnd(w) % 100 < 50) type = ET_SNAIL;   /* a gentle start */
                    spawn_enemy(w, type, px, py);
                    break;
                }
                default: break;
                }
            }
        }
        row += CHUNK_H;
    }
    /* the bottom: open rows, then the floor (the boss: a taller cave) */
    int open = w->boss_level ? 14 : END_ROWS - 3;
    for (int r = row; r < row + open + 3; r++) {
        w->cell[r][0] = w->cell[r][COLS - 1] = C_ROCK;
        if (r >= row + open) for (int c = 1; c < COLS - 1; c++) w->cell[r][c] = C_FLOOR;
    }
    w->rows = row + open + 3;
    /* the critters settle: walkers and puffcaps on the rock below them, icicles under the rock above */
    for (int i = 0; i < w->n_en; i++) {
        enemy *e = &w->en[i];
        int beh = EN_INFO[e->type].beh, px = e->x / SUB, py = e->y / SUB;
        if (beh == B_WALK || beh == B_TURRET) {
            int y = py;
            while (y < py + 64 && !solid_px(w, px, y + EN_INFO[e->type].hh)) y++;
            e->y = y * SUB;
        } else if (beh == B_DROP) {
            int y = py, ok = 0;
            for (; y > py - 64; y--) if (solid_px(w, px, y - EN_INFO[e->type].hh - 1)) { ok = 1; break; }
            if (ok) e->y = y * SUB;
            else e->type = ET_ICEBAT;
        }
        if (e->type == ET_BAT || e->type == ET_ICEBAT) {     /* bats hang under the rock above, else hover */
            int y = e->y / SUB, ok = 0;
            for (; y > py - 48; y--) if (solid_px(w, px, y - EN_INFO[e->type].hh - 1)) { ok = 1; break; }
            if (ok) e->y = y * SUB;
            else e->st = 2;
        }
        e->home_y = (int16_t)(e->y / SUB);
    }
    if (w->boss_level) {
        w->boss = spawn_enemy(w, ET_WALRUS, SHAFT_W / 2, (row + open) * CELL - EN_INFO[ET_WALRUS].hh);
        w->boss_hpmax = EN_INFO[ET_WALRUS].hp;
    }
    /* the puffin on the ledge, its bubbles full, a fresh shield */
    hero *h = &w->h;
    memset(h, 0, sizeof *h);
    h->x = (CELL * 2 + 8) * SUB;
    h->y = 5 * CELL * SUB;
    h->ground = 1;
    h->shell = (w->passives & P_SHELL) ? 1 : 0;
    w->ammo = w->ammo_max;
}

/* ---- damage --------------------------------------------------------------------------------------------------------- */
static void hurt(world *w, int sx)
{
    hero *h = &w->h;
    if (h->hurt_t || h->dead) return;
    h->hurt_t = HURT_IFRAMES;
    w->hitstop = HITSTOP_HURT;
    h->vy = -HURT_KICK_VY;
    h->vx = sx >= 0 ? 320 : -320;
    if (h->shell) {
        h->shell = 0;
        w->events |= EV_SHIELD;
        fx(w, FX_HURT, h->x / SUB, h->y / SUB - 8, 1);
        return;
    }
    w->hp--;
    w->events |= EV_HURT;
    fx(w, FX_HURT, h->x / SUB, h->y / SUB - 8, 0);
    if (w->hp <= 0) {
        w->hp = 0;
        h->dead = 1;
        w->events |= EV_DEAD;
    }
}

static void kill_enemy(world *w, int i, int by_stomp)
{
    enemy *e = &w->en[i];
    if (!e->alive) return;
    e->alive = 0;
    const enemy_info *in = &EN_INFO[e->type];
    int x = e->x / SUB, y = e->y / SUB;
    if (in->beh != B_BULLET) {
        w->kills++;
        w->score += 5;
        drop_gems(w, x, y, in->gems);
    }
    w->events |= EV_KILL;
    fx(w, FX_KILL, x, y, e->type);
    if (!w->h.ground && in->beh != B_BULLET) {
        w->combo++;
        w->combo_t = 40;
        if (w->combo >= 2) {
            w->events |= EV_COMBO;
            popn(w, POP_COMBO, w->h.x / SUB, w->h.y / SUB - 22, w->combo);
        }
    }
    if (i == w->boss) {
        w->won = 1;
        w->events |= EV_WIN;
        w->score += 500;
        w->hitstop = 30;
        for (int k = 0; k < w->n_en; k++) if (w->en[k].type == ET_SNOWBALL) w->en[k].alive = 0;
    }
    (void)by_stomp;
}

static void damage(world *w, int i, int dmg, int by_stomp)
{
    enemy *e = &w->en[i];
    if (!e->alive) return;
    w->events |= i == w->boss ? EV_BOSSHIT : EV_HIT;
    e->flash = 6;
    if (e->hp <= dmg) kill_enemy(w, i, by_stomp);
    else {
        e->hp = (uint8_t)(e->hp - dmg);
        fx(w, FX_HIT, e->x / SUB, e->y / SUB, e->type);
        if (EN_INFO[e->type].beh == B_PUFF) e->st = 1, e->t = 0;     /* a hit puffs it up */
    }
}

static void break_cell(world *w, int r, int c, int dmg)
{
    if (r < 0 || r >= w->rows || c <= 0 || c >= COLS - 1) return;
    int k = w->cell[r][c];
    if (k != C_CRYSTAL && k != C_CRACKED) return;
    if (w->cell_hp[r][c] > dmg) {
        w->cell_hp[r][c] = (uint8_t)(w->cell_hp[r][c] - dmg);
        w->cell[r][c] = C_CRACKED;
        dirty_row(w, r);
        fx(w, FX_HIT, c * CELL + 8, r * CELL + 8, 255);
        return;
    }
    w->cell[r][c] = C_EMPTY;
    w->cell_hp[r][c] = 0;
    dirty_row(w, r);
    w->events |= EV_BREAK;
    fx(w, FX_BREAK, c * CELL + 8, r * CELL + 8, 0);
    w->score += 2;
    if (rnd(w) % 100 < 70) drop_gems(w, c * CELL + 8, r * CELL + 8, 1 + rndn(w, 3));
}

/* ---- the boots -------------------------------------------------------------------------------------------------------- */
static void add_shot(world *w, int x, int y, int vx, int vy, int dmg, int kind)
{
    for (int i = 0; i < MAX_SHOT; i++) {
        struct shot *s = &w->sh[i];
        if (s->alive) continue;
        s->x = x * SUB;
        s->y = y * SUB;
        s->vx = (int16_t)vx;
        s->vy = (int16_t)vy;
        s->alive = 1;
        s->dmg = (uint8_t)dmg;
        s->kind = (uint8_t)kind;
        s->t = 0;
        return;
    }
}

static void fire_beam(world *w, int x, int y)
{
    int dmg = WEAPONS[W_BEAM].dmg, end = y + BEAM_LEN;
    for (int yy = y; yy < y + BEAM_LEN; yy += 4) {
        int k = cell_at(w, x, yy);
        if (SOLID(k)) {
            if (k == C_CRYSTAL || k == C_CRACKED) break_cell(w, yy >> 4, x >> 4, dmg);
            end = (yy & ~15);
            break;
        }
    }
    if (end < y) end = y;
    for (int i = 0; i < w->n_en; i++) {
        enemy *e = &w->en[i];
        if (!e->alive) continue;
        const enemy_info *in = &EN_INFO[e->type];
        int ex = e->x / SUB, ey = e->y / SUB;
        if (iabs(ex - x) <= in->hw + 4 && ey + in->hh >= y && ey - in->hh <= end) damage(w, i, dmg, 0);
    }
    w->beam_t = BEAM_FRAMES;
    w->beam_x = x;
    w->beam_y0 = y;
    w->beam_y1 = end;
    w->events |= EV_BEAM;
    fx(w, FX_BEAM, x, y, 0);
}

static void fire(world *w)
{
    hero *h = &w->h;
    const weapon_info *wi = &WEAPONS[w->weapon];
    if (w->ammo < wi->ammo) {
        if (w->ammo > 0 && w->weapon == W_BEAM) w->ammo = 0;   /* (the last bubble of a beam: a dud) */
        w->events |= EV_EMPTY;
        h->fire_t = 12;
        return;
    }
    w->ammo -= wi->ammo;
    int x = h->x / SUB, y = h->y / SUB + 2;
    switch (w->weapon) {
    case W_SPREAD:
        add_shot(w, x, y, 0, SHOT_V, wi->dmg, 0);
        add_shot(w, x - 3, y, -SHOT_SIDE_V, SHOT_V - 200, wi->dmg, 1);
        add_shot(w, x + 3, y, SHOT_SIDE_V, SHOT_V - 200, wi->dmg, 1);
        break;
    case W_BEAM: fire_beam(w, x, y); break;
    default: add_shot(w, x, y, 0, SHOT_V, wi->dmg, 0); break;
    }
    int hover = wi->hover;
    if (w->passives & P_FEATHER) hover -= 90;
    if (h->vy > hover) h->vy = hover;
    h->fire_t = wi->period;
    w->events |= EV_SHOT;
    fx(w, FX_SHOT, x, y, w->weapon);
}

/* ---- the puffin ---------------------------------------------------------------------------------------------------------- */
static void land(world *w)
{
    hero *h = &w->h;
    if (w->combo >= 4) {                            /* a combo pays out on landing */
        int bonus = w->combo * 2 + (w->combo >= 8 ? w->combo : 0);
        w->gems += bonus;
        w->score += bonus * 2;
        popn(w, POP_BONUS, h->x / SUB, h->y / SUB - 24, bonus);
        if (w->combo >= 10 && w->hp < w->hpmax) {
            w->hp++;
            popn(w, POP_HEART, h->x / SUB, h->y / SUB - 36, 1);
        }
    }
    if (w->combo > w->best_combo) w->best_combo = w->combo;
    w->combo = 0;
    if (w->ammo < w->ammo_max) w->events |= EV_REFILL;
    w->ammo = w->ammo_max;
    h->land_t = 6;
    w->events |= EV_LAND;
    fx(w, FX_LAND, h->x / SUB, h->y / SUB, 0);
}

static void hero_step(world *w, int in, int pressed, int released)
{
    hero *h = &w->h;
    int dir = ((in & IN_RIGHT) ? 1 : 0) - ((in & IN_LEFT) ? 1 : 0);
    if (h->hurt_t > HURT_IFRAMES - 12) dir = 0;     /* thrown back: no control for a moment */
    /* run */
    int acc = h->ground ? RUN_ACCEL : AIR_ACCEL, dec = h->ground ? RUN_DECEL : AIR_DECEL;
    if (dir) {
        if ((dir > 0 && h->vx < 0) || (dir < 0 && h->vx > 0)) acc = h->ground ? TURN_ACCEL : AIR_ACCEL + 48;
        h->vx += dir * acc;
        if (h->vx > RUN_MAX) h->vx = RUN_MAX;
        if (h->vx < -RUN_MAX) h->vx = -RUN_MAX;
        h->face = dir < 0;
        if (h->ground) h->walk_t++;
    } else {
        if (h->vx > 0) h->vx = h->vx > dec ? h->vx - dec : 0;
        else if (h->vx < 0) h->vx = -h->vx > dec ? h->vx + dec : 0;
    }
    /* jump, fire */
    if (h->fire_t) h->fire_t--;
    if (released & IN_JUMP) {
        h->fire_held = 0;
        if (h->vy < -JUMP_CUT && h->jump_t) h->vy = -JUMP_CUT;
    }
    if (pressed & IN_JUMP) {
        if (h->ground || h->coyote) {
            h->vy = -JUMP_V;
            h->ground = 0;
            h->coyote = 0;
            h->jump_t = 8;
            w->events |= EV_JUMP;
            fx(w, FX_JUMP, h->x / SUB, h->y / SUB, 0);
        } else if (w->ammo > 0) {
            h->fire_held = 1;
            h->fire_t = 0;
        } else {
            h->buffer = JUMP_BUFFER;
            w->events |= EV_EMPTY;
        }
    }
    if (!(in & IN_JUMP)) h->fire_held = 0;
    if (h->fire_held && !h->ground && !h->fire_t) fire(w);
    if (h->jump_t) h->jump_t--;
    /* gravity */
    if (!h->ground) {
        h->vy += GRAVITY;
        if (h->vy > FALL_MAX) h->vy = FALL_MAX;
        h->air_t++;
    }
    /* move: x then y, pixel by pixel against the rock */
    int px = h->x / SUB, py = h->y / SUB;
    int32_t nx = h->x + h->vx;
    int tx = nx / SUB;
    if (tx != px) {
        int d = tx > px ? 1 : -1, p = px;
        for (; p != tx; p += d)
            if (hero_solid(w, p + d, py)) break;
        if (p != tx) { h->x = p * SUB + (d > 0 ? SUB - 1 : 0); h->vx = 0; }
        else h->x = nx;
    } else {
        h->x = nx;
    }
    px = h->x / SUB;
    int32_t ny = h->y + h->vy;
    int ty = ny / SUB, was_ground = h->ground;
    if (ty != py) {
        int d = ty > py ? 1 : -1, p = py, hit = 0;
        for (; p != ty; p += d)
            if (hero_solid(w, px, p + d)) { hit = 1; break; }
        if (hit) {
            h->y = p * SUB + (d > 0 ? 0 : 0);
            if (d > 0) {
                h->y = p * SUB;
                if (!h->ground) { h->ground = 1; h->vy = 0; land(w); }
            } else {
                h->vy = 0;
            }
        } else {
            h->y = ny;
        }
    } else {
        h->y = ny;
    }
    /* still on the rock? */
    int on = hero_solid(w, h->x / SUB, h->y / SUB + 1) && h->vy >= 0;
    if (on && !h->ground && h->vy >= 0) { h->ground = 1; h->vy = 0; h->y = (h->y / SUB) * SUB; land(w); }
    if (!on && h->ground) {
        h->ground = 0;
        h->coyote = COYOTE;
        h->air_t = 0;
    } else if (h->coyote && !h->ground) {
        h->coyote--;
    }
    if (h->ground) {
        h->air_t = 0;
        h->coyote = 0;
        if (h->buffer) {                            /* A pressed just before landing with no bubbles: a jump */
            h->buffer = 0;
            h->vy = -JUMP_V;
            h->ground = 0;
            h->jump_t = 8;
            w->events |= EV_JUMP;
            fx(w, FX_JUMP, h->x / SUB, h->y / SUB, 0);
        }
    }
    if (h->buffer) h->buffer--;
    if (h->land_t) h->land_t--;
    if (h->hurt_t) h->hurt_t--;
    if (h->stomp_t) h->stomp_t--;
    (void)was_ground;
}

/* ---- critters ------------------------------------------------------------------------------------------------------------- */
static int en_solid(const world *w, const enemy *e, int x, int y)
{
    const enemy_info *in = &EN_INFO[e->type];
    return box_solid(w, x - in->hw, y - in->hh, x + in->hw, y + in->hh);
}

/* moves a critter by its speed against the rock; returns 1 (x blocked), 2 (y blocked) */
static int en_move(world *w, enemy *e)
{
    int r = 0;
    int32_t nx = e->x + e->vx, ny = e->y + e->vy;
    if (en_solid(w, e, nx / SUB, e->y / SUB)) { r |= 1; nx = e->x; }
    if (en_solid(w, e, nx / SUB, ny / SUB)) { r |= 2; ny = e->y; }
    e->x = nx;
    e->y = ny;
    return r;
}

static void steer(enemy *e, int tx, int ty, int acc, int vmax)
{
    int dx = tx - e->x / SUB, dy = ty - e->y / SUB;
    e->vx = (int16_t)clampi(e->vx + (dx > 2 ? acc : dx < -2 ? -acc : 0), -vmax, vmax);
    e->vy = (int16_t)clampi(e->vy + (dy > 2 ? acc : dy < -2 ? -acc : 0), -vmax, vmax);
}

static const int8_t SINE16[16] = {0, 5, 9, 12, 13, 12, 9, 5, 0, -5, -9, -12, -13, -12, -9, -5};

static void boss_step(world *w, int i)
{
    enemy *e = &w->en[i];
    const hero *h = &w->h;
    int hx = h->x / SUB, ex = e->x / SUB, floor_y = e->home_y;
    int angry = e->hp * 2 < w->boss_hpmax;
    e->t++;
    switch (e->st) {
    case 0:                                         /* waits for the puffin to come down */
        if (h->y / SUB > floor_y - 150) { e->st = 1; e->t = 0; w->events |= EV_BOSS; }
        break;
    case 1:                                         /* slides along the floor */
        e->vx = (int16_t)(e->face ? -(EN_INFO[ET_WALRUS].speed + angry * 80) : EN_INFO[ET_WALRUS].speed + angry * 80);
        if (en_move(w, e) & 1) e->face ^= 1;
        if (e->t > (angry ? 70 : 100)) { e->st = 2; e->t = 0; e->vx = 0; }
        break;
    case 2:                                         /* rears up and throws snowballs up at the puffin */
        if (e->t == 20 || (angry && e->t == 34) || e->t == 48) {
            int k = spawn_enemy(w, ET_SNOWBALL, ex + (e->face ? -10 : 10), floor_y - 16);
            if (k >= 0) {
                enemy *s = &w->en[k];
                s->vx = (int16_t)clampi((hx - ex) * 6, -360, 360) + (int16_t)(rndn(w, 3) * 60 - 60);
                s->vy = (int16_t)(-900 - rndn(w, 200));
                s->awake = 1;
            }
        }
        if (e->t > 64) { e->st = 3; e->t = 0; }
        break;
    case 3:                                         /* a belly bounce: up and down again */
        if (e->t == 1) e->vy = -700;
        e->vy = (int16_t)(e->vy + 40);
        if (en_move(w, e) & 2) {
            if (e->vy > 0) { e->y = floor_y * SUB; e->vy = 0; e->st = 1; e->t = 0; e->face = hx < ex; }
            e->vy = 0;
        }
        if (e->y / SUB > floor_y) { e->y = floor_y * SUB; e->vy = 0; e->st = 1; e->t = 0; e->face = hx < ex; }
        break;
    }
    if (e->st != 3) { e->y = floor_y * SUB; e->face = e->st == 1 ? e->face : (uint8_t)(hx < ex); }
}

static void enemies_step(world *w)
{
    hero *h = &w->h;
    int hx = h->x / SUB, hy = h->y / SUB - HB_H / 2;
    int sp_mul = 256 + w->level * 12;               /* deeper: a little faster */
    for (int i = 0; i < w->n_en; i++) {
        enemy *e = &w->en[i];
        if (!e->alive) continue;
        const enemy_info *in = &EN_INFO[e->type];
        int ex = e->x / SUB, ey = e->y / SUB;
        if (e->flash) e->flash--;
        if (!e->awake) {
            if (ey < hy - WAKE_ABOVE || ey > hy + WAKE_BELOW) continue;
            e->awake = 1;
        }
        if (in->beh != B_BULLET && in->beh != B_BOSS && (ey < hy - WAKE_ABOVE - 120)) {   /* far behind: asleep */
            e->awake = 0;
            continue;
        }
        e->t++;
        int spd = in->speed * sp_mul / 256;
        switch (in->beh) {
        case B_WALK: {
            int foot = ey + in->hh;
            if (!solid_px(w, ex, foot)) {           /* in the air: falls */
                e->vy = (int16_t)clampi(e->vy + GRAVITY, 0, FALL_MAX);
                e->vx = 0;
                if (en_move(w, e) & 2) e->vy = 0;
                break;
            }
            e->vy = 0;
            int d = e->face ? -1 : 1;
            int ahead = ex + d * (in->hw + 1);
            if (solid_px(w, ahead, ey) || !solid_px(w, ahead, foot)) e->face ^= 1, d = -d;
            e->vx = (int16_t)(d * spd);
            if (en_move(w, e) & 1) e->face ^= 1;
            break;
        }
        case B_FLY:
            if (e->st != 1) {                       /* asleep, hanging (0) or hovering (2): wakes when the puffin comes near */
                if (e->st == 2) e->y = (e->home_y + SINE16[(e->t / 4) & 15] / 3) * SUB;
                if (iabs(hx - ex) < 96 && hy > ey - 120 && hy < ey + 60) { e->st = 1; e->t = 0; }
                break;
            }
            steer(e, hx, hy + (e->type == ET_MOTH ? SINE16[(e->t / 3) & 15] * 2 : 0), 14, spd);
            en_move(w, e);
            e->face = e->vx < 0;
            break;
        case B_SWIM:
            e->vx = (int16_t)(e->face ? -spd : spd);
            e->vy = (int16_t)(SINE16[(e->t / 4) & 15] * 6);
            if (en_move(w, e) & 1) e->face ^= 1;
            break;
        case B_STATIC: break;
        case B_FLOAT: {
            int ty = e->home_y + SINE16[(e->t / 5) & 15];
            e->vy = (int16_t)clampi((ty - ey) * 64, -120, 120);
            e->vx = (int16_t)(e->face ? -spd / 2 : spd / 2);
            if (en_move(w, e) & 1) e->face ^= 1;
            break;
        }
        case B_PUFF:
            if (e->st == 0) {
                e->vx = (int16_t)(e->face ? -spd : spd);
                e->vy = (int16_t)(SINE16[(e->t / 6) & 15] * 4);
                if (en_move(w, e) & 1) e->face ^= 1;
                if (iabs(hx - ex) < 40 && iabs(hy - ey) < 40) { e->st = 1; e->t = 0; }
            } else if (e->t > 100) {
                e->st = 0;
                e->t = 0;
            }
            break;
        case B_TURRET:
            if (e->t % 110 == 60 && hy < ey + 16) {
                int k = spawn_enemy(w, ET_SPORE, ex, ey - 8);
                if (k >= 0) {
                    enemy *s = &w->en[k];
                    e = &w->en[i];
                    s->vx = (int16_t)clampi((hx - ex) * 3, -160, 160);
                    s->vy = -300;
                    s->awake = 1;
                }
            }
            break;
        case B_BULLET:
            e->vy = (int16_t)(e->vy + (e->type == ET_SNOWBALL ? 34 : -4));
            if (e->type == ET_SPORE && e->vy < -220) e->vy = -220;
            e->x += e->vx;
            e->y += e->vy;
            if (solid_px(w, e->x / SUB, e->y / SUB) || e->t > 360 || e->y / SUB > hy + WAKE_BELOW + 64) {
                e->alive = 0;
                fx(w, FX_SPORE_POP, e->x / SUB, e->y / SUB, e->type);
            }
            break;
        case B_DROP:
            if (e->st == 0) {
                if (iabs(hx - ex) < 18 && hy > ey && hy < ey + 140) { e->st = 1; e->t = 0; }
            } else if (e->st == 1) {                /* a shiver, then the fall */
                if (e->t > 14) e->st = 2;
            } else {
                e->vy = (int16_t)clampi(e->vy + GRAVITY, 0, FALL_MAX);
                if (en_move(w, e) & 2) { e->alive = 0; fx(w, FX_BREAK, ex, ey, 1); }
            }
            break;
        case B_WISP:
            steer(e, hx, hy, 5, spd);
            e->x += e->vx;
            e->y += e->vy;
            e->face = e->vx < 0;
            break;
        case B_BOSS: boss_step(w, i); break;
        }
    }
}

/* the puffin against the critters: a stomp from above, else a hurt */
static void contacts(world *w, int32_t prev_feet)
{
    hero *h = &w->h;
    if (h->dead) return;
    int hx0 = h->x / SUB - HB_W / 2, hx1 = hx0 + HB_W, hy1 = h->y / SUB, hy0 = hy1 - HB_H;
    for (int i = 0; i < w->n_en; i++) {
        enemy *e = &w->en[i];
        if (!e->alive) continue;
        const enemy_info *in = &EN_INFO[e->type];
        int hw = in->hw + (in->beh == B_PUFF && e->st ? 2 : 0), hh = in->hh + (in->beh == B_PUFF && e->st ? 2 : 0);
        int ex = e->x / SUB, ey = e->y / SUB;
        if (hx1 <= ex - hw || hx0 >= ex + hw || hy1 <= ey - hh || hy0 >= ey + hh) continue;
        int soft = in->soft && !(in->beh == B_PUFF && e->st);
        int from_above = h->vy > 0 && prev_feet / SUB <= ey - hh + 6;
        if (from_above && !h->stomp_t) {
            h->stomp_t = 6;
            if (soft) {
                int dmg = (w->passives & P_SPRINGS) ? 4 : 2;
                if (i == w->boss) dmg = (w->passives & P_SPRINGS) ? 3 : 2;
                h->vy = -((h->fire_held || h->buffer) ? STOMP_VY_HELD : STOMP_VY) - ((w->passives & P_SPRINGS) ? SPRING_BONUS : 0);
                h->y = (ey - hh) * SUB;
                w->ammo = w->ammo_max;
                w->events |= EV_STOMP | EV_REFILL;
                fx(w, FX_STOMP, ex, ey - hh, 0);
                w->hitstop = HITSTOP_KILL;
                damage(w, i, dmg, 1);
                h->fire_held = 0;
                continue;
            }
            h->vy = -STOMP_VY / 2;                  /* spiky: it bounces the puffin off, and stings */
        }
        if (i == w->boss && e->st == 0) continue;
        hurt(w, hx0 + HB_W / 2 - ex);
    }
}

static void shots_step(world *w)
{
    int hy = w->h.y / SUB;
    for (int k = 0; k < MAX_SHOT; k++) {
        struct shot *s = &w->sh[k];
        if (!s->alive) continue;
        s->t++;
        s->x += s->vx;
        s->y += s->vy;
        int x = s->x / SUB, y = s->y / SUB;
        int c = cell_at(w, x, y + 3);
        if (SOLID(c)) {
            if (c == C_CRYSTAL || c == C_CRACKED) break_cell(w, (y + 3) >> 4, x >> 4, s->dmg);
            else fx(w, FX_HIT, x, y + 3, 254);
            s->alive = 0;
            continue;
        }
        if (y > hy + 280 || x < 0 || x >= SHAFT_W) { s->alive = 0; continue; }
        for (int i = 0; i < w->n_en; i++) {
            enemy *e = &w->en[i];
            if (!e->alive) continue;
            const enemy_info *in = &EN_INFO[e->type];
            int ex = e->x / SUB, ey = e->y / SUB;
            if (iabs(x - ex) <= in->hw + 2 && y + 4 >= ey - in->hh && y - 4 <= ey + in->hh) {
                damage(w, i, s->dmg, 0);
                s->alive = 0;
                break;
            }
        }
    }
    if (w->beam_t) w->beam_t--;
}

static void gems_step(world *w)
{
    hero *h = &w->h;
    int hx = h->x / SUB, hy = h->y / SUB - 7, r = (w->passives & P_MAGNET) ? MAGNET_R_BIG : MAGNET_R;
    for (int k = 0; k < MAX_GEM; k++) {
        gem *g = &w->gm[k];
        if (!g->alive) continue;
        if (g->t < 255) g->t++;
        int gx = g->x / SUB, gy = g->y / SUB, dx = hx - gx, dy = hy - gy;
        if (iabs(dx) < PICK_R && iabs(dy) < PICK_R + 2 && !h->dead) {
            g->alive = 0;
            int v = g->big ? 5 : 1;
            w->gems += v;
            w->score += v;
            w->gems_got += v;
            w->events |= EV_GEM;
            fx(w, FX_GEM, gx, gy, g->big);
            continue;
        }
        if (iabs(dx) < r && iabs(dy) < r && g->t > 16) {     /* the pull: straight at the puffin, faster and faster */
            int sp = 300 + g->rest * 40;
            if (g->rest < 200) g->rest++;
            int ax = iabs(dx), ay = iabs(dy), l = ax > ay ? ax + ay / 2 : ay + ax / 2;
            if (l < 1) l = 1;
            g->x += dx * sp / l;
            g->y += dy * sp / l;
            continue;
        }
        g->rest = 0;
        if (g->vy || g->vx || !solid_px(w, gx, gy + 4)) {   /* a gem thrown out falls back on the rock */
            g->vy = (int16_t)clampi(g->vy + 36, -1000, 520);
            int32_t nx = g->x + g->vx, ny = g->y + g->vy;
            if (solid_px(w, nx / SUB + (g->vx > 0 ? 3 : -3), gy)) { g->vx = (int16_t)(-g->vx / 2); nx = g->x; }
            if (g->vy > 0 && solid_px(w, nx / SUB, ny / SUB + 4)) {
                ny = ((ny / SUB + 4) / CELL * CELL - 4) * SUB;
                g->vy = (int16_t)(g->vy > 160 ? -g->vy / 3 : 0);
                g->vx = (int16_t)(g->vx / 2);
            } else if (g->vy < 0 && solid_px(w, nx / SUB, ny / SUB - 4)) {
                g->vy = 0;
                ny = g->y;
            }
            g->x = nx;
            g->y = ny;
        }
        if (gy > hy + WAKE_BELOW + 200) g->alive = 0;
    }
}

/* the side rooms' doors: the puffin on the ledge, pushing into the door */
static void doors_step(world *w, int in)
{
    hero *h = &w->h;
    if (!h->ground || h->dead) return;
    int hx = h->x / SUB, row = (h->y / SUB - 8) >> 4;
    for (int d = 0; d < w->ndoors; d++) {
        if (w->door_row[d] != row || w->cell[row][w->door_col[d]] != C_DOOR) continue;
        int left = w->door_col[d] == 0;
        if ((left && hx - HB_W / 2 <= CELL + 1 && (in & IN_LEFT)) || (!left && hx + HB_W / 2 >= SHAFT_W - CELL - 1 && (in & IN_RIGHT))) {
            w->room = d + 1;
            w->events |= EV_ROOM;
            world_make_offers(w, 2, w->door_kind[d] ? 2 : 1);
            return;
        }
    }
}


/* the bottom's floor reached: the level is done (the boss's cave: the results come once the walrus is beaten) */
static void bottom_step(world *w)
{
    hero *h = &w->h;
    if (!h->ground || h->dead || w->cleared || w->boss_level) return;
    int r = (h->y / SUB + 1) >> 4;
    if (r < 0 || r >= w->rows || w->cell[r][clampi(h->x / SUB / CELL, 1, COLS - 2)] != C_FLOOR) return;
    w->cleared = 1;
    w->score += 100;
    w->events |= EV_CLEAR;
    world_make_offers(w, MAX_OFFER, 0);
}

void world_step(world *w, int in)
{
    int pressed = in & ~w->prev_in, released = ~in & w->prev_in;
    w->prev_in = in;
    w->events = 0;
    w->n_fx = 0;
    w->n_pops = 0;
    w->n_dirty = 0;
    w->gems_got = 0;
    if (w->hitstop) { w->hitstop--; return; }
    if (w->cleared || w->room || w->h.dead || w->won) return;
    w->t++;
    if (w->combo_t) w->combo_t--;
    int32_t prev_feet = w->h.y;
    hero_step(w, in, pressed, released);
    enemies_step(w);
    shots_step(w);
    contacts(w, prev_feet);
    gems_step(w);
    doors_step(w, in);
    bottom_step(w);
    int d = w->h.y / SUB - TOP_ROWS * CELL;
    if (d > w->depth_px) w->depth_px = d;
    int look = clampi(w->h.vy * 10, -6 * SUB, 36 * SUB);   /* the camera looks ahead of a fall */
    if (w->h.ground) look = 0;
    w->look += (look - w->look) / 12;
}

/* ---- the shops ----------------------------------------------------------------------------------------------------- */
int world_can_buy(const world *w, int up)
{
    switch (up) {
    case UP_HEART: return w->hpmax < MAX_HP;
    case UP_HEAL: return w->hp < w->hpmax;
    case UP_AMMO: return w->ammo_max < MAX_AMMO;
    case UP_SPREAD: return w->weapon != W_SPREAD;
    case UP_BEAM: return w->weapon != W_BEAM;
    case UP_BURST: return w->weapon != W_BURST;
    case UP_MAGNET: return !(w->passives & P_MAGNET);
    case UP_SPRINGS: return !(w->passives & P_SPRINGS);
    case UP_FEATHER: return !(w->passives & P_FEATHER);
    case UP_SHELL: return !(w->passives & P_SHELL);
    case UP_LUCKY: return !(w->passives & P_LUCKY);
    }
    return 0;
}

int world_offer_price(const world *w, int up)
{
    if (up < 0 || up >= UP_COUNT) return 0;
    int p = PRICE[up][0] + PRICE[up][1] * w->level;
    if (w->shop_mode == 1) p = p * 4 / 5;
    if (w->shop_mode == 2) p = 0;
    return p;
}

void world_make_offers(world *w, int n, int mode)
{
    w->shop_mode = mode;
    w->n_offers = 0;
    uint8_t pool[UP_COUNT];
    int np = 0;
    for (int u = 0; u < UP_COUNT; u++) {
        if (!world_can_buy(w, u)) continue;
        if (mode == 2 && u != UP_HEART && u != UP_HEAL && u != UP_AMMO && u != UP_LUCKY && u != UP_MAGNET) continue;
        pool[np++] = (uint8_t)u;
    }
    /* the level's shop always has a heal when the puffin is hurt */
    if (mode == 0 && w->hp < w->hpmax && n > 0) {
        w->offers[w->n_offers++] = (offer){UP_HEAL, 0, (uint16_t)world_offer_price(w, UP_HEAL)};
        for (int i = 0; i < np; i++) if (pool[i] == UP_HEAL) { pool[i] = pool[--np]; break; }
    }
    while (w->n_offers < n && np > 0) {
        int k = rndn(w, np);
        int u = pool[k];
        pool[k] = pool[--np];
        w->offers[w->n_offers++] = (offer){(uint8_t)u, 0, (uint16_t)world_offer_price(w, u)};
    }
}

int world_buy(world *w, int k)
{
    if (k < 0 || k >= w->n_offers) return 0;
    offer *o = &w->offers[k];
    if (o->sold || w->gems < o->price || !world_can_buy(w, o->up)) return 0;
    w->gems -= o->price;
    o->sold = 1;
    switch (o->up) {
    case UP_HEART: w->hpmax++; w->hp++; break;
    case UP_HEAL: w->hp = w->hp + 2 > w->hpmax ? w->hpmax : w->hp + 2; break;
    case UP_AMMO: w->ammo_max += 2; w->ammo = w->ammo_max; break;
    case UP_SPREAD: w->weapon = W_SPREAD; break;
    case UP_BEAM: w->weapon = W_BEAM; break;
    case UP_BURST: w->weapon = W_BURST; break;
    case UP_MAGNET: w->passives |= P_MAGNET; break;
    case UP_SPRINGS: w->passives |= P_SPRINGS; break;
    case UP_FEATHER: w->passives |= P_FEATHER; break;
    case UP_SHELL: w->passives |= P_SHELL; w->h.shell = 1; break;
    case UP_LUCKY: w->passives |= P_LUCKY; break;
    }
    if (w->shop_mode == 2) for (int i = 0; i < w->n_offers; i++) w->offers[i].sold = 1;   /* a gift: one of them */
    return 1;
}

void world_leave_room(world *w)
{
    if (!w->room) return;
    int d = w->room - 1;
    w->cell[w->door_row[d]][w->door_col[d]] = C_SHUT;
    w->n_dirty = 0;
    dirty_row(w, w->door_row[d]);
    w->room = 0;
    w->n_offers = 0;
    w->prev_in = 0;
    w->h.vx = 0;
}

/* ---- CONTINUE ------------------------------------------------------------------------------------------------------------ */
void world_snapshot(const world *w, snapshot *s)
{
    memset(s, 0, sizeof *s);
    s->level = (int8_t)w->level;
    s->hp = (int8_t)w->hp;
    s->hpmax = (int8_t)w->hpmax;
    s->ammo_max = (int8_t)w->ammo_max;
    s->weapon = (int8_t)w->weapon;
    s->passives = (int8_t)w->passives;
    s->gems = (uint16_t)clampi(w->gems, 0, 65535);
    s->kills = (uint16_t)clampi(w->kills, 0, 65535);
    s->seed = w->seed;
    s->score = (uint32_t)w->score;
    s->best_combo = (uint16_t)w->best_combo;
}

void world_resume(world *w, const snapshot *s)
{
    world_new_run(w, s->seed);
    w->level = clampi(s->level, 0, LEVELS - 1);
    w->hpmax = clampi(s->hpmax, 1, MAX_HP);
    w->hp = clampi(s->hp, 1, w->hpmax);
    w->ammo_max = clampi(s->ammo_max, START_AMMO, MAX_AMMO);
    w->ammo = w->ammo_max;
    w->weapon = clampi(s->weapon, 0, W_COUNT - 1);
    w->passives = s->passives & 31;
    w->gems = s->gems;
    w->kills = s->kills;
    w->score = (int)s->score;
    w->best_combo = s->best_combo;
}

uint32_t world_hash(const world *w)
{
    uint32_t h = 2166136261u;
#define HM(v) (h = (h ^ (uint32_t)(v)) * 16777619u)
    HM(w->h.x); HM(w->h.y); HM(w->h.vx); HM(w->h.vy); HM(w->hp); HM(w->ammo); HM(w->gems); HM(w->score);
    HM(w->level); HM(w->combo); HM(w->t); HM(w->rng);
    for (int i = 0; i < w->n_en; i++) if (w->en[i].alive) { HM(w->en[i].x); HM(w->en[i].y); HM(w->en[i].hp); }
#undef HM
    return h;
}

/* ---- the bot ------------------------------------------------------------------------------------------------------------------
 * In the air it steers to the column with the longest clear fall (away from spiky critters) and fires at what is under it;
 * on a ledge it walks to the nearest gap, jumping at a critter in its way. */
static int col_score(const world *w, int col, int hx, int feet)
{
    int x = col * CELL + 8, r0 = feet >> 4, free_rows = 0;
    for (int r = r0; r < r0 + 10 && r < w->rows; r++) {
        if (SOLID(w->cell[r][col])) break;
        free_rows++;
    }
    int s = free_rows * 10 - iabs(x - hx) / 4;
    /* the way there along this row must be open */
    for (int k = hx >> 4; k != col; k += col > (hx >> 4) ? 1 : -1)
        if (SOLID(cell_at(w, k * CELL + 8, feet - 4)) || SOLID(cell_at(w, k * CELL + 8, feet - 12))) { s -= 200; break; }
    for (int i = 0; i < w->n_en; i++) {
        const enemy *e = &w->en[i];
        if (!e->alive) continue;
        int ex = e->x / SUB, ey = e->y / SUB;
        if (ey < feet - 24 || ey > feet + 150 || iabs(ex - x) > 16) continue;
        s -= EN_INFO[e->type].soft ? 6 : 30;
    }
    return s;
}

int bot_input(const world *w, uint32_t *st)
{
    const hero *h = &w->h;
    int hx = h->x / SUB, feet = h->y / SUB, in = 0;
    int was_jump = (*st & 1) != 0;
    int target = hx;
    if (h->ground) {
        int r = (feet + 1) >> 4, row = (feet - 8) >> 4, best = -1, bd = 999;
        if (r >= w->rows) r = w->rows - 1;
        for (int c = 1; c < COLS - 1; c++) {        /* the nearest gap along this ledge */
            if (SOLID(w->cell[r][c])) continue;
            int cx = c * CELL + 8, d = iabs(cx - hx), blocked = 0;
            for (int k = hx >> 4; k != c; k += c > (hx >> 4) ? 1 : -1)     /* a wall: hopped over when low */
                if (SOLID(w->cell[row][k]) && (row < 2 || SOLID(w->cell[row - 2][k]))) {
                    blocked = 1;
                    break;
                }
            if (!blocked && d < bd) { bd = d; best = c; }
        }
        if (best >= 0) target = best * CELL + 8;
        else target = (*st & 2) ? 24 : SHAFT_W - 24;
        if (best < 0 && (hx < 26 || hx > SHAFT_W - 26)) *st ^= 2;
        int dir = target > hx + 3 ? 1 : target < hx - 3 ? -1 : 0;
        if (dir && SOLID(cell_at(w, hx + dir * (HB_W / 2 + 3), feet - 4)) && !was_jump) in |= IN_JUMP;
        for (int i = 0; i < w->n_en; i++) {         /* a critter on the ledge, in the way: jump (onto it) */
            const enemy *e = &w->en[i];
            if (!e->alive) continue;
            int ex = e->x / SUB, ey = e->y / SUB;
            if (iabs(ey - (feet - 8)) < 14 && iabs(ex - hx) < 34 && ((ex > hx) == (target > hx) || iabs(ex - hx) < 14))
                if (!was_jump) in |= IN_JUMP;
        }
    } else {
        int best = -9999;
        for (int c = 1; c < COLS - 1; c++) {
            int s = col_score(w, c, hx, feet);
            if (s > best) { best = s; target = c * CELL + 8; }
        }
        int shoot = 0;                              /* fire at what is under the puffin */
        for (int i = 0; i < w->n_en; i++) {
            const enemy *e = &w->en[i];
            if (!e->alive) continue;
            int ex = e->x / SUB, ey = e->y / SUB;
            if (ey > feet - 4 && ey < feet + 120 && iabs(ex - hx) < 14) shoot = 1;
            if (ey > feet - 40 && ey < feet + 40 && iabs(ex - hx) < 30 && !EN_INFO[e->type].soft) shoot = 1;
        }
        int below = w->cell[clampi((feet + 6) >> 4, 0, w->rows - 1)][clampi(hx >> 4, 0, COLS - 1)];
        if (below == C_CRYSTAL || below == C_CRACKED) shoot = 1;
        if (h->vy < 0 && h->jump_t) in |= IN_JUMP;   /* a jump held to its top */
        else if (shoot && w->ammo > 0 && h->air_t > COYOTE + 1) {
            if (h->fire_held || !was_jump) in |= IN_JUMP;
        }
    }
    if (target > hx + 3) in |= IN_RIGHT;
    else if (target < hx - 3) in |= IN_LEFT;
    *st = (*st & ~1u) | ((in & IN_JUMP) ? 1u : 0u);
    return in;
}

int bot_shop(const world *w)
{
    static const uint8_t pref[] = {UP_HEAL, UP_HEART, UP_SHELL, UP_AMMO, UP_SPREAD, UP_MAGNET, UP_SPRINGS, UP_LUCKY,
                                   UP_FEATHER, UP_BURST};
    for (unsigned p = 0; p < sizeof pref; p++)
        for (int k = 0; k < w->n_offers; k++) {
            const offer *o = &w->offers[k];
            if (o->up != pref[p] || o->sold || o->price > w->gems || !world_can_buy(w, o->up)) continue;
            if (o->up == UP_HEAL && w->hp > w->hpmax - 2 && w->hp > 2) continue;
            return k;
        }
    return -1;
}
