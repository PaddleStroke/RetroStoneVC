/*
 * Tile Rummy: the rules (tilerummy.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "cards.h"
#include "games/tilerummy.h"

static uint8_t FACE[CARDS_MAX];

int tr_face(int tile)
{
    if (!FACE[TR_TILES - 1]) cards_pack(PACK_TILES, FACE);
    return FACE[tile];
}

int tr_is_joker(int tile) { return CARD_SUIT(tr_face(tile)) == SUIT_JOKER; }
static int num(int tile) { return CARD_RANK(tr_face(tile)); }
static int col(int tile) { return CARD_SUIT(tr_face(tile)); }
int tr_penalty(int tile) { return tr_is_joker(tile) ? TR_JOKER_PEN : num(tile); }

/* ---- a set: a run or a group, its worth, its order --------------------------------------------------------------- */
/* the tiles t[0..n) (ids): the kind; the worth (a joker the tile it stands for: at a run's high end when it can); the
 * tiles in order into o (a run's jokers in their places; else by colour, or number, the jokers last) */
static int analyse(const uint8_t *t, int n, int *value, uint8_t *o)
{
    int jokers = 0, lo = 99, hi = 0, colours = 0, nums = 0, same_col = 1, same_num = 1, c0 = -1, n0 = -1;
    unsigned seen_num = 0;
    for (int i = 0; i < n; i++) {
        if (tr_is_joker(t[i])) { jokers++; continue; }
        int c = col(t[i]), v = num(t[i]);
        if (c0 < 0) c0 = c, n0 = v;
        same_col &= c == c0;
        same_num &= v == n0;
        if (seen_num >> v & 1) nums = -1000;    /* a number twice */
        seen_num |= 1u << v;
        colours |= 1 << c;
        nums++;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    int m = n - jokers, run = 0, group = 0, rv = 0, gv = 0, start = 0;
    if (n >= 3 && n <= 13 && m > 0 && same_col && nums == m && hi - lo + 1 <= n) {
        run = 1;
        start = lo < 14 - n ? lo : 14 - n;
        rv = n * start + n * (n - 1) / 2;
    }
    int ncol = 0;
    for (int c = 0; c < 4; c++) ncol += colours >> c & 1;
    if (n >= 3 && n <= 4 && m > 0 && same_num && ncol == m) group = 1, gv = n0 * n;
    int kind = run && (!group || rv >= gv) ? TS_RUN : group ? TS_GROUP : TS_BAD;
    if (value) *value = kind == TS_RUN ? rv : kind == TS_GROUP ? gv : 0;
    if (!o) return kind;
    if (kind == TS_RUN) {
        uint8_t slot[13];
        memset(slot, TR_NONE, sizeof slot);
        for (int i = 0; i < n; i++)
            if (!tr_is_joker(t[i])) slot[num(t[i]) - start] = t[i];
        for (int i = 0, k = 0; i < n; i++) {
            if (!tr_is_joker(t[i])) continue;
            while (slot[k] != TR_NONE) k++;
            slot[k] = t[i];
        }
        memcpy(o, slot, (size_t)n);
        return kind;
    }
    int key[TR_TILES];
    for (int i = 0; i < n; i++) {
        o[i] = t[i];
        key[i] = tr_is_joker(t[i]) ? 1000 + t[i]
                 : (kind == TS_GROUP || (same_num && !same_col)) ? col(t[i]) * 16 + num(t[i])
                 : same_col ? num(t[i]) * 4 + col(t[i])
                            : col(t[i]) * 16 + num(t[i]);
    }
    for (int i = 1; i < n; i++)                 /* insertion sort: a handful of tiles */
        for (int j = i; j > 0 && key[j] < key[j - 1]; j--) {
            int k = key[j];
            key[j] = key[j - 1], key[j - 1] = k;
            uint8_t x = o[j];
            o[j] = o[j - 1], o[j - 1] = x;
        }
    return kind;
}

static int collect(const tpos *p, int set, uint8_t *t)
{
    int n = 0;
    for (int i = 0; i < TR_TILES; i++)
        if (p->where[i] == set) t[n++] = (uint8_t)i;
    return n;
}

int tr_set(const tpos *p, int set, uint8_t *tiles, int *kind, int *value)
{
    uint8_t t[TR_TILES];
    int n = collect(p, set, t), k = analyse(t, n, value, tiles);
    if (kind) *kind = k;
    return n;
}

int tr_rack(const tpos *p, int pl, int by_number, uint8_t *o)
{
    int n = 0, key[TR_TILES];
    for (int i = 0; i < TR_TILES; i++) {
        if (p->where[i] != TR_RACK + pl) continue;
        int k = tr_is_joker(i) ? 1000 : by_number ? num(i) * 8 + col(i) : col(i) * 16 + num(i);
        int j = n++;
        while (j > 0 && key[j - 1] > k) { key[j] = key[j - 1]; o[j] = o[j - 1]; j--; }
        key[j] = k;
        o[j] = (uint8_t)i;
    }
    return n;
}

/* ---- the turn ---------------------------------------------------------------------------------------------------- */
int tr_mine(const tgame *g, int t) { return g->start.where[t] == TR_RACK + g->start.turn; }

static int on_table(const tpos *p, int t) { return p->where[t] < p->nsets; }

int tr_changed(const tgame *g) { return memcmp(g->pos.where, g->start.where, sizeof g->pos.where) != 0; }

int tr_laid(const tgame *g)
{
    int n = 0;
    for (int t = 0; t < TR_TILES; t++) n += tr_mine(g, t) && on_table(&g->pos, t);
    return n;
}

/* a set made of the player's tiles alone (this turn) */
static int all_mine(const tgame *g, int set)
{
    int n = 0;
    for (int t = 0; t < TR_TILES; t++)
        if (g->pos.where[t] == set) {
            if (!tr_mine(g, t)) return 0;
            n++;
        }
    return n > 0;
}

int tr_meld(const tgame *g)
{
    int sum = 0;
    for (int s = 0; s < g->pos.nsets; s++) {
        int kind, v;
        if (!all_mine(g, s)) continue;
        tr_set(&g->pos, s, NULL, &kind, &v);
        if (kind != TS_BAD) sum += v;
    }
    return sum;
}

/* before the first meld: the table's sets just as they were, none of the player's tiles in them */
static int untouched(const tgame *g)
{
    const tpos *p = &g->pos, *s = &g->start;
    for (int k = 0; k < p->nsets; k++) {
        int from = -1, nt = 0, nm = 0;
        for (int t = 0; t < TR_TILES; t++) {
            if (p->where[t] != k) continue;
            if (tr_mine(g, t)) { nm++; continue; }
            if (s->where[t] >= s->nsets) return 0;      /* from nowhere: cannot be */
            if (from >= 0 && s->where[t] != from) return 0;
            from = s->where[t];
            nt++;
        }
        if (nt && nm) return 0;
        if (nt) {
            int was = 0;
            for (int t = 0; t < TR_TILES; t++) was += s->where[t] == from;
            if (was != nt) return 0;
        }
    }
    return 1;
}

int tr_check(const tgame *g)
{
    const tpos *p = &g->pos;
    if (p->phase != TP_PLAY) return TC_NOTHING;
    if (!tr_laid(g)) return TC_NOTHING;
    for (int k = 0; k < p->nsets; k++) {
        int kind;
        tr_set(p, k, NULL, &kind, NULL);
        if (kind == TS_BAD) return TC_INVALID;
    }
    for (int t = 0; t < TR_TILES; t++)          /* nothing of the table back in a rack */
        if (on_table(&g->start, t) && !on_table(p, t)) return TC_TOUCHED;
    if (!p->out[p->turn]) {
        if (!untouched(g)) return TC_TOUCHED;
        if (tr_meld(g) < TR_FIRST) return TC_LOW;
    }
    return TC_OK;
}

static int set_has_table_tile(const tgame *g, int set)
{
    for (int t = 0; t < TR_TILES; t++)
        if (g->pos.where[t] == set && !tr_mine(g, t)) return 1;
    return 0;
}

int tr_can_put(const tgame *g, int t, int dest)
{
    const tpos *p = &g->pos;
    if (p->phase != TP_PLAY || t < 0 || t >= TR_TILES) return 0;
    int w = p->where[t], me = TR_RACK + p->turn, mine = tr_mine(g, t), out = p->out[p->turn];
    if (w != me && w >= p->nsets) return 0;     /* the pool, another rack */
    if (w < p->nsets && !mine && !out) return 0;   /* the table's tiles wait for the first meld */
    if (dest == TR_TORACK) return mine && w != me;
    if (dest == TR_NEWSET) return 1;
    if (dest >= p->nsets || dest == w) return 0;
    return out || !set_has_table_tile(g, dest);
}

/* empty sets taken away, the others keeping their order */
static void compact(tpos *p)
{
    int count[TR_MAXSETS + 1] = {0}, map[TR_MAXSETS + 1], n = 0;
    for (int t = 0; t < TR_TILES; t++)
        if (p->where[t] < TR_MAXSETS) count[p->where[t]]++;
    for (int k = 0; k < TR_MAXSETS; k++) map[k] = count[k] ? n++ : -1;
    for (int t = 0; t < TR_TILES; t++)
        if (p->where[t] < TR_MAXSETS) p->where[t] = (uint8_t)map[p->where[t]];
    p->nsets = (uint8_t)n;
}

int tr_put(tgame *g, const uint8_t *tiles, int n, int dest)
{
    tpos *p = &g->pos;
    uint8_t list[TR_TILES];
    int k = 0;
    for (int i = 0; i < n; i++) {
        int t = tiles[i], dup = 0;
        if (t >= TR_TILES) return 0;
        for (int j = 0; j < k; j++) dup |= list[j] == t;
        if (dup || (dest < TR_MAXSETS && p->where[t] == dest)) continue;   /* there already */
        if (!tr_can_put(g, t, dest)) return 0;
        list[k++] = (uint8_t)t;
    }
    if (!k || (dest == TR_NEWSET && p->nsets >= TR_MAXSETS)) return 0;
    if (g->nundo >= TR_UNDO) {
        memmove(g->undo, g->undo + 1, sizeof g->undo[0] * (TR_UNDO - 1));
        g->nundo--;
    }
    g->undo[g->nundo++] = *p;
    int to = dest == TR_TORACK ? TR_RACK + p->turn : dest;
    if (dest == TR_NEWSET) {                    /* a new set: next to the set the tiles all come from, else last */
        int from = -1, one = 1;
        for (int i = 0; i < k; i++) {
            int w = p->where[list[i]];
            if (w >= p->nsets || (from >= 0 && w != from)) one = 0;
            from = w;
        }
        if (one) {
            for (int t = 0; t < TR_TILES; t++)
                if (p->where[t] > from && p->where[t] < p->nsets) p->where[t]++;
            to = from + 1;
        } else to = p->nsets;
        p->nsets++;
    }
    for (int i = 0; i < k; i++) p->where[list[i]] = (uint8_t)to;
    compact(p);
    return 1;
}

int tr_undo(tgame *g)
{
    if (!g->nundo || g->pos.phase != TP_PLAY) return 0;
    g->pos = g->undo[--g->nundo];
    return 1;
}

int tr_reset(tgame *g)
{
    if (g->pos.phase != TP_PLAY || (!g->nundo && !tr_changed(g))) return 0;
    g->pos = g->start;
    g->nundo = 0;
    return 1;
}

/* ---- the rounds -------------------------------------------------------------------------------------------------- */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void deal(tpos *p)
{
    for (int t = 0; t < TR_TILES; t++) p->pool[t] = (uint8_t)t, p->where[t] = TR_POOL;
    cards_shuffle(p->pool, TR_TILES, mix(p->seed, 1000u + p->round));
    p->npool = TR_TILES;
    for (int i = 0; i < TR_DEAL; i++)
        for (int k = 0; k < p->players; k++)
            p->where[p->pool[--p->npool]] = (uint8_t)(TR_RACK + (p->first + k) % p->players);
    p->nsets = 0;
    p->passes = 0;
    p->winner = TR_NONE;
    memset(p->out, 0, sizeof p->out);
    memset(p->lastplay, 0, sizeof p->lastplay);
    p->turn = p->first;
    p->phase = TP_PLAY;
}

static void turn_start(tgame *g)
{
    g->start = g->pos;
    g->nundo = 0;
}

void tr_new(tgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    tpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant == TR_MATCH ? TR_MATCH : TR_ONE);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > TR_MAXP ? TR_MAXP : players);
    p->first = 0;
    deal(p);
    turn_start(g);
}

