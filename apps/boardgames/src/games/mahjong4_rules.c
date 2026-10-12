/*
 * Mahjong for four, Hong Kong Old Style: the rules (mahjong4.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "cards.h"
#include "games/mahjong4.h"

const int M4_POINTS[M4_LIMIT + 1] = {1, 2, 4, 8, 16, 24, 32, 48, 64, 96, 128};

/* ---- scoring a hand ---------------------------------------------------------------------------------------------- */
typedef struct { uint8_t type, kind, concealed; } xset;
typedef struct { xset s[4]; int n, pair; } reading;
#define READINGS 48

static int chow_start(int k) { return k < M4_WINDS && k % 9 <= 6; }
static int dragon(int k) { return k >= M4_DRAGONS && k < M4_FLOWERS; }
static int wind(int k) { return k >= M4_WINDS && k < M4_DRAGONS; }

/* every way to read the counts c as `need` sets (pungs and chows) */
static void sets(int *c, int need, reading *cur, reading *out, int *nout)
{
    if (*nout >= READINGS) return;
    int k = 0;
    while (k < M4_KINDS && !c[k]) k++;
    if (k == M4_KINDS) {
        if (!need) out[(*nout)++] = *cur;
        return;
    }
    if (!need) return;
    if (c[k] >= 3) {
        c[k] -= 3;
        cur->s[cur->n++] = (xset){MS_PUNG, (uint8_t)k, 1};
        sets(c, need - 1, cur, out, nout);
        cur->n--;
        c[k] += 3;
    }
    if (chow_start(k) && c[k + 1] && c[k + 2]) {
        c[k]--, c[k + 1]--, c[k + 2]--;
        cur->s[cur->n++] = (xset){MS_CHOW, (uint8_t)k, 1};
        sets(c, need - 1, cur, out, nout);
        cur->n--;
        c[k]++, c[k + 1]++, c[k + 2]++;
    }
}

static void add(m4score *s, int item, int v)
{
    if (v <= 0) return;
    if (s->n < 16) {
        s->item[s->n] = (uint8_t)item;
        s->value[s->n] = (uint8_t)v;
        s->n++;
    }
    s->raw += v;
}

/* the kinds a hand is made of: one suit with or without honours (a flush), honours only, terminals only */
typedef struct { int suits, honours, plain; } makeup;   /* suits: bit per suit; plain: a 2..8 tile */
static void see(makeup *m, int kind, int type)
{
    if (kind >= M4_WINDS) { m->honours = 1; return; }
    m->suits |= 1 << (kind / 9);
    if (type == MS_CHOW || (kind % 9 && kind % 9 != 8)) m->plain = 1;
}

/* what every winning hand may add: how it was won, concealed, the flowers */
static void common(const m4hand *h, m4score *s, int concealed)
{
    if (h->self_drawn) add(s, MI_SELF_DRAWN, 1);
    if (concealed) add(s, MI_CONCEALED, 1);
    int own = 0, fl = 0, se = 0;
    for (int i = 0; i < h->nflower; i++) {
        int k = h->flower[i];
        own += k == M4_FLOWERS + h->seat || k == M4_SEASONS + h->seat;
        fl += k >= M4_FLOWERS && k < M4_SEASONS;
        se += k >= M4_SEASONS;
    }
    add(s, MI_OWN_FLOWER, own);
    if (!h->nflower) add(s, MI_NO_FLOWERS, 1);
    add(s, MI_FLOWER_SET, 2 * (fl == 4) + 2 * (se == 4));
    if (h->last) add(s, MI_LAST_TILE, 1);
    if (h->replacement) add(s, MI_REPLACEMENT, 1);
    if (h->robbing) add(s, MI_ROBBING, 1);
}

static void flush(const makeup *m, m4score *s)
{
    if (!m->suits || (m->suits & (m->suits - 1))) return;   /* none, or more than one suit */
    if (m->honours) add(s, MI_HALF_FLUSH, 3);
    else add(s, MI_FULL_FLUSH, 7);
}

static void finish(m4score *s, int limit)
{
    if (limit) {                                /* a limit hand: the limit, nothing else counts */
        m4score t = *s;
        s->n = 0;
        s->raw = 0;
        for (int i = 0; i < t.n; i++)
            if (t.item[i] >= MI_THIRTEEN) add(s, t.item[i], M4_LIMIT);
    }
    s->faan = s->raw > M4_LIMIT ? M4_LIMIT : s->raw;
}

static int concealed_hand(const m4hand *h)
{
    for (int i = 0; i < h->nmeld; i++)
        if (!h->meld[i].concealed) return 0;
    return 1;
}

