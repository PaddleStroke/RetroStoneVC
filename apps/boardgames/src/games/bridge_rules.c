/*
 * Contract bridge: the rules (bridge.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/bridge.h"
#include "games/tricks.h"

static void deal(brg_pos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_52, deck);
    cards_shuffle(deck, k, trick_mix(p->seed, 1000u + p->dealt));
    p->dealt++;
    for (int pl = 0; pl < BRG_P; pl++) p->n[pl] = 0, p->won[pl] = 0;
    for (int i = 0; i < k; i++) {
        int pl = (p->dealer + 1 + i) % BRG_P;  /* from the dealer's left, the dealer last */
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    for (int pl = 0; pl < BRG_P; pl++) trick_sort(p->hand[pl], p->n[pl]);
    memcpy(p->orig, p->hand, sizeof p->orig);
    p->ncalls = 0;
    p->bid = 0, p->bidder = BRG_NONE, p->dbl = 0, p->passes = 0;
    memset(p->first, BRG_NONE, sizeof p->first);
    p->declarer = BRG_NONE, p->level = 0, p->strain = 0, p->contract_dbl = 0;
    memset(p->trick, CARD_NONE, sizeof p->trick);
    memset(p->last, CARD_NONE, sizeof p->last);
    p->ntrick = p->tricks = 0;
    p->last_winner = p->last_leader = BRG_NONE;
    p->passed_out = 0;
    p->keeper[0] = p->keeper[1] = BRG_NONE;
    p->phase = BRG_AUCTION;
    p->turn = p->leader = p->dealer;
}

void brg_new(brg_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->pos.variant = (uint8_t)(variant == BRG_RUBBER ? BRG_RUBBER : BRG_CHICAGO);
    g->pos.dealer = 0;
    deal(&g->pos);
}

int brg_dummy(const brg_pos *p) { return p->level ? (p->declarer + 2) % BRG_P : BRG_NONE; }

int brg_dummy_shown(const brg_pos *p) { return p->phase == BRG_PLAY && (p->tricks > 0 || p->ntrick > 0); }

int brg_controller(const brg_pos *p)
{
    return p->phase == BRG_PLAY && p->turn == brg_dummy(p) ? p->declarer : p->turn;
}

int brg_vulnerable(const brg_pos *p, int side)
{
    if (p->variant == BRG_RUBBER) return p->games[side] > 0;
    int d = p->deals % 4;                       /* Chicago: none, the dealer's side twice, both */
    return d == 0 ? 0 : d == 3 ? 1 : brg_side(p->dealer) == side;
}

int brg_can_call(const brg_pos *p, int call)
{
    if (p->phase != BRG_AUCTION) return 0;
    if (call == BRG_PASS) return 1;
    if (call >= 1 && call <= 35) return call > p->bid;
    int mine = p->bid && brg_side(p->bidder) == brg_side(p->turn);
    if (call == BRG_DOUBLE) return p->bid && !mine && p->dbl == 0;
    if (call == BRG_REDOUBLE) return p->bid && mine && p->dbl == 1;
    return 0;
}

