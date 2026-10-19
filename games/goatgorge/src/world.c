/*
 * Goat Gorge: the rules. The rooms (parsed from the levels' text), the kid's movement (a precision platformer's:
 * acceleration on the ground and in the air, a variable jump, coyote time, jump and dash buffers, half gravity at the
 * apex, fast-fall, corner correction, wall slide and wall jump, one 8-way dash with a freeze frame, springs, refill
 * gems, crumbling blocks, moving platforms, wind) and everything it touches. No SDK call: deterministic, linked alone
 * by the tests and the room solver.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
 *
 * Positions are whole pixels with a sub-pixel remainder (1/256): every move goes pixel by pixel against the solids,
 * so nothing ever tunnels and the corrections (a corner, a ledge's lip) are exact.
 */
#include "game.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int iabs(int v) { return v < 0 ? -v : v; }
static int sgn(int v) { return (v > 0) - (v < 0); }
static int approach(int v, int target, int step)
{
    if (v < target) return v + step > target ? target : v + step;
    return v - step < target ? target : v - step;
}
static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

/* ---- rooms ---------------------------------------------------------------------------------------------------------- */
static int is_rock_char(char c) { return c == '#'; }

int room_parse(const char *text, room *r, char *err, int errlen)
{
    char g[ROOM_H][ROOM_W + 1];
    const char *p = text;
    int row = 0;
    memset(r, 0, sizeof *r);
    memset(r->crumble_at, -1, sizeof r->crumble_at);
    /* the room line's settings: wind=X,Y gust=ON,OFF */
    if (!strncmp(p, "room", 4)) {
        const char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p);
        char line[160];
        if (len > 159) len = 159;
        memcpy(line, p, (size_t)len);
        line[len] = 0;
        const char *s;
        if ((s = strstr(line, "wind="))) {
            int x = 0, y = 0;
            sscanf(s + 5, "%d,%d", &x, &y);
            r->wind_x = (int16_t)x;
            r->wind_y = (int16_t)y;
        }
        if ((s = strstr(line, "hint="))) r->hint = (uint8_t)(s[5] - '0');
        if ((s = strstr(line, "exit=")))                    /* the way out named: the other open sides are sky */
            r->exit_set = s[5] == 'l' ? EXIT_LEFT : s[5] == 'r' ? EXIT_RIGHT : EXIT_TOP;
        if ((s = strstr(line, "gust="))) {
            int a = 0, b = 0;
            sscanf(s + 5, "%d,%d", &a, &b);
            r->wind_on = (int16_t)a;
            r->wind_off = (int16_t)b;
        }
        p = e ? e + 1 : p + len;
    }
    while (*p && row < ROOM_H) {
        const char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p);
        if (len > 0 && p[len - 1] == '\r') len--;
        if (len > 0 && p[0] != ';') {
            if (len != ROOM_W) {
                snprintf(err, (size_t)errlen, "row %d is %d characters (20 expected)", row + 1, len);
                return -1;
            }
            memcpy(g[row], p, ROOM_W);
            g[row][ROOM_W] = 0;
            row++;
        }
        if (!e) break;
        p = e + 1;
    }
    if (row != ROOM_H) {
        snprintf(err, (size_t)errlen, "%d rows (15 expected)", row);
        return -1;
    }
    int starts = 0, berries = 0;
    uint8_t used_x[ROOM_H][ROOM_W];
    memset(used_x, 0, sizeof used_x);
    for (int y = 0; y < ROOM_H; y++)
        for (int x = 0; x < ROOM_W; x++) {
            char c = g[y][x];
            uint8_t k = C_EMPTY;
            switch (c) {
            case '#': k = C_ROCK; break;
            case '-': k = C_LEDGE; break;
            case '=':
                k = C_CRUMBLE;
                if (r->ncrumble >= MAX_CRUMBLE) { snprintf(err, (size_t)errlen, "too many crumbling cells"); return -1; }
                r->crumble_cx[r->ncrumble] = (uint8_t)x;
                r->crumble_cy[r->ncrumble] = (uint8_t)y;
                /* a run of them in a row crumbles together */
                r->crumble_group[r->ncrumble] = (x > 0 && g[y][x - 1] == '=') ? r->crumble_group[r->ncrumble - 1]
                                                                                : r->ncrumble;
                r->crumble_at[y][x] = (int8_t)r->ncrumble++;
                break;
            case '^': k = C_SPIKE_U; break;
            case 'v': k = C_SPIKE_D; break;
            case '<': k = C_SPIKE_L; break;
            case '>': k = C_SPIKE_R; break;
            case 'S':
                starts++;
                r->start_x = (int16_t)(x * CELL + (CELL - HB_W) / 2);
                r->start_y = (int16_t)(y * CELL + CELL - HB_H);
                break;
            case 'B':
                berries++;
                r->has_berry = 1;
                r->berry_x = (int16_t)(x * CELL + CELL / 2);
                r->berry_y = (int16_t)(y * CELL + CELL / 2);
                break;
            case '*':
                if (r->ngems >= MAX_GEMS) { snprintf(err, (size_t)errlen, "too many gems"); return -1; }
                r->gem_x[r->ngems] = (int16_t)(x * CELL + CELL / 2);
                r->gem_y[r->ngems] = (int16_t)(y * CELL + CELL / 2);
                r->ngems++;
                break;
            case 'o':
                if (r->nsprings >= MAX_SPRINGS) { snprintf(err, (size_t)errlen, "too many springs"); return -1; }
                if (y + 1 >= ROOM_H || !is_rock_char(g[y + 1][x])) {
                    snprintf(err, (size_t)errlen, "the spring at %d,%d has no rock under it", x + 1, y + 1);
                    return -1;
                }
                r->spring_x[r->nsprings] = (int16_t)(x * CELL);
                r->spring_y[r->nsprings] = (int16_t)(y * CELL);
                r->nsprings++;
                break;
            case 'H': case 'V':
                if (x > 0 && g[y][x - 1] == c) break;            /* (a run: its first cell makes it) */
                {
                    if (r->nplats >= MAX_PLATS) { snprintf(err, (size_t)errlen, "too many platforms"); return -1; }
                    int n = 0;
                    while (x + n < ROOM_W && g[y][x + n] == c) n++;
                    plat_def *pd = &r->plat[r->nplats];
                    pd->x0 = pd->x1 = (int16_t)(x * CELL);
                    pd->y0 = pd->y1 = (int16_t)(y * CELL);
                    pd->w = (int16_t)(n * CELL);
                    pd->h = 8;
                    int found = 0;
                    if (c == 'H') {         /* the path's end: an 'x' in the same row (right first) */
                        for (int k2 = x + n; k2 < ROOM_W && !found; k2++)
                            if (g[y][k2] == 'x' && !used_x[y][k2]) { pd->x1 = (int16_t)(k2 * CELL - pd->w + CELL); used_x[y][k2] = 1; found = 1; }
                        for (int k2 = x - 1; k2 >= 0 && !found; k2--)
                            if (g[y][k2] == 'x' && !used_x[y][k2]) { pd->x1 = (int16_t)(k2 * CELL); used_x[y][k2] = 1; found = 1; }
                    } else {                /* an 'x' in the same column (up first) */
                        for (int k2 = y - 1; k2 >= 0 && !found; k2--)
                            if (g[k2][x] == 'x' && !used_x[k2][x]) { pd->y1 = (int16_t)(k2 * CELL); used_x[k2][x] = 1; found = 1; }
                        for (int k2 = y + 1; k2 < ROOM_H && !found; k2++)
                            if (g[k2][x] == 'x' && !used_x[k2][x]) { pd->y1 = (int16_t)(k2 * CELL); used_x[k2][x] = 1; found = 1; }
                    }
                    if (!found) {
                        snprintf(err, (size_t)errlen, "the platform at %d,%d has no 'x' (its path's end)", x + 1, y + 1);
                        return -1;
                    }
                    pd->dist = (int16_t)(iabs(pd->x1 - pd->x0) + iabs(pd->y1 - pd->y0));
                    r->nplats++;
                }
                break;
            case 'd':
                if (r->ndeco >= MAX_DECO) { snprintf(err, (size_t)errlen, "too many props (d)"); return -1; }
                r->deco_cx[r->ndeco] = (uint8_t)x;
                r->deco_cy[r->ndeco] = (uint8_t)y;
                r->ndeco++;
                break;
            case 'x': case '.': case ' ': break;
            default:
                snprintf(err, (size_t)errlen, "unknown character '%c' at %d,%d", c, x + 1, y + 1);
                return -1;
            }
            r->cell[y][x] = k;
        }
    for (int y = 0; y < ROOM_H; y++)
        for (int x = 0; x < ROOM_W; x++)
            if (g[y][x] == 'x' && !used_x[y][x]) {
                snprintf(err, (size_t)errlen, "the 'x' at %d,%d ends no platform's path", x + 1, y + 1);
                return -1;
            }
    if (starts != 1) { snprintf(err, (size_t)errlen, "%d starts (S) instead of 1", starts); return -1; }
    if (berries > 1) { snprintf(err, (size_t)errlen, "%d berries (B), 1 at most", berries); return -1; }
    /* the exit: an opening of the left or the right column; else the top row's. With a side's exit the top may be open
     * too: the sky (the kid may jump above the screen, it comes back down). The bottom: a fall. */
    int top = 0, left = 0, right = 0;
    for (int x = 1; x < ROOM_W - 1; x++)
        if (r->cell[0][x] != C_ROCK) top = 1;
    for (int y = 0; y < ROOM_H; y++) {
        if (r->cell[y][0] != C_ROCK) left = 1;
        if (r->cell[y][ROOM_W - 1] != C_ROCK) right = 1;
    }
    if (r->exit_set) {                                      /* exit=: that side, the others open on the sky */
        if ((r->exit_set == EXIT_LEFT && !left) || (r->exit_set == EXIT_RIGHT && !right) || (r->exit_set == EXIT_TOP && !top)) {
            snprintf(err, (size_t)errlen, "exit= names a side with no opening");
            return -1;
        }
        r->exit_side = r->exit_set;
        r->wall_sides = 1;
        r->open_top = (uint8_t)top;
        return 0;
    }
    if (left && right) {
        snprintf(err, (size_t)errlen, "the room opens on both sides (one way out: left, right or the top; or exit=)");
        return -1;
    }
    if (!left && !right && !top) { snprintf(err, (size_t)errlen, "the room has no way out"); return -1; }
    r->exit_side = left ? EXIT_LEFT : right ? EXIT_RIGHT : EXIT_TOP;
    r->open_top = (uint8_t)top;
    return 0;
}