static int rack_count(const tpos *p, int pl)
{
    int n = 0;
    for (int t = 0; t < TR_TILES; t++) n += p->where[t] == TR_RACK + pl;
    return n;
}

static void round_end(tpos *p, int w)
{
    int pen[TR_MAXP] = {0}, low = 1 << 20, nlow = 0;
    for (int t = 0; t < TR_TILES; t++)
        if (p->where[t] >= TR_RACK && p->where[t] < TR_RACK + p->players) pen[p->where[t] - TR_RACK] += tr_penalty(t);
    for (int q = 0; q < p->players; q++) {
        if (pen[q] < low) low = pen[q], nlow = 0;
        if (pen[q] == low) nlow++;
    }
    int out = w != TR_NONE, gain = 0;            /* out: a player went out; else blocked */
    if (!out && nlow == 1)
        for (int q = 0; q < p->players; q++)
            if (pen[q] == low) w = q;
    for (int q = 0; q < p->players; q++) {
        int lose = out ? (q == w ? 0 : pen[q]) : pen[q] - low;
        p->last[q] = (int16_t)-lose;
        gain += lose;
    }
    for (int q = 0; q < p->players; q++)
        if (out ? q == w : pen[q] == low) p->last[q] = (int16_t)(gain / (out ? 1 : nlow));
    for (int q = 0; q < p->players; q++) p->total[q] = (int16_t)(p->total[q] + p->last[q]);
    p->winner = (uint8_t)w;
    p->round++;
    p->first = (uint8_t)((p->first + 1) % p->players);
    p->phase = p->variant == TR_ONE || p->round >= p->players ? TP_OVER : TP_ROUND;
}

