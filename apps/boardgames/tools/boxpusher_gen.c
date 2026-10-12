/*
 * Box Pusher's levels, made by chance and chosen: src/games/boxpusher_levels.h. Our own levels: no published set is
 * used. Each candidate:
 *   - a room: a rectangle of floor in walls, corners cut away, walls dropped inside at random (the floor stays in one
 *     piece, no cell walled on three sides);
 *   - places drawn at random, the crates set on them;
 *   - every position reached from there by pulling crates (the keeper steps back from a crate and drags it), breadth
 *     first: the farthest one (the most pulls, then the most cells the crates travelled) is the start, its pulls the
 *     fewest pushes that solve it;
 *   - checked by the rules' own solver (bp_solve: the same count), then played by its hints to count how often the
 *     crate pushed changes.
 * The candidates are graded (pushes, crate changes, crates) and 60 chosen, evenly over the grades, into chapters of
 * 20 from the easiest.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/bpgen apps/boardgames/tools/boxpusher_gen.c \
 *       apps/boardgames/src/games/boxpusher_rules.c && /tmp/bpgen > apps/boardgames/src/games/boxpusher_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/boxpusher.h"
#include "games/puzrand.h"

typedef struct {
    int w, h, nbox, keeper;
    uint8_t map[BP_CELLS], goal[BP_CELLS], box[BP_CELLS];
    int pushes, changes, score;
    char rows[BP_CELLS + BP_MAXH + 1];
} level;

static uint32_t rng;

static int nxt(int w, int h, int c, int d)
{
    static const int DX[4] = {0, 0, -1, 1}, DY[4] = {-1, 1, 0, 0};
    int x = c % w + DX[d], y = c / w + DY[d];
    return x < 0 || y < 0 || x >= w || y >= h ? -1 : x + y * w;
}

static int floor_cells(const level *L)
{
    int n = 0;
    for (int c = 0; c < L->w * L->h; c++) n += L->map[c] == BP_FLOOR;
    return n;
}

static int connected(const level *L)
{
    int seen[BP_CELLS] = {0}, q[BP_CELLS], h = 0, t = 0, start = -1, n = floor_cells(L);
    for (int c = 0; c < L->w * L->h && start < 0; c++) if (L->map[c] == BP_FLOOR) start = c;
    if (start < 0) return 0;
    seen[start] = 1;
    q[t++] = start;
    while (h < t) {
        int c = q[h++];
        for (int d = 0; d < 4; d++) {
            int m = nxt(L->w, L->h, c, d);
            if (m >= 0 && !seen[m] && L->map[m] == BP_FLOOR) { seen[m] = 1; q[t++] = m; }
        }
    }
    return t == n;
}

static int walled_three(const level *L)
{
    for (int c = 0; c < L->w * L->h; c++) {
        if (L->map[c] != BP_FLOOR) continue;
        int walls = 0;
        for (int d = 0; d < 4; d++) {
            int m = nxt(L->w, L->h, c, d);
            walls += m < 0 || L->map[m] != BP_FLOOR;
        }
        if (walls >= 3) return 1;
    }
    return 0;
}

static void room(level *L, int iw, int ih, int drops)
{
    L->w = iw + 2;
    L->h = ih + 2;
    memset(L->map, BP_WALL, sizeof L->map);
    for (int y = 1; y <= ih; y++)
        for (int x = 1; x <= iw; x++) L->map[x + y * L->w] = BP_FLOOR;
    for (int k = 0; k < 4; k++) {               /* corners cut away */
        if (pr_below(&rng, 3)) continue;
        int cw = 1 + pr_below(&rng, iw / 2), ch = 1 + pr_below(&rng, ih / 2);
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++) {
                int cx = k & 1 ? iw - x : 1 + x, cy = k & 2 ? ih - y : 1 + y;
                L->map[cx + cy * L->w] = BP_WALL;
            }
    }
    for (int k = 0, tries = 0; k < drops && tries < 400; tries++) {
        int c = (1 + pr_below(&rng, iw)) + (1 + pr_below(&rng, ih)) * L->w;
        if (L->map[c] != BP_FLOOR) continue;
        L->map[c] = BP_WALL;
        if (!connected(L) || walled_three(L)) L->map[c] = BP_FLOOR;
        else k++;
    }
}

/* ---- the pulls: breadth first from the crates on their places ------------------------------------------------- */
#define TABLE (1 << 21)
static uint64_t *key, *order;
static int *depth_, *travel;

static void reach(const level *L, const uint8_t *box, int from, uint8_t *seen)
{
    int q[BP_CELLS], h = 0, t = 0;
    memset(seen, 0, BP_CELLS);
    seen[from] = 1;
    q[t++] = from;
    while (h < t) {
        int c = q[h++];
        for (int d = 0; d < 4; d++) {
            int m = nxt(L->w, L->h, c, d);
            if (m >= 0 && !seen[m] && L->map[m] == BP_FLOOR && !box[m]) { seen[m] = 1; q[t++] = m; }
        }
    }
}