static void eval_std(const m4hand *h, const reading *r, const int *c, m4score *s)
{
    memset(s, 0, sizeof *s);
    xset all[4];
    int n = 0;
    for (int i = 0; i < h->nmeld; i++) all[n++] = (xset){h->meld[i].type, h->meld[i].kind, h->meld[i].concealed};
    for (int i = 0; i < r->n; i++) all[n++] = r->s[i];
    int win_free = h->self_drawn || r->pair == h->win_kind;   /* the winning tile did not complete a concealed pung */
    for (int i = 0; i < r->n; i++)
        if (r->s[i].type == MS_CHOW && h->win_kind >= r->s[i].kind && h->win_kind <= r->s[i].kind + 2) win_free = 1;
    int pungs = 0, chows = 0, kongs = 0, hidden = 0, dragons = 0, winds = 0, seat = 0, round = 0, limit = 0;
    makeup m = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        see(&m, all[i].kind, all[i].type);
        if (all[i].type == MS_CHOW) { chows++; continue; }
        pungs++;
        kongs += all[i].type == MS_KONG;
        hidden += all[i].concealed;
        dragons += dragon(all[i].kind);
        winds += wind(all[i].kind);
        seat += all[i].kind == M4_WINDS + h->seat;
        round += all[i].kind == M4_WINDS + h->wind;
    }
    see(&m, r->pair, MS_PAIR);
    if (!win_free) hidden--;                    /* won on a discard: that pung is not concealed */
    /* the limit hands */
    if (kongs == 4) add(s, MI_FOUR_KONGS, M4_LIMIT), limit = 1;
    if (hidden == 4) add(s, MI_FOUR_CONCEALED, M4_LIMIT), limit = 1;
    if (winds == 4) add(s, MI_BIG_WINDS, M4_LIMIT), limit = 1;
    else if (winds == 3 && wind(r->pair)) add(s, MI_SMALL_WINDS, M4_LIMIT), limit = 1;
    if (!m.suits) add(s, MI_ALL_HONOURS, M4_LIMIT), limit = 1;
    if (!m.honours && !m.plain) add(s, MI_ALL_TERMINALS, M4_LIMIT), limit = 1;
    if (!h->nmeld && m.suits && !(m.suits & (m.suits - 1)) && !m.honours) {   /* nine gates: 1112345678999 + one */
        int base = r->pair / 9 * 9, ok = 1;
        for (int k = 0; k < 9; k++) ok &= c[base + k] >= (k == 0 || k == 8 ? 3 : 1);
        if (ok) add(s, MI_NINE_GATES, M4_LIMIT), limit = 1;
    }
    if (!limit) {
        common(h, s, concealed_hand(h));
        if (chows == 4) add(s, MI_ALL_CHOWS, 1);
        if (pungs == 4) add(s, MI_ALL_PUNGS, 3);
        flush(&m, s);
        if (dragons == 3) add(s, MI_BIG_DRAGONS, 8);
        else if (dragons == 2 && dragon(r->pair)) add(s, MI_SMALL_DRAGONS, 5);
        else add(s, MI_DRAGON_PUNG, dragons);
        add(s, MI_SEAT_WIND, seat);
        add(s, MI_ROUND_WIND, round);
    }
    finish(s, limit);
    s->nset = 0;
    for (int i = 0; i < r->n; i++) {
        s->set_type[s->nset] = r->s[i].type;
        s->set_kind[s->nset++] = r->s[i].kind;
    }
    s->set_type[s->nset] = MS_PAIR;
    s->set_kind[s->nset++] = (uint8_t)r->pair;
}

static int better(const m4score *a, const m4score *b)
{
    return a->faan > b->faan || (a->faan == b->faan && a->raw > b->raw);
}

int m4_score_hand(const m4hand *h, m4score *best)
{
    int c[M4_KINDS] = {0}, need = 4 - h->nmeld;
    m4score s, tmp;
    if (!best) best = &tmp;
    memset(best, 0, sizeof *best);
    best->faan = -1;
    if (need < 0 || h->nconc != 3 * need + 2) return -1;
    for (int i = 0; i < h->nconc; i++) {
        if (h->conc[i] >= M4_KINDS) return -1;
        c[h->conc[i]]++;
    }
    if (!h->nmeld) {
        static const uint8_t ORPHANS[13] = {0, 8, 9, 17, 18, 26, 27, 28, 29, 30, 31, 32, 33};
        int ok = 1, n = 0;
        for (int i = 0; i < 13; i++) ok &= c[ORPHANS[i]] >= 1, n += c[ORPHANS[i]];
        if (ok && n == 14) {                    /* thirteen orphans */
            memset(&s, 0, sizeof s);
            add(&s, MI_THIRTEEN, M4_LIMIT);
            finish(&s, 1);
            if (better(&s, best)) *best = s;
        }
        int pairs = 0;
        for (int k = 0; k < M4_KINDS; k++) pairs += c[k] == 2;
        if (pairs == 7) {                       /* seven pairs (seven different kinds) */
            memset(&s, 0, sizeof s);
            makeup m = {0, 0, 0};
            for (int k = 0; k < M4_KINDS; k++)
                if (c[k]) {
                    see(&m, k, MS_PAIR);
                    s.set_type[s.nset] = MS_PAIR;
                    s.set_kind[s.nset++] = (uint8_t)k;
                }
            if (!m.suits) {
                add(&s, MI_ALL_HONOURS, M4_LIMIT);
                finish(&s, 1);
            } else {
                add(&s, MI_SEVEN_PAIRS, 4);
                common(h, &s, 1);
                flush(&m, &s);
                finish(&s, 0);
            }
            if (better(&s, best)) *best = s;
        }
    }
    reading out[READINGS], cur;
    for (int p = 0; p < M4_KINDS; p++) {
        if (c[p] < 2) continue;
        int nout = 0;
        c[p] -= 2;
        cur.n = 0;
        cur.pair = p;
        sets(c, need, &cur, out, &nout);
        c[p] += 2;
        for (int i = 0; i < nout; i++) {
            out[i].pair = p;
            eval_std(h, &out[i], c, &s);
            if (better(&s, best)) *best = s;
        }
    }
    return best->faan;
}