int chapter_parse(const char *text, room *rooms, int max, char *err, int errlen)
{
    int n = 0;
    const char *p = text;
    while (*p) {
        if (!strncmp(p, "room", 4)) {
            if (n >= max) { snprintf(err, (size_t)errlen, "more than %d rooms", max); return -1; }
            char e2[120];
            if (room_parse(p, &rooms[n], e2, sizeof e2)) {
                snprintf(err, (size_t)errlen, "room %d: %s", n + 1, e2);
                return -1;
            }
            n++;
        }
        const char *e = strchr(p, '\n');
        if (!e) break;
        p = e + 1;
    }
    return n;
}

int world_cell(const room *r, int cx, int cy)
{
    if (cy >= ROOM_H) return C_EMPTY;                       /* below: the fall */
    if ((cx < 0 && r->exit_side != EXIT_LEFT && r->wall_sides) || (cx >= ROOM_W && r->exit_side != EXIT_RIGHT && r->wall_sides))
        return C_ROCK;                                      /* a side that is not the way out: an unseen wall */
    if (cx < 0 || cx >= ROOM_W || cy < 0) {                 /* outside: the border's rock carried on */
        int bx = cx < 0 ? 0 : cx >= ROOM_W ? ROOM_W - 1 : cx, by = cy < 0 ? 0 : cy;
        return r->cell[by][bx] == C_ROCK ? C_ROCK : C_EMPTY;
    }
    return r->cell[cy][cx];
}

