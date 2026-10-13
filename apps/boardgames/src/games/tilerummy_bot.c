/*
 * Tile Rummy's bot (bots.h), for 2 to 4 players and both variants: a solver for "what can I lay", by level. It reads
 * its own rack and the table (public), never another rack nor the pool. Runs on the worker thread: it copies the game
 * at entry and reads only that copy. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 *   The solver: the tiles by faces (colour and number, two of each, the jokers apart); a depth-first search that
 *   takes the lowest face still to place (every table tile must end in a set; a rack tile may stay) and tries each
 *   run and group through it (jokers where a face is missing), keeping the arrangement that lays the most from the
 *   rack (a tile's number, a joker 20), within a node budget. Before a player's first meld the table is out of reach
 *   and the sets laid must be worth 30.
 *   The turn: the arrangement becomes puts in the notation (a set left as it was is not touched; each other set is
 *   built anew, its tiles from the rack or, "t", from the table), tried in full on a copy of the turn's start (the
 *   notation takes a table tile from the first set holding one: an order of the puts that ends with every set valid
 *   is searched); each think() plans again from the turn's start (the same plan: no randomness but the start's own)
 *   and gives the next put, then "e"; nothing to lay: "d".
 * Levels: easy lays the sets its rack makes greedily (the longest runs, the biggest groups) and single tiles at the
 * table's sets' ends, and holds back now and then; medium searches its rack's best sets and adds tiles to the
 * table's sets; hard searches the whole table with its rack (rearranging any set), and falls back to medium's play
 * when no order of the puts works.
 * Its moves are the game's notation ("r5+r6+r7>n", "tk9+b9+a9>n", "a4>2", "e", "d", "n"), played by tilerummy_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "cards.h"
#include "games/tilerummy.h"

#define TRB_FACES 52                             /* (number - 1) * 4 + colour */
#define TRB_JOKER 52                             /* a joker in a set's faces */
#define TRB_SETS 40
#define TRB_STEPS 48
#define TRB_HARD_NODES 150000
#define TRB_JOKER_WORTH 20

static int face_of(int tile)
{
    return tr_is_joker(tile) ? TRB_JOKER : (CARD_RANK(tr_face(tile)) - 1) * 4 + CARD_SUIT(tr_face(tile));
}
static int fnum(int f) { return f / 4 + 1; }
static int fcol(int f) { return f % 4; }

typedef struct { uint8_t f[14], n; } fset;      /* a set by faces (TRB_JOKER: a joker) */

/* a set's validity and worth (as the rules count it: a joker the tile it stands for, at a run's high end when it
 * can); 0: not a set */