/* ---- the position ------------------------------------------------------------------------------------------------ */
int m4_seat(const m4pos *p, int q) { return (q - p->dealer + M4_P) % M4_P; }

int m4_live(const m4pos *p)
{
    int n = p->tail - p->head - M4_DEAD;
    return n > 0 ? n : 0;
}

static int count(const m4pos *p, int q, int kind)
{
    int n = 0;
    for (int i = 0; i < p->nhand[q]; i++) n += m4_kind(p->hand[q][i]) == kind;
    return n;
}

static void take(m4pos *p, int q, int id) { p->hand[q][p->nhand[q]++] = (uint8_t)id; }

static int drop(m4pos *p, int q, int id)
{
    for (int i = 0; i < p->nhand[q]; i++)
        if (p->hand[q][i] == id) {
            memmove(&p->hand[q][i], &p->hand[q][i + 1], (size_t)(p->nhand[q] - i - 1));
            p->nhand[q]--;
            return 1;
        }
    return 0;
}

static int drop_kind(m4pos *p, int q, int kind)   /* a tile of that kind out of the hand (not the drawn one if
                                                   * another will do); its id, -1 */
{
    int found = -1;
    for (int i = 0; i < p->nhand[q]; i++)
        if (m4_kind(p->hand[q][i]) == kind && (found < 0 || p->hand[q][i] != p->drawn)) found = p->hand[q][i];
    if (found >= 0) drop(p, q, found);
    return found;
}

static void hand_of(const m4pos *p, int q, int extra, int self, int rob, m4hand *h)
{
    memset(h, 0, sizeof *h);
    for (int i = 0; i < p->nhand[q] && h->nconc < M4_HAND; i++) h->conc[h->nconc++] = (uint8_t)m4_kind(p->hand[q][i]);
    if (extra != M4_NONE && h->nconc < M4_HAND) h->conc[h->nconc++] = (uint8_t)extra;
    for (int i = 0; i < p->nmeld[q]; i++) {
        const m4meld *m = &p->meld[q][i];
        h->meld[h->nmeld].type = m->type;
        h->meld[h->nmeld].kind = m->kind;
        h->meld[h->nmeld++].concealed = m->from == M4_NONE;
    }
    for (int i = 0; i < p->nflower[q]; i++) h->flower[h->nflower++] = (uint8_t)m4_kind(p->flower[q][i]);
    h->seat = m4_seat(p, q);
    h->wind = p->wind;
    h->self_drawn = self;
    h->win_kind = extra != M4_NONE ? extra : p->drawn != M4_NONE ? m4_kind(p->drawn) : -1;
    h->last = m4_live(p) == 0;
    h->replacement = self && p->replacement;
    h->robbing = rob;
}

int m4_wins(const m4pos *p, int q, int kind, int self, int rob, m4score *s)
{
    m4hand h;
    m4score t;
    hand_of(p, q, kind, self, rob, &h);
    if (!s) s = &t;
    int f = m4_score_hand(&h, s);
    return f >= M4_MIN ? f : -1;
}

/* the chows player q could make with a tile of `kind`: their lowest kinds */
static int chows(const m4pos *p, int q, int kind, int *out)
{
    int n = 0;
    if (kind >= M4_WINDS) return 0;
    for (int l = kind - 2; l <= kind; l++) {
        if (l < 0 || !chow_start(l) || l / 9 != kind / 9) continue;
        int ok = 1;
        for (int k = l; k < l + 3; k++)
            if (k != kind && !count(p, q, k)) ok = 0;
        if (ok) {
            if (out) out[n] = l;
            n++;
        }
    }
    return n;
}

