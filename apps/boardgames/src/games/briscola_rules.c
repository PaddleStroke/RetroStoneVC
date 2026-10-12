/*
 * Briscola: the rules (briscola.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/briscola.h"

int briscola_points(int card)
{
    static const uint8_t pts[11] = {0, 11, 0, 10, 0, 0, 0, 0, 2, 3, 4};
    int r = CARD_RANK(card);
    return r >= 1 && r <= IT_RE ? pts[r] : 0;
}

int briscola_strength(int card)
{
    static const uint8_t st[11] = {0, 10, 1, 9, 2, 3, 4, 5, 6, 7, 8};
    int r = CARD_RANK(card);
    return r >= 1 && r <= IT_RE ? st[r] : 0;
}

int briscola_side(const brpos *p, int player) { return p->players == 4 ? player & 1 : player; }

int briscola_trick_winner(const uint8_t *cards, int n, int trump)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        int s = CARD_SUIT(cards[i]), bs = CARD_SUIT(cards[best]);
        if (s == bs ? briscola_strength(cards[i]) > briscola_strength(cards[best]) : s == trump) best = i;
    }
    return best;
}

void briscola_new(brgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->players = (uint8_t)(players == 4 ? 4 : 2);
    g->seed = seed;
    brpos *p = &g->pos;
    p->players = g->players;
    p->seed = seed;
    p->nstock = (uint8_t)cards_pack(PACK_40, p->stock);
    cards_shuffle(p->stock, p->nstock, it_mix(seed, 77));
    for (int k = 0; k < p->players; k++)        /* three each, from player 0 (the dealer the last player) */
        for (int i = 0; i < BRISCOLA_HAND; i++) p->hand[k][p->n[k]++] = p->stock[--p->nstock];
    /* the next card turned up: it goes under the stock (its first place), the last one drawn */
    uint8_t b = p->stock[--p->nstock];
    memmove(p->stock + 1, p->stock, p->nstock);
    p->stock[0] = b;
    p->nstock++;
    p->briscola = b;
    p->trump = (uint8_t)CARD_SUIT(b);
    p->turn = p->leader = 0;
    p->last_leader = p->last_winner = BRISCOLA_NONE;
    p->phase = BR_PLAY;
}

static int in_hand(const brpos *p, int card)
{
    for (int i = 0; i < p->n[p->turn]; i++)
        if (p->hand[p->turn][i] == card) return i;
    return -1;
}

int briscola_legal(const brpos *p, int card) { return p->phase == BR_PLAY && card >= 0 && in_hand(p, card) >= 0; }

static void apply(brpos *p, int card)
{
    int pl = p->turn, i = in_hand(p, card);
    for (; i < p->n[pl] - 1; i++) p->hand[pl][i] = p->hand[pl][i + 1];
    p->n[pl]--;
    p->trick[p->ntrick++] = (uint8_t)card;
    p->ndrew = 0;
    if (p->ntrick < p->players) {
        p->turn = (uint8_t)((pl + 1) % p->players);
        return;
    }
    int w = (p->leader + briscola_trick_winner(p->trick, p->ntrick, p->trump)) % p->players, s = briscola_side(p, w);
    memcpy(p->last, p->trick, sizeof p->last);
    p->last_leader = p->leader;
    p->last_winner = (uint8_t)w;
    p->tricks++;
    for (int k = 0; k < p->ntrick; k++) {
        p->took[s][p->ntook[s]++] = p->trick[k];
        p->points[s] += briscola_points(p->trick[k]);
    }
    p->ntrick = 0;
    for (int k = 0; k < p->players && p->nstock; k++) {   /* the draw, from the winner on */
        int q = (w + k) % p->players;
        uint8_t c = p->stock[--p->nstock];
        p->hand[q][p->n[q]++] = c;
        p->drew[p->ndrew++] = c;
    }
    p->leader = p->turn = (uint8_t)w;
    if (!p->n[w]) p->phase = BR_OVER;
}

int briscola_play(brgame *g, int card)
{
    if (!briscola_legal(&g->pos, card) || g->n >= IT_CARDS) return 0;
    apply(&g->pos, card);
    g->act[g->n++] = (uint8_t)card;
    return 1;
}

int briscola_undo(brgame *g)
{
    if (!g->n) return 0;
    uint8_t acts[IT_CARDS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n);
    briscola_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) briscola_play(g, acts[i]);
    return 1;
}

int briscola_winner(const brpos *p)
{
    if (p->phase != BR_OVER) return -1;
    return p->points[0] == p->points[1] ? 2 : p->points[1] > p->points[0];
}

int briscola_auto(const brpos *p) { return p->phase == BR_PLAY && p->n[p->turn] ? p->hand[p->turn][0] : -1; }

int briscola_script(brgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int card;
        if (*s == '#') {
            s++;
            while (g->pos.phase == BR_PLAY)
                if (!briscola_play(g, briscola_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; card = briscola_auto(&g->pos); }
        else if ((card = it_parse(&s)) == CARD_NONE) return 0;
        if (!briscola_play(g, card)) return 0;
    }
    return 1;
}
