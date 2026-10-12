/*
 * Truco: the rules (truco.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/spanish.h"
#include "games/truco.h"

static int side(int pl) { return pl & 1; }
static int next_pl(const tpos *p, int pl) { return (pl + 1) % p->players; }

/* ---- the cards --------------------------------------------------------------------------------------------------- */
int truco_power(int card)
{
    int r = CARD_RANK(card), s = CARD_SUIT(card);
    if (r == 1) return s == ES_ESPADAS ? 14 : s == ES_BASTOS ? 13 : 8;
    if (r == 7) return s == ES_ESPADAS ? 12 : s == ES_OROS ? 11 : 4;
    switch (r) {
    case 3: return 10;
    case 2: return 9;
    case ES_REY: return 7;
    case ES_CABALLO: return 6;
    case ES_SOTA: return 5;
    default: return r - 3;                      /* 6 3, 5 2, 4 1 */
    }
}

static int env_card(int card) { return CARD_RANK(card) <= 7 ? CARD_RANK(card) : 0; }

int truco_envido(const uint8_t *c)
{
    int best = 0;
    for (int i = 0; i < 3; i++) {
        if (env_card(c[i]) > best) best = env_card(c[i]);
        for (int j = i + 1; j < 3; j++)
            if (CARD_SUIT(c[i]) == CARD_SUIT(c[j]) && 20 + env_card(c[i]) + env_card(c[j]) > best)
                best = 20 + env_card(c[i]) + env_card(c[j]);
    }
    return best;
}

int truco_has_flor(const uint8_t *c) { return CARD_SUIT(c[0]) == CARD_SUIT(c[1]) && CARD_SUIT(c[1]) == CARD_SUIT(c[2]); }

static int flor_value(const uint8_t *c) { return 20 + env_card(c[0]) + env_card(c[1]) + env_card(c[2]); }

int truco_falta(const tpos *p, int s)
{
    int lead = p->score[0] > p->score[1] ? p->score[0] : p->score[1];
    if (p->goal == 30 && lead < 15) return p->goal - p->score[s];   /* both in the malas: the whole game */
    return p->goal - lead;
}

/* ---- scoring ----------------------------------------------------------------------------------------------------- */
static int scores(tpos *p, int s, int pts, int kind)   /* 1: the game is over */
{
    p->score[s] = (int16_t)(p->score[s] + pts);
    p->gained[s][kind] = (int8_t)(p->gained[s][kind] + pts);
    if (p->score[s] < p->goal) return 0;
    p->phase = TP_OVER;
    p->winner = (uint8_t)s;
    p->call = 0;
    if (p->env == TE_CALLING) p->env = TE_DONE;
    return 1;
}