/* a tile drawn by q: from the live wall, or the dead wall's far end; flowers and seasons set aside and replaced */
static void draw(m4pos *p, int q, int dead)
{
    p->drawn = M4_NONE;
    while (p->tail > p->head) {
        int id = dead ? p->wall[--p->tail] : p->wall[p->head++];
        if (m4_bonus(m4_kind(id))) {
            p->flower[q][p->nflower[q]++] = (uint8_t)id;
            dead = 1;
            continue;
        }
        take(p, q, id);
        p->drawn = (uint8_t)id;
        p->replacement = (uint8_t)dead;
        return;
    }
}

static void deal(m4pos *p)
{
    uint32_t hs = p->seed * 2654435761u ^ (uint32_t)(p->hands + 1) * 0x9e3779b9u;
    for (int i = 0; i < M4_TILES; i++) p->wall[i] = (uint8_t)i;
    cards_shuffle(p->wall, M4_TILES, hs);
    uint32_t r = hs ^ 0x5bd1e995u;
    int t = 0;
    for (int i = 0; i < 3; i++) t += p->dice[i] = (uint8_t)(1 + cards_rand(&r) % 6);
    /* counted round from the dealer (1) in turn order: whose wall; then t stacks from its right end: the break */
    int owner = (p->dealer + t - 1) % M4_P, block = (M4_P - owner) % M4_P;
    p->start = (uint8_t)((block * 18 + t) % M4_STACKS);
    p->head = 0;
    p->tail = M4_TILES;
    memset(p->nhand, 0, sizeof p->nhand);
    memset(p->nmeld, 0, sizeof p->nmeld);
    memset(p->nflower, 0, sizeof p->nflower);
    memset(p->npond, 0, sizeof p->npond);
    for (int round = 0; round < 3; round++)     /* four tiles each, three times; then one each, the dealer two */
        for (int k = 0; k < M4_P; k++)
            for (int i = 0; i < 4; i++) take(p, (p->dealer + k) % M4_P, p->wall[p->head++]);
    for (int k = 0; k <= M4_P; k++) take(p, (p->dealer + k) % M4_P, p->wall[p->head++]);
    for (int k = 0; k < M4_P; k++) {            /* the flowers and seasons set aside, in turn from the dealer */
        int q = (p->dealer + k) % M4_P;
        for (int i = 0; i < p->nhand[q];) {
            int id = p->hand[q][i];
            if (!m4_bonus(m4_kind(id))) { i++; continue; }
            p->flower[q][p->nflower[q]++] = (uint8_t)id;
            p->hand[q][i] = p->wall[--p->tail];
        }
    }
    p->turn = p->dealer;
    p->drawn = p->hand[p->dealer][M4_HAND - 1];
    p->replacement = 0;
    p->discards = 0;
    p->phase = MP_DISCARD;
    p->pend = M4_NONE;
    memset(&p->result, 0, sizeof p->result);
    p->result.winner = p->result.payer = M4_NONE;
}

void m4_new(m4game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    m4pos *p = &g->pos;
    p->variant = (uint8_t)(variant >= 0 && variant < M4_VARIANTS ? variant : M4_EAST);
    p->seed = seed;
    deal(p);
    g->start = *p;
}

int m4_actor(const m4pos *p)
{
    switch (p->phase) {
    case MP_DISCARD: return p->turn;
    case MP_CLAIM:
    case MP_ROB: return p->ask[p->k];
    case MP_SCORE: return p->result.winner == p->dealer ? p->dealer : (p->dealer + 1) % M4_P;
    default: return -1;
    }
}

int m4_kongs(const m4pos *p, int *kinds)
{
    int n = 0, q = p->turn;
    if (p->phase != MP_DISCARD || p->drawn == M4_NONE || !m4_live(p)) return 0;
    for (int k = 0; k < M4_KINDS; k++) {
        int c = count(p, q, k), ok = c == 4;
        for (int i = 0; i < p->nmeld[q] && !ok; i++)
            ok = c >= 1 && p->meld[q][i].type == MS_PUNG && p->meld[q][i].kind == k;
        if (ok) {
            if (kinds) kinds[n] = k;
            n++;
        }
    }
    return n;
}

int m4_options(const m4pos *p, int *out, int *nchow)
{
    if (nchow) *nchow = 0;
    if (p->phase != MP_CLAIM && p->phase != MP_ROB) return 0;
    int q = p->ask[p->k], o = p->opts[q];
    if (p->phase == MP_CLAIM && p->k == p->nask - 1 && p->pend != M4_NONE) o &= MO_WIN;   /* beaten by a claim */
    if ((o & MO_CHOW) && nchow) *nchow = chows(p, q, m4_kind(p->tile), out);
    return o;
}