static int first_of(const uint8_t *seen, int n) { for (int c = 0; c < n; c++) if (seen[c]) return c; return -1; }

static uint64_t pack(const uint8_t *box, int n, int keeper)
{
    uint64_t k = (uint64_t)keeper + 1;
    for (int c = 0; c < n; c++) if (box[c]) k = k << 8 | (uint64_t)c;
    return k;
}

static void unpack(uint64_t k, int nbox, uint8_t *box, int *keeper)
{
    memset(box, 0, BP_CELLS);
    for (int i = 0; i < nbox; i++, k >>= 8) box[k & 255] = 1;
    *keeper = (int)(k & 255) - 1;
}

static int find(uint64_t k, int add)
{
    int s = (int)((k * 0x9e3779b97f4a7c15ull) >> 43);
    while (key[s] && key[s] != k) s = (s + 1) & (TABLE - 1);
    if (key[s]) return s;
    if (add) key[s] = k;
    return add ? s : -1;
}

/* the farthest start by pulls; 0 when the room gives nothing worth playing */
static int pull_search(level *L)
{
    int N = L->w * L->h, head = 0, tail = 0;
    memset(key, 0, TABLE * sizeof *key);
    uint8_t box[BP_CELLS], seen[BP_CELLS];
    memcpy(box, L->goal, sizeof box);
    uint8_t done[BP_CELLS] = {0};
    for (int c = 0; c < N; c++) {               /* the keeper anywhere: each area once */
        if (L->map[c] != BP_FLOOR || box[c] || done[c]) continue;
        reach(L, box, c, seen);
        for (int k = 0; k < N; k++) done[k] |= seen[k];
        uint64_t k = pack(box, N, first_of(seen, N));
        find(k, 1);
        order[tail] = k;
        depth_[tail] = 0;
        travel[tail++] = 0;
    }
    int best = -1;
    while (head < tail) {
        int at = head++, keeper;
        unpack(order[at], L->nbox, box, &keeper);
        reach(L, box, keeper, seen);
        for (int b = 0; b < N; b++) {
            if (!box[b]) continue;
            for (int d = 0; d < 4; d++) {       /* keeper on p = b + d, steps to p + d, the crate follows to p */
                int p = nxt(L->w, L->h, b, d), p2 = p < 0 ? -1 : nxt(L->w, L->h, p, d);
                if (p < 0 || p2 < 0 || !seen[p] || L->map[p2] != BP_FLOOR || box[p2]) continue;
                box[b] = 0;
                box[p] = 1;
                uint8_t s2[BP_CELLS];
                reach(L, box, p2, s2);
                uint64_t k = pack(box, N, first_of(s2, N));
                box[p] = 0;
                box[b] = 1;
                if (find(k, 0) >= 0) continue;
                if (tail >= TABLE / 2) return 0;
                find(k, 1);
                order[tail] = k;
                depth_[tail] = depth_[at] + 1;
                travel[tail] = travel[at] + 1;
                tail++;
            }
        }
    }
    for (int i = 0; i < tail; i++) {
        unpack(order[i], L->nbox, box, &head);
        int on = 0;
        for (int c = 0; c < N; c++) on += box[c] && L->goal[c];
        if (on) continue;                       /* every crate off its place at the start */
        if (best < 0 || depth_[i] > depth_[best]) best = i;
    }
    if (best < 0) return 0;
    int keeper;
    unpack(order[best], L->nbox, L->box, &keeper);
    reach(L, L->box, keeper, seen);
    int cand[BP_CELLS], n = 0;
    for (int c = 0; c < N; c++) if (seen[c]) cand[n++] = c;
    L->keeper = cand[pr_below(&rng, n)];
    L->pushes = depth_[best];
    return 1;
}

static void to_rows(level *L)
{
    /* walls with no floor round them (eight ways) are outside */
    char *o = L->rows;
    for (int y = 0; y < L->h; y++) {
        for (int x = 0; x < L->w; x++) {
            int c = x + y * L->w, near = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < L->w && ny < L->h && L->map[nx + ny * L->w] == BP_FLOOR) near = 1;
                }
            char ch = L->map[c] == BP_WALL ? (near ? '#' : '_') : ' ';
            if (L->map[c] == BP_FLOOR) {
                if (L->box[c]) ch = L->goal[c] ? '*' : '$';
                else if (c == L->keeper) ch = L->goal[c] ? '+' : '@';
                else if (L->goal[c]) ch = '.';
            }
            *o++ = ch;
        }
        if (y < L->h - 1) *o++ = '|';
    }
    *o = 0;
}