void plat_pos(const plat_def *p, int32_t t, int *x, int *y)
{
    int d = p->dist, s = 0;
    if (d > 0) {
        int period = 2 * (d + PLAT_PAUSE), ph = (int)(t % period);
        if (ph < PLAT_PAUSE) s = 0;
        else if (ph < PLAT_PAUSE + d) s = ph - PLAT_PAUSE;
        else if (ph < 2 * PLAT_PAUSE + d) s = d;
        else s = d - (ph - 2 * PLAT_PAUSE - d);
    }
    *x = p->x0 + (d ? (p->x1 - p->x0) * s / d : 0);
    *y = p->y0 + (d ? (p->y1 - p->y0) * s / d : 0);
}

int crumble_solid(const world *w, int i) { return w->crumble[i] >= 0; }

int wind_active(const room *r, int32_t t)
{
    if (!r->wind_x && !r->wind_y) return 0;
    if (r->wind_off <= 0) return 1;
    return (int)(t % (r->wind_on + r->wind_off)) < r->wind_on;
}

int world_solid(const world *w, const room *r, int x, int y, int wd, int ht)
{
    int cx0 = (x >= 0 ? x / CELL : (x - CELL + 1) / CELL), cx1 = (x + wd - 1 >= 0 ? (x + wd - 1) / CELL : (x + wd - CELL) / CELL);
    int cy0 = (y >= 0 ? y / CELL : (y - CELL + 1) / CELL), cy1 = (y + ht - 1 >= 0 ? (y + ht - 1) / CELL : (y + ht - CELL) / CELL);
    for (int cy = cy0; cy <= cy1; cy++)
        for (int cx = cx0; cx <= cx1; cx++) {
            int c = world_cell(r, cx, cy);
            if (c == C_ROCK) return 1;
            if (c == C_CRUMBLE && crumble_solid(w, r->crumble_at[cy][cx])) return 1;
        }
    for (int i = 0; i < r->nplats; i++)
        if (i != w->ignore_plat && overlap(x, y, wd, ht, w->plat_x[i], w->plat_y[i], r->plat[i].w, r->plat[i].h)) return 1;
    return 0;
}

/* a ledge (one-way, solid from above) right under a box whose bottom is at y + ht */
static int ledge_under(const room *r, int x, int y, int wd, int ht)
{
    int b = y + ht;
    if (b % CELL || b < 0 || b >= ROOM_PX_H) return 0;
    int cy = b / CELL;
    for (int cx = (x < 0 ? -1 : x / CELL); cx <= (x + wd - 1) / CELL; cx++)
        if (world_cell(r, cx, cy) == C_LEDGE) return 1;
    return 0;
}