int tr_end(tgame *g)
{
    if (tr_check(g) != TC_OK) return 0;
    tpos *p = &g->pos;
    int me = p->turn;
    for (int t = 0; t < TR_TILES; t++) p->lastplay[t] = (uint8_t)(tr_mine(g, t) && on_table(p, t));
    p->out[me] = 1;
    p->passes = 0;
    if (!rack_count(p, me)) round_end(p, me);
    else p->turn = (uint8_t)((me + 1) % p->players);
    turn_start(g);
    return 1;
}

int tr_draw(tgame *g)
{
    tpos *p = &g->pos;
    if (p->phase != TP_PLAY || tr_changed(g)) return 0;
    *p = g->start;
    memset(p->lastplay, 0, sizeof p->lastplay);
    if (p->npool) {
        p->where[p->pool[--p->npool]] = (uint8_t)(TR_RACK + p->turn);
        p->passes = 0;
    } else if (++p->passes >= p->players) {
        round_end(p, TR_NONE);
        turn_start(g);
        return 1;
    }
    p->turn = (uint8_t)((p->turn + 1) % p->players);
    turn_start(g);
    return 1;
}

int tr_next(tgame *g)
{
    if (g->pos.phase != TP_ROUND) return 0;
    deal(&g->pos);
    turn_start(g);
    return 1;
}