/* the rules' solver agrees; the hints played: how often the crate pushed changes */
static int check(level *L)
{
    bp_game g;
    g.level = 0;
    if (!bp_load(&g, L->rows)) return 0;
    if (bp_solve(&g, NULL, NULL) != L->pushes) return 0;
    int last = -1, changes = 0;
    while (!bp_solved(&g)) {
        int bx, d;
        if (bp_solve(&g, &bx, &d) <= 0) return 0;
        if (bx != last) changes++;
        if (bp_hint(&g) < 0) return 0;
        last = bx + (d == BP_LEFT ? -1 : d == BP_RIGHT ? 1 : d == BP_UP ? -g.w : g.w);
    }
    L->changes = changes;
    return 1;
}

static int cmp(const void *a, const void *b) { return ((const level *)a)->score - ((const level *)b)->score; }

#define CANDS 400

int main(void)
{
    key = malloc(TABLE * sizeof *key);
    order = malloc(TABLE / 2 * sizeof *order);
    depth_ = malloc(TABLE / 2 * sizeof *depth_);
    travel = malloc(TABLE / 2 * sizeof *travel);
    static level cand[3][CANDS];
    int nc[3] = {0};
    rng = pr_seed(20261012);
    /* the chapters' rooms: (inside width, height, walls dropped, crates) */
    for (int ch = 0; ch < 3; ch++) {
        for (int tries = 0; nc[ch] < CANDS && tries < 40000; tries++) {
            level L;
            memset(&L, 0, sizeof L);
            int iw = ch == 0 ? 5 + pr_below(&rng, 2) : 6 + pr_below(&rng, 3);
            int ih = ch == 0 ? 5 + pr_below(&rng, 2) : 6 + pr_below(&rng, 3);
            L.nbox = ch == 0 ? 2 + pr_below(&rng, 2) : ch == 1 ? 3 : 3 + pr_below(&rng, 2);
            room(&L, iw, ih, (iw * ih) / (ch == 0 ? 5 : 4) + pr_below(&rng, 4));
            int fl = floor_cells(&L);
            if (fl < L.nbox * 4 + 6) continue;
            for (int k = 0; k < L.nbox;) {      /* the places */
                int c = pr_below(&rng, L.w * L.h);
                if (L.map[c] == BP_FLOOR && !L.goal[c]) { L.goal[c] = 1; k++; }
            }
            if (!pull_search(&L)) continue;
            int lo = ch == 0 ? 8 : ch == 1 ? 18 : 28;
            if (L.pushes < lo) continue;
            to_rows(&L);
            if (!check(&L)) continue;
            if (L.changes < L.nbox + (ch ? 2 : 0)) continue;
            L.score = L.pushes + 3 * L.changes + 6 * L.nbox;
            cand[ch][nc[ch]++] = L;
        }
        qsort(cand[ch], (size_t)nc[ch], sizeof(level), cmp);
        fprintf(stderr, "chapter %d: %d candidates, scores %d..%d\n", ch + 1, nc[ch], nc[ch] ? cand[ch][0].score : 0,
                nc[ch] ? cand[ch][nc[ch] - 1].score : 0);
    }
    static const char *const head[] = {
        "/*",
        " * Box Pusher's levels: our own, made by tools/boxpusher_gen.c (rooms drawn by chance, starts found by",
        " * pulling the crates back from their places, each checked by the rules' solver), three chapters of 20 from",
        " * the easiest. Each: the rows, then (in the comment) its fewest pushes and how often the crate pushed",
        " * changes on that way.",
        " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).",
        " */",
    };
    for (size_t i = 0; i < sizeof head / sizeof head[0]; i++) printf("%s\n", head[i]);
    printf("const char *const bp_levels[BP_LEVELS] = {\n");
    for (int ch = 0; ch < 3; ch++) {
        /* 20 evenly through the upper part of the chapter's grades (the lowest ones are too plain), easiest first */
        int from = nc[ch] / (ch == 0 ? 4 : 3), span = nc[ch] - from;
        for (int i = 0; i < BP_PER_CHAPTER; i++) {
            level *L = &cand[ch][from + (int)((long)span * i / BP_PER_CHAPTER + span / (2 * BP_PER_CHAPTER))];
            printf("    \"%s\", /* %d: %d pushes, %d changes, %d crates */\n", L->rows, ch * 20 + i + 1, L->pushes,
                   L->changes, L->nbox);
        }
    }
    printf("};\n/* each level's fewest pushes (the stars: three for as few) */\nconst int16_t bp_par[BP_LEVELS] = {");
    for (int ch = 0, n = 0; ch < 3; ch++) {
        int from = nc[ch] / (ch == 0 ? 4 : 3), span = nc[ch] - from;
        for (int i = 0; i < BP_PER_CHAPTER; i++, n++)
            printf("%s%d", n ? ", " : "",
                   cand[ch][from + (int)((long)span * i / BP_PER_CHAPTER + span / (2 * BP_PER_CHAPTER))].pushes);
    }
    printf("};\n");
    return 0;
}
