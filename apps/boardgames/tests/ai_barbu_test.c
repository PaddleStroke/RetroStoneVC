/*
 * Barbu's bot (src/games/barbu_bot.c, trickbot.h): the core's rules against the game's in every trick contract (the
 * legal cards: hearts not led, the king thrown, trumps over trumps; the tricks' winners, the penalty cards, the early
 * ends), the determinised deals and the soundness of what the plays showed, the view keeps to what the seat may know
 * (the others' hidden cards swapped: the same move), the contracts chosen and the doubles, the cancel, then self-play
 * at every level (tests/ai_selfplay.h; the short game, with and without doubling, and the whole game; the even seats
 * one level, the odd the other). Headless: this file plays the screen's part (barbu_live(), barbu_show()). --full for
 * the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/barbu.h"

static barbu_game T, U, S;
static int VARIANT = BARBU_SHORT, dealt;
const barbu_game *barbu_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        barbu_new(&T, VARIANT, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!barbu_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int barbu_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

static int result(void)
{
    if (!dealt || T.pos.phase != BBP_OVER) return -1;
    unsigned w = barbu_winners(&T.pos);
    int half = 0;
    for (int q = 0; q < BARBU_P; q++)
        if (w >> q & 1) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    static const char ranks[] = "23456789TJQKA", suits[] = "shdc";
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const barbu_pos *p = &T.pos;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn], k = 0;
    switch (p->phase) {
    case BBP_CHOOSE: {
        int c;
        do c = ai_rand_n(r, BARBU_CONTRACTS); while (!barbu_can_choose(p, c));
        k = snprintf(m, (size_t)cap, "c%d", c);
        if (c == BB_TRUMPS) m[k++] = suits[ai_rand_n(r, 4)], m[k] = 0;
        if (c == BB_DOMINO) m[k++] = ranks[ai_rand_n(r, 13)], m[k] = 0;
        return 1;
    }
    case BBP_DOUBLE:
    case BBP_REDOUBLE: {
        unsigned mask = barbu_double_mask(p);
        m[k++] = p->phase == BBP_DOUBLE ? 'd' : 'r';
        for (int q = 0; q < BARBU_P; q++)
            if (mask >> q & 1 && ai_rand_n(r, 3) == 0) m[k++] = (char)('0' + q);
        m[k] = 0;
        return 1;
    }
    case BBP_PLAY: {
        uint8_t L[BARBU_HAND];
        for (int i = 0; i < n; i++)
            if (barbu_can_play(p, h[i])) L[k++] = h[i];
        if (!k) { snprintf(m, (size_t)cap, "p"); return 1; }
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case BBP_ROUND: snprintf(m, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != BBP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 17);
    tb_state v, w;
    {   /* the core's rules are the game's, in each trick contract */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1, seen[BARBU_CONTRACTS] = {0};
        for (int gn = 0; gn < 200 && same && tricks_ok; gn++) {
            barbu_new(&T, gn % BARBU_VARIANTS, 500 + gn);
            dealt = 1;
            fresh = 1;
            while (T.pos.phase != BBP_OVER && T.pos.deal < 7 && same && tricks_ok) {
                const barbu_pos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                int trick_play = p->phase == BBP_PLAY && p->contract != BB_DOMINO;
                if (trick_play) {
                    bab_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += barbu_can_play(p, p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = barbu_can_play(p, L[i]);
                    if (fresh) w = v, tt_reveal(&w, &p->hand[0][0], BARBU_HAND, p->n), fresh = 0, seen[p->contract]++;
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, contract = p->contract;
                if (trick_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (trick_play && (p->tricks != before || p->phase == BBP_ROUND || p->phase == BBP_OVER)) {
                    int ended = p->phase == BBP_ROUND || p->phase == BBP_OVER;
                    tricks_ok &= tb_over(&w) == ended;
                    for (int s = 0; s < BARBU_P && !ended; s++) {
                        tricks_ok &= w.won[s] == p->won[s];
                        if (contract == BB_NOHEARTS || contract == BB_NOQUEENS || contract == BB_KING)
                            tricks_ok &= w.pts[s] == -p->pts[s];
                    }
                }
                if (p->phase == BBP_ROUND) fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns; %d %d %d %d %d %d deals)", plays,
                 seen[0], seen[1], seen[2], seen[3], seen[4], seen[5]);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, penalties and early ends", tricks_ok);
    }
    {   /* the determinisation; what the plays showed holds in the true hands */
        int ok = 1, tried = 0, sound = 1, capped = 0;
        for (int gn = 0; gn < 300 && ok; gn++) {
            barbu_new(&T, BARBU_SHORT_PLAIN, 900 + gn);
            dealt = 1;
            walk(&r, 2 + gn % 50);
            const barbu_pos *p = &T.pos;
            if (p->phase != BBP_PLAY || p->contract == BB_DOMINO) continue;
            bab_view(&T, p->turn, &v, &S);
            for (int s = 0; s < BARBU_P; s++) {
                for (int u = 0; u < 4; u++) capped += v.cap[s][u] != TB_NOCAP;
                for (int i = 0; i < p->n[s]; i++) sound &= tb_fits(&v, s, p->hand[s][i]);
            }
            for (int d = 0; d < 10 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
                if (!ok && getenv("DEBUG")) {
                    printf("     game %d contract %d tricks %d ntrick %d turn %d sound %d\n", gn, p->contract,
                           p->tricks, p->ntrick, p->turn, sound);
                    for (int s = 0; s < 4; s++) {
                        printf("     seat %d n %d/%d known %d caps %d %d %d %d:", s, w.n[s], v.n[s], v.nknown[s],
                               v.cap[s][0], v.cap[s][1], v.cap[s][2], v.cap[s][3]);
                        for (int i = 0; i < w.n[s]; i++)
                            printf(" %02x%s", w.hand[s][i], tb_fits(&v, s, w.hand[s][i]) ? "" : "!");
                        printf("\n");
                    }
                    printf("     unseen %d rest %d\n", v.nunseen, w.nrest);
                }
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, caps, own hand (%d); %d caps, all true", tried,
                 capped);
        tt_check(name, ok && sound && tried > 300 && capped > 50);
    }
    const bot *b = bot_find("barbu");
    tt_check("registered: bot_find(\"barbu\")", b && b->think && b->play == barbu_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suit, keeping what the plays showed): the same move, in the
         * trick contracts and in dominoes */
        int same = 1, tried = 0, dom = 0;
        for (int gn = 0; gn < 200 && tried < 60; gn++) {
            barbu_new(&T, BARBU_SHORT_PLAIN, 3000 + gn);
            dealt = 1;
            walk(&r, 3 + gn % 30);
            barbu_pos *p = &T.pos;
            if (p->phase != BBP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 3) % 4, swapped = 0, domino = p->contract == BB_DOMINO;
            if (!domino) bab_view(&T, me, &v, &S);
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++) {
                    int x = p->hand[a][i], y = p->hand[c][j];
                    if (CARD_SUIT(x) != CARD_SUIT(y)) continue;
                    if (domino ? barbu_must_pass(p) : !tb_fits(&v, a, y) || !tb_fits(&v, c, x)) continue;
                    p->hand[a][i] = (uint8_t)y;
                    p->hand[c][j] = (uint8_t)x;
                    swapped = 1;
                }
            if (!swapped) continue;
            int m1 = bab_choose(&T, me, AI_MEDIUM, 77, NULL);
            barbu_game keep = T;
            barbu_new(&T, BARBU_SHORT_PLAIN, 3000 + gn);
            for (int i = 0; i < keep.n; i++) barbu_play(&T, keep.act[i]);
            same &= m1 == bab_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
            dom += domino;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions, %d dominoes)",
                 tried, dom);
        tt_check(name, same && tried >= 40);
    }
    {   /* the calls: legal, every contract chosen at some point first, doubles now and then */
        int ok = 1, first[BARBU_CONTRACTS] = {0}, doubles = 0, says = 0, kinds = 0;
        for (int gn = 0; gn < 40; gn++) {
            barbu_new(&T, BARBU_FULL, 6000 + gn);
            dealt = 1;
            int a = bab_choose(&T, T.pos.turn, AI_MEDIUM, 3, NULL);
            ok &= BBA_KIND(a) == BBA_CHOOSE && barbu_play(&T, a);
            first[BBA_ARG(a) & 15]++;
            while (T.pos.phase == BBP_DOUBLE || T.pos.phase == BBP_REDOUBLE) {
                a = bab_choose(&T, T.pos.turn, AI_MEDIUM, 3, NULL);
                ok &= barbu_play(&T, a);
                says++;
                doubles += BBA_ARG(a) != 0;
            }
        }
        for (int c = 0; c < BARBU_CONTRACTS; c++) kinds += first[c] > 0;
        snprintf(name, sizeof name, "calls legal; first contracts %d %d %d %d %d %d %d; %d doubles in %d says",
                 first[0], first[1], first[2], first[3], first[4], first[5], first[6], doubles, says);
        tt_check(name, ok && kinds >= 4 && doubles > 0 && doubles < says);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        barbu_new(&T, BARBU_SHORT_PLAIN, 5);
        dealt = 1;
        one("c0");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work: card play (tricks and dominoes) and the calls */
        int npos = 8;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 32;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0, cms = 0, cworst = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                barbu_new(&T, BARBU_SHORT, 40 + k);
                dealt = 1;
                double t0 = ai_now_ms();
                bab_choose(&T, T.pos.turn, lv, 5, NULL);
                double dt = ai_now_ms() - t0;
                cms += dt;
                if (dt > cworst) cworst = dt;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 6 + k % 20 + retry);
                if (T.pos.phase != BBP_PLAY || barbu_must_pass(&T.pos)) { k--; continue; }
                ai_stats s;
                t0 = ai_now_ms();
                bab_choose(&T, T.pos.turn, lv, 5, &s);
                dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f); a contract %6.1f ms (worst %6.1f)\n",
                   sp_name(lv), nodes / npos, ms / npos, worst, cms / npos, cworst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "barbu", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 80},
        {AI_MEDIUM, AI_EASY, 100, 52},
        {AI_HARD, AI_MEDIUM, 100, 50},
        {AI_HARD, AI_EASY, 60, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    printf("-- the short game\n");
    VARIANT = BARBU_SHORT;
    int bad = sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 20, 70}, {AI_HARD, AI_EASY, 20, 0}};
    tt_targets(Q, 2, argc, argv);
    printf("-- the short game without doubling\n");
    VARIANT = BARBU_SHORT_PLAIN;
    bad |= sp_run(&G, Q, 2, argc, argv);
    printf("-- the whole game\n");
    VARIANT = BARBU_FULL;
    bad |= sp_run(&G, Q, 2, argc, argv);
    return bad;
}
