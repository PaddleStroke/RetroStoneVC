/*
 * Manille: the rules (manille.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/manille.h"
#include "games/tricks.h"

int man_rank(int c)
{
    static const int8_t r[14] = {0, 7, 0, 0, 0, 0, 0, 1, 2, 3, 8, 4, 5, 6};   /* by rank: ace .. king */
    int k = CARD_RANK(c);
    return k <= 13 ? r[k] : 0;
}

int man_points(int c)
{
    int r = CARD_RANK(c);
    return r == 10 ? 5 : r == RANK_ACE ? 4 : r == RANK_KING ? 3 : r == RANK_QUEEN ? 2 : r == RANK_JACK ? 1 : 0;
}

static int sort_key(int c, int trump)
{
    static const int8_t order[4] = {1, 2, 4, 3};   /* spades, hearts, clubs, diamonds: the colours alternate */
    int s = CARD_SUIT(c), k = s == trump ? 0 : order[s & 3];
    return k * 16 + (15 - man_rank(c));
}

void man_sort(uint8_t *h, int n, int trump)
{
    for (int i = 1; i < n; i++) {
        uint8_t c = h[i];
        int j = i;
        while (j > 0 && sort_key(h[j - 1], trump) > sort_key(c, trump)) { h[j] = h[j - 1]; j--; }
        h[j] = c;
    }
}

static void deal(mpos *p)
{
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_32, pack);
    cards_shuffle(pack, n, trick_mix(p->seed, 100u + p->deal));
    int k = 0;
    memset(p->n, 0, sizeof p->n);
    for (int round = 0; round < 2; round++)     /* packets of 4 from the player after the dealer; the dealer last */
        for (int i = 1; i <= MAN_P; i++) {
            int q = (p->dealer + i) % MAN_P;
            for (int j = 0; j < 4; j++) p->hand[q][p->n[q]++] = pack[k++];
        }
    p->turned = pack[n - 1];                    /* the dealer's last card */
    p->trump = (uint8_t)CARD_SUIT(p->turned);
    for (int q = 0; q < MAN_P; q++) man_sort(p->hand[q], p->n[q], p->trump);
    p->phase = MP_SPEAK;
    p->mult = 1;
    p->doubler = MAN_NONE;
    p->need = 0xf;
    memset(p->said, 0, sizeof p->said);
    p->turn = p->leader = (uint8_t)((p->dealer + 1) % MAN_P);
    p->ntrick = p->tricks = 0;
    p->last_winner = p->last_leader = MAN_NONE;
    p->won[0] = p->won[1] = 0;
    p->pts[0] = p->pts[1] = 0;
}

void man_new(mgame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    mpos *p = &g->pos;
    g->seed = p->seed = seed;
    p->dealer = MAN_P - 1;
    deal(p);
}

/* the card that holds the trick: the highest trump, else the highest of the suit led */
int man_best(const mpos *p)
{
    if (!p->ntrick) return -1;
    int best = 0;
    for (int i = 1; i < p->ntrick; i++) {
        int s = CARD_SUIT(p->trick[i]), bs = CARD_SUIT(p->trick[best]);
        if (s == bs ? man_rank(p->trick[i]) > man_rank(p->trick[best]) : s == p->trump) best = i;
    }
    return best;
}

int man_can_play(const mpos *p, int c)
{
    if (p->phase != MP_PLAY) return 0;
    int pl = p->turn, n = p->n[pl];
    const uint8_t *h = p->hand[pl];
    if (trick_find(h, n, c) < 0) return 0;
    if (!p->ntrick) return 1;
    int led = CARD_SUIT(p->trick[0]), b = man_best(p), bc = p->trick[b], holder = (p->leader + b) % MAN_P;
    int partner = man_team(holder) == man_team(pl), s = CARD_SUIT(c);
    if (trick_has_suit(h, n, led)) {
        if (s != led) return 0;
        if (partner || CARD_SUIT(bc) != led) return 1;   /* the partner holds it, or it is trumped: any of the suit */
        int over = 0;                           /* monter: beat the opponent's card if you can */
        for (int i = 0; i < n; i++) over |= CARD_SUIT(h[i]) == led && man_rank(h[i]) > man_rank(bc);
        return !over || man_rank(c) > man_rank(bc);
    }
    if (partner || !trick_has_suit(h, n, p->trump)) return 1;
    if (CARD_SUIT(bc) != p->trump) return s == p->trump;   /* trump */
    int over = 0;                               /* over-trump if you can, else anything */
    for (int i = 0; i < n; i++) over |= CARD_SUIT(h[i]) == p->trump && man_rank(h[i]) > man_rank(bc);
    return !over || (s == p->trump && man_rank(c) > man_rank(bc));
}

