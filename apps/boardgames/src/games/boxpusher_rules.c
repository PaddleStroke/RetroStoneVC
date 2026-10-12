/*
 * Box Pusher: the rules (boxpusher.h): the levels read, steps and pushes, walks round the crates, undo by actions,
 * and the push-optimal solver (the hint, the levels' check). No SDL: tested by tests/boxpusher_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/boxpusher.h"

#include <stdlib.h>
#include <string.h>

#include "games/boxpusher_levels.h"

int bp_dx(int d) { return d == BP_LEFT ? -1 : d == BP_RIGHT ? 1 : 0; }
int bp_dy(int d) { return d == BP_UP ? -1 : d == BP_DOWN ? 1 : 0; }

int bp_load(bp_game *g, const char *rows)
{
    int level = g->level;
    memset(g, 0, sizeof *g);
    g->level = level;
    g->player = -1;
    memset(g->map, BP_OUT, sizeof g->map);
    int x = 0, y = 0, places = 0;
    for (const char *s = rows; *s; s++) {
        if (*s == '|') {
            x = 0;
            if (++y >= BP_MAXH) return 0;
            continue;
        }
        if (x >= BP_MAXW) return 0;
        int c = x + y * BP_MAXW;                /* laid out at the full width first, packed below */
        switch (*s) {
        case '#': g->map[c] = BP_WALL; break;
        case ' ': g->map[c] = BP_FLOOR; break;
        case '.': g->map[c] = BP_FLOOR; g->goal[c] = 1; break;
        case '$': g->map[c] = BP_FLOOR; g->box[c] = 1; break;
        case '*': g->map[c] = BP_FLOOR; g->goal[c] = g->box[c] = 1; break;
        case '@': g->map[c] = BP_FLOOR; g->player = c; break;
        case '+': g->map[c] = BP_FLOOR; g->goal[c] = 1; g->player = c; break;
        case '_': break;
        default: return 0;
        }
        if (++x > g->w) g->w = x;
    }
    g->h = y + 1;
    /* packed to the level's own width */
    uint8_t map[BP_CELLS], goal[BP_CELLS], box[BP_CELLS];
    memset(map, BP_OUT, sizeof map);
    memset(goal, 0, sizeof goal);
    memset(box, 0, sizeof box);
    for (y = 0; y < g->h; y++)
        for (x = 0; x < g->w; x++) {
            int a = x + y * BP_MAXW, b = x + y * g->w;
            map[b] = g->map[a];
            goal[b] = g->goal[a];
            box[b] = g->box[a];
            if (g->player == a) g->player = -2 - b;
        }
    memcpy(g->map, map, sizeof map);
    memcpy(g->goal, goal, sizeof goal);
    memcpy(g->box, box, sizeof box);
    g->player = g->player <= -2 ? -2 - g->player : -1;
    for (int c = 0; c < g->w * g->h; c++) {
        g->nbox += g->box[c];
        places += g->goal[c];
    }
    return g->player >= 0 && g->nbox > 0 && g->nbox <= BP_BOXES && g->nbox == places;
}

void bp_new(bp_game *g, int level)
{
    if (level < 0 || level >= BP_LEVELS) level = 0;
    g->level = level;
    bp_load(g, bp_levels[level]);
}

int bp_free(const bp_game *g, int c)
{
    return c >= 0 && c < g->w * g->h && g->map[c] == BP_FLOOR && !g->box[c];
}

/* the cell next to c that way; -1 off the board */
static int next(const bp_game *g, int c, int d)
{
    int x = c % g->w + bp_dx(d), y = c / g->w + bp_dy(d);
    return x < 0 || y < 0 || x >= g->w || y >= g->h ? -1 : x + y * g->w;
}

int bp_can(const bp_game *g, int d)
{
    int a = next(g, g->player, d);
    if (a < 0 || g->map[a] != BP_FLOOR) return 0;
    if (!g->box[a]) return 1;
    return bp_free(g, next(g, a, d));
}

static int begin_next;

void bp_begin(bp_game *g)
{
    (void)g;
    begin_next = 1;
}

int bp_step(bp_game *g, int d)
{
    if (!bp_can(g, d) || g->nh >= BP_HIST) return 0;
    int a = next(g, g->player, d), push = g->box[a];
    if (push) {
        g->box[a] = 0;
        g->box[next(g, a, d)] = 1;
        g->pushes++;
    }
    g->player = a;
    g->moves++;
    int first = begin_next || !g->nh;
    g->hist[g->nh++] = (uint8_t)(d | push << 2 | first << 3);
    begin_next = 0;
    return 1 + push;
}