/* the best of these players (ties: the nearer to the mano) by value */
static int best_of(const tpos *p, const uint8_t *value, unsigned mask)
{
    int best = -1;
    for (int k = 0; k < p->players; k++) {
        int pl = (p->mano + k) % p->players;
        if (mask >> pl & 1 && (best < 0 || value[pl] > value[best])) best = pl;
    }
    return best;
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static void deal(tpos *p)
{
    uint8_t pack[48];
    int n = cards_pack(PACK_ES40, pack), k = 0;
    cards_shuffle(pack, n, es_mix(p->seed, 2000u + p->deal));
    p->dealer = (uint8_t)((p->players - 1 + p->deal) % p->players);
    p->mano = (uint8_t)next_pl(p, p->dealer);
    for (int r = 0; r < 3; r++)
        for (int q = 0; q < p->players; q++) p->dealt[(p->mano + q) % p->players][r] = pack[k++];
    memcpy(p->hand, p->dealt, sizeof p->hand);
    memset(p->played, CARD_NONE, sizeof p->played);
    memset(p->result, TR_NONE, sizeof p->result);
    memset(p->best, TR_NONE, sizeof p->best);
    memset(p->word, TW_NONE, sizeof p->word);
    memset(p->wordn, 0, sizeof p->wordn);
    memset(p->gained, 0, sizeof p->gained);
    for (int pl = 0; pl < TR_P; pl++) p->n[pl] = (uint8_t)(pl < p->players ? 3 : 0);
    p->round = p->nplayed = 0;
    p->leader = p->turn = p->mano;
    p->level = 1;
    p->owner = p->call = 0;
    p->owner = TR_NONE;
    p->caller = p->responder = TR_NONE;
    p->env = TE_OPEN;
    p->nenv = 0;
    p->env_caller = p->env_responder = p->env_winner = TR_NONE;
    p->env_pts = 0;
    for (int pl = 0; pl < p->players; pl++) p->env_value[pl] = (uint8_t)truco_envido(p->dealt[pl]);
    p->hand_side = TR_NONE;
    p->hand_pts = p->mazo = 0;
    p->phase = TP_PLAY;
    p->flor_mask = 0;
    p->flor_side = TR_NONE;
    p->flor_pts = 0;
    if (!p->flor) return;
    uint8_t fv[TR_P] = {0};
    int count = 0, sides = 0;
    for (int pl = 0; pl < p->players; pl++)
        if (truco_has_flor(p->dealt[pl])) {
            p->flor_mask |= (uint8_t)(1u << pl);
            p->word[pl] = TW_FLOR;
            fv[pl] = (uint8_t)flor_value(p->dealt[pl]);
            count++;
            sides |= 1 << side(pl);
        }
    if (!count) return;
    p->env = TE_DONE;                           /* a flor: no envido this hand */
    int s = sides == 3 ? side(best_of(p, fv, p->flor_mask)) : sides == 2;
    p->flor_side = (uint8_t)s;
    p->flor_pts = (uint8_t)(3 * count);
    scores(p, s, 3 * count, 1);
}

void truco_new(tgame *g, int players, int goal, int flor, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->pos.players = (uint8_t)(players == 4 ? 4 : 2);
    g->pos.goal = (uint8_t)(goal == 30 ? 30 : 15);
    g->pos.flor = (uint8_t)(flor != 0);
    g->pos.winner = TR_NONE;
    deal(&g->pos);
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int truco_actor(const tpos *p)
{
    if (p->env == TE_CALLING) return p->env_responder;
    if (p->call) return p->responder;
    return p->turn;
}

int truco_can_envido(const tpos *p)
{
    int a = truco_actor(p);
    if (p->phase != TP_PLAY || p->env != TE_OPEN || p->round != 0 || p->played[0][a] != CARD_NONE) return 0;
    return p->call ? p->call == 2 && a == p->responder : p->level == 1;
}

int truco_can_truco(const tpos *p)
{
    if (p->phase != TP_PLAY || p->env == TE_CALLING) return 0;
    if (p->call) return p->call < 4;
    return p->level < 4 && (p->level == 1 || p->owner == side(p->turn));
}

static int envido_raise_ok(const tpos *p, int kind)
{
    int ne = 0, real = 0, falta = 0;
    for (int i = 0; i < p->nenv; i++) ne += p->chain[i] == TE_ENVIDO, real |= p->chain[i] == TE_REAL,
                                       falta |= p->chain[i] == TE_FALTA;
    if (falta) return 0;
    if (kind == TE_ENVIDO) return ne < 2 && !real;
    if (kind == TE_REAL) return !real;
    return 1;
}

int truco_legal(const tpos *p, int act)
{
    int a = TA_ARG(act), kind = TA_KIND(act);
    if (p->phase == TP_HAND) return kind == TA_NEXT && !a;
    if (p->phase != TP_PLAY) return 0;
    switch (kind) {
    case TA_PLAY:
        if (p->env == TE_CALLING || p->call) return 0;
        for (int i = 0; i < p->n[p->turn]; i++)
            if (p->hand[p->turn][i] == a) return 1;
        return 0;
    case TA_TRUCO: return !a && truco_can_truco(p);
    case TA_ENVIDO:
    case TA_REAL:
    case TA_FALTA: {
        int k = kind == TA_ENVIDO ? TE_ENVIDO : kind == TA_REAL ? TE_REAL : TE_FALTA;
        if (a) return 0;
        return p->env == TE_CALLING ? envido_raise_ok(p, k) : truco_can_envido(p);
    }
    case TA_QUIERO:
    case TA_NOQUIERO: return !a && (p->env == TE_CALLING || p->call);
    case TA_MAZO: return !a && p->env != TE_CALLING && !p->call;
    default: return 0;
    }
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void hand_end(tpos *p, int s, int pts)
{
    p->hand_side = (uint8_t)s;
    p->hand_pts = (uint8_t)pts;
    p->call = 0;
    p->phase = TP_HAND;
    p->turn = (uint8_t)next_pl(p, p->mano);    /* the next hand's mano */
    scores(p, s, pts, 2);
}

/* the hand's winner after the rounds played (-1: not decided yet) */
static int decided(const tpos *p)
{
    const uint8_t *r = p->result;
    int wins[2] = {0, 0};
    for (int i = 0; i <= p->round; i++)
        if (r[i] < 2) wins[r[i]]++;
    if (wins[0] == 2 || wins[1] == 2) return wins[1] == 2;
    if (p->round == 0) return -1;
    if (p->round == 1) {
        if (r[0] == TR_PARDA && r[1] != TR_PARDA) return r[1];
        if (r[0] != TR_PARDA && r[1] == TR_PARDA) return r[0];
        return -1;
    }
    if (r[0] == TR_PARDA && r[1] == TR_PARDA) return r[2] != TR_PARDA ? r[2] : side(p->mano);
    return r[2] == TR_PARDA ? r[0] : r[2];
}

static void round_end(tpos *p)
{
    int k = p->round, top = 0, best = -1, sides = 0;
    for (int i = 0; i < p->players; i++) {
        int pl = (p->leader + i) % p->players, w = truco_power(p->played[k][pl]);
        if (w > top) top = w, best = pl, sides = 0;
        if (w == top) sides |= 1 << side(pl);
    }
    p->result[k] = (uint8_t)(sides == 3 ? TR_PARDA : side(best));
    p->best[k] = (uint8_t)best;
    int w = decided(p);
    if (w >= 0) { hand_end(p, w, p->level); return; }
    p->round++;
    p->nplayed = 0;
    if (p->result[k] != TR_PARDA) p->leader = (uint8_t)best;
    p->turn = p->leader;
}

static void envido_ends(tpos *p, int accepted)
{
    int sum = 0, pts;
    for (int i = 0; i < p->nenv - (accepted ? 0 : 1); i++) sum += p->chain[i] == TE_REAL ? 3 : 2;
    p->env = TE_DONE;
    if (!accepted) {
        pts = p->nenv == 1 ? 1 : sum;
        scores(p, p->env_caller, pts, 0);
        p->env_pts = (uint8_t)pts;
        return;
    }
    unsigned all = (1u << p->players) - 1;
    int w = best_of(p, p->env_value, all), s = side(w);
    pts = p->chain[p->nenv - 1] == TE_FALTA ? truco_falta(p, s) : sum;
    p->env_winner = (uint8_t)w;
    p->env_pts = (uint8_t)pts;
    scores(p, s, pts, 0);
}

static void apply(tpos *p, int act)
{
    int a = TA_ARG(act), pl = truco_actor(p);
    switch (TA_KIND(act)) {
    case TA_PLAY: {
        int i = 0;
        while (p->hand[pl][i] != a) i++;
        for (; i < p->n[pl] - 1; i++) p->hand[pl][i] = p->hand[pl][i + 1];
        p->n[pl]--;
        p->played[p->round][pl] = (uint8_t)a;
        p->nplayed++;
        p->turn = (uint8_t)next_pl(p, pl);
        if (p->nplayed == p->players) round_end(p);
        break;
    }
    case TA_TRUCO:
        p->word[pl] = TW_TRUCO;
        if (p->call) {                          /* a raise: the call before it is accepted */
            p->level = p->call;
            p->owner = (uint8_t)side(pl);
            p->call++;
        } else
            p->call = (uint8_t)(p->level + 1);
        p->caller = (uint8_t)side(pl);
        p->wordn[pl] = p->call;
        p->responder = (uint8_t)next_pl(p, pl);
        break;
    case TA_ENVIDO:
    case TA_REAL:
    case TA_FALTA: {
        int k = TA_KIND(act) == TA_ENVIDO ? TE_ENVIDO : TA_KIND(act) == TA_REAL ? TE_REAL : TE_FALTA;
        p->word[pl] = (uint8_t)(TW_ENVIDO + k - TE_ENVIDO);
        if (p->env != TE_CALLING) p->nenv = 0;
        p->chain[p->nenv++] = (uint8_t)k;
        p->env = TE_CALLING;
        p->env_caller = (uint8_t)side(pl);
        p->env_responder = (uint8_t)next_pl(p, pl);
        break;
    }
    case TA_QUIERO:
        p->word[pl] = TW_QUIERO;
        if (p->env == TE_CALLING) { envido_ends(p, 1); break; }
        p->level = p->call;
        p->owner = (uint8_t)side(pl);
        p->call = 0;
        break;
    case TA_NOQUIERO:
        p->word[pl] = TW_NOQUIERO;
        if (p->env == TE_CALLING) { envido_ends(p, 0); break; }
        hand_end(p, p->caller, p->level);
        break;
    case TA_MAZO:
        p->word[pl] = TW_MAZO;
        p->mazo = (uint8_t)(pl + 1);
        if (p->env == TE_OPEN) p->env = TE_DONE;
        hand_end(p, !side(pl), p->level);
        break;
    case TA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int truco_play(tgame *g, int act)
{
    if (g->n >= TR_MAX_ACTS || !truco_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int truco_undo(tgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[TR_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    tpos q = g->pos;
    truco_new(g, q.players, q.goal, q.flor, g->seed);
    for (int i = 0; i < n; i++) truco_play(g, acts[i]);
    return 1;
}

int truco_winner(const tpos *p) { return p->phase == TP_OVER ? p->winner : -1; }

int truco_actions(const tpos *p, int *out)
{
    int n = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (truco_legal(p, TA_PLAY | p->hand[p->turn][i])) out[n++] = TA_PLAY | p->hand[p->turn][i];
    for (int k = 2; k <= 9; k++)
        if (truco_legal(p, k << 8)) out[n++] = k << 8;
    return n;
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
int truco_auto(const tpos *p)
{
    int pl = truco_actor(p);
    if (p->phase == TP_HAND) return TA_NEXT;
    if (p->phase != TP_PLAY) return 0;
    if (p->env == TE_CALLING) return p->env_value[pl] >= 27 ? TA_QUIERO : TA_NOQUIERO;
    if (p->call) {
        int s = 0;
        for (int i = 0; i < p->n[pl]; i++) s += truco_power(p->hand[pl][i]);
        return s >= 7 * p->n[pl] ? TA_QUIERO : TA_NOQUIERO;
    }
    /* the lowest card that beats the table, else the lowest (leading: the highest) */
    int top = 0, lo = -1, hi = -1, beat = -1;
    for (int i = 0; i < p->players; i++)
        if (p->played[p->round][i] != CARD_NONE && truco_power(p->played[p->round][i]) > top)
            top = truco_power(p->played[p->round][i]);
    for (int i = 0; i < p->n[pl]; i++) {
        int w = truco_power(p->hand[pl][i]);
        if (lo < 0 || w < truco_power(p->hand[pl][lo])) lo = i;
        if (hi < 0 || w > truco_power(p->hand[pl][hi])) hi = i;
        if (w > top && (beat < 0 || w < truco_power(p->hand[pl][beat]))) beat = i;
    }
    int i = !p->nplayed ? hi : beat >= 0 ? beat : lo;
    return TA_PLAY | p->hand[pl][i];
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int truco_script(tgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const tpos *p = &g->pos;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != TP_OVER && (all || p->phase != TP_HAND))
                if (!truco_play(g, truco_auto(p))) return 0;
            continue;
        }
        const char *t = s;
        int card = es_parse(&t);
        if (card != CARD_NONE) {
            s = t;
            act = TA_PLAY | card;
        } else
            switch (*s++) {
            case '.': act = truco_auto(p); break;
            case 't': act = TA_TRUCO; break;
            case 'e': act = TA_ENVIDO; break;
            case 'r': act = TA_REAL; break;
            case 'f': act = TA_FALTA; break;
            case 'q': act = TA_QUIERO; break;
            case 'n': act = TA_NOQUIERO; break;
            case 'm': act = TA_MAZO; break;
            case 'N': act = TA_NEXT; break;
            default: return 0;
            }
        if (!truco_play(g, act)) return 0;
    }
    return 1;
}
