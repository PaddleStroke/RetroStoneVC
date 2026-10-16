/*
 * Whist: the rules (whist.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/tricks.h"
#include "games/whist.h"

int whist_side(const whist_pos *p, int player) { return p->players == 4 ? player & 1 : player; }

int whist_honours_count(const whist_pos *p) { return p->players == 4 && p->variant != WV_SHORT; }

static int is_honour(int c, int trump)
{
    int r = trick_rank(c);
    return CARD_SUIT(c) == trump && r >= 11;
}

static void deal(whist_pos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_52, deck), P = p->players;
    cards_shuffle(deck, k, trick_mix(p->seed, 2000u + p->round));
    p->dealer = (uint8_t)(p->round % P);
    for (int pl = 0; pl < WHIST_MAXP; pl++) p->n[pl] = p->won[pl] = p->late[pl] = 0;
    int i = 0;
    for (; i < P * WHIST_HAND; i++) {
        int pl = (p->dealer + 1 + i) % P;
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    p->nstock = 0;
    if (P == 2) {
        for (; i < k; i++) p->stock[p->nstock++] = deck[i];
        p->up = p->stock[p->nstock - 1];
    } else
        p->up = deck[P * WHIST_HAND - 1];       /* the dealer's last card */
    p->trump = (uint8_t)CARD_SUIT(p->up);
    p->honours[0] = p->honours[1] = 0;
    for (int pl = 0; pl < P; pl++) {
        for (int j = 0; j < p->n[pl]; j++) p->honours[whist_side(p, pl)] += is_honour(p->hand[pl][j], p->trump);
        trick_sort(p->hand[pl], p->n[pl]);
    }
    memset(p->last, CARD_NONE, sizeof p->last);
    p->drew[0] = p->drew[1] = CARD_NONE;
    p->ntrick = p->tricks = 0;
    p->last_winner = p->last_leader = WHIST_NONE;
    p->phase = WP_PLAY;
    p->turn = p->leader = (uint8_t)((p->dealer + 1) % P);
}

void whist_new(whist_game *g, int players, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->players = (uint8_t)(players == 2 ? 2 : 4);
    g->variant = (uint8_t)(variant >= 0 && variant < WHIST_VARIANTS ? variant : WV_SHORT);
    g->seed = g->pos.seed = seed;
    g->pos.players = g->players;
    g->pos.variant = g->variant;
    g->pos.target = g->variant == WV_LONG ? 10 : 5;
    deal(&g->pos);
}

int whist_can_play(const whist_pos *p, int card)
{
    if (p->phase != WP_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    return trick_follows(h, n, p->ntrick ? CARD_SUIT(p->trick[0]) : -1, card);
}

int whist_legal(const whist_pos *p, int act)
{
    switch (WA_KIND(act)) {
    case WA_PLAY: return whist_can_play(p, WA_ARG(act));
    case WA_NEXT: return p->phase == WP_ROUND && act == WA_NEXT;
    default: return 0;
    }
}

int whist_side_tricks(const whist_pos *p, int side)
{
    if (p->players == 2) return p->late[side];
    return p->won[side] + p->won[side + 2];
}

static void round_end(whist_pos *p)
{
    int before[2] = {p->score[0], p->score[1]}, t = p->target, winner = -1;
    for (int s = 0; s < 2; s++) {
        int k = whist_side_tricks(p, s);
        p->pts[s] = (int16_t)(k > 6 ? k - 6 : 0);
        p->score[s] = (int16_t)(p->score[s] + p->pts[s]);
        if (p->score[s] >= t) winner = s;
        p->hon[s] = 0;
    }
    if (winner < 0 && whist_honours_count(p))   /* the tricks first; then the honours, not one point short */
        for (int s = 0; s < 2; s++) {
            int h = p->honours[s] == 4 ? 4 : p->honours[s] == 3 ? 2 : 0;
            if (!h || before[s] >= t - 1) continue;
            p->hon[s] = (int16_t)h;
            p->score[s] = (int16_t)(p->score[s] + h);
        }
    p->round++;
    p->phase = p->score[0] >= t || p->score[1] >= t ? WP_OVER : WP_ROUND;
    p->turn = (uint8_t)((p->round + 1) % p->players);
}

static void apply(whist_pos *p, int act)
{
    int pl = p->turn, P = p->players;
    if (WA_KIND(act) == WA_NEXT) { deal(p); return; }
    int c = WA_ARG(act);
    trick_remove(p->hand[pl], &p->n[pl], c);
    p->trick[p->ntrick++] = (uint8_t)c;
    if (p->ntrick < P) { p->turn = (uint8_t)((pl + 1) % P); return; }
    int w = (p->leader + trick_winner(p->trick, P, p->trump)) % P;
    memcpy(p->last, p->trick, (size_t)P);
    p->won[w]++;
    if (P == 2 && !p->nstock) p->late[w]++;
    p->last_leader = p->leader;
    p->last_winner = (uint8_t)w;
    p->ntrick = 0;
    p->tricks++;
    p->turn = p->leader = (uint8_t)w;
    p->drew[0] = p->drew[1] = CARD_NONE;
    if (P == 2 && p->nstock >= 2) {             /* the winner the face-up card, the loser the next */
        for (int k = 0; k < 2; k++) {
            int q = (w + k) % 2, d = p->stock[--p->nstock];
            p->drew[k] = (uint8_t)d;
            p->hand[q][p->n[q]++] = (uint8_t)d;
            trick_sort(p->hand[q], p->n[q]);
        }
        if (p->nstock) p->up = p->stock[p->nstock - 1];
    }
    if (p->n[w] == 0) round_end(p);
}

int whist_play(whist_game *g, int act)
{
    if (g->n >= WHIST_MAX_ACTS || !whist_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int whist_undo(whist_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[WHIST_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    whist_new(g, g->players, g->variant, g->seed);
    for (int i = 0; i < n; i++) whist_play(g, acts[i]);
    return 1;
}

int whist_winner(const whist_pos *p)
{
    if (p->phase != WP_OVER) return -1;
    if (p->score[0] >= p->target && p->score[1] >= p->target) return p->score[1] > p->score[0];
    return p->score[1] >= p->target;
}

int whist_auto(const whist_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case WP_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (whist_can_play(p, h[i])) return WA_PLAY | h[i];
        return 0;
    case WP_ROUND: return WA_NEXT;
    default: return 0;
    }
}

int whist_script(whist_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (g->pos.phase != WP_OVER && (all || g->pos.phase == WP_PLAY))
                if (!whist_play(g, whist_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = whist_auto(&g->pos); }
        else if (*s == 'n') { s++; act = WA_NEXT; }
        else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = WA_PLAY | c;
        }
        if (!whist_play(g, act)) return 0;
    }
    return 1;
}