/* the cells the keeper can walk to (around the crates): from[c] its way in (-1 unreached; 4 the keeper's cell) */
static void walks(const bp_game *g, const uint8_t *box, int from_cell, int8_t *from)
{
    int queue[BP_CELLS], head = 0, tail = 0;
    memset(from, -1, BP_CELLS);
    from[from_cell] = 4;
    queue[tail++] = from_cell;
    while (head < tail) {
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int n = next(g, c, d);
            if (n < 0 || from[n] >= 0 || g->map[n] != BP_FLOOR || box[n]) continue;
            from[n] = (int8_t)d;
            queue[tail++] = n;
        }
    }
}

int bp_path(const bp_game *g, int to, uint8_t *dirs, int max)
{
    if (to < 0 || to >= g->w * g->h) return -1;
    int8_t from[BP_CELLS];
    walks(g, g->box, g->player, from);
    if (from[to] < 0) return -1;
    int n = 0;
    for (int c = to; c != g->player; c = next(g, c, from[c] ^ 1)) n++;   /* the way back: up <-> down, l <-> r */
    if (n > max) return -1;
    int k = n;
    for (int c = to; c != g->player; c = next(g, c, from[c] ^ 1)) dirs[--k] = (uint8_t)from[c];
    return n;
}

int bp_walk(bp_game *g, int to)
{
    uint8_t dirs[BP_CELLS];
    int n = bp_path(g, to, dirs, BP_CELLS);
    if (n <= 0) return 0;
    bp_begin(g);
    for (int i = 0; i < n; i++) bp_step(g, dirs[i]);
    return 1;
}

int bp_undo(bp_game *g)
{
    if (!g->nh) return 0;
    for (;;) {
        uint8_t h = g->hist[--g->nh];
        int d = h & 3, back = next(g, g->player, d ^ 1);
        if (h & 4) {                            /* the crate in front comes back */
            int a = next(g, g->player, d);
            g->box[a] = 0;
            g->box[g->player] = 1;
            g->pushes--;
        }
        g->player = back;
        g->moves--;
        if ((h & 8) || !g->nh) break;
    }
    return 1;
}

int bp_solved(const bp_game *g)
{
    for (int c = 0; c < g->w * g->h; c++)
        if (g->box[c] && !g->goal[c]) return 0;
    return 1;
}

/* ---- the solver: positions after each push (the crates, and the keeper's area named by its first cell), breadth
 * first, so the first way found has the fewest pushes. Cells from which a crate can never reach a place (dead) and
 * four crates or walls in a square with a crate off its place are never entered. -------------------------------- */
#define TABLE (1 << 20)
#define MAX_STATES (TABLE / 2)

static uint64_t pack(const uint8_t *box, int n, int keeper, const bp_game *g)
{
    uint64_t k = (uint64_t)keeper + 1;
    for (int c = 0, i = 0; c < g->w * g->h && i < n; c++)
        if (box[c]) { k = k << 8 | (uint64_t)c; i++; }
    return k;
}

static void unpack(uint64_t k, int n, uint8_t *box, int *keeper)
{
    memset(box, 0, BP_CELLS);
    for (int i = 0; i < n; i++, k >>= 8) box[k & 255] = 1;
    *keeper = (int)(k & 255) - 1;
}

static void dead_cells(const bp_game *g, uint8_t *live)
{
    int queue[BP_CELLS], head = 0, tail = 0;
    memset(live, 0, BP_CELLS);
    for (int c = 0; c < g->w * g->h; c++)
        if (g->goal[c]) { live[c] = 1; queue[tail++] = c; }
    while (head < tail) {                       /* a crate on c could come from c - d, pushed from c - 2d */
        int c = queue[head++];
        for (int d = 0; d < 4; d++) {
            int y = next(g, c, d ^ 1), p = y < 0 ? -1 : next(g, y, d ^ 1);
            if (y < 0 || p < 0 || live[y] || g->map[y] != BP_FLOOR || g->map[p] != BP_FLOOR) continue;
            live[y] = 1;
            queue[tail++] = y;
        }
    }
}

static int solid(const bp_game *g, const uint8_t *box, int x, int y)
{
    if (x < 0 || y < 0 || x >= g->w || y >= g->h) return 1;
    int c = x + y * g->w;
    return g->map[c] != BP_FLOOR || box[c];
}

/* a square of four crates or walls round c holding a crate off its place: stuck for ever */
static int squared(const bp_game *g, const uint8_t *box, int c)
{
    int x = c % g->w, y = c / g->w;
    for (int ox = -1; ox <= 0; ox++)
        for (int oy = -1; oy <= 0; oy++) {
            int all = 1, off = 0;
            for (int k = 0; k < 4 && all; k++) {
                int cx = x + ox + (k & 1), cy = y + oy + (k >> 1);
                if (!solid(g, box, cx, cy)) all = 0;
                else if (cx >= 0 && cy >= 0 && cx < g->w && cy < g->h && box[cx + cy * g->w] &&
                         !g->goal[cx + cy * g->w])
                    off = 1;
            }
            if (all && off) return 1;
        }
    return 0;
}