static int hsolid(const world *w, const room *r, int x, int y) { return world_solid(w, r, x, y, HB_W, HB_H); }

static int on_ground(const world *w, const room *r)
{
    const hero *h = &w->h;
    return hsolid(w, r, h->x, h->y + 1) || ledge_under(r, h->x, h->y, HB_W, HB_H);
}

/* the platform under the kid's feet (index + 1), 0 none */
static int platform_under(const world *w, const room *r)
{
    const hero *h = &w->h;
    for (int i = 0; i < r->nplats; i++)
        if (h->y + HB_H == w->plat_y[i] && h->x < w->plat_x[i] + r->plat[i].w && w->plat_x[i] < h->x + HB_W) return i + 1;
    return 0;
}

static void die(world *w)
{
    if (w->h.dead_t) return;
    w->h.dead_t = 1;
    w->events |= EV_DEATH;
}

/* ---- moving pixel by pixel ----------------------------------------------------------------------------------------- */
static int take(int16_t *rem, int amount)
{
    int v = *rem + amount;
    int mv = v >= 0 ? (v + SUB / 2) / SUB : -((-v + SUB / 2 - 1) / SUB);
    *rem = (int16_t)(v - mv * SUB);
    return mv;
}

/* horizontal: returns 1 when a solid stopped it. pop: px the kid may step up over a lip (a ledge, a corner) */
static int move_x(world *w, const room *r, int px, int pop)
{
    hero *h = &w->h;
    int s = sgn(px);
    while (px) {
        if (!hsolid(w, r, h->x + s, h->y)) { h->x = (int16_t)(h->x + s); px -= s; continue; }
        int k;
        for (k = 1; k <= pop; k++)
            if (!hsolid(w, r, h->x + s, h->y - k) && !hsolid(w, r, h->x, h->y - k)) break;
        if (k <= pop) { h->y = (int16_t)(h->y - k); h->x = (int16_t)(h->x + s); px -= s; continue; }
        return 1;
    }
    return 0;
}

/* vertical: returns 1 when stopped. Up: a ceiling's corner within `corner` px is slid around (toward the motion). */
static int move_y(world *w, const room *r, int py, int corner)
{
    hero *h = &w->h;
    int s = sgn(py);
    while (py) {
        if (s > 0) {
            if (hsolid(w, r, h->x, h->y + 1) || ledge_under(r, h->x, h->y, HB_W, HB_H)) return 1;
        } else if (hsolid(w, r, h->x, h->y - 1)) {
            int k, d = 0;
            for (k = 1; k <= corner && !d; k++) {
                int pref = h->vx > 0 ? 1 : h->vx < 0 ? -1 : 0;
                if (pref >= 0 && !hsolid(w, r, h->x + k, h->y - 1)) d = k;
                else if (pref <= 0 && !hsolid(w, r, h->x - k, h->y - 1)) d = -k;
            }
            if (!d) return 1;
            h->x = (int16_t)(h->x + d);
        }
        h->y = (int16_t)(h->y + s);
        py -= s;
    }
    return 0;
}

/* ---- the room's moving parts ------------------------------------------------------------------------------------------ */
static void shake_group(world *w, const room *r, int i)
{
    int g = r->crumble_group[i], any = 0;
    for (int k = 0; k < r->ncrumble; k++)
        if (r->crumble_group[k] == g && w->crumble[k] == 0) { w->crumble[k] = CRUMBLE_SHAKE; any = 1; }
    if (any) w->events |= EV_CRUMBLE;
}

/* the crumbling cells the box touches (under the feet, beside it) start to shake */
static void touch_crumbles(world *w, const room *r, int x, int y, int wd, int ht)
{
    if (!r->ncrumble) return;
    for (int cy = (y < 0 ? 0 : y / CELL); cy <= (y + ht - 1) / CELL && cy < ROOM_H; cy++)
        for (int cx = (x < 0 ? 0 : x / CELL); cx <= (x + wd - 1) / CELL && cx < ROOM_W; cx++) {
            int i = r->crumble_at[cy][cx];
            if (i >= 0 && w->crumble[i] == 0) shake_group(w, r, i);
        }
}

static void entities_step(world *w, const room *r)
{
    hero *h = &w->h;
    for (int i = 0; i < r->ncrumble; i++) {
        int16_t *c = &w->crumble[i];
        if (*c > 0) {
            if (--*c == 0) *c = -CRUMBLE_BACK;
        } else if (*c < 0) {
            if (*c == -1) {             /* back only when the kid is not in its cell */
                int cx = r->crumble_cx[i] * CELL, cy = r->crumble_cy[i] * CELL;
                if (!overlap(h->x, h->y, HB_W, HB_H, cx, cy, CELL, CELL) || h->dead_t) *c = 0;
            } else {
                (*c)++;
            }
        }
    }
    for (int i = 0; i < r->ngems; i++)
        if (w->gem_t[i] > 0) w->gem_t[i]--;
    for (int i = 0; i < r->nsprings; i++)
        if (w->spring_t[i] > 0) w->spring_t[i]--;
}

