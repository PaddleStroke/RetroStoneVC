/*
 * Schafkopf's bot (src/games/schafkopf_bot.c, trickbot.h): the core's rules against the game's (the legal cards in
 * every game, the called ace's rules among them; a revealed deal played card by card keeps the game's points, turns
 * and every seat's score), the determinised deals keep to the seat's knowledge, the view is honest (the others' hidden
 * cards swapped: the same move), the bidding is sensible (the games said counted), the cancel, the levels' time, then
 * self-play (tests/ai_selfplay.h) in both variants (a deal all passed thrown in, or a Ramsch). Headless: this file
 * plays the screen's part (schafkopf_live(), schafkopf_show()). A test game is the first deals of a match (3 in the
 * short run, 4 with --full; the Ramsch's run: all 12 with --full), the halves of the table (even seats, odd seats)
 * scored by their best total. --checks: the checks alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include <time.h>

#include "games/schafkopf.h"

int sfb_choose(const sfgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
void sfb_view(const sfgame *g, int me, tb_state *v, sfgame *t);
void sfb_scores(const tb_state *st, int *score);
int sfb_power(int card, int kind, int suit);

static sfgame T, U, S;                          /* the "screen's" game; a scratch copy; the view's replay */
static int VAR = SFK_PLAIN, DEALS = 3, dealt, GAMES = 30;   /* GAMES: the random player says a game, percent */
const sfgame *schafkopf_live(void) { return &T; }