/* ---- the turn ---------------------------------------------------------------------------------------------------- */
static void hand_over(m4pos *p, int winner, int payer, int rob)
{
    m4result *r = &p->result;
    memset(r, 0, sizeof *r);
    r->winner = (uint8_t)winner;
    r->payer = (uint8_t)payer;
    r->tile = p->tile;
    p->phase = MP_SCORE;
    if (winner == M4_NONE) return;              /* a drawn hand: nobody pays */
    if (payer == M4_NONE) r->tile = p->drawn;
    m4_wins(p, winner, payer == M4_NONE ? M4_NONE : m4_kind(p->tile), payer == M4_NONE, rob, &r->score);
    if (payer != M4_NONE) take(p, winner, p->tile);
    int pts = M4_POINTS[r->score.faan < 0 ? 0 : r->score.faan];
    for (int q = 0; q < M4_P; q++) {
        if (q == winner) continue;
        int a = payer == M4_NONE || q == payer ? 2 * pts : pts;
        r->pay[q] -= a;
        r->pay[winner] += a;
    }
    for (int q = 0; q < M4_P; q++) p->total[q] += r->pay[q];
}

static void open_claims(m4pos *p)
{
    int d = p->from, a = (d + 2) % M4_P, b = (d + 3) % M4_P, next = (d + 1) % M4_P;
    memset(p->opts, 0, sizeof p->opts);
    for (int j = 1; j < M4_P; j++) {
        int q = (d + j) % M4_P, k = m4_kind(p->tile), o = 0, n = count(p, q, k);
        if (m4_wins(p, q, k, 0, 0, NULL) >= 0) o |= MO_WIN;
        if (m4_live(p)) {
            if (n >= 2) o |= MO_PUNG;
            if (n >= 3) o |= MO_KONG;
            if (q == next && chows(p, q, k, NULL)) o |= MO_CHOW;
        }
        p->opts[q] = (uint8_t)o;
    }
    p->nask = p->k = 0;
    p->pend = M4_NONE;
    p->pend_act = 0;
    if (p->opts[a] | p->opts[b]) {              /* both of them, always in this order */
        p->ask[p->nask++] = (uint8_t)a;
        p->ask[p->nask++] = (uint8_t)b;
    }
    p->ask[p->nask++] = (uint8_t)next;          /* the next player: last */
    p->phase = MP_CLAIM;
}

static void claim(m4pos *p, int q, int act);

/* the claims decided: the best one made, or the next player draws (the wall used up: the hand is drawn) */
static void resolve(m4pos *p)
{
    if (p->pend != M4_NONE) { claim(p, p->pend, p->pend_act); return; }
    int next = (p->from + 1) % M4_P;
    if (!m4_live(p)) { hand_over(p, M4_NONE, M4_NONE, 0); return; }
    p->turn = (uint8_t)next;
    p->phase = MP_DISCARD;
    draw(p, next, 0);
}

/* on to the next player to ask; the next player (last) is skipped when a claim made beats all they could do, or
 * when they could only draw and the wall is used up */
static void advance(m4pos *p)
{
    if (p->k == p->nask - 1) {
        int q = p->ask[p->k], win = p->opts[q] & MO_WIN;
        if ((p->pend != M4_NONE || !m4_live(p)) && !win) { resolve(p); return; }
    }
    if (p->k >= p->nask) resolve(p);
}

static void meld(m4pos *p, int q, int type, int kind, int from, const int *ids, int n, int claimed)
{
    m4meld *m = &p->meld[q][p->nmeld[q]++];
    m->type = (uint8_t)type;
    m->kind = (uint8_t)kind;
    m->n = (uint8_t)n;
    m->from = (uint8_t)from;
    m->claimed = (uint8_t)claimed;
    for (int i = 0; i < 4; i++) m->tile[i] = (uint8_t)(i < n ? ids[i] : M4_NONE);
}

/* a claim on the discard carried out */
static void claim(m4pos *p, int q, int act)
{
    int d = p->from, id = p->tile, k = m4_kind(id), ids[4], n = 0;
    p->npond[d]--;                              /* the discard leaves the pond */
    if (MA_KIND(act) == MA_WIN) { hand_over(p, q, d, 0); return; }
    if (MA_KIND(act) == MA_CHOW) {
        int l = MA_ARG(act);
        for (int j = l; j < l + 3; j++) ids[n++] = j == k ? id : drop_kind(p, q, j);
        meld(p, q, MS_CHOW, l, d, ids, 3, id);
    } else {
        int kong = MA_KIND(act) == MA_KONG;
        for (int i = 0; i < (kong ? 3 : 2); i++) ids[n++] = drop_kind(p, q, k);
        ids[n++] = id;
        meld(p, q, kong ? MS_KONG : MS_PUNG, k, d, ids, n, id);
    }
    p->turn = (uint8_t)q;
    p->phase = MP_DISCARD;
    p->drawn = M4_NONE;
    p->replacement = 0;
    if (MA_KIND(act) == MA_KONG) draw(p, q, 1);
}