/* the platforms move; a rider goes with its own, the others push the kid (crushed against a solid: a death) */
static void platforms_step(world *w, const room *r)
{
    hero *h = &w->h;
    for (int i = 0; i < r->nplats; i++) {
        int nx, ny;
        plat_pos(&r->plat[i], w->t, &nx, &ny);
        int dx = nx - w->plat_x[i], dy = ny - w->plat_y[i];
        w->plat_x[i] = (int16_t)nx;
        w->plat_y[i] = (int16_t)ny;
        if (!dx && !dy) continue;
        w->ignore_plat = (int8_t)i;
        if (h->riding == i + 1 && !h->dead_t) {
            move_x(w, r, dx, 0);
            move_y(w, r, dy, 0);
        }
        if (!h->dead_t && overlap(h->x, h->y, HB_W, HB_H, nx, ny, r->plat[i].w, r->plat[i].h)) {
            int blocked = 0;
            if (dy < 0) blocked = move_y(w, r, ny - (h->y + HB_H), 0);
            else if (dy > 0) blocked = move_y(w, r, ny + r->plat[i].h - h->y, 0);
            else if (dx < 0) blocked = move_x(w, r, nx - (h->x + HB_W), 0);
            else blocked = move_x(w, r, nx + r->plat[i].w - h->x, 0);
            if (blocked) die(w);
        }
        w->ignore_plat = -1;
        if (!h->dead_t && hsolid(w, r, h->x, h->y)) die(w);
    }
}

/* ---- the kid ------------------------------------------------------------------------------------------------------------ */
void world_spawn(world *w, const room *r, int held)
{
    memset(w, 0, sizeof *w);
    w->ignore_plat = -1;
    hero *h = &w->h;
    h->x = r->start_x;
    h->y = r->start_y;
    h->facing = (int8_t)(r->start_x + HB_W / 2 < ROOM_PX_W / 2 ? 1 : -1);
    h->dash_avail = 1;
    h->held = (uint8_t)held;
    h->spawn_t = SPAWN_FLASH;
    h->land_t = 99;
    h->jump_t = 99;
    for (int i = 0; i < r->nplats; i++) {
        int x, y;
        plat_pos(&r->plat[i], 0, &x, &y);
        w->plat_x[i] = (int16_t)x;
        w->plat_y[i] = (int16_t)y;
    }
    h->grounded = (uint8_t)on_ground(w, r);
    h->riding = (uint8_t)platform_under(w, r);
    w->wind_now = (uint8_t)wind_active(r, 0);
}

static void hazards(world *w, const room *r)
{
    hero *h = &w->h;
    if (h->y > ROOM_PX_H + 2) { die(w); return; }           /* the fall */
    int hx = h->x + HURT_IN_X, hy = h->y + HURT_IN_TOP, hw = HB_W - 2 * HURT_IN_X, hh = HB_H - HURT_IN_TOP - HURT_IN_BOT;
    for (int cy = (hy < 0 ? 0 : hy / CELL); cy <= (hy + hh - 1) / CELL && cy < ROOM_H; cy++)
        for (int cx = (hx < 0 ? 0 : hx / CELL); cx <= (hx + hw - 1) / CELL && cx < ROOM_W; cx++) {
            int c = r->cell[cy][cx], X = cx * CELL, Y = cy * CELL;
            /* a spike's sharp part, and only when the kid moves into it (sliding along its back is fine) */
            if (c == C_SPIKE_U && h->vy >= 0 && overlap(hx, hy, hw, hh, X + 1, Y + 10, 14, 6)) die(w);
            else if (c == C_SPIKE_D && h->vy <= 0 && overlap(hx, hy, hw, hh, X + 1, Y, 14, 6)) die(w);
            else if (c == C_SPIKE_L && h->vx >= 0 && overlap(hx, hy, hw, hh, X + 10, Y + 1, 6, 14)) die(w);
            else if (c == C_SPIKE_R && h->vx <= 0 && overlap(hx, hy, hw, hh, X, Y + 1, 6, 14)) die(w);
        }
}

