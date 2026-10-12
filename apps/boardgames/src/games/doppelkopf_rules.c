/*
 * Doppelkopf: the rules (doppelkopf.h), after the DDV's tournament rules. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/doppelkopf.h"
#include "games/trick4.h"
#include "games/tricks.h"

#define QC CARD(SUIT_CLUBS, RANK_QUEEN)
#define FOX CARD(SUIT_DIAMONDS, RANK_ACE)
#define CHARLIE CARD(SUIT_CLUBS, RANK_JACK)

int dko_deals(int variant) { return variant == DKO_LONG ? 24 : 12; }

int dko_is_solo(int game) { return game >= DKG_QUEENS && game < DKG_TYPES; }

/* ---- the cards: 9, 10, jack 11, queen 12, king 13, ace 1 --------------------------------------------------------- */
static int rank_of(int card)
{
    int r = CARD_RANK(card) & 15;
    return r > 13 ? 0 : r;
}

int dko_points(int card)
{
    static const int8_t P[14] = {0, 11, 0, 0, 0, 0, 0, 0, 0, 0, 10, 2, 3, 4};
    return P[rank_of(card)];
}

/* the games played with the normal trumps (the Dulle, queens, jacks, then a suit): their trump suit (-1: other games) */
static int trump_suit(int game)
{
    if (game >= DKG_SUIT && game < DKG_TYPES) return game - DKG_SUIT;
    return game <= DKG_SILENT ? SUIT_DIAMONDS : -1;
}

int dko_power(int card, int game)
{
    static const int8_t PLAIN[14] = {0, 6, 0, 0, 0, 0, 0, 0, 0, 1, 5, 2, 3, 4};   /* A 10 K Q J 9 */
    static const int8_t HIGH[4] = {2, 1, 0, 3};   /* clubs, spades, hearts, diamonds (indexed by cards.h's suits) */
    int r = rank_of(card), s = CARD_SUIT(card) & 3, ts = trump_suit(game);
    if (ts >= 0) {
        if (s == SUIT_HEARTS && r == 10) return 100;   /* the Dulle */
        if (r == RANK_QUEEN) return 90 + HIGH[s];
        if (r == RANK_JACK) return 80 + HIGH[s];
        return s == ts ? 70 + PLAIN[r] : PLAIN[r];
    }
    if (game == DKG_QUEENS && r == RANK_QUEEN) return 90 + HIGH[s];
    if (game == DKG_JACKS && r == RANK_JACK) return 80 + HIGH[s];
    return PLAIN[r];
}

int dko_trump(int card, int game) { return dko_power(card, game) >= 70; }

static int eff_suit(int card, int game) { return dko_trump(card, game) ? SUIT_TRUMP : CARD_SUIT(card); }

int dko_winner(const uint8_t *cards, int n, int game)
{
    int su[TK4_P], pw[TK4_P];
    for (int i = 0; i < n && i < TK4_P; i++) su[i] = eff_suit(cards[i], game), pw[i] = dko_power(cards[i], game);
    return tk4_best(su, pw, n);
}

