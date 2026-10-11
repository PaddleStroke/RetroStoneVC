/*
 * Word Tiles: the rules' test (the letter sets, the premium squares, the bag from a seed, where tiles may go, the
 * scores with their premiums and cross words, the challenge, the exchange, the end and its leftover tiles, undo,
 * the notation; whole random games from seeds, the tiles counted after every action). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "cards.h"
#include "games/wordtiles.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static wgame G, H;
static wpos *P = &G.pos;

/* the English set's tile for a letter */
static int en(char c)
{
    const wset *s = wt_set(0);
    for (int i = 0; i < s->n; i++)
        if (s->l[i].cp == (uint32_t)c) return i + 1;
    return 0;
}

/* a player's rack set by hand (English letters, '?' a blank, '.' an empty slot); the tiles it takes come out of
 * the bag (so they are counted once), the rack's old tiles go back in */
static void rack(int pl, const char *letters)
{
    for (int i = 0; i < WT_RACK; i++)
        if (P->rack[pl][i]) P->bag[P->nbag++] = P->rack[pl][i], P->rack[pl][i] = 0;
    for (int i = 0; i < WT_RACK && letters[i]; i++) {
        int t = letters[i] == '?' ? WT_BLANK : letters[i] == '.' ? 0 : en(letters[i]);
        if (!t) continue;
        int found = 0;
        for (int k = P->nbag - 1; k >= 0 && !found; k--)
            if (P->bag[k] == t) {
                memmove(P->bag + k, P->bag + k + 1, (size_t)(P->nbag - k - 1));
                P->nbag--;
                found = 1;
            }
        for (int q = 0; q < P->players && !found; q++)     /* on another rack: that one gets a tile of the bag */
            for (int k = 0; k < WT_RACK && !found && q != pl; k++)
                if (P->rack[q][k] == t) P->rack[q][k] = P->bag[--P->nbag], found = 1;
        P->rack[pl][i] = (uint8_t)t;
    }
}

/* every tile of the set is somewhere: on the board, on a rack or in the bag */
static int counted(const wpos *p)
{
    int n[WT_SET_MAX + 2] = {0}, blanks = 0;
    for (int i = 0; i < WT_CELLS; i++)
        if (p->board[i]) (p->board[i] & WT_BLANK ? blanks++ : n[p->board[i]]++);
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < WT_RACK; i++)
            if (p->rack[q][i]) (p->rack[q][i] == WT_BLANK ? blanks++ : n[p->rack[q][i]]++);
    for (int i = 0; i < p->nbag; i++) (p->bag[i] == WT_BLANK ? blanks++ : n[p->bag[i]]++);
    const wset *s = wt_set(p->set);
    int ok = blanks == WT_BLANKS;
    for (int i = 0; i < s->n; i++) ok &= n[i + 1] == s->l[i].count;
    return ok;
}

static void test_sets(void)
{
    CHECK(wt_sets() == 18);
    for (int k = 0; k < wt_sets(); k++) {
        const wset *s = wt_set(k);
        int n = 0, ok = s->n > 0 && s->n <= WT_SET_MAX;
        for (int i = 0; i < s->n; i++) {
            n += s->l[i].count;
            ok &= s->l[i].count >= 1 && s->l[i].value >= 1 && s->l[i].value <= 10;
            for (int j = 0; j < i; j++) ok &= s->l[i].cp != s->l[j].cp;
        }
        CHECK(ok && n == WT_LETTERS);
    }
    CHECK(!strcmp(wt_set(wt_set_for("fr"))->lang, "fr"));
    CHECK(!strcmp(wt_set(wt_set_for("pt_BR"))->lang, "pt"));
    CHECK(!strcmp(wt_set(wt_set_for("pt_PT"))->lang, "pt"));
    CHECK(!strcmp(wt_set(wt_set_for("nb"))->lang, "nb"));
    CHECK(!strcmp(wt_set(wt_set_for("ja"))->lang, "en"));
    CHECK(!strcmp(wt_set(wt_set_for("zh_CN"))->lang, "en"));
    CHECK(!strcmp(wt_set(wt_set_for("el"))->lang, "el") && wt_set(wt_set_for("el"))->l[0].cp == 0x391);
    CHECK(wt_set_for("id") != wt_set_for("it"));
    /* French folds its accents; Spanish keeps Ñ; Turkish has both I and İ */
    const wset *fr = wt_set(wt_set_for("fr")), *es = wt_set(wt_set_for("es")), *tr = wt_set(wt_set_for("tr"));
    int acc = 0, nn = 0, i1 = 0, i2 = 0;
    for (int i = 0; i < fr->n; i++) acc |= fr->l[i].cp > 'Z';
    for (int i = 0; i < es->n; i++) nn |= es->l[i].cp == 0xD1;
    for (int i = 0; i < tr->n; i++) i1 |= tr->l[i].cp == 'I', i2 |= tr->l[i].cp == 0x130;
    CHECK(!acc && nn && i1 && i2);
}

