/*
 * Happy Families' bot (bots.h): it asks for the cards it is likeliest to get, from what it has heard at the table.
 * Like a player, it knows its own hand and every question asked so far with its answer (who asked whom for which
 * card: given, or "pioche"), the cards it drew itself, how many cards each player holds and the families laid down.
 * From those it tracks, for every card it lacks: who is known to hold it (they were given it, or drew it on asking),
 * who is known not to (asked for it in vain, and has not drawn since), and which families each player has shown an
 * interest in (asking for one shows you hold one). The others' hands and the stock it never sees.
 *   easy:   remembers only the last few questions; asks for a card of the family it holds most of, of anyone.
 *   medium: remembers everything; asks a player known to hold a card it needs, else for a card of the family it
 *           holds most of, of anyone not known to lack it.
 *   hard:   the same memory, and reads it: a card known to be somewhere first (in the family it holds most), else
 *           the family it holds most, asked of a player who once asked for that family (they hold some of it): at a
 *           table of four, it wins about half the games against three medium bots.
 * Runs on the worker thread: it copies the game at entry and touches nothing else (bots.h). No SDL.
 * The table's past is the list of questions (the game keeps no other record): the bot replays it from the deal and
 * keeps only what every player saw happen (the answers) and its own draws; nothing else of the replay is read.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation: "a<p><f><m>", ask player p for family f's member m; played by families_show().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/families.h"

#define NCARD (FAM_FAMILIES * FAM_MEMBERS)
#define IDX(c) (FAM_FAMILY(c) * FAM_MEMBERS + FAM_MEMBER(c))

typedef struct {
    int8_t holder[NCARD];                       /* the player known to hold the card (-1: not known) */
    uint8_t lacks[NCARD];                       /* bit q: q is known not to hold it */
    uint8_t keen[FAM_MAXP];                     /* bit f: q asked for family f (so holds one) */
} knowledge;

/* what `seat` knows of the table in g: the questions and answers since the deal (the last `memory` of them; <0 all),
 * replayed from the deal for their answers alone */
static void listen(const fgame *g, int seat, int memory, knowledge *k)
{
    memset(k->holder, -1, sizeof k->holder);
    memset(k->lacks, 0, sizeof k->lacks);
    memset(k->keen, 0, sizeof k->keen);
    fgame *r = malloc(sizeof *r);
    if (!r) return;
    fam_new(r, g->players, g->seed);
    int from_act = memory < 0 || g->n <= memory ? 0 : g->n - memory;
    for (int i = 0; i < g->n; i++) {
        const fpos *p = &r->pos;
        int a = g->act[i], asker = p->turn, to = FAM_ASK_PLAYER(a), card = FAM_ASK_CARD(a), was[FAM_MAXP];
        for (int q = 0; q < p->players; q++) was[q] = p->n[q];
        if (!fam_play(r, a)) break;
        if (i < from_act) continue;
        /* what everyone saw: the question, the answer; the asker's cards counted (a draw shows as one more) */
        int f = FAM_FAMILY(card), c = IDX(card);
        k->keen[asker] |= (uint8_t)(1u << f);
        if (p->answer == FAM_GIVEN || p->answer == FAM_LUCKY) {
            k->holder[c] = (int8_t)asker;
            k->lacks[c] = 0;
        } else {
            k->lacks[c] |= (uint8_t)(1u << to);
            if (k->holder[c] == to) k->holder[c] = -1;
            for (int x = 0; x < NCARD; x++) k->lacks[x] &= (uint8_t)~(1u << asker);   /* it drew: anything now */
            if (asker == seat && p->drawn != FAM_NONE) k->holder[IDX(p->drawn)] = (int8_t)seat;
        }
        for (int q = 0; q < p->players; q++)    /* a turn begun with no cards: a card drawn (unseen) */
            if (q != asker && p->n[q] > was[q])
                for (int x = 0; x < NCARD; x++) k->lacks[x] &= (uint8_t)~(1u << q);
    }
    for (int c = 0; c < NCARD; c++)             /* the families laid down: those cards are no one's now */
        if (r->pos.owner[c / FAM_MEMBERS] != FAM_NONE) k->holder[c] = -1;
    free(r);
}

