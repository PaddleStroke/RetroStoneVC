/*
 * Euchre's bot (src/games/euchre_bot.c, trickbot.h): the core's rules against the game's (the legal cards with the
 * left bower following trumps, the tricks' winners, a lone hand's partner out, card by card over random hands), the
 * determinised deals, the view keeps to what the seat may know (the others' hidden cards swapped: the same move),
 * sensible trump making, the cancel, then self-play at every level by sides (tests/ai_selfplay.h: seats 0 and 2 one
 * level, 1 and 3 the other). Headless: this file plays the screen's part (euchre_live(), euchre_show()). --full for
 * the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/euchre.h"

static egame T, U, S;
static int dealt;
const egame *euchre_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        euchre_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!euchre_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int euchre_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == EP_OVER ? euchre_winner(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    static const char suits[] = "shdc";
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const epos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    char c[3];
    switch (p->phase) {
    case EP_ROUND1: snprintf(m, (size_t)cap, "%s", ai_rand_n(r, 4) ? "p" : "o"); return 1;
    case EP_ROUND2: {
        if (p->turn != p->dealer && ai_rand_n(r, 3)) { snprintf(m, (size_t)cap, "p"); return 1; }
        int s;
        do s = ai_rand_n(r, 4); while (s == CARD_SUIT(p->up));
        snprintf(m, (size_t)cap, "n%c", suits[s]);
        return 1;
    }
    case EP_DISCARD: trick_name(h[ai_rand_n(r, n)], c); snprintf(m, (size_t)cap, "d%s", c); return 1;
    case EP_PLAY: {
        uint8_t L[EU_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (euchre_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case EP_HAND: snprintf(m, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != EP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 14);
    tb_state v, w;
    {   /* the core's rules are the game's (alone hands too: random "oa") */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1, alone = 0;
        for (int gn = 0; gn < 150 && same && tricks_ok; gn++) {
            euchre_new(&T, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != EP_OVER && T.pos.deal < 12 && same && tricks_ok) {
                const epos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == EP_PLAY) {
                    eub_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += euchre_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = euchre_can_play(p, L[i]);
                    if (fresh) {
                        uint8_t nn[EU_P];
                        for (int s = 0; s < EU_P; s++) nn[s] = s == p->out ? 0 : p->n[s];
                        w = v, tt_reveal(&w, &p->hand[0][0], EU_HAND, nn), fresh = 0;
                        alone += p->out != EU_NONE;
                    }
                    plays++;
                }
                random_move(&r, m, sizeof m);
                if (p->phase == EP_ROUND1 && m[0] == 'o' && gn % 3 == 0) strcat(m, "a");
                int before = p->tricks, was_play = p->phase == EP_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->tricks != before)
                    for (int s = 0; s < EU_P; s++)
                        tricks_ok &= w.won[s] == p->took[s] && (p->phase != EP_PLAY || w.turn == p->turn);
                if (p->phase == EP_HAND || p->phase == EP_OVER) tricks_ok &= tb_over(&w), fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns, %d lone hands)", plays, alone);
        tt_check(name, same && alone > 20);
        tt_check("a revealed hand played by the core: the same tricks, turns and end", tricks_ok);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            euchre_new(&T, 900 + gn);
            dealt = 1;
            walk(&r, 4 + gn % 40);
            if (T.pos.phase != EP_PLAY) continue;
            eub_view(&T, T.pos.turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand, the dealer's card (%d)", tried);
        tt_check(name, ok && tried > 100);
    }
    const bot *b = bot_find("euchre");
    tt_check("registered: bot_find(\"euchre\")", b && b->think && b->play == euchre_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suits): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 80; gn++) {
            euchre_new(&T, 3000 + gn);
            dealt = 1;
            walk(&r, 6 + gn % 13);
            epos *p = &T.pos;
            if (p->phase != EP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 3) % 4, swapped = 0;
            if (a == p->out || c == p->out) continue;
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++)
                    if (euchre_suit(p->hand[a][i], p->trump) == euchre_suit(p->hand[c][j], p->trump) &&
                        p->hand[a][i] != p->up && p->hand[c][j] != p->up) {
                        uint8_t t = p->hand[a][i];
                        p->hand[a][i] = p->hand[c][j];
                        p->hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            int m1 = eub_choose(&T, me, AI_MEDIUM, 77, NULL);
            egame keep = T;
            euchre_new(&T, 3000 + gn);
            for (int i = 0; i < keep.n; i++) euchre_play(&T, keep.act[i]);
            same &= m1 == eub_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* making trumps: the makers win the hand most of the time (hard against hard) */
        int hands = 0, made = 0, ok = 1;
        for (int gn = 0; gn < 60; gn++) {
            euchre_new(&T, 6000 + gn);
            dealt = 1;
            while (T.pos.phase != EP_HAND && T.pos.phase != EP_OVER) {
                int a = eub_choose(&T, T.pos.turn, AI_MEDIUM, 3, NULL);
                ok &= euchre_play(&T, a);
            }
            hands++;
            made += T.pos.pts[T.pos.maker & 1] > 0;
        }
        snprintf(name, sizeof name, "trumps made by rule: the makers take the hand %d times in %d", made, hands);
        tt_check(name, ok && made * 10 >= hands * 6);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        euchre_new(&T, 5);
        dealt = 1;
        while (T.pos.phase != EP_PLAY || T.pos.n[T.pos.turn] < 4) walk(&r, 1);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                euchre_new(&T, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 4 + k % 9 + retry);
                if (T.pos.phase != EP_PLAY || T.pos.n[T.pos.turn] < 3) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                eub_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "euchre", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 50},
        {AI_HARD, AI_EASY, 40, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
