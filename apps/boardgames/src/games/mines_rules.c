/*
 * Mines: the rules (mines.h): laying the mines (no guessing: checked by the reasoner, laid again until it clears the
 * field), uncovering, chords, flags, undo by replaying the actions, hints. No SDL: tested by tests/mines_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/mines.h"

#include <string.h>

#include "games/puzrand.h"

static const int SIZE[MNS_SIZES][3] = {{9, 9, 10}, {8, 12, 15}, {16, 16, 40}, {16, 30, 99}};

void mns_size(int variant, int *w, int *h, int *mines)
{
    if (variant < 0 || variant >= MNS_VARIANTS) variant = 0;
    const int *s = SIZE[variant % MNS_SIZES];
    *w = s[0];
    *h = s[1];
    *mines = s[2];
}

/* the neighbours of c (up to 8) */
static int around(int w, int h, int c, int *nb)
{
    int x = c % w, y = c / w, n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < w && ny < h) nb[n++] = nx + ny * w;
        }
    return n;
}

static void count_adj(int w, int h, const uint8_t *mine, uint8_t *adj)
{
    int nb[8];
    for (int c = 0; c < w * h; c++) {
        int k = 0, n = around(w, h, c, nb);
        for (int i = 0; i < n; i++) k += mine[nb[i]];
        adj[c] = (uint8_t)k;
    }
}

/* ---- the reasoner ------------------------------------------------------------------------------------------------
 * kn: 0 covered and unknown, 1 uncovered (its number known), 2 a mine found, 3 found safe (not uncovered yet). One
 * round finds what can be known now: the safe cells are set to 3, the mines to 2; how many were found. */
enum { K_UNKNOWN, K_OPEN, K_MINE, K_SAFE };

typedef struct { int n, m, cell[8]; } constraint;   /* m mines among n unknown cells */

static int constraint_of(int w, int h, const uint8_t *adj, const uint8_t *kn, int c, constraint *k)
{
    int nb[8], n = around(w, h, c, nb);
    k->n = 0;
    k->m = adj[c];
    for (int i = 0; i < n; i++) {
        if (kn[nb[i]] == K_MINE) k->m--;
        else if (kn[nb[i]] == K_UNKNOWN) k->cell[k->n++] = nb[i];
    }
    return k->n > 0;
}

static int has(const constraint *k, int c)
{
    for (int i = 0; i < k->n; i++)
        if (k->cell[i] == c) return 1;
    return 0;
}

static int set_all(uint8_t *kn, const constraint *k, int v, const constraint *except)
{
    int found = 0;
    for (int i = 0; i < k->n; i++)
        if (kn[k->cell[i]] == K_UNKNOWN && !(except && has(except, k->cell[i]))) {
            kn[k->cell[i]] = (uint8_t)v;
            found++;
        }
    return found;
}

static int reason(int w, int h, int mines, const uint8_t *adj, uint8_t *kn, int *tier)
{
    int found = 0, N = w * h;
    constraint a, b;
    *tier = MNS_SINGLE;
    for (int c = 0; c < N; c++) {               /* each number alone */
        if (kn[c] != K_OPEN || !constraint_of(w, h, adj, kn, c, &a)) continue;
        if (a.m == 0) found += set_all(kn, &a, K_SAFE, NULL);
        else if (a.m == a.n) found += set_all(kn, &a, K_MINE, NULL);
    }
    if (found) return found;
    *tier = MNS_PAIR;
    for (int c = 0; c < N; c++) {               /* two numbers side by side */
        if (kn[c] != K_OPEN || !constraint_of(w, h, adj, kn, c, &a)) continue;
        int x = c % w, y = c / w;
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                int nx = x + dx, ny = y + dy, d = nx + ny * w;
                if ((!dx && !dy) || nx < 0 || ny < 0 || nx >= w || ny >= h || kn[d] != K_OPEN) continue;
                if (!constraint_of(w, h, adj, kn, d, &b)) continue;
                int shared = 0;
                for (int i = 0; i < a.n; i++) shared += has(&b, a.cell[i]);
                if (!shared) continue;
                int only_b = b.n - shared;
                if (b.m - a.m == only_b && (only_b || a.n > shared)) {
                    found += set_all(kn, &b, K_MINE, &a);   /* b's own cells: mines; a's own cells: safe */
                    found += set_all(kn, &a, K_SAFE, &b);
                    if (found) return found;
                }
            }
    }
    *tier = MNS_COUNT;
    int unknown = 0, left = mines;              /* the count of mines left */
    for (int c = 0; c < N; c++) {
        unknown += kn[c] == K_UNKNOWN;
        left -= kn[c] == K_MINE;
    }
    if (unknown && (left == 0 || left == unknown))
        for (int c = 0; c < N; c++)
            if (kn[c] == K_UNKNOWN) { kn[c] = left ? K_MINE : K_SAFE; found++; }
    return found;
}