static void test_board(void)
{
    int n[6] = {0}, sym = 1;
    for (int r = 0; r < WT_N; r++)
        for (int c = 0; c < WT_N; c++) {
            int q = wt_premium(r * WT_N + c);
            n[q]++;
            sym &= q == wt_premium(c * WT_N + r) && q == wt_premium(r * WT_N + 14 - c) &&
                   q == wt_premium((14 - r) * WT_N + c);
        }
    CHECK(sym);
    CHECK(wt_premium(WT_CENTRE) == WQ_STAR && n[WQ_STAR] == 1);
    CHECK(n[WQ_TW] == 8 && n[WQ_DW] == 12 && n[WQ_TL] == 8 && n[WQ_DL] == 24);
    CHECK(wt_premium(0) == WQ_TL && wt_premium(4) == WQ_TW && wt_premium(7) == WQ_DL);
}

static void test_deal(void)
{
    wt_new(&G, 0, 4, 42);
    wt_new(&H, 0, 4, 42);
    CHECK(!memcmp(&G.pos, &H.pos, sizeof G.pos));
    CHECK(P->nbag == WT_TILES - 28 && P->turn == 0 && P->phase == WP_PLAY && counted(P));
    for (int q = 0; q < 4; q++) CHECK(wt_rack_count(P, q) == 7);
    wt_new(&H, 0, 4, 43);
    CHECK(memcmp(H.pos.rack, G.pos.rack, sizeof G.pos.rack) != 0);
    wt_new(&G, wt_set_for("ru"), 2, 5);
    CHECK(P->nbag == WT_TILES - 14 && counted(P) && wt_char(P, P->rack[0][0] & WT_BLANK ? 1 : P->rack[0][0]) >= 0x410);
}

static void test_place(void)
{
    wt_new(&G, 0, 2, 1);
    rack(0, "QUIETSA");
    rack(1, "SATIRE?");
    CHECK(counted(P));
    wact a;
    memset(&a, 0, sizeof a);
    a.kind = WA_PLAY;
    a.n = 2;
    a.slot[0] = 0, a.slot[1] = 1;
    a.cell[0] = 0, a.cell[1] = 1;               /* not through the centre */
    CHECK(wt_eval(P, &a, NULL) == WE_CENTRE);
    a.n = 1;
    a.cell[0] = WT_CENTRE;                      /* one tile is not a word */
    CHECK(wt_eval(P, &a, NULL) == WE_ONE);
    a.n = 2;
    a.cell[1] = WT_CENTRE + 16;                 /* not in a line */
    CHECK(wt_eval(P, &a, NULL) == WE_LINE);
    a.cell[1] = WT_CENTRE + 2;                  /* a gap */
    CHECK(wt_eval(P, &a, NULL) == WE_GAP);
    a.cell[1] = WT_CENTRE;                      /* twice the same cell */
    CHECK(wt_eval(P, &a, NULL) == WE_CELL);
    a.slot[1] = 0, a.cell[1] = WT_CENTRE + 1;   /* twice the same tile */
    CHECK(wt_eval(P, &a, NULL) == WE_TILES);
    /* QUIET from d8 across: Q on a triple letter, through the star (a double word): (24 + 2 + 1 + 1 + 1) x 2 */
    CHECK(wt_script(&G, "d8>QUIET"));
    CHECK(P->score[0] == 58 && P->turn == 1 && P->last.kind == WA_PLAY && counted(P));
    CHECK(wt_rack_count(P, 0) == 7 && P->nbag == WT_TILES - 14 - 5);
    /* a tile alone, away from the others */
    a.kind = WA_PLAY, a.n = 1, a.slot[0] = 0, a.cell[0] = 0;
    CHECK(wt_eval(P, &a, NULL) == WE_ALONE);
    /* S at i8 makes QUIETS: no premium under the old tiles: 8 + 2 + 1 + 1 + 1 + 1 */
    wscore s;
    a.cell[0] = 7 * WT_N + 8;
    CHECK(wt_eval(P, &a, &s) == 14 && s.words == 1 && s.w[0].len == 6 && s.w[0].at == 7 * WT_N + 3 && !s.w[0].down);
    /* AT at e9 under UI: AT (a double letter under the A) 3, and down UA 4 and IT 2 */
    a.n = 2, a.slot[0] = 1, a.slot[1] = 2, a.cell[0] = 8 * WT_N + 4, a.cell[1] = 8 * WT_N + 5;
    CHECK(wt_eval(P, &a, &s) == 9 && s.words == 3 && s.w[0].len == 2 && !s.w[0].down && s.w[1].down);
    /* the blank: no points, a letter needed */
    a.n = 1, a.slot[0] = 6, a.cell[0] = 7 * WT_N + 8, a.letter[0] = 0;
    CHECK(wt_eval(P, &a, NULL) == WE_TILES);
    a.letter[0] = (uint8_t)en('S');
    CHECK(wt_eval(P, &a, &s) == 13);
    CHECK(wt_script(&G, "i8>?S"));
    CHECK(P->board[7 * WT_N + 8] == (WT_BLANK | en('S')) && P->score[1] == 13 && counted(P));
}