int m4_legal(const m4pos *p, int act)
{
    int kind = MA_KIND(act), arg = MA_ARG(act);
    switch (p->phase) {
    case MP_DISCARD: {
        int q = p->turn;
        if (kind == MA_DISCARD) {
            for (int i = 0; i < p->nhand[q]; i++)
                if (p->hand[q][i] == arg) return 1;
            return 0;
        }
        if (kind == MA_WIN) return p->drawn != M4_NONE && m4_wins(p, q, M4_NONE, 1, 0, NULL) >= 0;
        if (kind == MA_KONG) {
            int ks[M4_KINDS], n = m4_kongs(p, ks);
            for (int i = 0; i < n; i++)
                if (ks[i] == arg) return 1;
        }
        return 0;
    }
    case MP_CLAIM:
    case MP_ROB: {
        int ch[3], nch, o = m4_options(p, ch, &nch);
        if (kind == MA_PASS) return 1;
        if (kind == MA_WIN) return (o & MO_WIN) != 0;
        if (kind == MA_PUNG) return (o & MO_PUNG) != 0;
        if (kind == MA_KONG) return (o & MO_KONG) != 0;
        if (kind == MA_CHOW)
            for (int i = 0; i < nch; i++)
                if (ch[i] == arg) return 1;
        return 0;
    }
    case MP_SCORE: return kind == MA_NEXT;
    default: return 0;
    }
}

static void apply(m4pos *p, int act)
{
    int kind = MA_KIND(act), arg = MA_ARG(act);
    if (p->phase == MP_DISCARD) {
        int q = p->turn;
        if (kind == MA_DISCARD) {
            drop(p, q, arg);
            p->pond[q][p->npond[q]++] = (uint8_t)arg;
            p->discards++;
            p->tile = (uint8_t)arg;
            p->from = (uint8_t)q;
            p->drawn = M4_NONE;
            open_claims(p);
            advance(p);
        } else if (kind == MA_WIN) {
            hand_over(p, q, M4_NONE, 0);
        } else if (kind == MA_KONG) {
            int ids[4], n = 0;
            for (int i = 0; i < p->nmeld[q]; i++) {
                m4meld *m = &p->meld[q][i];
                if (m->type != MS_PUNG || m->kind != arg) continue;
                int id = drop_kind(p, q, arg);  /* added to an exposed pung: a kong that could be robbed */
                m->type = MS_KONG;
                m->tile[3] = (uint8_t)id;
                m->n = 4;
                p->tile = (uint8_t)id;
                p->from = (uint8_t)q;
                p->nask = p->k = 0;
                p->pend = M4_NONE;
                memset(p->opts, 0, sizeof p->opts);
                int any = 0;
                for (int j = 1; j < M4_P; j++) {
                    int r = (q + j) % M4_P;
                    if (m4_wins(p, r, arg, 0, 1, NULL) >= 0) p->opts[r] = MO_WIN, any = 1;
                }
                if (any) {                      /* all three asked, in turn order */
                    for (int j = 1; j < M4_P; j++) p->ask[p->nask++] = (uint8_t)((q + j) % M4_P);
                    p->phase = MP_ROB;
                    return;
                }
                draw(p, q, 1);
                return;
            }
            while (n < 4) ids[n++] = drop_kind(p, q, arg);   /* four in the hand: a concealed kong */
            meld(p, q, MS_KONG, arg, M4_NONE, ids, 4, M4_NONE);
            draw(p, q, 1);
        }
        return;
    }
    if (p->phase == MP_CLAIM) {
        int q = p->ask[p->k];
        if (kind == MA_PASS) {
            p->k++;
            advance(p);
            return;
        }
        if (p->k == p->nask - 1) { claim(p, q, act); return; }   /* the next player, last: theirs is the best */
        int win = kind == MA_WIN;
        if (p->pend == M4_NONE || (win && MA_KIND(p->pend_act) != MA_WIN)) {
            p->pend = (uint8_t)q;
            p->pend_act = (uint16_t)act;
        }
        p->k++;
        advance(p);
        return;
    }
    if (p->phase == MP_ROB) {
        int q = p->ask[p->k], konger = p->from;
        if (kind == MA_WIN) {                   /* robbed: the kong is a pung again */
            for (int i = 0; i < p->nmeld[konger]; i++) {
                m4meld *m = &p->meld[konger][i];
                if (m->type == MS_KONG && m->n == 4 && m->tile[3] == p->tile) {
                    m->type = MS_PUNG;
                    m->n = 3;
                    m->tile[3] = M4_NONE;
                }
            }
            hand_over(p, q, konger, 1);
            return;
        }
        if (++p->k >= p->nask) {                /* nobody robbed it: the replacement */
            p->turn = (uint8_t)konger;
            p->phase = MP_DISCARD;
            draw(p, konger, 1);
        }
        return;
    }
    if (p->phase == MP_SCORE && kind == MA_NEXT) {
        if (p->result.winner == p->dealer) p->streak++;
        else {
            p->streak = 0;
            p->dealer = (uint8_t)((p->dealer + 1) % M4_P);
            if (!p->dealer) p->wind++;
        }
        if (p->wind >= (p->variant == M4_FULL ? 2 : 1)) { p->phase = MP_OVER; return; }
        p->hands++;
        deal(p);
    }
}

