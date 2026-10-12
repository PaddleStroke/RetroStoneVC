/*
 * Mus: the rules (mus.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/mus.h"
#include "games/spanish.h"

static int side(int pl) { return pl & 1; }

/* ---- the hands --------------------------------------------------------------------------------------------------- */
int mus_value(int card)
{
    int r = CARD_RANK(card);
    return r == 3 ? ES_REY : r == 2 ? ES_AS : r;
}

static int card_points(int card)
{
    int v = mus_value(card);
    return v >= ES_SOTA ? 10 : v;
}

int mus_points(const uint8_t *h)
{
    int s = 0;
    for (int i = 0; i < 4; i++) s += card_points(h[i]);
    return s;
}

/* the highest first; the same value: by suit */
void mus_sort(uint8_t *h)
{
    for (int i = 1; i < 4; i++) {
        uint8_t c = h[i];
        int j = i;
#define KEY(x) (mus_value(x) * 8 + 7 - CARD_SUIT(x))
        while (j > 0 && KEY(h[j - 1]) < KEY(c)) { h[j] = h[j - 1]; j--; }
#undef KEY
        h[j] = c;
    }
}

int mus_pares(const uint8_t *h, int *key)
{
    int count[13] = {0};
    for (int i = 0; i < 4; i++) count[mus_value(h[i])]++;
    int hi = 0, lo = 0, three = 0;
    for (int v = 12; v >= 1; v--) {
        if (count[v] == 4) { *key = v * 16 + v; return MUS_DUPLES; }
        if (count[v] == 3) three = v;
        if (count[v] == 2) {
            if (!hi) hi = v;
            else lo = v;
        }
    }
    if (hi && lo) { *key = hi * 16 + lo; return MUS_DUPLES; }
    if (three) { *key = three; return MUS_MEDIAS; }
    if (hi) { *key = hi; return MUS_PAR; }
    *key = 0;
    return MUS_NO_PARES;
}

/* the juego's order: 31, 32, 40, 37, 36, 35, 34, 33 (higher is better) */
static int juego_rank(int pts)
{
    static const int8_t order[] = {33, 34, 35, 36, 37, 40, 32, 31};
    for (int i = 0; i < 8; i++)
        if (order[i] == pts) return i + 1;
    return 0;
}

int mus_cmp(int lance, int punto, const uint8_t *a, const uint8_t *b)
{
    uint8_t x[4], y[4];
    memcpy(x, a, 4), memcpy(y, b, 4);
    mus_sort(x), mus_sort(y);
    switch (lance) {
    case ML_GRANDE:
        for (int i = 0; i < 4; i++)
            if (mus_value(x[i]) != mus_value(y[i])) return mus_value(x[i]) - mus_value(y[i]);
        return 0;
    case ML_CHICA:
        for (int i = 3; i >= 0; i--)
            if (mus_value(x[i]) != mus_value(y[i])) return mus_value(y[i]) - mus_value(x[i]);
        return 0;
    case ML_PARES: {
        int ka, kb, ta = mus_pares(x, &ka), tb = mus_pares(y, &kb);
        return ta != tb ? ta - tb : ka - kb;
    }
    default:
        if (punto) return mus_points(x) - mus_points(y);
        return juego_rank(mus_points(x)) - juego_rank(mus_points(y));
    }
}

int mus_has(const mpos *p, int lance, int player)
{
    int key;
    if (lance == ML_PARES) return mus_pares(p->hand[player], &key) != MUS_NO_PARES;
    if (lance == ML_JUEGO && !p->punto) return mus_points(p->hand[player]) >= 31;
    return 1;
}

int mus_jugada(const mpos *p, int lance, int player)
{
    int key;
    if (lance == ML_PARES) return mus_pares(p->hand[player], &key);
    if (lance == ML_JUEGO && !p->punto) {
        int s = mus_points(p->hand[player]);
        return s == 31 ? 3 : s > 31 ? 2 : 0;
    }
    return 0;
}

int mus_best(const mpos *p, int lance, unsigned players)
{
    int best = -1;
    for (int k = 0; k < MUS_P; k++) {
        int pl = (p->mano + k) % MUS_P;
        if (!(players >> pl & 1)) continue;
        if (best < 0 || mus_cmp(lance, p->punto, p->hand[pl], p->hand[best]) > 0) best = pl;
    }
    return best;
}