/* a safe cell uncovered by the reasoner: an empty one uncovers its neighbours */
static void open_known(int w, int h, const uint8_t *adj, uint8_t *kn, int c)
{
    int stack[MNS_CELLS], sp = 0, nb[8];
    kn[c] = K_OPEN;
    stack[sp++] = c;
    while (sp) {
        int x = stack[--sp];
        if (adj[x]) continue;
        int n = around(w, h, x, nb);
        for (int i = 0; i < n; i++)
            if (kn[nb[i]] == K_UNKNOWN || kn[nb[i]] == K_SAFE) {
                kn[nb[i]] = K_OPEN;
                stack[sp++] = nb[i];
            }
    }
}

int mns_effort(int w, int h, int mines, const uint8_t *mine, int first, int *rounds)
{
    uint8_t adj[MNS_CELLS], kn[MNS_CELLS] = {0};
    int N = w * h, tier;
    if (rounds) memset(rounds, 0, MNS_TIERS * sizeof *rounds);
    count_adj(w, h, mine, adj);
    if (first < 0 || first >= N || mine[first]) return 0;
    open_known(w, h, adj, kn, first);
    while (reason(w, h, mines, adj, kn, &tier)) {
        if (rounds) rounds[tier]++;
        for (int c = 0; c < N; c++)
            if (kn[c] == K_SAFE) {
                if (mine[c]) return 0;          /* cannot happen: the reasoner is sound */
                open_known(w, h, adj, kn, c);
            }
    }
    for (int c = 0; c < N; c++)
        if (!mine[c] && kn[c] != K_OPEN) return 0;
    return 1;
}

int mns_logical(int w, int h, int mines, const uint8_t *mine, int first)
{
    return mns_effort(w, h, mines, mine, first, NULL);
}

/* ---- the levels' characters -------------------------------------------------------------------------------------- */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

void mns_encode(int w, int h, const uint8_t *mine, char *out)
{
    int N = w * h, n = 0;
    for (int i = 0; i < N; i += 6) {
        int v = 0;
        for (int k = 0; k < 6 && i + k < N; k++) v |= (mine[i + k] ? 1 : 0) << k;
        out[n++] = B64[v];
    }
    out[n] = 0;
}

int mns_decode(const mns_level *L, uint8_t *mine)
{
    int N = L->w * L->h, count = 0;
    if (L->w < 2 || L->h < 2 || N > MNS_CELLS || L->first >= N || !L->bits) return -1;
    memset(mine, 0, MNS_CELLS);
    for (int i = 0; i < N; i += 6) {
        char ch = L->bits[i / 6];
        const char *p = ch ? strchr(B64, ch) : NULL;
        if (!p) return -1;
        int v = (int)(p - B64);
        for (int k = 0; k < 6; k++) {
            if (!(v >> k & 1)) continue;
            if (i + k >= N) return -1;
            mine[i + k] = 1;
            count++;
        }
    }
    return L->bits[(N + 5) / 6] ? -1 : count;
}