static void test_bingo(void)
{
    wt_new(&G, 0, 2, 3);
    rack(0, "RATINGS");
    CHECK(wt_script(&G, "h4vRATINGS"));         /* h4 to h10 down: RATINGS, the star under the N */
    /* h4 a triple letter: R 1 x 3; A 1; T 1; I 1; N 1 (the star: doubled); G 3; S 1 -> 11 x 2 = 22, and the bingo */
    CHECK(P->score[0] == 22 + WT_BINGO);
}

static void test_challenge(void)
{
    wt_new(&G, 0, 3, 9);
    rack(0, "QUIETSA");
    CHECK(wt_script(&G, "d8>QUIET"));
    uint8_t r1[WT_RACK];
    memcpy(r1, P->rack[1], sizeof r1);
    CHECK(wt_can_challenge(P));
    CHECK(wt_script(&G, "c"));
    /* taken back: the board empty, QUIET back on the rack (hidden), the score gone, the challenger plays on */
    int empty = 1;
    for (int i = 0; i < WT_CELLS; i++) empty &= !P->board[i];
    CHECK(empty && P->score[0] == 0 && P->turn == 1 && P->scoreless == 1 && counted(P));
    CHECK(P->rack[0][0] == en('Q') && P->rack[0][4] == en('T') && P->rack[0][5] == en('S'));
    CHECK(!memcmp(r1, P->rack[1], sizeof r1));
    CHECK(!wt_can_challenge(P) && !wt_script(&G, "c"));     /* no challenge of a challenge */
    /* a challenge only right after the play: once the next player has played, it is too late */
    rack(1, "STONE..");
    CHECK(wt_script(&G, "h8vSTONE"));
    CHECK(wt_can_challenge(P) && P->turn == 2);
    CHECK(wt_script(&G, "-") && !wt_can_challenge(P));
    /* undo takes the challenge back too (a replay: the racks as dealt) */
    wt_new(&G, 0, 2, 9);
    CHECK(wt_script(&G, "#") && P->last.kind == WA_PLAY);
    H = G;
    CHECK(wt_script(&G, "c") && P->score[0] == 0);
    CHECK(wt_undo(&G) && !memcmp(&G.pos, &H.pos, sizeof G.pos) && wt_can_challenge(P));
}

static void test_exchange(void)
{
    wt_new(&G, 0, 2, 11);
    rack(0, "AEIOUZQ");
    int nb = P->nbag;
    CHECK(wt_script(&G, "xZQ"));
    CHECK(P->nbag == nb && P->turn == 1 && P->scoreless == 1 && wt_rack_count(P, 0) == 7 && counted(P));
    CHECK(P->rack[0][0] == en('A') && P->rack[0][4] == en('U'));
    P->nbag = 6;                                /* too few in the bag: only a pass */
    wact a;
    memset(&a, 0, sizeof a);
    a.kind = WA_EXCHANGE, a.n = 1;
    CHECK(!wt_legal(P, &a));
    a.n = 0;
    CHECK(wt_legal(P, &a));
}