/* both partners' pares or juego */
static int jugadas(const mpos *p, int lance, int s)
{
    return mus_jugada(p, lance, s) + mus_jugada(p, lance, s + 2);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static void deal(mpos *p)
{
    uint8_t pack[48];
    int n = es_pack(0, pack), k = 0;
    cards_shuffle(pack, n, es_mix(p->seed, 1000u + p->deal));
    p->dealer = (uint8_t)((3 + p->deal) % MUS_P);
    p->mano = (uint8_t)((p->dealer + 1) % MUS_P);
    for (int r = 0; r < 4; r++)
        for (int q = 0; q < MUS_P; q++) p->hand[(p->mano + q) % MUS_P][r] = pack[k++];
    for (int pl = 0; pl < MUS_P; pl++) mus_sort(p->hand[pl]);
    p->nstock = 0;
    while (k < n) p->stock[p->nstock++] = pack[k++];
    p->nlaid = p->refills = p->rounds = 0;
    memset(p->took, 0, sizeof p->took);
    memset(p->word, MW_NONE, sizeof p->word);
    memset(p->wordn, 0, sizeof p->wordn);
    memset(p->ls, LS_WAIT, sizeof p->ls);
    memset(p->lamount, 0, sizeof p->lamount);
    memset(p->lside, MUS_NONE, sizeof p->lside);
    memset(p->lwin, MUS_NONE, sizeof p->lwin);
    memset(p->lpts, 0, sizeof p->lpts);
    p->gained[0] = p->gained[1] = 0;
    p->lance = p->punto = 0;
    p->eligible = 0;
    p->bet = p->prev = p->ordago = p->refused = 0;
    p->bettor = MUS_NONE;
    p->phase = MP_MUS;
    p->turn = p->mano;
}

void mus_new(mgame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->pos.winner = MUS_NONE;
    deal(&g->pos);
}

/* ---- the lances -------------------------------------------------------------------------------------------------- */
static int scores(mpos *p, int s, int pts)      /* 1: the side has won the game */
{
    p->score[s] = (int16_t)(p->score[s] + pts);
    p->gained[s] = (int8_t)(p->gained[s] + pts);
    if (p->score[s] < MUS_GOAL) return 0;
    p->phase = MP_OVER;
    p->winner = (uint8_t)s;
    return 1;
}

/* the first speaker from the k0-th seat after the mano on (not wrapping past the postre, the mano's right): -1 none */
static int next_open(const mpos *p, int k0)
{
    for (int k = k0; k < MUS_P; k++) {
        int pl = (p->mano + k) % MUS_P;
        if (p->eligible >> pl & 1) return pl;
    }
    return -1;
}

/* the next opponent of `pl` who speaks in the lance, going round */
static int next_opponent(const mpos *p, int pl)
{
    for (int k = 1; k < MUS_P; k++) {
        int q = (pl + k) % MUS_P;
        if (side(q) != side(pl) && p->eligible >> q & 1) return q;
    }
    return -1;
}

static void count(mpos *p);

/* the lances from p->lance on: the first that is bet (MP_BET), or the count */
static void lances(mpos *p)
{
    for (; p->lance < ML_N; p->lance++) {
        int l = p->lance;
        p->punto = 1;                           /* nobody has juego: punto */
        for (int pl = 0; pl < MUS_P; pl++)
            if (mus_points(p->hand[pl]) >= 31) p->punto = 0;
        unsigned el = 0;
        for (int pl = 0; pl < MUS_P; pl++)
            if (mus_has(p, l, pl)) el |= 1u << pl;
        p->eligible = (uint8_t)el;
        if (!el) { p->ls[l] = LS_NONE; continue; }
        if (!(el & 0x5) || !(el & 0xa)) {       /* one side only: no bet */
            p->ls[l] = LS_ONE;
            p->lside[l] = (uint8_t)(el & 0x5 ? 0 : 1);
            continue;
        }
        memset(p->word, MW_NONE, sizeof p->word);
        memset(p->wordn, 0, sizeof p->wordn);
        p->bet = p->prev = p->ordago = p->refused = 0;
        p->bettor = MUS_NONE;
        p->phase = MP_BET;
        p->turn = (uint8_t)next_open(p, 0);
        return;
    }
    count(p);
}

static void next_lance(mpos *p)
{
    p->lance++;
    lances(p);
}

/* the count at the end of the hand, lance by lance, until a side reaches 40 */
static void count(mpos *p)
{
    p->phase = MP_HAND;
    p->turn = (uint8_t)((p->mano + 1) % MUS_P);   /* the next hand's mano */
    p->eligible = 0;
    for (int l = 0; l < ML_N; l++) {
        unsigned el = 0;
        for (int pl = 0; pl < MUS_P; pl++)
            if (mus_has(p, l, pl)) el |= 1u << pl;
        int s = -1, pts = 0, jug = l == ML_PARES || (l == ML_JUEGO && !p->punto);
        switch (p->ls[l]) {
        case LS_PASO:
        case LS_SEEN:
            s = side(mus_best(p, l, el));
            pts = (p->ls[l] == LS_SEEN ? p->lamount[l] : 0) + (jug ? jugadas(p, l, s) : p->ls[l] == LS_PASO ||
                                                                 l == ML_JUEGO ? 1 : 0);
            break;
        case LS_DEJE:
        case LS_ONE:
            s = p->lside[l];
            pts = jug ? jugadas(p, l, s) : 0;
            break;
        default: break;
        }
        if (s < 0) continue;
        p->lwin[l] = (uint8_t)s;
        p->lpts[l] = (uint8_t)pts;
        if (pts && scores(p, s, pts)) return;
    }
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int mus_legal(const mpos *p, int act)
{
    int a = MA_ARG(act), facing = p->bet > 0 || p->ordago;
    switch (MA_KIND(act)) {
    case MA_MUS:
    case MA_CUT: return p->phase == MP_MUS && !a;
    case MA_DISCARD: return p->phase == MP_DISCARD && a >= 1 && a <= 15;
    case MA_PASS: return p->phase == MP_BET && !facing && !a;
    case MA_BET:
        if (p->phase != MP_BET || p->ordago) return 0;
        return facing ? a >= 1 && p->bet + a <= MUS_MAX_BET : a >= 2 && a <= MUS_MAX_BET;
    case MA_ORDAGO: return p->phase == MP_BET && !p->ordago && !a;
    case MA_QUIERO:
    case MA_NOQUIERO: return p->phase == MP_BET && facing && !a;
    case MA_NEXT: return p->phase == MP_HAND && !a;
    default: return 0;
    }
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
/* card `c` drawn from the stock; an empty stock is made again of the cards laid away, but the last `keep` */
static uint8_t draw(mpos *p, int keep)
{
    if (!p->nstock) {
        int n = p->nlaid - keep;
        memcpy(p->stock, p->laid, (size_t)n);
        memmove(p->laid, p->laid + n, (size_t)keep);
        p->nlaid = (uint8_t)keep;
        p->nstock = (uint8_t)n;
        cards_shuffle(p->stock, n, es_mix(p->seed, 5000u + 64u * p->deal + p->refills++));
    }
    return p->stock[--p->nstock];
}

static void bet_made(mpos *p, int pl)
{
    p->bettor = (uint8_t)side(pl);
    p->refused = 0;
    p->turn = (uint8_t)next_opponent(p, pl);
}

static void apply(mpos *p, int act)
{
    int pl = p->turn, a = MA_ARG(act), l = p->lance;
    switch (MA_KIND(act)) {
    case MA_MUS:
        p->word[pl] = MW_MUS;
        p->turn = (uint8_t)((pl + 1) % MUS_P);
        if (p->turn == p->mano) {
            p->phase = MP_DISCARD;
            memset(p->took, 0, sizeof p->took);
        }
        break;
    case MA_CUT:
        p->word[pl] = MW_CUT;
        p->lance = 0;
        lances(p);
        break;
    case MA_DISCARD: {
        int k = 0;
        uint8_t keep[4];
        for (int i = 0; i < 4; i++) {
            if (a >> i & 1) p->laid[p->nlaid++] = p->hand[pl][i], k++;
            else keep[i - k] = p->hand[pl][i];
        }
        for (int i = 0; i < k; i++) keep[4 - k + i] = draw(p, k);
        memcpy(p->hand[pl], keep, 4);
        mus_sort(p->hand[pl]);
        p->took[pl] = (uint8_t)k;
        p->turn = (uint8_t)((pl + 1) % MUS_P);
        if (p->turn == p->mano) {
            p->phase = MP_MUS;
            p->rounds++;
            memset(p->word, MW_NONE, sizeof p->word);
        }
        break;
    }
    case MA_PASS: {
        p->word[pl] = MW_PASS;
        int nx = next_open(p, (pl - p->mano + MUS_P) % MUS_P + 1);
        if (nx >= 0) p->turn = (uint8_t)nx;
        else {
            p->ls[l] = LS_PASO;
            next_lance(p);
        }
        break;
    }
    case MA_BET:
        p->word[pl] = p->bet ? MW_RAISE : MW_BET;
        p->wordn[pl] = (uint8_t)a;
        p->prev = p->bet;
        p->bet = (uint8_t)(p->bet + a);
        bet_made(p, pl);
        break;
    case MA_ORDAGO:
        p->word[pl] = MW_ORDAGO;
        p->prev = p->bet;
        p->ordago = 1;
        bet_made(p, pl);
        break;
    case MA_QUIERO:
        p->word[pl] = MW_QUIERO;
        if (p->ordago) {                        /* the hands are shown: the lance's winner wins the game */
            int w = side(mus_best(p, l, p->eligible));
            p->ls[l] = LS_ORDAGO;
            p->lside[l] = p->lwin[l] = (uint8_t)w;
            p->phase = MP_OVER;
            p->winner = (uint8_t)w;
            break;
        }
        p->ls[l] = LS_SEEN;
        p->lamount[l] = p->bet;
        next_lance(p);
        break;
    case MA_NOQUIERO: {
        p->word[pl] = MW_NOQUIERO;
        p->refused |= (uint8_t)(1u << pl);
        int partner = (pl + 2) % MUS_P;
        if (p->eligible >> partner & 1 && !(p->refused >> partner & 1)) {
            p->turn = (uint8_t)partner;
            break;
        }
        int deje = p->prev ? p->prev : 1;
        p->ls[l] = LS_DEJE;
        p->lamount[l] = (uint8_t)deje;
        p->lside[l] = p->bettor;
        if (scores(p, p->bettor, deje)) break;
        next_lance(p);
        break;
    }
    case MA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int mus_play(mgame *g, int act)
{
    if (g->n >= MUS_MAX_ACTS || !mus_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int mus_undo(mgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[MUS_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    mus_new(g, g->seed);
    for (int i = 0; i < n; i++) mus_play(g, acts[i]);
    return 1;
}

int mus_winner(const mpos *p) { return p->phase == MP_OVER ? p->winner : -1; }

int mus_actions(const mpos *p, int *out)
{
    static const int amounts[] = {1, 2, 3, 5, 10};
    int n = 0;
    for (int k = 1; k <= 9; k++) {
        int kind = k << 8;
        if (kind == MA_DISCARD) {
            for (int m = 1; m <= 15; m++)
                if (mus_legal(p, kind | m)) out[n++] = kind | m;
        } else if (kind == MA_BET) {
            for (int i = 0; i < 5; i++)
                if (mus_legal(p, kind | amounts[i])) out[n++] = kind | amounts[i];
        } else if (mus_legal(p, kind))
            out[n++] = kind;
    }
    return n;
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int strong(const mpos *p, int pl)        /* a hand worth betting in this lance */
{
    const uint8_t *h = p->hand[pl];
    int key, t;
    switch (p->lance) {
    case ML_GRANDE: return mus_value(h[0]) == ES_REY && mus_value(h[1]) == ES_REY;
    case ML_CHICA: return mus_value(h[3]) == ES_AS && mus_value(h[2]) == ES_AS;
    case ML_PARES: t = mus_pares(h, &key); return t >= MUS_MEDIAS || (t == MUS_PAR && key >= ES_REY);
    default: return p->punto ? mus_points(h) >= 28 : mus_points(h) == 31;
    }
}

int mus_auto(const mpos *p)
{
    int pl = p->turn;
    const uint8_t *h = p->hand[pl];
    int key;
    switch (p->phase) {
    case MP_MUS:
        return p->rounds < 2 && mus_pares(h, &key) == MUS_NO_PARES && mus_points(h) < 31 ? MA_MUS : MA_CUT;
    case MP_DISCARD: {
        int count[13] = {0}, m = 0;
        for (int i = 0; i < 4; i++) count[mus_value(h[i])]++;
        for (int i = 0; i < 4; i++)
            if (count[mus_value(h[i])] < 2 && mus_value(h[i]) < ES_SOTA) m |= 1 << i;
        return MA_DISCARD | (m ? m : 8);
    }
    case MP_BET:
        if (p->bet || p->ordago) return strong(p, pl) ? MA_QUIERO : MA_NOQUIERO;
        return strong(p, pl) ? MA_BET | 2 : MA_PASS;
    case MP_HAND: return MA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int mus_script(mgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const mpos *p = &g->pos;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != MP_OVER && (all || p->phase != MP_HAND))
                if (!mus_play(g, mus_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        switch (c) {
        case '.': act = mus_auto(p); break;
        case 'm': act = MA_MUS; break;
        case 'c': act = MA_CUT; break;
        case 'p': act = MA_PASS; break;
        case 'o': act = MA_ORDAGO; break;
        case 'q': act = MA_QUIERO; break;
        case 'n': act = MA_NOQUIERO; break;
        case 'N': act = MA_NEXT; break;
        case 'b': {
            int v = 0;
            if (*s < '0' || *s > '9') return 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
            if (v > 255) return 0;
            act = MA_BET | v;
            break;
        }
        case 'd': {
            int m = 0;
            for (;;) {
                const char *t = s;
                int card = es_parse(&t);
                if (card == CARD_NONE) break;
                s = t;
                int i = 0;
                while (i < 4 && p->hand[p->turn][i] != card) i++;
                if (i == 4) return 0;
                m |= 1 << i;
            }
            act = MA_DISCARD | m;
            break;
        }
        default: return 0;
        }
        if (!mus_play(g, act)) return 0;
    }
    return 1;
}