unsigned tr_winners(const tpos *p)
{
    if (p->phase != TP_OVER) return 0;
    int top = -(1 << 20);
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) top = p->total[q] > top ? p->total[q] : top;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->total[q] == top) << q;
    return m;
}

/* ---- a plain turn (the tests, the pictures) ---------------------------------------------------------------------- */
/* the best set among the free tiles (no jokers): the longest runs and the biggest groups; its tiles into o */
static int best_meld(const uint8_t *free_tile, uint8_t *o, int *value)
{
    int best = 0, n = 0;
    for (int c = 0; c < 4; c++) {               /* runs: numbers in a row of one colour */
        int at[14];
        for (int v = 1; v <= 13; v++) {
            at[v] = -1;
            for (int t = 0; t < 104 && at[v] < 0; t++)
                if (free_tile[t] && col(t) == c && num(t) == v) at[v] = t;
        }
        for (int v = 1; v <= 13; v++) {
            int len = 0, sum = 0;
            while (v + len <= 13 && at[v + len] >= 0) sum += v + len, len++;
            if (len >= 3 && sum > best) {
                best = sum, n = len;
                for (int i = 0; i < len; i++) o[i] = (uint8_t)at[v + i];
            }
        }
    }
    for (int v = 1; v <= 13; v++) {             /* groups: one number in different colours */
        uint8_t g[4];
        int k = 0;
        for (int c = 0; c < 4; c++)
            for (int t = 0; t < 104; t++)
                if (free_tile[t] && col(t) == c && num(t) == v) { g[k++] = (uint8_t)t; break; }
        if (k >= 3 && v * k > best) best = v * k, n = k, memcpy(o, g, (size_t)k);
    }
    *value = best;
    return n;
}

