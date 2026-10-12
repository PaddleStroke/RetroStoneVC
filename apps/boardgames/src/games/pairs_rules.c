/*
 * Pairs: the rules (pairs.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "cards.h"
#include "games/pairs.h"

int pr_cards(int variant) { return variant == PR_SMALL ? 12 : variant == PR_LARGE ? 36 : 24; }

/* the pieces: king, knight and rook in ivory, red lacquer and amber; queen, bishop and pawn in ebony, jade and lapis
 * (the lapis pawn left out); chess.h's PAWN 1 .. KING 6 */
void pr_piece(int picture, int *type, int *colour)
{
    static const uint8_t T[PR_PIECES][2] = {
        {6, 0}, {2, 0}, {4, 0}, {5, 1}, {3, 1}, {1, 1}, {6, 2}, {2, 2}, {4, 2},
        {5, 3}, {3, 3}, {1, 3}, {6, 4}, {2, 4}, {4, 4}, {5, 5}, {3, 5},
    };
    int k = picture - PR_ANIMALS;
    k = k < 0 ? 0 : k >= PR_PIECES ? PR_PIECES - 1 : k;
    *type = T[k][0];
    *colour = T[k][1];
}

void pr_new(prgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    prpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant >= 0 && variant < PR_VARIANTS ? variant : PR_MEDIUM);
    g->players = p->players = (uint8_t)(players < 1 ? 1 : players > PR_MAXP ? PR_MAXP : players);
    p->n = (uint8_t)pr_cards(p->variant);
    p->cols = p->variant == PR_SMALL ? 3 : p->variant == PR_MEDIUM ? 4 : 6;
    p->rows = (uint8_t)(p->n / p->cols);
    /* the pictures: the animals first, then the pieces, each lot in an order of the seed */
    uint8_t animals[PR_ANIMALS], pieces[PR_PIECES], deck[PR_MAX];
    for (int i = 0; i < PR_ANIMALS; i++) animals[i] = (uint8_t)i;
    for (int i = 0; i < PR_PIECES; i++) pieces[i] = (uint8_t)(PR_ANIMALS + i);
    cards_shuffle(animals, PR_ANIMALS, seed ^ 0x5a5a5a5au);
    cards_shuffle(pieces, PR_PIECES, seed ^ 0xa5a5a5a5u);
    int want = p->n / 2;
    for (int i = 0; i < want; i++) {
        int pic = i < PR_ANIMALS ? animals[i] : pieces[i - PR_ANIMALS];
        deck[2 * i] = deck[2 * i + 1] = (uint8_t)pic;
    }
    cards_shuffle(deck, p->n, seed);
    memcpy(p->pic, deck, p->n);
    memset(p->found, PR_NONE, sizeof p->found);
    p->up[0] = p->up[1] = -1;
    p->phase = PP_PLAY;
}

int pr_left(const prpos *p)
{
    int k = 0;
    for (int i = 0; i < p->n; i++) k += p->found[i] == PR_NONE;
    return k;
}

int pr_legal(const prpos *p, int act)
{
    int k = PA_KIND(act), c = PA_CARD(act);
    if (p->phase == PP_SHOW) return k == PA_BACK;
    if (p->phase != PP_PLAY || k != PA_TURN) return 0;
    return c < p->n && p->found[c] == PR_NONE && c != p->up[0];
}

static void apply(prpos *p, int act)
{
    int c = PA_CARD(act);
    if (PA_KIND(act) == PA_BACK) {
        p->up[0] = p->up[1] = -1;
        p->turn = (uint8_t)((p->turn + 1) % p->players);
        p->phase = PP_PLAY;
        return;
    }
    p->seen[c] = 1;
    if (p->up[0] < 0) { p->up[0] = (int8_t)c; return; }
    p->up[1] = (int8_t)c;
    p->moves++;
    int a = p->up[0];
    if (p->pic[a] != p->pic[c]) { p->phase = PP_SHOW; return; }
    p->found[a] = p->found[c] = p->turn;        /* a pair: kept, and the same player again */
    p->pairs[p->turn]++;
    p->up[0] = p->up[1] = -1;
    if (!pr_left(p)) p->phase = PP_OVER;
}

int pr_play(prgame *g, int act)
{
    if (g->n >= PR_MAX_ACTS || !pr_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint8_t)act;
    return 1;
}

int pr_undo(prgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    uint8_t acts[PR_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n);
    pr_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) pr_play(g, acts[i]);
    return 1;
}

unsigned pr_winners(const prpos *p)
{
    if (p->phase != PP_OVER) return 0;
    int best = 0;
    for (int q = 0; q < p->players; q++) best = p->pairs[q] > best ? p->pairs[q] : best;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->pairs[q] == best) << q;
    return m;
}

/* a player who remembers every card seen: a pair known, else a card never seen (and its match, if seen) */
int pr_auto(const prpos *p)
{
    if (p->phase == PP_SHOW) return PA_BACK;
    if (p->phase != PP_PLAY) return 0;
    int first = p->up[0];
    if (first >= 0) {
        for (int i = 0; i < p->n; i++)
            if (i != first && p->found[i] == PR_NONE && p->seen[i] && p->pic[i] == p->pic[first]) return PA_TURN | i;
    } else {
        for (int i = 0; i < p->n; i++)
            for (int j = i + 1; j < p->n; j++)
                if (p->found[i] == PR_NONE && p->found[j] == PR_NONE && p->seen[i] && p->seen[j] &&
                    p->pic[i] == p->pic[j])
                    return PA_TURN | i;
    }
    for (int i = 0; i < p->n; i++)
        if (p->found[i] == PR_NONE && !p->seen[i] && i != first) return PA_TURN | i;
    for (int i = 0; i < p->n; i++)
        if (p->found[i] == PR_NONE && i != first) return PA_TURN | i;
    return 0;
}

int pr_script(prgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int n = 0, digits = 0;
        while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0'), digits++;
        int act;
        if (c == 'a' || c == '#') {
            do {
                if (g->pos.phase == PP_OVER) break;
                if (!pr_play(g, pr_auto(&g->pos))) return 0;
            } while (c == '#');
            continue;
        }
        if (c == 't' && digits && n < PR_MAX) act = PA_TURN | n;
        else if (c == 'h') act = PA_BACK;
        else return 0;
        if (!pr_play(g, act)) return 0;
    }
    return 1;
}