static void pickups(world *w, const room *r)
{
    hero *h = &w->h;
    for (int i = 0; i < r->nsprings; i++) {
        int sx = r->spring_x[i], sy = r->spring_y[i];
        if (h->vy >= 0 && overlap(h->x, h->y, HB_W, HB_H, sx + 2, sy + 9, CELL - 4, 7)) {
            if (!hsolid(w, r, h->x, sy + 9 - HB_H)) h->y = (int16_t)(sy + 9 - HB_H);
            h->ry = 0;
            h->vy = -SPRING_V;
            h->var_t = SPRING_VAR;
            h->var_v = SPRING_V;
            h->auto_var = 1;
            h->can_cut = 0;
            h->dash_t = 0;
            h->dash_avail = 1;
            h->refill_cool = 0;
            h->wall_lock = 0;
            h->coyote_t = 0;
            h->jump_t = 0;
            if (iabs(h->vx) > MAX_RUN) h->vx = (int16_t)(sgn(h->vx) * MAX_RUN);
            w->spring_t[i] = 12;
            w->events |= EV_SPRING;
        }
    }
    for (int i = 0; i < r->ngems; i++)
        if (!w->gem_t[i] && !h->dash_avail && overlap(h->x, h->y, HB_W, HB_H, r->gem_x[i] - 7, r->gem_y[i] - 7, 14, 14)) {
            h->dash_avail = 1;
            w->gem_t[i] = GEM_RESPAWN;
            w->events |= EV_GEM;
        }
    if (r->has_berry && !w->berry_got && overlap(h->x, h->y, HB_W, HB_H, r->berry_x - 7, r->berry_y - 7, 14, 14)) {
        w->berry_got = 1;
        w->events |= EV_BERRY;
    }
}

static void start_dash(hero *h, int in)
{
    int dx = (in & IN_RIGHT ? 1 : 0) - (in & IN_LEFT ? 1 : 0), dy = (in & IN_DOWN ? 1 : 0) - (in & IN_UP ? 1 : 0);
    if (!dx && !dy) dx = h->facing;
    h->dash_dx = (int8_t)dx;
    h->dash_dy = (int8_t)dy;
    int sp = dx && dy ? DASH_DIAG : DASH_SPEED;
    h->vx = (int16_t)(dx * sp);
    h->vy = (int16_t)(dy * sp);
    h->rx = h->ry = 0;
    h->dash_t = DASH_FRAMES;
    if (dx) h->facing = (int8_t)dx;
}

static void end_dash(hero *h)
{
    int dx = h->dash_dx, dy = h->dash_dy;
    if (dy <= 0) {                                  /* up and sideways: some speed is kept; down: all of it */
        int sp = dx && dy ? END_DASH * 181 / 256 : END_DASH;
        h->vx = (int16_t)(dx * sp);
        h->vy = (int16_t)(dy * sp * 3 / 4);
    }
}

