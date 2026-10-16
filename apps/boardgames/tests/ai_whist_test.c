/*
 * Whist's bot (src/games/whist_bot.c, trickbot.h): the core's rules against the game's (the legal cards, the tricks'
 * winners, German whist's draws, card by card over random deals), the determinised deals, the cancel, then self-play
 * at every level by sides, four players and two (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (whist_live(), whist_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/whist.h"

static whist_game T, U, S;
static int dealt, players = 4;
const whist_game *whist_live(void) { return &T; }

/* one action of the notation, as whist_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        whist_new(&T, players, WV_HONOURS, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!whist_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int whist_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == WP_OVER ? whist_winner(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    const whist_pos *p = &T.pos;
    if (p->phase == WP_ROUND) { snprintf(m, (size_t)cap, "n"); return 1; }
    if (p->phase != WP_PLAY) return 0;
    uint8_t L[WHIST_HAND];
    int k = 0;
    for (int i = 0; i < p->n[p->turn]; i++)
        if (whist_can_play(p, p->hand[p->turn][i])) L[k++] = p->hand[p->turn][i];
    if (!k) return 0;
    char c[3];
    trick_name(L[ai_rand_n(r, k)], c);
    snprintf(m, (size_t)cap, "%s", c);
    return 1;
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase == WP_PLAY; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 12);
    tb_state v, w;
    for (players = 4; players >= 2; players -= 2) {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1;
        for (int gn = 0; gn < 120 && same && tricks_ok; gn++) {
            whist_new(&T, players, WV_SHORT, 500 + gn);
            dealt = 1;
            int fresh = 1;
            while (T.pos.phase == WP_PLAY && same && tricks_ok) {
                const whist_pos *p = &T.pos;
                whb_view(&T, p->turn, &v, &S);
                uint8_t L[TB_HAND];
                int k = tb_legal(&v, L), want = 0;
                for (int i = 0; i < p->n[p->turn]; i++) want += whist_can_play(p, p->hand[p->turn][i]);
                same = k == want;
                for (int i = 0; i < k && same; i++) same = whist_can_play(p, L[i]);
                if (fresh) {                    /* the deal revealed: every hand and the stock in its order */
                    w = v;
                    tt_reveal(&w, &p->hand[0][0], WHIST_HAND, p->n);
                    memcpy(w.stock, p->stock, p->nstock);
                    w.nstock = w.nstock_known = p->nstock;
                    fresh = 0;
                }
                plays++;
                char m[BOT_MOVE_CAP];
                random_move(&r, m, sizeof m);
                const char *s = m;
                tb_play(&w, trick_parse(&s));
                int before = p->tricks;
                one(m);
                if (p->tricks != before)
                    for (int q = 0; q < players; q++) {
                        tricks_ok &= w.won[q] == p->won[q] && (p->phase != WP_PLAY || w.turn == p->turn);
                        if (p->phase == WP_PLAY) tricks_ok &= w.n[q] == p->n[q];
                    }
                if (p->phase != WP_PLAY) tricks_ok &= tb_over(&w);
            }
        }
        snprintf(name, sizeof name, "%d players: the core's legal cards = the game's (%d turns)", players, plays);
        tt_check(name, same);
        snprintf(name, sizeof name, "%d players: a revealed deal played by the core, the same tricks and end", players);
        tt_check(name, tricks_ok);
        int ok = 1, tried = 0;                  /* the determinisation */
        for (int gn = 0; gn < 40 && ok; gn++) {
            whist_new(&T, players, WV_SHORT, 900 + gn);
            dealt = 1;
            walk(&r, 3 + gn % 40);
            if (T.pos.phase != WP_PLAY) continue;
            whb_view(&T, T.pos.turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                if (players == 2 && T.pos.nstock > 1) {   /* the face-up card on top; back at the bottom to check */
                    ok &= w.stock[w.nstock - 1] == T.pos.up;
                    uint8_t t = w.stock[0];
                    w.stock[0] = w.stock[w.nstock - 1];
                    w.stock[w.nstock - 1] = t;
                }
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "%d players: determinised deals keep counts, voids, own hand (%d)", players, tried);
        tt_check(name, ok && tried > 100);
    }
    players = 4;
    const bot *b = bot_find("whist");
    tt_check("registered: bot_find(\"whist\")", b && b->think && b->play == whist_show && b->actor);
    {
        char m[BOT_MOVE_CAP] = "";
        whist_new(&T, 4, WV_SHORT, 5);
        dealt = 1;
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    for (players = 4; players >= 2; players -= 2) {   /* the levels' work */
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int npos = 5;
            for (int k = 0; k < npos; k++) {
                whist_new(&T, players, WV_SHORT, 40 + k);
                dealt = 1;
                ai_seed(&r, 99 + k);
                walk(&r, 1 + k * 3);
                if (T.pos.phase != WP_PLAY) continue;
                ai_stats s;
                double t0 = ai_now_ms();
                whb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %d players %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", players, sp_name(lv),
                   nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "whist", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 70},
        {AI_MEDIUM, AI_EASY, 60, 50},
        {AI_HARD, AI_MEDIUM, 60, 45},
    };
    players = 4;
    int fail = tt_run(&G, P, 3, argc, argv);
    players = 2;
    static sp_pair P2[] = {
        {AI_EASY, SP_RANDOM, 60, 70},
        {AI_HARD, AI_EASY, 60, 55},
    };
    return fail | tt_run(&G, P2, 2, argc, argv);
}