static int set_value(const fset *s, int *value)
{
    int n = s->n, j = 0, lo = 99, hi = 0, c0 = -1, v0 = -1, same_c = 1, same_v = 1;
    unsigned nums = 0, cols = 0;
    for (int i = 0; i < n; i++) {
        int f = s->f[i];
        if (f == TRB_JOKER) { j++; continue; }
        int c = fcol(f), v = fnum(f);
        if (c0 < 0) c0 = c, v0 = v;
        same_c &= c == c0;
        same_v &= v == v0;
        if (same_c && (nums >> v & 1)) same_c = 0;
        nums |= 1u << v;
        if (cols >> c & 1) same_v = same_v && 0;
        cols |= 1u << c;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    int m = n - j;
    if (n < 3 || !m) return 0;
    int run = n <= 13 && same_c && hi - lo + 1 <= n, group = n <= 4 && same_v && __builtin_popcount(cols) == m;
    int start = lo < 14 - n ? lo : 14 - n, rv = n * start + n * (n - 1) / 2, gv = v0 * n;
    if (!run && !group) return 0;
    if (value) *value = run && (!group || rv >= gv) ? rv : gv;
    return 1;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
typedef struct {
    int8_t avail[TRB_FACES], tab[TRB_FACES];      /* copies left to use (table + rack); table copies still to place */
    int javail, jtab;                           /* jokers left; table jokers still to place */
    int first;                                  /* before the first meld: the sets laid must be worth TR_FIRST */
    fset sets[TRB_SETS];
    int nsets, score, meld;
    fset best[TRB_SETS];
    int nbest, bscore, bmeld;
    long nodes, budget;
    int rack_left;                              /* the rack's worth not laid yet (the bound) */
} solver;

static int worth(int f) { return f == TRB_JOKER ? TRB_JOKER_WORTH : fnum(f); }

/* takes a face (a table copy first: no score); undo with give() */
static int take(solver *S, int f)
{
    if (f == TRB_JOKER) {
        S->javail--;
        if (S->jtab > 0) { S->jtab--; return 0; }
        S->rack_left -= TRB_JOKER_WORTH;
        return TRB_JOKER_WORTH;
    }
    S->avail[f]--;
    if (S->tab[f] > 0) { S->tab[f]--; return 0; }
    S->rack_left -= fnum(f);
    return fnum(f);
}

static void give(solver *S, int f, int scored)
{
    if (f == TRB_JOKER) {
        S->javail++;
        if (scored) S->rack_left += TRB_JOKER_WORTH;
        else S->jtab++;
        return;
    }
    S->avail[f]++;
    if (scored) S->rack_left += fnum(f);
    else S->tab[f]++;
}

static void dfs(solver *S, int pos);

/* tries the set: takes its faces, searches on, gives them back */
static void try_set(solver *S, const fset *s, int pos)
{
    if (S->nsets >= TRB_SETS) return;
    int got[14], gain = 0, v = 0;
    if (!set_value(s, &v)) return;
    for (int i = 0; i < s->n; i++) got[i] = take(S, s->f[i]), gain += got[i];
    S->sets[S->nsets++] = *s;
    S->score += gain;
    S->meld += v;
    dfs(S, pos);
    S->meld -= v;
    S->score -= gain;
    S->nsets--;
    for (int i = s->n - 1; i >= 0; i--) give(S, s->f[i], got[i]);
}

/* the sets through face f (its lowest real face at f or below): groups at its number, runs of its colour */
static void sets_through(solver *S, int f, int pos)
{
    int v = fnum(f), c = fcol(f);
    /* groups: f and other colours of its number, jokers to make 3 or 4 */
    for (unsigned m = 0; m < 16; m++) {
        if (m >> c & 1) continue;
        fset s = {.n = 0};
        s.f[s.n++] = (uint8_t)f;
        int ok = 1;
        for (int k = 0; k < 4; k++)
            if (m >> k & 1) {
                int g = (v - 1) * 4 + k;
                if (S->avail[g] <= 0) { ok = 0; break; }
                s.f[s.n++] = (uint8_t)g;
            }
        if (!ok) continue;
        for (int j = 0; j <= 2 && j <= S->javail && s.n + j <= 4; j++) {
            fset t = s;
            for (int i = 0; i < j; i++) t.f[t.n++] = TRB_JOKER;
            if (t.n >= 3) try_set(S, &t, pos);
            if (S->nodes > S->budget) return;
        }
    }
    /* runs: from s to e through v, the faces missing as jokers */
    S->avail[f]--;                              /* (f itself is in every run below) */
    for (int st = v; st >= 1 && st >= v - 12; st--) {
        int jneed = 0;
        for (int p = st; p < v; p++) jneed += S->avail[(p - 1) * 4 + c] <= 0;
        if (jneed > S->javail) break;
        int jn = jneed;
        for (int e = v; e <= 13 && e - st < 13; e++) {
            if (e > v) jn += S->avail[(e - 1) * 4 + c] <= 0;
            if (jn > S->javail) break;
            if (e - st + 1 < 3) continue;
            fset s = {.n = 0};
            for (int p = st; p <= e; p++) {
                int g = (p - 1) * 4 + c;
                s.f[s.n++] = p == v ? (uint8_t)f : S->avail[g] > 0 ? (uint8_t)g : TRB_JOKER;
            }
            S->avail[f]++;
            try_set(S, &s, pos);
            S->avail[f]--;
            if (S->nodes > S->budget) { S->avail[f]++; return; }
        }
    }
    S->avail[f]++;
}

/* places the table's jokers still out: on a set's end or in a group of three; 1: all placed */
static int place_jokers(solver *S)
{
    while (S->jtab > 0) {
        int done = 0;
        for (int k = 0; k < S->nsets && !done; k++) {
            fset t = S->sets[k];
            if (t.n >= 13) continue;
            t.f[t.n++] = TRB_JOKER;
            if (set_value(&t, NULL)) S->sets[k] = t, S->jtab--, S->javail--, done = 1;
        }
        if (!done) return 0;
    }
    return 1;
}

static void record(solver *S)
{
    if (S->first && S->meld < TR_FIRST && S->nsets) return;
    int jt = S->jtab, ja = S->javail;
    fset keep[TRB_SETS];
    memcpy(keep, S->sets, sizeof(fset) * (size_t)S->nsets);
    int ok = place_jokers(S);
    if (ok && (S->score > S->bscore || (S->score == S->bscore && S->nsets < S->nbest))) {
        S->bscore = S->score;
        S->nbest = S->nsets;
        S->bmeld = S->meld;
        memcpy(S->best, S->sets, sizeof(fset) * (size_t)S->nsets);
    }
    memcpy(S->sets, keep, sizeof(fset) * (size_t)S->nsets);
    S->jtab = jt;
    S->javail = ja;
}

static void dfs(solver *S, int pos)
{
    if (++S->nodes > S->budget) return;
    if (S->score + S->rack_left <= S->bscore) return;   /* no better than the best already */
    while (pos < TRB_FACES && S->avail[pos] <= 0) pos++;
    if (pos >= TRB_FACES) { record(S); return; }
    if (S->tab[pos] > 0) {                      /* a table tile: it must go somewhere */
        sets_through(S, pos, pos);
        return;
    }
    sets_through(S, pos, pos);                  /* a rack tile: in a set, or left in the rack */
    if (S->nodes > S->budget) return;
    int a = S->avail[pos];
    S->avail[pos] = 0;
    S->rack_left -= a * fnum(pos);
    dfs(S, pos + 1);
    S->rack_left += a * fnum(pos);
    S->avail[pos] = (int8_t)a;
}

/* ---- the view ---------------------------------------------------------------------------------------------------- */
/* the table's sets by faces */
static int table_sets(const tpos *p, fset *out)
{
    for (int k = 0; k < p->nsets; k++) out[k].n = 0;
    for (int t = 0; t < TR_TILES; t++)
        if (p->where[t] < p->nsets && out[p->where[t]].n < 14) {
            fset *s = &out[p->where[t]];
            s->f[s->n++] = (uint8_t)face_of(t);
        }
    return p->nsets;
}

static void solver_init(solver *S, const tpos *p, int me, int with_table, long budget)
{
    memset(S, 0, sizeof *S);
    S->budget = budget;
    S->bscore = 0;
    S->nbest = 0;
    S->first = !p->out[me];
    for (int t = 0; t < TR_TILES; t++) {
        int w = p->where[t], f = face_of(t), rack = w == TR_RACK + me, table = w < p->nsets && with_table;
        if (!rack && !table) continue;
        if (f == TRB_JOKER) {
            S->javail++;
            S->jtab += table;
        } else {
            S->avail[f]++;
            S->tab[f] += (int8_t)table;
        }
        if (rack) S->rack_left += worth(f);
    }
}

/* ---- the turn's puts --------------------------------------------------------------------------------------------- */
typedef struct { char s[TRB_STEPS][BOT_MOVE_CAP]; int n; } plan;

static int face_text(int f, int table, char *o)
{
    int k = 0;
    if (table) o[k++] = 't';
    if (f == TRB_JOKER) { o[k++] = 'j'; return k; }
    o[k++] = "rbak"[fcol(f)];
    k += snprintf(o + k, 4, "%d", fnum(f));
    return k;
}

static int same_faces(const fset *a, const fset *b)
{
    if (a->n != b->n) return 0;
    int cnt[TRB_FACES + 1] = {0};
    for (int i = 0; i < a->n; i++) cnt[a->f[i]]++;
    for (int i = 0; i < b->n; i++)
        if (--cnt[b->f[i]] < 0) return 0;
    return 1;
}

/* the puts that build the sets (in this order) from the turn's start; tried on sc: 1 if the turn may then end */
static int build(tgame *sc, const tgame *g, const fset *sets, const int *order, int n, int keep, plan *P)
{
    *sc = *g;
    sc->pos = g->start;
    sc->nundo = 0;
    const tpos *p = &sc->pos;
    int me = p->turn;
    fset tab[TR_MAXSETS];
    int nt = table_sets(&g->start, tab), kept[TR_MAXSETS] = {0}, skip[TRB_SETS] = {0};
    for (int i = 0; i < n && keep; i++)        /* the sets already on the table as they are */
        for (int k = 0; k < nt; k++)
            if (!kept[k] && same_faces(&sets[order[i]], &tab[k])) { kept[k] = 1, skip[i] = 1; break; }
    /* the rack's copies of each face: the rest comes from the table */
    int rack[TRB_FACES + 1] = {0};
    for (int t = 0; t < TR_TILES; t++)
        if (p->where[t] == TR_RACK + me) rack[face_of(t)]++;
    P->n = 0;
    /* the rack's tiles go to the sets laid last first (a set kept takes none) */
    int from_rack[TRB_SETS][14];
    int need_table[TRB_FACES + 1] = {0};
    for (int k = 0; k < nt; k++)
        if (!kept[k])
            for (int i = 0; i < tab[k].n; i++) need_table[tab[k].f[i]]++;
    for (int i = 0; i < n; i++) {
        const fset *s = &sets[order[i]];
        for (int j = 0; j < s->n; j++) {
            int f = s->f[j];
            if (skip[i]) { from_rack[i][j] = 0; continue; }
            if (need_table[f] > 0) need_table[f]--, from_rack[i][j] = 0;
            else from_rack[i][j] = 1, rack[f]--;
        }
    }
    for (int f = 0; f <= TRB_FACES; f++)
        if (rack[f] < 0) return 0;
    for (int i = 0; i < n; i++) {
        if (skip[i]) continue;
        const fset *s = &sets[order[i]];
        char m[96];
        int k = 0;
        for (int j = 0; j < s->n; j++) {
            if (j) m[k++] = '+';
            k += face_text(s->f[j], !from_rack[i][j], m + k);
        }
        m[k++] = '>';
        m[k++] = 'n';
        m[k] = 0;
        if (k >= BOT_MOVE_CAP || P->n >= TRB_STEPS) return 0;
        uint8_t was[TR_TILES];
        memcpy(was, p->where, sizeof was);
        if (!tr_script(sc, m)) return 0;
        if (!memcmp(was, p->where, sizeof was)) continue;   /* a set moved whole: nothing changed */
        memcpy(P->s[P->n++], m, (size_t)k + 1);
    }
    return tr_check(sc) == TC_OK;
}

/* the arrangement's puts in an order that works (a few orders tried); 1: found */
static int make_plan(tgame *sc, const tgame *g, const fset *sets, int n, plan *P, uint64_t seed)
{
    int order[TRB_SETS];
    ai_rng r;
    ai_seed(&r, seed);
    for (int tryn = 0; tryn < 16; tryn++) {
        for (int i = 0; i < n; i++) order[i] = tryn % 8 == 1 ? n - 1 - i : i;
        if (tryn % 8 >= 2)
            for (int i = n - 1; i > 0; i--) {
                int j = ai_rand_n(&r, i + 1), t = order[i];
                order[i] = order[j], order[j] = t;
            }
        if (build(sc, g, sets, order, n, tryn < 8, P)) return 1;   /* (then every set built anew) */
    }
    P->n = 0;
    return 0;
}

/* medium and easy: the rack's own sets, then the rack's tiles added to the table's sets (after the first meld) */
static int rack_plan(tgame *sc, const tgame *g, int level, plan *P, ai_rng *r)
{
    const tpos *p = &g->start;
    int me = p->turn;
    solver *S = malloc(sizeof *S);
    if (!S) return 0;
    fset sets[TRB_SETS];
    int n = 0;
    if (level == AI_EASY) {                     /* greedy: the dearest set the rack makes, again and again */
        solver_init(S, p, me, 0, 1);
        for (;;) {
            fset best = {.n = 0};
            int bv = 0;
            for (int f = 0; f < TRB_FACES; f++) {
                if (S->avail[f] <= 0) continue;
                int c = fcol(f), v = fnum(f);
                fset run = {.n = 0};
                for (int x = v; x <= 13 && S->avail[(x - 1) * 4 + c] > 0; x++)
                    run.f[run.n++] = (uint8_t)((x - 1) * 4 + c);
                int val;
                if (run.n >= 3 && set_value(&run, &val) && val > bv) bv = val, best = run;
                fset grp = {.n = 0};
                for (int k = 0; k < 4; k++)
                    if (S->avail[(v - 1) * 4 + k] > 0) grp.f[grp.n++] = (uint8_t)((v - 1) * 4 + k);
                if (grp.n >= 3 && set_value(&grp, &val) && val > bv) bv = val, best = grp;
            }
            if (!best.n || n >= TRB_SETS) break;
            for (int i = 0; i < best.n; i++) S->avail[best.f[i]]--;
            S->meld += bv;
            sets[n++] = best;
        }
        if (S->first && S->meld < TR_FIRST) n = 0;
        if (n && ai_rand_n(r, 100) < 12) n--;   /* holds a set back now and then */
    } else {
        solver_init(S, p, me, 0, TRB_HARD_NODES / 4);
        dfs(S, 0);
        n = S->nbest;
        memcpy(sets, S->best, sizeof(fset) * (size_t)n);
    }
    free(S);
    if (n && !make_plan(sc, g, sets, n, P, 1)) n = 0, P->n = 0;
    if (!n) P->n = 0;
    if (!p->out[me] && !n) return 0;
    /* the rack's tiles on the table's sets: a tile that keeps a set valid (easy: two at most; jokers: hard only) */
    *sc = *g;
    sc->pos = g->start;
    sc->nundo = 0;
    for (int i = 0; i < P->n; i++) tr_script(sc, P->s[i]);
    if (!p->out[me] && !tr_laid(sc)) return 0;
    for (int again = 1, added = 0; again && (level > AI_EASY || added < 2);) {
        again = 0;
        fset tab[TR_MAXSETS];
        int nt = table_sets(&sc->pos, tab);
        for (int t = 0; t < TR_TILES && P->n < TRB_STEPS; t++) {
            if (sc->pos.where[t] != TR_RACK + me || (tr_is_joker(t) && level < AI_HARD)) continue;
            int f = face_of(t);
            for (int k = 0; k < nt; k++) {
                if (!p->out[me] && k < g->start.nsets) continue;   /* (the first meld: not the table's sets) */
                fset s = tab[k];
                s.f[s.n++] = (uint8_t)f;
                if (s.n > 13 || !set_value(&s, NULL)) continue;
                char m[24];
                int len = face_text(f, 0, m);
                snprintf(m + len, sizeof m - (size_t)len, ">%d", k);
                if (!tr_script(sc, m)) continue;
                memcpy(P->s[P->n++], m, strlen(m) + 1);
                again = 1;
                added++;
                break;
            }
            if (again) break;
        }
    }
    return P->n > 0 && tr_check(sc) == TC_OK;
}

/* the turn's plan from its start, for seat at a level: the puts (P->n 0: draw) */
static void turn_plan(const tgame *g, int level, plan *P)
{
    P->n = 0;
    tgame *sc = malloc(sizeof *sc);
    if (!sc) return;
    const tpos *p = &g->start;
    uint64_t h = 0x7e;
    for (int t = 0; t < TR_TILES; t++) h = ai_mix(h, p->where[t]);
    ai_rng r;
    ai_seed(&r, ai_mix(h, (uint64_t)level));
    plan *Q = malloc(sizeof *Q);
    int okq = Q && rack_plan(sc, g, level, Q, &r);
    if (level == AI_HARD && p->out[p->turn]) {  /* the whole table with the rack */
        solver *S = malloc(sizeof *S);
        if (S) {
            solver_init(S, p, p->turn, 1, TRB_HARD_NODES);
            /* medium's plan sets the bar: the search must lay more */
            int laid_q = 0;
            if (okq) {
                *sc = *g;
                sc->pos = g->start;
                for (int i = 0; i < Q->n; i++) tr_script(sc, Q->s[i]);
                for (int t = 0; t < TR_TILES; t++)
                    if (g->start.where[t] == TR_RACK + p->turn && sc->pos.where[t] < sc->pos.nsets)
                        laid_q += tr_is_joker(t) ? TRB_JOKER_WORTH : CARD_RANK(tr_face(t));
            }
            S->bscore = laid_q;
            dfs(S, 0);
            if (S->nbest && S->bscore > laid_q && make_plan(sc, g, S->best, S->nbest, P, h)) okq = 0;
            free(S);
        }
    }
    if (okq) *P = *Q;
    free(Q);
    free(sc);
}

/* ---- the bot ----------------------------------------------------------------------------------------------------- */
/* the next action for seat at a level, in the notation (0: nothing to do) */
int tilb_choose(const tgame *g, int seat, int level, char *move, int cap)
{
    const tpos *p = &g->pos;
    if (p->phase == TP_ROUND) return snprintf(move, (size_t)cap, "n"), 1;
    if (p->phase != TP_PLAY || p->turn != seat) return 0;
    plan *P = malloc(sizeof *P);
    tgame *sc = malloc(sizeof *sc);
    int ok = 0;
    if (P && sc) {
        turn_plan(g, level, P);
        /* where the turn is: the start, or after one of the plan's puts */
        *sc = *g;
        sc->pos = g->start;
        sc->nundo = 0;
        int at = memcmp(sc->pos.where, p->where, sizeof p->where) ? -1 : 0;
        for (int i = 0; i < P->n; i++) {             /* (the last match) */
            tr_script(sc, P->s[i]);
            if (!memcmp(sc->pos.where, p->where, sizeof p->where)) at = i + 1;
        }
        if (at < 0) snprintf(move, (size_t)cap, "x");            /* lost: back to the start */
        else if (at < P->n) snprintf(move, (size_t)cap, "%s", P->s[at]);
        else if (P->n) snprintf(move, (size_t)cap, "e");
        else snprintf(move, (size_t)cap, "d");
        ok = 1;
    }
    free(P);
    free(sc);
    return ok;
}

static int think(int seat, int level, char *move, int cap)
{
    tgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *tilerummy_live();                     /* the copy, at entry: nothing else is read */
    int ok = tilb_choose(g, seat, level, move, cap);
    free(g);
    return ok && !ai_cancelled();
}

static int actor(void)
{
    const tpos *p = &tilerummy_live()->pos;
    return p->phase == TP_PLAY ? p->turn : -1;
}

static const bot tilerummy_bot = {.think = think, .actor = actor, .play = tilerummy_show};
REGISTER_BOT(tilerummy, &tilerummy_bot)