/* one action of the notation, as schafkopf_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        sfk_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!sfk_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int schafkopf_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }

/* the winning half of the table (the seats' parity) by the best total after DEALS deals; 2 both halves */
static int result(void)
{
    const sfpos *p = &T.pos;
    if (!dealt || (p->phase != SFP_OVER && !(p->phase == SFP_DEAL && p->ndeals >= DEALS))) return -1;
    int best = -(1 << 20), half = 0;
    for (int q = 0; q < TK4_P; q++) best = p->total[q] > best ? p->total[q] : best;
    for (int q = 0; q < TK4_P; q++)
        if (p->total[q] == best) half |= 1 << (q & 1);
    return half == 3 ? 2 : half == 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const sfpos *p = &T.pos;
    const uint8_t *h = p->t.hand[p->turn];
    int n = p->t.n[p->turn];
    static const char SUITS[4] = {'s', 'h', 'd', 'c'};
    switch (p->phase) {
    case SFP_BID: {
        int g[9], k = 0;
        for (int s = 0; s < 4; s++) {
            if (sfk_game_ok(p, SFG(SFG_RUF, s))) g[k++] = SFG(SFG_RUF, s);
            g[k++] = SFG(SFG_SOLO, s);
        }
        g[k++] = SFG(SFG_WENZ, 0);
        int x = g[ai_rand_n(r, k)];
        if (ai_rand_n(r, 100) >= GAMES) snprintf(m, (size_t)cap, "p");
        else if (SFG_KIND(x) == SFG_WENZ) snprintf(m, (size_t)cap, "w");
        else snprintf(m, (size_t)cap, "%c%c", SFG_KIND(x) == SFG_RUF ? 'r' : 's', SUITS[SFG_SUIT(x)]);
        return 1;
    }
    case SFP_PLAY: {
        uint8_t L[SFK_HAND];
        int k = 0;
        for (int i = 0; i < n; i++)
            if (sfk_can_play(p, h[i])) L[k++] = h[i];
        if (!k) return 0;
        char c[3];
        trick_name(L[ai_rand_n(r, k)], c);
        snprintf(m, (size_t)cap, "%s", c);
        return 1;
    }
    case SFP_DEAL: snprintf(m, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

/* the view of the seat to move with every hand revealed (a test may look): the partner and the run's holders dealt */
static void reveal(tb_state *w, const sfpos *p)
{
    sfb_view(&T, p->turn, w, &S);
    uint8_t n[TK4_P];
    for (int s = 0; s < TK4_P; s++) n[s] = p->t.n[s];
    tt_reveal(w, &p->t.hand[0][0], TK4_HAND, n);
    w->R->dealt(w);
}

static int legal_count(const sfpos *p)
{
    int k = 0;
    for (int i = 0; i < p->t.n[p->turn]; i++) k += sfk_can_play(p, p->t.hand[p->turn][i]);
    return k;
}

int main(int argc, char **argv)
{
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    char name[200];
    ai_rng r;
    ai_seed(&r, 11);
    static tb_state v, w;
    {   /* the bot's card strengths are the rules' */
        static const int ranks[8] = {RANK_ACE, 7, 8, 9, 10, RANK_JACK, RANK_QUEEN, RANK_KING};
        int same = 1;
        for (int kind = SFG_RUF; kind <= SFG_RAMSCH; kind++)
            for (int su = 0; su < 4; su++)
                for (int s = 0; s < 4; s++)
                    for (int k = 0; k < 8; k++)
                        same &= sfb_power(CARD(s, ranks[k]), kind, su) == sfk_power(CARD(s, ranks[k]), kind, su);
        tt_check("the bot's card strengths = the rules' (every game)", same);
    }
    /* the core's rules are the game's: legal cards at every turn of every game, and a revealed deal played card by
     * card keeps the game's points, turns, and every seat's score */
    {
        int same = 1, plays = 0, tricks_ok = 1, deals = 0, games[SFG_RAMSCH + 1] = {0};
        GAMES = 25;
        for (int gn = 0; gn < 500 && same && tricks_ok; gn++) {
            sfk_new(&T, gn & 1, 500 + gn);
            dealt = 1;
            int fresh = 1;
            while (T.pos.phase != SFP_OVER && T.pos.ndeals < 3 && same && tricks_ok) {
                const sfpos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == SFP_PLAY) {
                    if (fresh) reveal(&w, p), fresh = 0, games[p->kind]++;
                    sfb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L);
                    same = k == legal_count(p);
                    for (int i = 0; i < k && same; i++) same = sfk_can_play(p, L[i]);
                    if (!same) printf("     game %d: the core gives %d cards, the rules %d\n", gn, k, legal_count(p));
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int was_play = p->phase == SFP_PLAY, before = p->t.tricks, nd = p->ndeals;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (p->ndeals != nd) fresh = 1;
                if (was_play && (p->t.tricks != before || p->ndeals != nd)) {
                    for (int s = 0; s < TK4_P && p->ndeals == nd; s++)
                        tricks_ok &= w.pts[s] == p->t.eyes[s] && w.turn == p->t.leader;
                    if (p->ndeals != nd) {
                        const sfdeal *d = &p->list[p->ndeals - 1];
                        int sc[TK4_P];
                        sfb_scores(&w, sc);
                        tricks_ok &= tb_over(&w);
                        for (int s = 0; s < TK4_P; s++) tricks_ok &= sc[s] == d->score[s];
                        if (!tricks_ok)
                            printf("     game %d deal %d (game %d): core %d %d %d %d, rules %d %d %d %d; over %d\n",
                                   gn, nd, d->kind, sc[0], sc[1], sc[2], sc[3], d->score[0], d->score[1], d->score[2],
                                   d->score[3], tb_over(&w));
                        deals++;
                    }
                }
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns; Rufspiel %d, Wenz %d, Solo %d, "
                 "Ramsch %d)", plays, games[SFG_RUF], games[SFG_WENZ], games[SFG_SOLO], games[SFG_RAMSCH]);
        tt_check(name, same && plays > 5000 && games[SFG_RUF] && games[SFG_WENZ] && games[SFG_SOLO] &&
                 games[SFG_RAMSCH]);
        snprintf(name, sizeof name, "a revealed deal played by the core: the same points, turns and scores (%d deals)",
                 deals);
        tt_check(name, tricks_ok && deals > 300);
    }
    /* the determinisation keeps the counts, the voids, the seat's own hand, the called ace out of the hands that
     * showed they lack it */
    {
        int ok = 1, tried = 0;
        GAMES = 25;
        for (int gn = 0; gn < 120 && ok; gn++) {
            sfk_new(&T, SFK_RAMSCH, 900 + gn);
            dealt = 1;
            for (int k = 0; k < 4 + gn % 30 && T.pos.phase != SFP_DEAL; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            const sfpos *p = &T.pos;
            if (p->phase != SFP_PLAY) continue;
            sfb_view(&T, p->turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                tried++;
                ok &= tt_dealt_ok(&v, &w);
                if (p->kind == SFG_RUF) {       /* the side as dealt: the declarer and the ace's holder */
                    int ace = CARD(p->suit, RANK_ACE), holder = -1;
                    for (int s = 0; s < TK4_P; s++)
                        if (trick_find(w.hand[s], w.n[s], ace) >= 0) holder = s;
                    ok &= holder != p->declarer && w.team[p->declarer] == 0 && (holder < 0 || w.team[holder] == 0);
                    if (p->revealed) ok &= w.team[p->partner] == 0;
                }
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, own hand, the called ace's signs (%d)",
                 tried);
        tt_check(name, ok && tried > 200);
    }

    const bot *b = bot_find("schafkopf");
    tt_check("registered: bot_find(\"schafkopf\")", b && b->think && b->play == schafkopf_show && b->actor);

    /* honest: two other seats' hidden cards swapped (the same suit as followed, not the called ace): the same move */
    {
        int same = 1, tried = 0;
        GAMES = 30;
        for (int gn = 0; gn < 60 && tried < 40; gn++) {
            sfk_new(&T, SFK_RAMSCH, 3000 + gn);
            dealt = 1;
            for (int k = 0; k < 6 + gn % 24 && T.pos.phase != SFP_DEAL; k++) {
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                one(m);
            }
            sfpos *p = &T.pos;
            if (p->phase != SFP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 2) % 4, swapped = 0, K = p->kind, Su = p->suit;
            int ace = K == SFG_RUF ? CARD(Su, RANK_ACE) : CARD_NONE;
            sfgame keep = T;
            for (int i = 0; i < p->t.n[a] && !swapped; i++)
                for (int j = 0; j < p->t.n[c] && !swapped; j++) {
                    int x = p->t.hand[a][i], y = p->t.hand[c][j];
                    int tx = sfk_trump(x, K, Su), ty = sfk_trump(y, K, Su);
                    if (x == ace || y == ace || tx != ty || (!tx && CARD_SUIT(x) != CARD_SUIT(y))) continue;
                    p->t.hand[a][i] = (uint8_t)y;
                    p->t.hand[c][j] = (uint8_t)x;
                    swapped = 1;
                }
            if (!swapped) continue;
            int m1 = sfb_choose(&T, me, AI_MEDIUM, 77, NULL);
            T = keep;
            int m2 = sfb_choose(&T, me, AI_MEDIUM, 77, NULL);
            same &= m1 == m2;
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    /* sensible bidding: the games the bot says over many hands, and the trumps behind them */
    {
        int hands = 0, said[SFG_RAMSCH] = {0}, bad = 0;
        double tr_ruf = 0, tr_pass = 0;
        for (int gn = 0; gn < (full ? 2000 : 600); gn++) {
            sfk_new(&T, SFK_PLAIN, 20000 + gn);
            dealt = 1;
            const sfpos *p = &T.pos;
            int me = p->turn, a = sfb_choose(&T, me, AI_MEDIUM, gn, NULL), tr = 0;
            for (int i = 0; i < p->t.n[me]; i++) tr += sfk_trump(p->t.hand[me][i], SFG_RUF, 0);
            hands++;
            bad += !sfk_legal(p, a) || (SFA_KIND(a) != SFA_PASS && SFA_KIND(a) != SFA_GAME);
            int kind = SFA_KIND(a) == SFA_GAME ? SFG_KIND(SFA_ARG(a)) : SFG_NONE;
            said[kind]++;
            if (kind == SFG_RUF) tr_ruf += tr;
            if (kind == SFG_NONE) tr_pass += tr;
        }
        snprintf(name, sizeof name, "the first to speak: %d hands, %d pass, %d Rufspiele (trumps %.1f; passing %.1f), "
                 "%d Wenz, %d Solos", hands, said[SFG_NONE], said[SFG_RUF], said[SFG_RUF] ? tr_ruf / said[SFG_RUF] : 0,
                 said[SFG_NONE] ? tr_pass / said[SFG_NONE] : 0, said[SFG_WENZ], said[SFG_SOLO]);
        tt_check(name, !bad && said[SFG_RUF] * 100 >= hands * 8 && said[SFG_RUF] * 100 <= hands * 50 &&
                 said[SFG_SOLO] + said[SFG_WENZ] > 0 && (said[SFG_SOLO] + said[SFG_WENZ]) * 100 <= hands * 15 &&
                 tr_ruf / said[SFG_RUF] > tr_pass / said[SFG_NONE] + 1);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        sfk_new(&T, SFK_RAMSCH, 5);
        dealt = 1;
        while (T.pos.phase == SFP_BID) one("p");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work: iterations and time on positions of random deals */
        int npos = full ? 30 : 6;
        GAMES = 40;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0, cpu = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                sfk_new(&T, SFK_RAMSCH, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                for (int i = 0; i < 4 + k % 16 + retry; i++) {
                    char m[BOT_MOVE_CAP];
                    random_move(&r, m, sizeof m);
                    one(m);
                }
                if (T.pos.phase != SFP_PLAY || legal_count(&T.pos) < 2) { k--; continue; }
                ai_stats s;
                clock_t c0 = clock();
                double t0 = ai_now_ms();
                sfb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                cpu += (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f), processor time %6.1f ms (a choice of cards)\n",
                   sp_name(lv), nodes / npos, ms / npos, worst, cpu / npos);
        }
        double cpu = 0, worst = 0;            /* the deal's first card, hard: the most work */
        for (int k = 0; k < npos; k++) {
            sfk_new(&T, SFK_RAMSCH, 70 + k);
            dealt = 1;
            while (T.pos.phase == SFP_BID) one("p");
            clock_t c0 = clock();
            sfb_choose(&T, T.pos.turn, AI_HARD, 5, NULL);
            double c = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
            cpu += c, worst = c > worst ? c : worst;
        }
        printf("     hard, the first card of a deal (a Ramsch): processor time %6.1f ms (worst %6.1f)\n",
               cpu / npos, worst);
    }
    if (tt_fails) return 1;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--checks")) return 0;   /* the checks alone, no self-play */

    GAMES = 15;
    static const sp_game G = {
        .id = "schafkopf", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 75},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 50},
        {AI_HARD, AI_EASY, 40, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    VAR = SFK_PLAIN;
    DEALS = full ? 4 : 3;
    int bad = sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static const sp_pair Q[] = {{AI_MEDIUM, SP_RANDOM, 20, 0}};
    printf("-- with the Ramsch\n");
    VAR = SFK_RAMSCH;
    DEALS = full ? SFK_DEALS : 3;
    bad |= sp_run(&G, Q, 1, argc, argv);
    return bad;
}