int man_legal(const mpos *p, int a)
{
    int k = MA_KIND(a);
    switch (p->phase) {
    case MP_SPEAK:
        if (a == MA_PASS) return 1;
        if (a == MA_DOUBLE) return p->mult == 1;
        if (a == MA_REDOUBLE) return p->mult == 2 && man_team(p->turn) != p->doubler;
        return 0;
    case MP_PLAY: return k == MA_PLAY && man_can_play(p, MA_ARG(a));
    case MP_DEAL: return a == MA_NEXT;
    }
    return 0;
}

static void next_speaker(mpos *p)
{
    for (int i = 1; i <= MAN_P; i++) {
        int q = (p->turn + i) % MAN_P;
        if (p->need >> q & 1) { p->turn = (uint8_t)q; return; }
    }
    p->phase = MP_PLAY;                         /* everyone has spoken: the play */
    p->turn = p->leader;
}

static void score_deal(mpos *p)
{
    for (int t = 0; t < 2; t++) p->made[t] = (int16_t)(p->pts[t] + p->won[t]);
    for (int t = 0; t < 2; t++) {
        int over = p->made[t] - MAN_HALF;
        p->score[t] = (int16_t)(over > 0 ? over * p->mult : 0);
        p->total[t] = (int16_t)(p->total[t] + p->score[t]);
    }
    p->deal++;
    p->phase = p->total[0] >= MAN_TARGET || p->total[1] >= MAN_TARGET ? MP_OVER : MP_DEAL;
}

static void apply(mpos *p, int a)
{
    int pl = p->turn;
    switch (MA_KIND(a)) {
    case MA_PASS:
        p->said[pl] = MS_PASS;
        p->need &= (uint8_t)~(1u << pl);
        next_speaker(p);
        break;
    case MA_DOUBLE:
        p->said[pl] = MS_DOUBLE;
        p->mult = 2;
        p->doubler = (uint8_t)man_team(pl);
        p->need = (uint8_t)(1u << ((pl + 1) % MAN_P) | 1u << ((pl + 3) % MAN_P));   /* the opponents */
        next_speaker(p);
        break;
    case MA_REDOUBLE:
        p->said[pl] = MS_REDOUBLE;
        p->mult = 4;
        p->need = 0;
        next_speaker(p);
        break;
    case MA_PLAY: {
        int c = MA_ARG(a);
        trick_remove(p->hand[pl], &p->n[pl], c);
        p->trick[p->ntrick++] = (uint8_t)c;
        if (p->ntrick < MAN_P) { p->turn = (uint8_t)((pl + 1) % MAN_P); break; }
        int w = (p->leader + man_best(p)) % MAN_P, t = man_team(w);
        for (int i = 0; i < MAN_P; i++) p->pts[t] = (int16_t)(p->pts[t] + man_points(p->trick[i]));
        p->won[t]++;
        memcpy(p->last, p->trick, sizeof p->last);
        p->last_leader = p->leader;
        p->last_winner = (uint8_t)w;
        p->ntrick = 0;
        p->tricks++;
        p->leader = p->turn = (uint8_t)w;
        if (p->tricks == MAN_HAND) score_deal(p);
        break;
    }
    case MA_NEXT:
        p->dealer = (uint8_t)((p->dealer + 1) % MAN_P);
        deal(p);
        break;
    }
}

int man_play(mgame *g, int a)
{
    if (g->n >= MAN_MAX_ACTS || !man_legal(&g->pos, a)) return 0;
    apply(&g->pos, a);
    g->act[g->n++] = (uint16_t)a;
    return 1;
}

int man_undo(mgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    uint16_t acts[MAN_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    man_new(g, g->seed);
    for (int i = 0; i < n; i++) man_play(g, acts[i]);
    return 1;
}

int man_winner_team(const mpos *p)
{
    if (p->phase != MP_OVER) return -1;
    return p->total[1] > p->total[0];
}

int man_auto(const mpos *p)
{
    switch (p->phase) {
    case MP_SPEAK: return MA_PASS;
    case MP_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (man_can_play(p, p->hand[p->turn][i])) return MA_PLAY | p->hand[p->turn][i];
        return 0;
    case MP_DEAL: return MA_NEXT;
    }
    return 0;
}

int man_script(mgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int a;
        if (*s == '#' || *s == '!') {
            int whole = *s++ == '!';
            for (int k = 0; k < 2000; k++) {
                int ph = g->pos.phase;
                if (ph == MP_OVER || (!whole && ph == MP_DEAL)) break;
                if (!man_play(g, man_auto(&g->pos))) return 0;
            }
            continue;
        }
        switch (*s) {
        case 'p': a = MA_PASS; s++; break;
        case 'd': a = MA_DOUBLE; s++; break;
        case 'r': a = MA_REDOUBLE; s++; break;
        case 'N': a = MA_NEXT; s++; break;
        case '.': a = man_auto(&g->pos); s++; break;
        default: {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            a = MA_PLAY | c;
        }
        }
        if (!man_play(g, a)) return 0;
    }
    return 1;
}