static int keeper_name(const int8_t *from, int cells)
{
    for (int c = 0; c < cells; c++)
        if (from[c] >= 0) return c;
    return 0;
}

int bp_solve(const bp_game *g, int *box_out, int *dir_out)
{
    if (bp_solved(g)) return 0;
    uint64_t *key = calloc(TABLE, sizeof *key), *order = malloc(MAX_STATES * sizeof *order);
    int32_t *parent = malloc(MAX_STATES * sizeof *parent);
    int16_t *push = malloc(MAX_STATES * sizeof *push), *depth = malloc(MAX_STATES * sizeof *depth);
    int result = -1, found = -1, N = g->w * g->h;
    if (!key || !order || !parent || !push || !depth) { result = -2; goto done; }
    uint8_t live[BP_CELLS], box[BP_CELLS];
    int8_t from[BP_CELLS];
    dead_cells(g, live);
    for (int c = 0; c < N; c++)
        if (g->box[c] && !live[c]) goto done;   /* a crate already where it can never leave for a place */
    walks(g, g->box, g->player, from);
    uint64_t root = pack(g->box, g->nbox, keeper_name(from, N), g);
    int head = 0, tail = 0, s;
    #define FIND(k, s)                                                                                              \
        do {                                                                                                        \
            s = (int)(((k) * 0x9e3779b97f4a7c15ull) >> 44);                                                        \
            while (key[s] && key[s] != (k)) s = (s + 1) & (TABLE - 1);                                              \
        } while (0)
    FIND(root, s);
    key[s] = root;
    order[tail] = root;
    parent[tail] = -1;
    push[tail] = -1;
    depth[tail++] = 0;
    while (head < tail && found < 0) {
        int at = head++, keeper;
        unpack(order[at], g->nbox, box, &keeper);
        walks(g, box, keeper, from);
        for (int c = 0; c < N && found < 0; c++) {
            if (!box[c]) continue;
            for (int d = 0; d < 4 && found < 0; d++) {
                int stand = next(g, c, d ^ 1), to = next(g, c, d);
                if (stand < 0 || to < 0 || from[stand] < 0 || g->map[to] != BP_FLOOR || box[to] || !live[to]) continue;
                box[c] = 0;
                box[to] = 1;
                int ok = !squared(g, box, to);
                uint64_t k = 0;
                if (ok) {
                    int8_t f2[BP_CELLS];
                    walks(g, box, c, f2);
                    k = pack(box, g->nbox, keeper_name(f2, N), g);
                }
                int solved = 1;
                for (int b = 0; b < N && solved; b++)
                    if (box[b] && !g->goal[b]) solved = 0;
                box[to] = 0;
                box[c] = 1;
                if (!ok) continue;
                FIND(k, s);
                if (key[s]) continue;
                if (tail >= MAX_STATES) { result = -2; goto done; }
                key[s] = k;
                order[tail] = k;
                parent[tail] = at;
                push[tail] = (int16_t)(c * 4 + d);
                depth[tail] = (int16_t)(depth[at] + 1);
                if (solved) found = tail;
                tail++;
            }
        }
    }
    #undef FIND
    if (found < 0) goto done;
    result = depth[found];
    int first = found;
    while (parent[first] > 0) first = parent[first];
    if (box_out) *box_out = push[first] / 4;
    if (dir_out) *dir_out = push[first] % 4;
done:
    free(key);
    free(order);
    free(parent);
    free(push);
    free(depth);
    return result;
}

/* the keeper walks behind the crate and pushes it: one action */
static int play_push(bp_game *g, int box, int d)
{
    int stand = next(g, box, d ^ 1);
    uint8_t dirs[BP_CELLS];
    int n = stand == g->player ? 0 : bp_path(g, stand, dirs, BP_CELLS);
    if (n < 0) return 0;
    bp_begin(g);
    for (int i = 0; i < n; i++) bp_step(g, dirs[i]);
    if (!n) bp_begin(g);
    return bp_step(g, d) == 2;
}

int bp_hint(bp_game *g)
{
    int box, d, k = bp_solve(g, &box, &d);
    for (int i = 0; k == -1 && i < 64 && g->nh; i++) {   /* stuck: back to where a way remains */
        bp_undo(g);
        k = bp_solve(g, &box, &d);
    }
    if (k <= 0 || !play_push(g, box, d)) return -1;
    return next(g, box, d);
}

int bp_script(bp_game *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ' || *s == ',') continue;
        if (*s == 'h' || *s == '=') {
            do {
                if (bp_hint(g) < 0) break;
            } while (*s == '=' && !bp_solved(g));
            continue;
        }
        const char *k = strchr("udlr", *s);
        if (!k) return 0;
        bp_begin(g);
        if (!bp_step(g, (int)(k - "udlr"))) return 0;
    }
    return 1;
}