void world_step(world *w, const room *r, int in)
{
    hero *h = &w->h;
    w->events = 0;
    if (w->exited) return;
    int pressed = in & ~h->held, released = h->held & ~in;
    if (h->dead_t) {
        h->held = (uint8_t)in;
        if (++h->dead_t > DEATH_FRAMES) {
            world_spawn(w, r, in);
            w->events |= EV_RESPAWN;
        }
        return;
    }
    if (pressed & IN_JUMP) h->buffer_t = JUMP_BUFFER;
    if (pressed & IN_DASH) h->dash_buf = DASH_BUFFER;
    if (h->freeze_t) {                              /* the dash's freeze frame: the world holds still */
        h->held = (uint8_t)in;
        if (--h->freeze_t == 0) {
            start_dash(h, in);
            w->events |= EV_DASH_GO;
        }
        return;
    }
    w->t++;
    w->wind_now = (uint8_t)wind_active(r, w->t);
    platforms_step(w, r);
    entities_step(w, r);
    if (h->dead_t) { h->held = (uint8_t)in; return; }

    /* the ground, the walls */
    int was_grounded = h->grounded;
    h->grounded = (uint8_t)(h->vy >= 0 && on_ground(w, r));
    if (h->grounded) {
        h->coyote_t = COYOTE;
        if (!h->dash_t && !h->refill_cool) h->dash_avail = 1;
        touch_crumbles(w, r, h->x, h->y + HB_H, HB_W, 1);
    } else if (h->coyote_t) {
        h->coyote_t--;
    }
    int wall_l = world_solid(w, r, h->x - WALL_CHECK, h->y, WALL_CHECK, HB_H);
    int wall_r = world_solid(w, r, h->x + HB_W, h->y, WALL_CHECK, HB_H);
    if (h->dash_cool) h->dash_cool--;
    if (h->refill_cool) h->refill_cool--;
    if (h->spawn_t) h->spawn_t--;
    if (h->land_t < 99) h->land_t++;
    if (h->jump_t < 99) h->jump_t++;

    /* the dash: a freeze frame, then 8 frames straight */
    if (h->dash_buf && h->dash_avail && !h->dash_cool) {
        h->dash_avail = 0;
        h->dash_buf = 0;
        h->freeze_t = DASH_FREEZE;
        h->dash_cool = DASH_COOLDOWN;
        h->refill_cool = DASH_REFILL_COOL;
        h->vx = h->vy = 0;
        h->rx = h->ry = 0;
        h->var_t = 0;
        h->auto_var = 0;
        h->can_cut = 0;
        h->wall_lock = 0;
        h->sliding = 0;
        w->events |= EV_DASH;
        if (h->buffer_t) h->buffer_t = 0;
        h->held = (uint8_t)in;
        return;
    }
    if (h->dash_buf) h->dash_buf--;

    int mx = (in & IN_RIGHT ? 1 : 0) - (in & IN_LEFT ? 1 : 0);
    int jumped = 0;
    if (h->dash_t) {
        /* a jump during a dash on the ground: the super jump (the dash's speed carried along the ground) */
        if (h->buffer_t && h->coyote_t && h->dash_dy >= 0 && h->dash_dx) {
            h->vx = (int16_t)(h->dash_dx * SUPER_VX);
            h->vy = -JUMP_V;
            h->var_t = VAR_JUMP;
            h->var_v = JUMP_V;
            h->auto_var = 0;
            h->can_cut = 1;
            h->dash_t = 0;
            h->buffer_t = h->coyote_t = 0;
            h->jump_t = 0;
            h->facing = h->dash_dx;
            w->events |= EV_JUMP | EV_SUPER;
            jumped = 1;
        } else if (h->buffer_t && !h->grounded && (wall_l || wall_r) && h->dash_dy < 0 && !h->dash_dx) {
            int dir = wall_l ? 1 : -1;              /* an up-dash along a wall: a wall jump out of it */
            h->vx = (int16_t)(dir * WALL_JUMP_VX);
            h->vy = -JUMP_V;
            h->var_t = VAR_JUMP;
            h->var_v = JUMP_V;
            h->can_cut = 1;
            h->dash_t = 0;
            h->buffer_t = 0;
            h->jump_t = 0;
            h->facing = (int8_t)dir;
            w->events |= EV_WALLJUMP;
            jumped = 1;
        }
    }
    if (!h->dash_t && !jumped) {
        if (h->wall_lock) {
            mx = h->wall_lock_dir;
            h->wall_lock--;
        }
        if (mx) h->facing = (int8_t)mx;
        /* horizontal: separate acceleration and deceleration on the ground and in the air */
        int v = h->vx, target = mx * MAX_RUN;
        if (h->grounded) {
            if (mx && v * mx > MAX_RUN) v = approach(v, target, OVER_DECEL);
            else if (!mx) v = approach(v, 0, iabs(v) > MAX_RUN ? OVER_DECEL : RUN_DECEL);
            else if (v * mx < 0) v = approach(v, target, TURN_ACCEL);
            else v = approach(v, target, RUN_ACCEL);
        } else {
            if (mx && v * mx > MAX_RUN) v = approach(v, target, OVER_DECEL_AIR);
            else if (!mx) v = approach(v, 0, iabs(v) > MAX_RUN ? OVER_DECEL_AIR : AIR_DECEL);
            else v = approach(v, target, AIR_ACCEL);
        }
        h->vx = (int16_t)v;
        /* jumps: from the ground (or within the coyote time), else off a wall */
        if (h->buffer_t) {
            if (h->coyote_t) {
                h->vy = -JUMP_V;
                h->vx = (int16_t)(h->vx + mx * JUMP_HBOOST);
                if (h->riding) {                    /* the platform's motion carries on */
                    const plat_def *p = &r->plat[h->riding - 1];
                    int ox, oy, nx2, ny2;
                    plat_pos(p, w->t - 1, &ox, &oy);
                    plat_pos(p, w->t, &nx2, &ny2);
                    h->vx = (int16_t)(h->vx + (nx2 - ox) * SUB);
                    if (ny2 < oy) h->vy = (int16_t)(h->vy + (ny2 - oy) * SUB);
                }
                h->var_t = VAR_JUMP;
                h->var_v = (int16_t)-h->vy;
                h->auto_var = 0;
                h->can_cut = 1;
                h->buffer_t = h->coyote_t = 0;
                h->jump_t = 0;
                h->grounded = 0;
                w->events |= EV_JUMP;
            } else if (wall_l || wall_r) {
                int dir = wall_l && wall_r ? -h->facing : wall_l ? 1 : -1;
                h->vx = (int16_t)(dir * WALL_JUMP_VX);
                h->vy = -JUMP_V;
                h->var_t = VAR_JUMP;
                h->var_v = JUMP_V;
                h->auto_var = 0;
                h->can_cut = 1;
                h->buffer_t = 0;
                h->jump_t = 0;
                if (mx) {                           /* pushing: held away a moment (a neutral jump is free) */
                    h->wall_lock = WALL_LOCK;
                    h->wall_lock_dir = (uint8_t)dir;
                }
                h->facing = (int8_t)dir;
                h->sliding = 0;
                touch_crumbles(w, r, dir > 0 ? h->x - WALL_CHECK : h->x + HB_W, h->y, WALL_CHECK, HB_H);
                w->events |= EV_WALLJUMP;
            }
        }
        /* the variable jump: held, the speed is kept; released, it is cut */
        if (h->var_t) {
            if ((in & IN_JUMP) || h->auto_var) {
                if (h->vy > -h->var_v) h->vy = (int16_t)-h->var_v;
                h->var_t--;
            } else {
                h->var_t = 0;
            }
        }
        if (h->can_cut && (released & IN_JUMP) && h->vy < -JUMP_CUT) h->vy = -JUMP_CUT;
        if (h->vy >= 0) h->can_cut = 0;
        if (!h->var_t) h->auto_var = 0;
        /* gravity: half at the apex while jump is held; the fall's speed capped, faster holding Down, slower sliding
         * down a wall the kid pushes into */
        h->sliding = 0;
        if (!h->grounded && !h->var_t) {
            int maxf = MAX_FALL, g = GRAVITY;
            if (iabs(h->vy) < APEX_V && (in & IN_JUMP)) g = APEX_GRAVITY;
            if (in & IN_DOWN) maxf = FAST_FALL;
            int pushing = (mx < 0 && world_solid(w, r, h->x - 1, h->y, 1, HB_H)) ||
                          (mx > 0 && world_solid(w, r, h->x + HB_W, h->y, 1, HB_H));
            if (pushing && h->vy > 0 && !(in & IN_DOWN)) {
                maxf = WALL_SLIDE;
                h->sliding = 1;
                touch_crumbles(w, r, mx < 0 ? h->x - 1 : h->x + HB_W, h->y, 1, HB_H);
            }
            if (h->vy > maxf) h->vy = (int16_t)approach(h->vy, maxf, h->sliding ? WALL_SLIDE_ACCEL * 3 : FAST_ACCEL);
            else h->vy = (int16_t)approach(h->vy, maxf, (in & IN_DOWN) && h->vy >= MAX_FALL ? FAST_ACCEL : g);
        }
    }
    h->wall_dir = (int8_t)(h->sliding ? mx : 0);

    /* the move: the kid's speed and the wind (not during a dash) */
    int ax = h->vx, ay = h->vy;
    if (w->wind_now && !h->dash_t) {
        ax += r->wind_x;
        ay += r->wind_y;
    }
    int dashing = h->dash_t > 0;
    int pop = dashing ? (h->dash_dy <= 0 ? CORNER_DASH : 0) : (h->vy >= 0 ? LEDGE_POP : 0);
    if (move_x(w, r, take(&h->rx, ax), pop)) {
        h->vx = 0;
        h->rx = 0;
    }
    int fall_v = h->vy;
    if (move_y(w, r, take(&h->ry, ay), dashing ? CORNER_DASH : CORNER_JUMP)) {
        if (ay < 0) {
            h->vy = 0;
            h->var_t = 0;
            h->ry = 0;
            w->events |= EV_BONK;
        } else {
            if (!was_grounded && fall_v > 0) {
                h->land_t = 0;
                h->land_v = (int16_t)fall_v;
                w->events |= EV_LAND;
            }
            h->vy = 0;
            h->ry = 0;
        }
    }
    if (h->dash_t && --h->dash_t == 0) end_dash(h);
    if (h->buffer_t) h->buffer_t--;
    if (h->grounded && h->vx && !dashing) h->run_t++;
    else if (!h->vx) h->run_t = 0;

    h->riding = (uint8_t)platform_under(w, r);
    hazards(w, r);
    if (!h->dead_t) pickups(w, r);
    /* the way out: the room's open side */
    if (!h->dead_t && ((r->exit_side == EXIT_TOP && h->y + HB_H / 2 < 0) ||
                       (r->exit_side == EXIT_LEFT && h->x + HB_W / 2 < 0) ||
                       (r->exit_side == EXIT_RIGHT && h->x + HB_W / 2 >= ROOM_PX_W))) {
        w->exited = 1;
        w->events |= EV_EXIT;
    }
    h->held = (uint8_t)in;
}

uint32_t world_hash(const world *w)
{
    const uint8_t *p = (const uint8_t *)w;
    uint32_t hsh = 2166136261u;
    for (size_t i = 0; i < offsetof(world, events); i++) hsh = (hsh ^ p[i]) * 16777619u;
    return hsh;
}

/* ---- the solutions' run-length code: "<letter><count>", the letter sol_abc[the input bits], the count 1 if none ---- */
const char sol_abc[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+-/:";

int sol_decode(const char *s, uint8_t *out, int max)
{
    int n = 0;
    while (*s) {
        if (*s == ' ' || *s == '\n' || *s == '\r') { s++; continue; }
        const char *f = strchr(sol_abc, *s);
        if (!f || !*s) return -1;
        int v = (int)(f - sol_abc);
        s++;
        int k = 0;
        while (*s >= '0' && *s <= '9' && k < 100000) k = k * 10 + (*s++ - '0');
        if (!k) k = 1;
        while (k-- > 0) {
            if (n >= max) return -1;
            out[n++] = (uint8_t)v;
        }
    }
    return n;
}