int mns_lay(int w, int h, int mines, int logic, uint32_t seed, int first, uint8_t *mine)
{
    uint32_t s = pr_seed(seed ^ (uint32_t)(first * 7919));
    int N = w * h, free_[MNS_CELLS], fx = first % w, fy = first / w;
    for (int tries = 1;; tries++) {
        int nf = 0;
        for (int c = 0; c < N; c++) {           /* never on the first cell nor round it */
            int dx = c % w - fx, dy = c / w - fy;
            if (dx < -1 || dx > 1 || dy < -1 || dy > 1) free_[nf++] = c;
        }
        memset(mine, 0, MNS_CELLS);
        for (int i = 0; i < mines && i < nf; i++) {
            int j = i + pr_below(&s, nf - i), t = free_[i];
            free_[i] = free_[j];
            free_[j] = t;
            mine[free_[i]] = 1;
        }
        if (!logic || mns_logical(w, h, mines, mine, first)) return tries;
    }
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
static void uncover(mns_game *g, int c);

/* the field as it began (a level: its first area uncovered) */
static void reset(mns_game *g)
{
    memset(g->st, MNS_HIDDEN, sizeof g->st);
    g->blast = -1;
    g->moves = g->opened = g->flags = 0;
    if (g->fixed) uncover(g, g->first);
}

void mns_new(mns_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (variant < 0 || variant >= MNS_VARIANTS) variant = 0;
    g->variant = variant;
    mns_size(variant, &g->w, &g->h, &g->mines);
    g->logic = variant < MNS_SIZES;
    g->seed = seed;
    g->first = -1;
    reset(g);
}

int mns_load(mns_game *g, const mns_level *L)
{
    memset(g, 0, sizeof *g);
    int n = mns_decode(L, g->mine);
    if (n < 1 || g->mine[L->first]) { mns_new(g, 0, 1); return 0; }
    g->variant = -1;
    g->w = L->w;
    g->h = L->h;
    g->mines = n;
    g->logic = 1;
    g->placed = g->fixed = 1;
    g->first = L->first;
    count_adj(g->w, g->h, g->mine, g->adj);
    reset(g);
    return 1;
}

static void uncover(mns_game *g, int c)
{
    if (g->st[c] != MNS_HIDDEN) return;
    if (g->mine[c]) {
        g->st[c] = MNS_OPEN;
        if (g->blast < 0) g->blast = c;
        return;
    }
    int stack[MNS_CELLS], sp = 0, nb[8];
    g->st[c] = MNS_OPEN;
    g->opened++;
    stack[sp++] = c;
    while (sp) {
        int x = stack[--sp];
        if (g->adj[x]) continue;
        int n = around(g->w, g->h, x, nb);
        for (int i = 0; i < n; i++)
            if (g->st[nb[i]] == MNS_HIDDEN) {
                g->st[nb[i]] = MNS_OPEN;
                g->opened++;
                stack[sp++] = nb[i];
            }
    }
}

int mns_flags_around(const mns_game *g, int c)
{
    int nb[8], n = around(g->w, g->h, c, nb), k = 0;
    for (int i = 0; i < n; i++) k += g->st[nb[i]] == MNS_FLAG;
    return k;
}

/* an action applied (no history): 0 not allowed */
static int apply(mns_game *g, int c, int kind)
{
    if (c < 0 || c >= g->w * g->h || g->blast >= 0) return 0;
    if (kind == MNS_MARK) {
        if (g->st[c] == MNS_OPEN) return 0;
        g->st[c] = g->st[c] == MNS_FLAG ? MNS_HIDDEN : MNS_FLAG;
        g->flags += g->st[c] == MNS_FLAG ? 1 : -1;
        g->moves++;
        return 1;
    }
    if (g->st[c] == MNS_FLAG) return 0;
    if (g->st[c] == MNS_HIDDEN) {
        if (!g->placed) {
            mns_lay(g->w, g->h, g->mines, g->logic, g->seed, c, g->mine);
            count_adj(g->w, g->h, g->mine, g->adj);
            g->placed = 1;
            g->first = c;
        }
        uncover(g, c);
        g->moves++;
        return 1;
    }
    /* a chord: a number with all its flags set uncovers its other neighbours */
    int nb[8], n = around(g->w, g->h, c, nb), hidden = 0;
    for (int i = 0; i < n; i++) hidden += g->st[nb[i]] == MNS_HIDDEN;
    if (!g->adj[c] || !hidden || mns_flags_around(g, c) != g->adj[c]) return 0;
    for (int i = 0; i < n; i++) uncover(g, nb[i]);
    g->moves++;
    return 1;
}

static int act(mns_game *g, int c, int kind)
{
    if (g->na >= MNS_HIST || !apply(g, c, kind)) return 0;
    g->act[g->na++] = (uint16_t)(c | kind << 12);
    if (g->blast >= 0) g->mistakes++;           /* a mine uncovered (a chord too): undo will not take it back */
    return 1;
}

int mns_dig(mns_game *g, int c) { return act(g, c, MNS_DIG); }
int mns_flag(mns_game *g, int c) { return act(g, c, MNS_MARK); }

int mns_undo(mns_game *g)
{
    if (!g->na) return 0;
    g->na--;
    int digs = 0;
    for (int i = 0; i < g->na; i++) digs += (g->act[i] >> 12) == MNS_DIG;
    if (!digs && !g->fixed) { g->placed = 0; g->first = -1; }   /* the first uncovering taken back: the mines too */
    reset(g);
    for (int i = 0; i < g->na; i++) apply(g, g->act[i] & 0xfff, g->act[i] >> 12);
    return 1;
}

int mns_solved(const mns_game *g)
{
    return g->placed && g->blast < 0 && g->opened == g->w * g->h - g->mines;
}

int mns_hint(const mns_game *g, int *kind)
{
    int N = g->w * g->h;
    *kind = MNS_DIG;
    if (g->blast >= 0) { *kind = MNS_UNDO; return g->blast; }
    if (mns_solved(g)) return -1;
    if (!g->placed) return g->w / 2 + g->h / 2 * g->w;
    for (int c = 0; c < N; c++)                 /* a wrong flag first */
        if (g->st[c] == MNS_FLAG && !g->mine[c]) { *kind = MNS_MARK; return c; }
    uint8_t kn[MNS_CELLS];
    for (int c = 0; c < N; c++) kn[c] = g->st[c] == MNS_OPEN ? K_OPEN : K_UNKNOWN;
    /* the reasoner from what is uncovered (the flags left aside: the player's own): a safe cell first; the mines it
     * finds on the way help it further, and the first one not flagged yet is the hint if no safe cell turns up */
    int sure_mine = -1, tier;
    while (reason(g->w, g->h, g->mines, g->adj, kn, &tier)) {
        for (int c = 0; c < N; c++)
            if (kn[c] == K_SAFE && g->st[c] == MNS_HIDDEN) return c;
        for (int c = 0; c < N && sure_mine < 0; c++)
            if (kn[c] == K_MINE && g->st[c] == MNS_HIDDEN) sure_mine = c;
    }
    if (sure_mine >= 0) { *kind = MNS_MARK; return sure_mine; }
    /* no reasoning helps (a classic field may need a guess): a safe cell next to the uncovered ones */
    int any = -1;
    for (int c = 0; c < N; c++) {
        if (g->st[c] != MNS_HIDDEN || g->mine[c]) continue;
        if (any < 0) any = c;
        int nb[8], n = around(g->w, g->h, c, nb);
        for (int i = 0; i < n; i++)
            if (g->st[nb[i]] == MNS_OPEN) return c;
    }
    return any;
}

int mns_play_hint(mns_game *g)
{
    int kind, c = mns_hint(g, &kind);
    if (c < 0) return -1;
    if (kind == MNS_UNDO) return mns_undo(g) ? c : -1;   /* the mine already counted: not a hint again */
    int wrong = kind == MNS_MARK && g->st[c] == MNS_FLAG;   /* a wrong flag pulled up: a mistake too */
    if (!act(g, c, kind)) return -1;
    g->hints++;
    g->mistakes += wrong;
    return c;
}

static int read_cell(const mns_game *g, const char **s)
{
    int x = 0, y = 0;
    if (**s < '0' || **s > '9') return -1;
    while (**s >= '0' && **s <= '9') x = x * 10 + *(*s)++ - '0';
    if (**s != '.') return -1;
    (*s)++;
    if (**s < '0' || **s > '9') return -1;
    while (**s >= '0' && **s <= '9') y = y * 10 + *(*s)++ - '0';
    if (x < 1 || y < 1 || x > g->w || y > g->h) return -1;
    return x - 1 + (y - 1) * g->w;
}

int mns_script(mns_game *g, const char *s)
{
    while (*s) {
        char k = *s;
        if (k == ' ' || k == ',') { s++; continue; }
        s++;
        if (k == 'h' || k == '=') {
            do {
                if (mns_play_hint(g) < 0) break;
            } while (k == '=' && !mns_solved(g));
            continue;
        }
        if (k == '+') {
            int n = 0;
            while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
            for (int i = 0; i < n && !mns_solved(g); i++)
                if (mns_play_hint(g) < 0) break;
            continue;
        }
        if (k == '@' || k == '#') {            /* every safe cell but one uncovered (#: all of them) */
            if (!g->placed) return 0;
            int last = -1;
            for (int c = g->w * g->h - 1; c >= 0 && last < 0 && k == '@'; c--)
                if (!g->mine[c] && g->st[c] == MNS_HIDDEN) last = c;
            for (int c = 0; c < g->w * g->h; c++)
                if (!g->mine[c] && g->st[c] == MNS_HIDDEN && c != last &&
                    (k == '#' || g->opened < g->w * g->h - g->mines - 1))
                    act(g, c, MNS_DIG);
            continue;
        }
        if (k != 'd' && k != 'f') return 0;
        int c = read_cell(g, &s);
        if (c < 0 || !act(g, c, k == 'd' ? MNS_DIG : MNS_MARK)) return 0;
    }
    return 1;
}