static void test_end(void)
{
    /* six scoreless turns: the end, each player's tiles taken off */
    wt_new(&G, 0, 2, 13);
    CHECK(wt_script(&G, "- - - - -") && P->phase == WP_PLAY);
    int l0 = wt_rack_value(P, 0), l1 = wt_rack_value(P, 1);
    CHECK(wt_script(&G, "-") && P->phase == WP_OVER);
    CHECK(P->score[0] == -l0 && P->score[1] == -l1 && P->ender == 255);
    /* out with the bag empty: the next player may challenge, or accept */
    wt_new(&G, 0, 2, 17);
    rack(0, "CAT....");
    rack(1, "DOGE...");
    P->nbag = 0;
    CHECK(wt_script(&G, "g8>CAT") && P->phase == WP_FINAL && P->turn == 1 && wt_can_challenge(P));
    CHECK(!wt_script(&G, "-"));                 /* no other play then */
    wgame keep = G;
    CHECK(wt_script(&G, "a") && P->phase == WP_OVER && P->ender == 0);
    int dog = wt_value(P, en('D')) + wt_value(P, en('O')) + wt_value(P, en('G')) + wt_value(P, en('E'));
    CHECK(P->left[1] == -dog && P->left[0] == dog && P->score[0] == 8 + dog && P->score[1] == -dog);
    CHECK(wt_winner(P) == 0 && wt_winners(P) == 1);
    G = keep;
    CHECK(wt_script(&G, "c") && P->phase == WP_PLAY && P->turn == 1 && wt_rack_count(P, 0) == 3);
}

/* whole games, random legal actions from a seed: the tiles always all there, the scores the sum of the plays, the
 * game always ending, undo a faithful replay */
static void test_random(void)
{
    int games = 0, ended = 0, challenges = 0, finals = 0, ok = 1;
    for (uint32_t seed = 1; seed <= 240; seed++) {
        int players = 2 + (int)(seed % 3), set = (int)(seed % (uint32_t)wt_sets());
        wt_new(&G, set, players, seed);
        uint32_t rng = seed * 7919u;
        int steps = 0, sum[WT_MAXP] = {0};
        while (P->phase != WP_OVER && steps < 900) {
            wact a;
            int before = P->turn;
            if (!wt_auto(P, &rng, &a)) { ok = 0; break; }
            wscore s;
            int v = a.kind == WA_PLAY ? wt_eval(P, &a, &s) : 0;
            if (a.kind == WA_CHALLENGE) sum[P->last.player] -= P->last.score, challenges++;
            if (!wt_play(&G, &a)) { ok = 0; break; }
            if (a.kind == WA_PLAY) sum[before] += v;
            if (P->phase == WP_FINAL) finals++;
            ok &= counted(P);
            steps++;
        }
        games++;
        if (P->phase == WP_OVER) {
            ended++;
            for (int q = 0; q < players; q++) ok &= P->score[q] == sum[q] + P->left[q];
            int total = 0;
            for (int q = 0; q < players; q++) total += P->left[q];
            ok &= P->ender == 255 ? total <= 0 : total == 0;
        }
        if (seed % 40 == 0) {                   /* undo replays */
            H = G;
            while (G.n > H.n - 5 && wt_undo(&G)) {}
            for (int i = G.n; i < H.n; i++) wt_play(&G, &H.act[i]);
            ok &= !memcmp(&G.pos, &H.pos, sizeof G.pos);
        }
    }
    CHECK(ok);
    CHECK(ended == games);
    CHECK(challenges > 0 && finals > 0);
    printf("  %d random games, %d challenges, %d ended by going out\n", games, challenges, finals);
}

static void test_notation(void)
{
    wt_new(&G, 0, 2, 21);
    rack(0, "CAT....");
    CHECK(!wt_script(&G, "h8>DOG"));            /* not on the rack */
    CHECK(!wt_script(&G, "p8>CAT"));            /* no such column */
    CHECK(wt_script(&G, "h8vCAT") && P->board[WT_CENTRE] == en('C') && P->board[WT_CENTRE + 2 * WT_N] == en('T'));
    rack(1, "OW.....");
    CHECK(wt_script(&G, "g9>OAW"));             /* the A already there */
    CHECK(P->board[8 * WT_N + 6] == en('O') && P->board[8 * WT_N + 8] == en('W'));
    CHECK(wt_script(&G, "#"));
}

int main(void)
{
    test_sets();
    test_board();
    test_deal();
    test_place();
    test_bingo();
    test_challenge();
    test_exchange();
    test_end();
    test_random();
    test_notation();
    printf("wordtiles: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