int m4_play(m4game *g, int act)
{
    if (!m4_legal(&g->pos, act)) return 0;
    if (MA_KIND(act) != MA_NEXT && g->n >= M4_ACTS) return 0;
    apply(&g->pos, act);
    if (MA_KIND(act) == MA_NEXT) {              /* a new hand: undo starts again from here */
        g->start = g->pos;
        g->n = 0;
    } else g->act[g->n++] = (uint16_t)act;
    return 1;
}

int m4_undo(m4game *g)
{
    if (!g->n) return 0;
    g->n--;
    g->pos = g->start;
    for (int i = 0; i < g->n; i++) apply(&g->pos, g->act[i]);
    return 1;
}

/* ---- plain play -------------------------------------------------------------------------------------------------- */
/* how far the counts c are from a complete hand of `need` sets and a pair (-1: complete, 0: one tile away), the
 * usual way: sets, partial sets (a pair, two in a row or with a gap) and a pair to finish on */
static void away_rec(int *c, int k, int need, int sets, int part, int head, int *best)
{
    while (k < M4_KINDS && !c[k]) k++;
    if (k == M4_KINDS) {
        int t = part < need - sets ? part : need - sets, s = 2 * (need - sets) - t - head;
        if (s < *best) *best = s;
        return;
    }
    if (c[k] >= 3) {
        c[k] -= 3;
        away_rec(c, k, need, sets + 1, part, head, best);
        c[k] += 3;
    }
    if (chow_start(k) && c[k + 1] && c[k + 2]) {
        c[k]--, c[k + 1]--, c[k + 2]--;
        away_rec(c, k, need, sets + 1, part, head, best);
        c[k]++, c[k + 1]++, c[k + 2]++;
    }
    if (c[k] >= 2) {
        c[k] -= 2;
        if (!head) away_rec(c, k, need, sets, part, 1, best);
        away_rec(c, k, need, sets, part + 1, head, best);
        c[k] += 2;
    }
    for (int g = 1; g <= 2; g++)
        if (k < M4_WINDS && k % 9 + g <= 8 && c[k + g]) {
            c[k]--, c[k + g]--;
            away_rec(c, k, need, sets, part + 1, head, best);
            c[k]++, c[k + g]++;
        }
    c[k]--;
    away_rec(c, k + 1, need, sets, part, head, best);
    c[k]++;
}

static int away(const m4pos *p, int q, int skip)   /* the player's hand without the tile `skip` (an id; -1) */
{
    int c[M4_KINDS] = {0}, best = 8, pairs = 0;
    for (int i = 0; i < p->nhand[q]; i++)
        if (p->hand[q][i] != skip) c[m4_kind(p->hand[q][i])]++;
    away_rec(c, 0, 4 - p->nmeld[q], 0, 0, 0, &best);
    if (!p->nmeld[q]) {                         /* seven pairs */
        for (int k = 0; k < M4_KINDS; k++) pairs += c[k] >= 2;
        if (6 - pairs < best) best = 6 - pairs;
    }
    return best;
}

/* how much a tile of `kind` helps the player's hand, beyond the above (the least is discarded first) */
static int worth(const m4pos *p, int q, int kind)
{
    int n = count(p, q, kind), w = (n - 1) * 6;
    if (kind >= M4_WINDS) {
        if (kind >= M4_DRAGONS || kind == M4_WINDS + m4_seat(p, q) || kind == M4_WINDS + p->wind) w += 2;
        return w;
    }
    int r = kind % 9;
    for (int d = -2; d <= 2; d++) {
        if (!d || r + d < 0 || r + d > 8) continue;
        int c = count(p, q, kind + d);
        w += c ? (d == 1 || d == -1 ? 3 : 2) : 0;
    }
    return w + (r && r != 8);
}

int m4_auto(const m4pos *p)
{
    switch (p->phase) {
    case MP_SCORE: return MA_NEXT;
    case MP_CLAIM:
    case MP_ROB: {
        int o = m4_options(p, NULL, NULL), k = m4_kind(p->tile), q = p->ask[p->k];
        if (o & MO_WIN) return MA_WIN;
        if ((o & MO_PUNG) && (k >= M4_DRAGONS || k == M4_WINDS + m4_seat(p, q) || k == M4_WINDS + p->wind))
            return MA_PUNG;                     /* a pung worth a faan */
        return MA_PASS;
    }
    case MP_DISCARD: {
        if (m4_legal(p, MA_WIN)) return MA_WIN;
        int q = p->turn, best = -1, ba = 99, bw = 1 << 30, seen[M4_KINDS] = {0};
        for (int i = 0; i < p->nhand[q]; i++) {
            int id = p->hand[q][i], k = m4_kind(id);
            if (seen[k]++ && id != p->drawn) continue;
            int a = away(p, q, id), w = worth(p, q, k);
            if (a < ba || (a == ba && (w < bw || (w == bw && id == p->drawn)))) ba = a, bw = w, best = id;
        }
        return best >= 0 ? MA_DISCARD | best : 0;
    }
    default: return 0;
    }
}