int brg_can_play(const brg_pos *p, int card)
{
    if (p->phase != BRG_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    return trick_follows(h, n, p->ntrick ? CARD_SUIT(p->trick[0]) : -1, card);
}

int brg_legal(const brg_pos *p, int act)
{
    switch (BRG_KIND(act)) {
    case BRG_ACALL: return brg_can_call(p, BRG_ARG(act));
    case BRG_APLAY: return brg_can_play(p, BRG_ARG(act));
    case BRG_ANEXT: return p->phase == BRG_SCORE && act == BRG_ANEXT;
    default: return 0;
    }
}

/* ---- the score --------------------------------------------------------------------------------------------------- */
void brg_score(int level, int strain, int dbl, int vul, int taken, int duplicate, int *below, int *above_decl,
               int *above_def)
{
    int need = 6 + level, mult = 1 << dbl;
    *below = *above_decl = *above_def = 0;
    if (taken >= need) {
        int per = strain <= BRG_DIAMONDS ? 20 : 30, over = taken - need;
        int tp = (level * per + (strain == BRG_NT ? 10 : 0)) * mult;
        int ot = dbl == 0 ? over * per : over * (vul ? 200 : 100) * (dbl == 2 ? 2 : 1);
        int insult = dbl ? 50 * dbl : 0;
        int slam = level == 6 ? (vul ? 750 : 500) : level == 7 ? (vul ? 1500 : 1000) : 0;
        int bonus = duplicate ? (tp >= 100 ? (vul ? 500 : 300) : 50) : 0;
        *below = tp;
        *above_decl = ot + insult + slam + bonus;
        return;
    }
    int down = need - taken, pen;
    if (!dbl) pen = down * (vul ? 100 : 50);
    else if (vul) pen = 200 + (down - 1) * 300;
    else pen = 100 + (down >= 2 ? 200 : 0) + (down >= 3 ? 200 : 0) + (down > 3 ? (down - 3) * 300 : 0);
    if (dbl == 2) pen *= 2;
    *above_def = pen;
}

int brg_honours(const uint8_t hand[BRG_HAND], int n, int strain)
{
    int k = 0;
    for (int i = 0; i < n; i++) {
        int c = hand[i], r = CARD_RANK(c);
        if (strain == BRG_NT) k += r == RANK_ACE;
        else k += CARD_SUIT(c) == brg_suit(strain) && (r == RANK_ACE || r >= 10);
    }
    if (strain == BRG_NT) return k == 4 ? 150 : 0;
    return k == 5 ? 150 : k == 4 ? 100 : 0;
}

static void deal_end(brg_pos *p)
{
    int ds = brg_side(p->declarer), taken = p->won[p->declarer] + p->won[(p->declarer + 2) % BRG_P];
    int vul = brg_vulnerable(p, ds), below, ad, af;
    brg_deal r;
    memset(&r, 0, sizeof r);
    r.dealer = p->dealer, r.declarer = p->declarer, r.level = p->level, r.strain = p->strain;
    r.dbl = p->contract_dbl, r.taken = (uint8_t)taken;
    r.vul = (uint8_t)(brg_vulnerable(p, 0) | brg_vulnerable(p, 1) << 1);
    brg_score(p->level, p->strain, p->contract_dbl, vul, taken, p->variant == BRG_CHICAGO, &below, &ad, &af);
    r.below[ds] = (int16_t)below;
    r.above[ds] = (int16_t)ad;
    r.above[ds ^ 1] = (int16_t)af;
    p->deals++;
    if (p->variant == BRG_RUBBER) {
        for (int pl = 0; pl < BRG_P; pl++) {   /* honours, whatever the result */
            int h = brg_honours(p->orig[pl], BRG_HAND, p->strain);
            r.honours[brg_side(pl)] = (int16_t)(r.honours[brg_side(pl)] + h);
            r.above[brg_side(pl)] = (int16_t)(r.above[brg_side(pl)] + h);
        }
        p->part[ds] = (int16_t)(p->part[ds] + below);
        if (p->part[ds] >= 100) {
            p->games[ds]++;
            r.game = (uint8_t)(ds + 1);
            p->part[0] = p->part[1] = 0;
            if (p->games[ds] == 2) {
                r.bonus = (int16_t)(p->games[ds ^ 1] ? 500 : 700);
                r.above[ds] = (int16_t)(r.above[ds] + r.bonus);
                r.rubber = (uint8_t)(ds + 1);
            }
        }
        p->dealer = (uint8_t)((p->dealer + 1) % BRG_P);
    } else {
        p->dealer = (uint8_t)(p->deals % BRG_P);
    }
    for (int s = 0; s < 2; s++) p->total[s] += r.below[s] + r.above[s];
    if (p->nrec < BRG_DEALS) p->rec[p->nrec++] = r;
    int over = p->variant == BRG_RUBBER ? r.rubber != 0 : p->deals >= 4;
    p->phase = over ? BRG_OVER : BRG_SCORE;
    p->turn = p->dealer;
}

static void auction_call(brg_pos *p, int call)
{
    int pl = p->turn;
    if (p->ncalls < BRG_CALLS) p->call[p->ncalls++] = (uint8_t)call;
    if (call == BRG_PASS) p->passes++;
    else if (call == BRG_DOUBLE) p->dbl = 1, p->passes = 0;
    else if (call == BRG_REDOUBLE) p->dbl = 2, p->passes = 0;
    else {
        p->bid = (uint8_t)call, p->bidder = (uint8_t)pl, p->dbl = 0, p->passes = 0;
        uint8_t *f = &p->first[brg_side(pl)][BRG_STRAIN(call)];
        if (*f == BRG_NONE) *f = (uint8_t)pl;
    }
    if (!p->bid && p->passes == 4) {           /* passed out */
        p->passed_out = 1;
        p->phase = BRG_SCORE;
        if (p->variant == BRG_RUBBER) p->dealer = (uint8_t)((p->dealer + 1) % BRG_P);
        p->turn = p->dealer;
        return;
    }
    if (p->bid && p->passes == 3) {            /* the contract */
        p->level = (uint8_t)BRG_LEVEL(p->bid);
        p->strain = (uint8_t)BRG_STRAIN(p->bid);
        p->contract_dbl = p->dbl;
        p->declarer = p->first[brg_side(p->bidder)][p->strain];
        p->phase = BRG_PLAY;
        p->turn = p->leader = (uint8_t)((p->declarer + 1) % BRG_P);
        return;
    }
    p->turn = (uint8_t)((pl + 1) % BRG_P);
}

static void apply(brg_pos *p, int act)
{
    int pl = p->turn;
    switch (BRG_KIND(act)) {
    case BRG_ACALL:
        auction_call(p, BRG_ARG(act));
        break;
    case BRG_APLAY: {
        int c = BRG_ARG(act);
        trick_remove(p->hand[pl], &p->n[pl], c);
        p->trick[p->ntrick++] = (uint8_t)c;
        if (p->ntrick < BRG_P) { p->turn = (uint8_t)((pl + 1) % BRG_P); break; }
        int w = (p->leader + trick_winner(p->trick, BRG_P, brg_suit(p->strain))) % BRG_P;
        memcpy(p->last, p->trick, BRG_P);
        memset(p->trick, CARD_NONE, sizeof p->trick);
        p->won[w]++;
        if (p->keeper[brg_side(w)] == BRG_NONE)
            p->keeper[brg_side(w)] = (uint8_t)(brg_side(w) == brg_side(p->declarer) ? p->declarer : w);
        p->last_leader = p->leader;
        p->last_winner = (uint8_t)w;
        p->ntrick = 0;
        p->tricks++;
        p->turn = p->leader = (uint8_t)w;
        if (p->tricks == BRG_HAND) deal_end(p);
        break;
    }
    case BRG_ANEXT:
        deal(p);
        break;
    }
}

int brg_play(brg_game *g, int act)
{
    if (g->n >= BRG_MAX_ACTS || !brg_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int brg_undo(brg_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[BRG_MAX_ACTS];
    int n = g->n - 1, variant = g->pos.variant;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    brg_new(g, variant, g->seed);
    for (int i = 0; i < n; i++) brg_play(g, acts[i]);
    return 1;
}

int brg_winner(const brg_pos *p)
{
    if (p->phase != BRG_OVER) return -1;
    return p->total[0] == p->total[1] ? 2 : p->total[1] > p->total[0];
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int hcp(const uint8_t *h, int n)
{
    int t = 0;
    for (int i = 0; i < n; i++) {
        int r = CARD_RANK(h[i]);
        t += r == RANK_ACE ? 4 : r >= RANK_JACK ? r - 10 : 0;
    }
    return t;
}

/* a plain call: the cheapest bid in the longest suit (no trumps when balanced) while the points allow its level (12
 * for one, 3 more a level; the partner's bid counts 12), else pass */
static int plain_call(const brg_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn], len[4] = {0}, best = BRG_CLUBS, shortest = 13;
    for (int i = 0; i < n; i++) len[CARD_SUIT(h[i])]++;
    for (int st = BRG_CLUBS; st <= BRG_SPADES; st++) {
        int l = len[brg_suit(st)];
        if (l >= len[brg_suit(best)]) best = st;
        if (l < shortest) shortest = l;
    }
    int strain = shortest >= 2 && len[brg_suit(best)] <= 5 ? BRG_NT : best;
    int pts = hcp(h, n) + (p->bid && p->bidder == (p->turn + 2) % BRG_P ? 12 : 0);
    for (int lv = 1; lv <= 7; lv++) {
        int c = BRG_BID(lv, strain);
        if (c > p->bid) return pts >= 12 + 3 * (lv - 1) ? c : BRG_PASS;
    }
    return BRG_PASS;
}

int brg_auto(const brg_pos *p)
{
    switch (p->phase) {
    case BRG_AUCTION: return BRG_ACALL | plain_call(p);
    case BRG_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (brg_can_play(p, p->hand[p->turn][i])) return BRG_APLAY | p->hand[p->turn][i];
        return 0;
    case BRG_SCORE: return BRG_ANEXT;
    default: return 0;
    }
}

int brg_script(brg_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {          /* plain actions to the end of the deal (!: of the game) */
            int all = *s++ == '!';
            while (g->pos.phase != BRG_OVER && (all || g->pos.phase == BRG_AUCTION || g->pos.phase == BRG_PLAY))
                if (!brg_play(g, brg_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = brg_auto(&g->pos); }
        else if (*s == 'N') { s++; act = BRG_ANEXT; }
        else if (*s == 'p') { s++; act = BRG_ACALL | BRG_PASS; }
        else if (s[0] == 'x' && s[1] == 'x') { s += 2; act = BRG_ACALL | BRG_REDOUBLE; }
        else if (*s == 'x') { s++; act = BRG_ACALL | BRG_DOUBLE; }
        else if (*s == 'b') {
            static const char strains[] = "cdhsn";
            if (s[1] < '1' || s[1] > '7' || !s[2]) return 0;
            const char *k = strchr(strains, s[2]);
            if (!k || !*k) return 0;
            act = BRG_ACALL | BRG_BID(s[1] - '0', (int)(k - strains));
            s += 3;
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = BRG_APLAY | c;
        }
        if (!brg_play(g, act)) return 0;
    }
    return 1;
}