/* the question for the seat to move in p (its view: the others' hands and the stock read only as counts) */
int famb_ask(const fgame *g, int level, uint64_t seed)
{
    const fpos *p = &g->pos;
    if (p->phase != FP_ASK) return -1;
    int me = p->turn;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 131 + (uint64_t)me));
    level = level <= AI_EASY ? AI_EASY : level >= AI_HARD ? AI_HARD : AI_MEDIUM;
    knowledge k;
    listen(g, me, level == AI_EASY ? 2 * p->players : -1, &k);
    int count[FAM_FAMILIES] = {0};
    for (int i = 0; i < p->n[me]; i++) count[FAM_FAMILY(p->hand[me][i])]++;
    int focus = -1;                             /* the family it holds most of (the first of those) */
    for (int f = 0; f < FAM_FAMILIES; f++)
        if (count[f] && (focus < 0 || count[f] > count[focus])) focus = f;
    int best = -1, ties = 0;
    double top = -1;
    for (int f = 0; f < FAM_FAMILIES; f++) {
        if (!count[f]) continue;
        for (int m = 0; m < FAM_MEMBERS; m++) {
            int card = FAM_CARD(f, m), c = IDX(card);
            if (fam_holds(p, me, card)) continue;
            for (int q = 0; q < p->players; q++) {
                int a = FAM_ASK(q, card);
                if (!fam_legal(p, a)) continue;
                double v;
                if (k.holder[c] >= 0) v = k.holder[c] == q ? 1000 + count[f] : 0;   /* sure: the fullest family */
                else if (k.lacks[c] >> q & 1) v = 0;
                else if (level == AI_EASY) v = 1 + count[f];
                else if (level == AI_MEDIUM) v = 1;
                else                            /* hard: the family it holds most, from a player who asked for that
                                                 * family (so holds some of it: the strongest hint at the table) */
                    v = (f == focus) * 10 + (k.keen[q] >> f & 1 ? 9 : 3);
                if (level == AI_EASY && v < 1000) v = 1 + count[f];
                if (v > top + 1e-9) top = v, best = a, ties = 1;
                else if (v > top - 1e-9 && ai_rand_n(&r, ++ties) == 0) best = a;
            }
        }
    }
    if (level == AI_MEDIUM && top < 1000 && best >= 0) {   /* nothing known for sure: the family held most */
        int alt[FAM_MEMBERS * FAM_MAXP], na = 0;
        for (int m = 0; m < FAM_MEMBERS; m++)
            for (int q = 0; q < p->players; q++) {
                int card = FAM_CARD(focus, m), a = FAM_ASK(q, card);
                if (fam_legal(p, a) && !(k.lacks[IDX(card)] >> q & 1)) alt[na++] = a;
            }
        if (na) best = alt[ai_rand_n(&r, na)];
    }
    return best;
}

static int think(int seat, int level, char *move, int cap)
{
    fgame g = *families_live();                 /* the copy, at entry */
    if (g.pos.phase != FP_ASK || g.pos.turn != seat || cap < 5) return 0;
    for (int q = 0; q < g.pos.players; q++)     /* the others' hands: their counts only (the replay deals anew) */
        if (q != seat) memset(g.pos.hand[q], FAM_NONE, sizeof g.pos.hand[q]);
    memset(g.pos.stock, FAM_NONE, sizeof g.pos.stock);
    int a = famb_ask(&g, level, ai_session_seed());
    if (a < 0 || ai_cancelled()) return 0;
    int card = FAM_ASK_CARD(a);
    snprintf(move, (size_t)cap, "a%d%d%d", FAM_ASK_PLAYER(a), FAM_FAMILY(card), FAM_MEMBER(card));
    return 1;
}

static const bot families_bot = {.think = think, .play = families_show};
REGISTER_BOT(families, &families_bot)