int tr_auto(tgame *g)
{
    tpos *p = &g->pos;
    if (p->phase == TP_ROUND) return tr_next(g);
    if (p->phase != TP_PLAY) return 0;
    tr_reset(g);
    uint8_t free_tile[TR_TILES] = {0}, melds[TR_TILES], len[40];
    int nm = 0, used = 0, worth = 0, v;
    for (int t = 0; t < TR_TILES; t++) free_tile[t] = p->where[t] == TR_RACK + p->turn;
    for (int n; nm < 40 && (n = best_meld(free_tile, melds + used, &v)) > 0; nm++) {
        for (int i = 0; i < n; i++) free_tile[melds[used + i]] = 0;
        len[nm] = (uint8_t)n;
        used += n;
        worth += v;
    }
    if (p->out[p->turn] || worth >= TR_FIRST)
        for (int i = 0, at = 0; i < nm; at += len[i++]) tr_put(g, melds + at, len[i], TR_NEWSET);
    if (p->out[p->turn])                        /* then single tiles where a set takes them */
        for (int t = 0; t < TR_TILES; t++) {
            if (p->where[t] != TR_RACK + p->turn) continue;
            for (int k = 0; k < p->nsets; k++) {
                uint8_t one = (uint8_t)t;
                int kind;
                if (!tr_put(g, &one, 1, k)) continue;
                tr_set(p, k, NULL, &kind, NULL);
                if (kind != TS_BAD) break;
                tr_undo(g);
            }
        }
    if (tr_laid(g) && tr_end(g)) return 1;
    tr_reset(g);
    return tr_draw(g);
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int face_tile(const tgame *g, const char **s, const uint8_t *taken, int ntaken)
{
    const tpos *p = &g->pos;
    int table = **s == 't', c = -1, v = 0, joker = 0;
    if (table) (*s)++;
    switch (**s) {
    case 'r': c = 0; break;
    case 'b': c = 1; break;
    case 'a': c = 2; break;
    case 'k': c = 3; break;
    case 'j': joker = 1; break;
    default: return -1;
    }
    (*s)++;
    while (!joker && **s >= '0' && **s <= '9') v = v * 10 + (*(*s)++ - '0');
    int best = -1, rank = 1 << 20;
    for (int t = 0; t < TR_TILES; t++) {
        if (joker ? !tr_is_joker(t) : tr_is_joker(t) || col(t) != c || num(t) != v) continue;
        int dup = 0;
        for (int i = 0; i < ntaken; i++) dup |= taken[i] == t;
        if (dup) continue;
        int w = p->where[t], r = w == TR_RACK + p->turn ? (table ? -1 : 0) : w < p->nsets ? 1 + w : -1;
        if (r >= 0 && r < rank) rank = r, best = t;
    }
    return best;
}

int tr_script(tgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int alone = s[1] == ' ' || s[1] == 0, ok = 1;
        if (alone && *s == 'e') ok = tr_end(g);
        else if (alone && *s == 'd') ok = tr_draw(g);
        else if (alone && *s == 'x') ok = tr_reset(g);
        else if (alone && *s == 'u') ok = tr_undo(g);
        else if (alone && *s == 'n') ok = tr_next(g);
        else if (alone && *s == 'a') ok = tr_auto(g);
        else if (alone && (*s == '#' || *s == '*')) {   /* to the end of the round, or of the game */
            while (ok && (g->pos.phase == TP_PLAY || (*s == '*' && g->pos.phase == TP_ROUND))) ok = tr_auto(g);
        } else {                                /* a put: faces joined by +, then > and where */
            uint8_t t[TR_TILES];
            int n = 0, dest;
            for (;;) {
                int k = face_tile(g, &s, t, n);
                if (k < 0) return 0;
                t[n++] = (uint8_t)k;
                if (*s == '+') { s++; continue; }
                break;
            }
            if (*s++ != '>') return 0;
            if (*s == 'n') dest = TR_NEWSET, s++;
            else if (*s == 'r') dest = TR_TORACK, s++;
            else if (*s >= '0' && *s <= '9') {
                dest = 0;
                while (*s >= '0' && *s <= '9') dest = dest * 10 + (*s++ - '0');
            } else return 0;
            if (!tr_put(g, t, n, dest)) return 0;
            continue;
        }
        if (!ok) return 0;
        s++;
    }
    return 1;
}