unsigned m4_winners(const m4pos *p)
{
    if (p->phase != MP_OVER) return 0;
    int32_t top = p->total[0];
    for (int q = 1; q < M4_P; q++)
        if (p->total[q] > top) top = p->total[q];
    unsigned m = 0;
    for (int q = 0; q < M4_P; q++)
        if (p->total[q] == top) m |= 1u << q;
    return m;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int m4_parse_tile(const char **s)
{
    const char *t = *s;
    if (t[0] >= '1' && t[0] <= '9' && (t[1] == 'o' || t[1] == 'b' || t[1] == 'c')) {
        *s += 2;
        return (t[1] == 'o' ? M4_DOTS : t[1] == 'b' ? M4_BAMBOO : M4_CHARS) + t[0] - '1';
    }
    if ((t[0] == 'F' || t[0] == 'S') && t[1] >= '1' && t[1] <= '4') {
        *s += 2;
        return (t[0] == 'F' ? M4_FLOWERS : M4_SEASONS) + t[1] - '1';
    }
    static const char HON[] = "ESWNRGP";
    for (int i = 0; HON[i]; i++)
        if (t[0] == HON[i]) {
            *s += 1;
            return M4_WINDS + i;
        }
    return -1;
}

/* the id of a tile of that kind in the player's hand: the drawn one if it is of that kind */
static int id_of(const m4pos *p, int q, int kind)
{
    if (p->drawn != M4_NONE && m4_kind(p->drawn) == kind) return p->drawn;
    for (int i = 0; i < p->nhand[q]; i++)
        if (m4_kind(p->hand[q][i]) == kind) return p->hand[q][i];
    return -1;
}

int m4_script(m4game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int act = 0, k;
        switch (c) {
        case 'x':
            if ((k = m4_parse_tile(&s)) < 0 || g->pos.phase != MP_DISCARD) return 0;
            if ((k = id_of(&g->pos, g->pos.turn, k)) < 0) return 0;
            act = MA_DISCARD | k;
            break;
        case 'K':
            if ((k = m4_parse_tile(&s)) < 0) return 0;
            act = MA_KONG | k;
            break;
        case 'c':
            if ((k = m4_parse_tile(&s)) < 0) return 0;
            act = MA_CHOW | k;
            break;
        case 'k': act = MA_KONG | (g->pos.phase == MP_CLAIM ? m4_kind(g->pos.tile) : 0); break;
        case 't': {                            /* take the first choice: a win, a kong, a pung, a chow */
            const m4pos *p = &g->pos;
            int ch[3], nch, o = m4_options(p, ch, &nch), ks[8];
            if (p->phase == MP_DISCARD) act = m4_legal(p, MA_WIN) ? MA_WIN : m4_kongs(p, ks) ? MA_KONG | ks[0] : 0;
            else act = o & MO_WIN ? MA_WIN : o & MO_KONG ? MA_KONG | m4_kind(p->tile) : o & MO_PUNG ? MA_PUNG :
                       nch ? MA_CHOW | ch[0] : 0;
            break;
        }
        case 'w': act = MA_WIN; break;
        case 'u': act = MA_PUNG; break;
        case 'p': act = MA_PASS; break;
        case 'n': act = MA_NEXT; break;
        case 'a': act = m4_auto(&g->pos); break;
        case '?':                              /* on to the next choice: a claim, a kong of one's own, a win */
            for (int i = 0; i < 100000 && g->pos.phase != MP_SCORE && g->pos.phase != MP_OVER; i++) {
                if (!m4_play(g, m4_auto(&g->pos))) return 0;
                const m4pos *p = &g->pos;
                if ((p->phase == MP_CLAIM || p->phase == MP_ROB) && m4_options(p, NULL, NULL)) break;
                if (p->phase == MP_DISCARD && (m4_legal(p, MA_WIN) || m4_kongs(p, NULL))) break;
            }
            continue;
        case '!':                              /* on to the next win that can be declared */
            for (int i = 0; i < 100000 && g->pos.phase != MP_OVER; i++) {
                const m4pos *p = &g->pos;
                if (m4_legal(p, MA_WIN)) break;
                if (!m4_play(g, m4_auto(p))) return 0;
            }
            continue;
        case '#':
        case '*': {
            int over = c == '#' ? MP_SCORE : MP_OVER;
            for (int i = 0; i < 100000 && g->pos.phase != over && g->pos.phase != MP_OVER; i++)
                if (!m4_play(g, m4_auto(&g->pos))) return 0;
            continue;
        }
        default: return 0;
        }
        if (!m4_play(g, act)) return 0;
    }
    return 1;
}