/* a hand as shown: trumps first (the strongest first), then the other suits, their colours alternating */
void dko_sort(uint8_t *h, int n, int game)
{
    static const int cycle[4] = {SUIT_CLUBS, SUIT_HEARTS, SUIT_SPADES, SUIT_DIAMONDS};
    int ts = trump_suit(game), place[4];
    if (ts < 0)
        for (int i = 0; i < 4; i++) place[cycle[i]] = i;
    else {                                      /* the other colour, the same, the other */
        int red = ts == SUIT_HEARTS || ts == SUIT_DIAMONDS, same = -1, other[2], no = 0;
        place[ts] = 3;
        for (int i = 0; i < 4; i++) {
            int s = cycle[i], rd = s == SUIT_HEARTS || s == SUIT_DIAMONDS;
            if (s == ts) continue;
            if (rd == red) same = s;
            else other[no++] = s;
        }
        place[other[0]] = 0;
        place[same] = 1;
        place[other[1]] = 2;
    }
    int16_t key[256];
    for (int c = 0; c < 256; c++) {
        if (c >= 128) { key[c] = 999; continue; }
        key[c] = (int16_t)(dko_trump(c, game) ? 127 - dko_power(c, game) : 128 + place[CARD_SUIT(c)] * 16 + 15 -
                                                                         dko_power(c, game));
    }
    tk4_sort(h, n, key);
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static int forehand(const dkpos *p) { return tk4_next(p->dealer); }

static void deal(dkpos *p)
{
    static const int ranks[] = {RANK_ACE, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING, 0};
    static const int packets[] = {3, 3, 3, 3, 0};
    uint8_t pack[48];
    tk4_pack(pack, ranks, 2, trick_mix(p->seed, 6000u + p->deal));
    p->dealer = (uint8_t)((3 + p->deal) % TK4_P);
    tk4_deal(&p->t, pack, packets, forehand(p));
    for (int q = 0; q < TK4_P; q++) dko_sort(p->t.hand[q], p->t.n[q], DKG_NORMAL);
    memset(p->said, DKW_NONE, sizeof p->said);
    memset(p->choice, 0, sizeof p->choice);
    memset(p->level, 0, sizeof p->level);
    memset(p->qc, 0, sizeof p->qc);
    p->spoke = 0;
    p->game = DKG_NORMAL;
    p->soloist = DKO_NONE;
    p->re = 0;
    p->open = p->shift = 0;
    p->ann[0] = p->ann[1] = 0;
    p->turn = (uint8_t)forehand(p);
    p->phase = DKP_RESERVE;
}

void dko_new(dkgame *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->variant = g->pos.variant = (uint8_t)(variant == DKO_LONG ? DKO_LONG : DKO_SHORT);
    deal(&g->pos);
}

/* ---- the parties ------------------------------------------------------------------------------------------------- */
static int count_of(const tk4_table *t, int pl, int card)
{
    int k = 0;
    for (int i = 0; i < t->n[pl]; i++) k += t->hand[pl][i] == card;
    return k;
}

int dko_party(const dkpos *p, int pl) { return p->re >> pl & 1 ? 0 : 1; }

int dko_known(const dkpos *p, int pl)
{
    if (p->phase == DKP_RESERVE) return -1;
    if (p->phase != DKP_PLAY) return dko_party(p, pl);
    if (dko_is_solo(p->game)) return pl == p->soloist ? 0 : 1;
    if (p->game == DKG_WEDDING) return p->open ? dko_party(p, pl) : pl == p->soloist ? 0 : -1;
    if (p->level[pl] || p->qc[pl]) return dko_party(p, pl);   /* announced, or a queen of clubs shown */
    int shown = 0, re = 0;                      /* both queens of clubs shown, or two Re players: the others Kontra */
    for (int q = 0; q < TK4_P; q++) {
        shown += p->qc[q];
        re += (p->qc[q] || (p->level[q] && dko_party(p, q) == 0));
    }
    return shown >= 2 || re >= 2 ? 1 : -1;
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int dko_say_ok(const dkpos *p, int choice)
{
    if (p->phase != DKP_RESERVE) return 0;
    if (choice == DKG_WEDDING) return count_of(&p->t, p->turn, QC) == 2;
    return choice == DKG_NORMAL || dko_is_solo(choice);
}

int dko_announce_ok(const dkpos *p)
{
    if (p->phase != DKP_PLAY || !p->open) return 0;
    int pl = p->turn, k = dko_party(p, pl), L = p->ann[k] + 1, n = p->t.n[pl];
    if (L > DKL_BLACK) return 0;
    if (n >= DKO_HAND - L - p->shift) return 1;
    return L == DKL_PARTY && p->ann[1 - k] && n >= DKO_HAND - 1 - p->ann[1 - k] - p->shift;   /* a reply */
}

int dko_can_play(const dkpos *p, int card)
{
    if (p->phase != DKP_PLAY) return 0;
    const tk4_table *t = &p->t;
    int pl = tk4_turn(t);
    if (!tk4_holds(t, pl, card)) return 0;
    if (!t->ntrick) return 1;
    int led = eff_suit(t->trick[0], p->game);
    if (eff_suit(card, p->game) == led) return 1;
    for (int i = 0; i < t->n[pl]; i++)
        if (eff_suit(t->hand[pl][i], p->game) == led) return 0;
    return 1;
}

int dko_legal(const dkpos *p, int act)
{
    int a = DKA_ARG(act);
    switch (DKA_KIND(act)) {
    case DKA_SAY: return dko_say_ok(p, a);
    case DKA_ANNOUNCE: return dko_announce_ok(p) && a == p->ann[dko_party(p, p->turn)] + 1;
    case DKA_PLAY: return dko_can_play(p, a);
    case DKA_NEXT: return p->phase == DKP_DEAL && !a;
    default: return 0;
    }
}

/* ---- the deal's end ---------------------------------------------------------------------------------------------- */
/* a party has made its game: its announcement kept (the other under 90, 60, 30, without a trick), else its points */
static int reached(const dkdeal *r, int k)
{
    static const int16_t under[3] = {90, 60, 30};
    int o = 1 - k, only_kontra = r->ann[1] && !r->ann[0];
    if (r->ann[k] == DKL_BLACK) return r->tricks[o] == 0;
    if (r->ann[k] >= DKL_90) return r->eyes[o] < under[r->ann[k] - DKL_90];
    return k == 0 ? r->eyes[0] >= (only_kontra ? 120 : 121) : r->eyes[1] >= (only_kontra ? 121 : 120);
}

static int game_value(const dkdeal *r)
{
    int w = r->winner, l = 1 - w, v = 1;
    v += (r->eyes[l] < 90) + (r->eyes[l] < 60) + (r->eyes[l] < 30) + (r->tricks[l] == 0);
    for (int k = 0; k < 2; k++) v += (r->ann[k] ? 2 : 0) + (r->ann[k] > DKL_PARTY ? r->ann[k] - DKL_PARTY : 0);
    v += (r->ann[l] >= DKL_90 && r->eyes[w] >= 120) + (r->ann[l] >= DKL_60 && r->eyes[w] >= 90) +
         (r->ann[l] >= DKL_30 && r->eyes[w] >= 60) + (r->ann[l] >= DKL_BLACK && r->eyes[w] >= 30);
    return v;
}

static void deal_end(dkpos *p)
{
    dkdeal *r = &p->list[p->ndeals < DKO_DEALS_MAX ? p->ndeals : DKO_DEALS_MAX - 1];
    memset(r, 0, sizeof *r);
    r->game = p->game;
    r->soloist = p->soloist;
    r->re = p->re;
    r->ann[0] = p->ann[0];
    r->ann[1] = p->ann[1];
    int nre = 0;
    for (int q = 0; q < TK4_P; q++) {
        int k = dko_party(p, q);
        nre += !k;
        r->eyes[k] = (int16_t)(r->eyes[k] + p->t.eyes[q]);
        r->tricks[k] = (uint8_t)(r->tricks[k] + p->t.won[q]);
    }
    r->solo = nre == 1;
    int re_ok = reached(r, 0), ko_ok = reached(r, 1), a0 = r->ann[0] >= DKL_90, a1 = r->ann[1] >= DKL_90;
    if (!a0 && !a1) r->winner = re_ok ? 0 : 1;
    else if (a0 && !a1) r->winner = re_ok ? 0 : 1;
    else if (!a0 && a1) r->winner = ko_ok ? 1 : 0;
    else r->winner = (int8_t)(re_ok ? 0 : ko_ok ? 1 : -1);
    r->value = (uint8_t)(r->winner >= 0 ? game_value(r) : 0);
    int v = r->winner == 0 ? r->value : r->winner == 1 ? -r->value : 0;
    if (!r->solo) {                             /* the extra points */
        for (int i = 0; i < DKO_HAND; i++) {
            const uint8_t *c = p->hist[i];
            int lead = p->hist_leader[i], wi = dko_winner(c, TK4_P, p->game), k = dko_party(p, (lead + wi) % TK4_P);
            int eyes = 0;
            for (int j = 0; j < TK4_P; j++) {
                eyes += dko_points(c[j]);
                if (c[j] == FOX && dko_party(p, (lead + j) % TK4_P) != k) r->fox[k]++;
            }
            if (eyes >= 40) r->doko[k]++;
            if (i == DKO_HAND - 1 && c[wi] == CHARLIE) r->karl[k]++;
        }
        r->alten = r->winner == 1;
        v += r->fox[0] + r->doko[0] + r->karl[0] - r->fox[1] - r->doko[1] - r->karl[1] - r->alten;
    }
    r->re_points = (int8_t)v;
    for (int q = 0; q < TK4_P; q++)
        r->score[q] = (int16_t)(dko_party(p, q) == 0 ? (r->solo ? 3 * v : v) : -v);
    for (int q = 0; q < TK4_P; q++) p->total[q] = (int16_t)(p->total[q] + r->score[q]);
    if (p->ndeals < DKO_DEALS_MAX) p->ndeals++;
    p->phase = p->ndeals >= dko_deals(p->variant) ? DKP_OVER : DKP_DEAL;
    p->turn = (uint8_t)tk4_next(tk4_next(p->dealer));   /* the next deal's first to speak */
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void begin_play(dkpos *p)
{
    int f = forehand(p), q;
    p->game = DKG_NORMAL;
    for (int i = 0; i < TK4_P && p->game == DKG_NORMAL; i++)   /* the first solo in turn */
        if (dko_is_solo(p->choice[q = (f + i) % TK4_P])) p->game = p->choice[q], p->soloist = (uint8_t)q;
    for (int i = 0; i < TK4_P && p->game == DKG_NORMAL; i++)   /* else a marriage */
        if (p->choice[q = (f + i) % TK4_P] == DKG_WEDDING) p->game = DKG_WEDDING, p->soloist = (uint8_t)q;
    if (p->game == DKG_NORMAL)
        for (q = 0; q < TK4_P; q++) {
            int k = count_of(&p->t, q, QC);
            if (k) p->re |= (uint8_t)(1 << q);
            if (k == 2) p->game = DKG_SILENT, p->soloist = (uint8_t)q;
        }
    else
        p->re = (uint8_t)(1 << p->soloist);
    p->open = p->game != DKG_WEDDING;
    for (q = 0; q < TK4_P; q++) dko_sort(p->t.hand[q], p->t.n[q], p->game);
    p->phase = DKP_PLAY;
    p->turn = (uint8_t)tk4_turn(&p->t);
}

static void apply(dkpos *p, int act)
{
    int pl = p->turn, a = DKA_ARG(act);
    switch (DKA_KIND(act)) {
    case DKA_SAY:
        p->said[pl] = a ? DKW_RESERVED : DKW_HEALTHY;
        p->choice[pl] = (uint8_t)a;
        p->turn = (uint8_t)tk4_next(pl);
        if (++p->spoke == TK4_P) begin_play(p);
        break;
    case DKA_ANNOUNCE:
        p->ann[dko_party(p, pl)] = (uint8_t)a;
        p->level[pl] = (uint8_t)a;
        break;
    case DKA_PLAY: {
        tk4_table *t = &p->t;
        int k = t->tricks;
        if (!t->ntrick) p->hist_leader[k] = t->leader;
        p->hist[k][t->ntrick] = (uint8_t)a;
        if (a == QC) p->qc[pl]++;
        tk4_lay(t, a);
        if (t->ntrick == TK4_P) {
            int w = (t->leader + dko_winner(t->trick, TK4_P, p->game)) % TK4_P, eyes = 0;
            for (int i = 0; i < TK4_P; i++) eyes += dko_points(t->trick[i]);
            tk4_take(t, w, eyes);
            if (p->game == DKG_WEDDING && !p->open) {   /* the marriage: the first trick another one takes */
                if (w != p->soloist) p->re |= (uint8_t)(1 << w), p->open = 1, p->shift = t->tricks;
                else if (t->tricks == 3) p->open = 1, p->shift = 3;   /* none in three: the bride alone */
            }
            if (t->tricks == DKO_HAND) { deal_end(p); break; }
        }
        p->turn = (uint8_t)tk4_turn(t);
        break;
    }
    case DKA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int dko_play(dkgame *g, int act)
{
    if (g->n >= DKO_MAX_ACTS || !dko_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int dko_undo(dkgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[DKO_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    dko_new(g, g->variant, g->seed);
    for (int i = 0; i < n; i++) dko_play(g, acts[i]);
    return 1;
}

int dko_winners(const dkpos *p)
{
    if (p->phase != DKP_OVER) return 0;
    int best = p->total[0], m = 0;
    for (int q = 1; q < TK4_P; q++) best = p->total[q] > best ? p->total[q] : best;
    for (int q = 0; q < TK4_P; q++) m |= (p->total[q] == best) << q;
    return m;
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
int dko_auto(const dkpos *p)
{
    const tk4_table *t = &p->t;
    int pl = p->turn;
    switch (p->phase) {
    case DKP_RESERVE: return DKA_SAY;
    case DKP_PLAY:
        for (int i = 0; i < t->n[pl]; i++)
            if (dko_can_play(p, t->hand[pl][i])) return DKA_PLAY | t->hand[pl][i];
        return 0;
    case DKP_DEAL: return DKA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int dko_script(dkgame *g, const char *s)
{
    const dkpos *p = &g->pos;
    for (;;) {
        tk4_space(&s);
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != DKP_OVER && (all || p->phase != DKP_DEAL))
                if (!dko_play(g, dko_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        if (c == '.') act = dko_auto(p);
        else if (c == 'g') act = DKA_SAY | DKG_NORMAL;
        else if (c == 'w') act = DKA_SAY | DKG_WEDDING;
        else if (c == 'q') act = DKA_SAY | DKG_QUEENS;
        else if (c == 'j') act = DKA_SAY | DKG_JACKS;
        else if (c == 'f') act = DKA_SAY | DKG_FLESHLESS;
        else if (c == 'a') act = DKA_ANNOUNCE | (p->phase == DKP_PLAY ? p->ann[dko_party(p, p->turn)] + 1 : 0);
        else if (c == 'N') act = DKA_NEXT;
        else if (c == 's') {
            int su = tk4_suit_letter(&s);
            if (su < 0) return 0;
            act = DKA_SAY | (DKG_SUIT + su);
        } else {
            s--;
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = DKA_PLAY | card;
        }
        if (!dko_play(g, act)) return 0;
    }
    return 1;
}
