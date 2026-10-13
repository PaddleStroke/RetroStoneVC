/*
 * Pairs' bot (src/games/pairs_bot.c): its registry entry, moves that never depend on a card never turned up, a known
 * pair taken, the moves a level needs alone, games of three and four bots, then self-play for two in every size
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (pairs_live(), pairs_show()). --full for the 200
 * games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/pairs.h"

static prgame T;                                /* the "screen's" game: the bot reads it */
static int variant, players = 2;
static uint32_t deal = 1;
static int pending;                             /* a new game, dealt at its first move from the game's seed */
static void deal_now(uint64_t seed);
const prgame *pairs_live(void) { deal_now(ai_session_seed()); return &T; }
int pairs_show(const char *move) { return pr_script(&T, move); }

static int one(const char *m) { return pr_script(&T, m); }
static void deal_now(uint64_t seed)
{
    if (pending) pr_new(&T, variant, players, (uint32_t)seed);
    pending = 0;
}
static void reset(void) { pending = 1; }          /* (the harness sets the game's seed after it) */
static void fresh(void) { pending = 1; deal_now(deal++ * 2654435761u); }   /* a game at once (the checks) */
static int actor(void) { return pending ? 0 : T.pos.turn; }  /* (the screen turns a miss back by itself: here the player does) */
static int result(void)
{
    if (pending) return -1;
    if (T.pos.phase != PP_OVER) return -1;
    unsigned w = pr_winners(&T.pos);
    return w == 3 ? 2 : w == 1 ? 0 : 1;
}
static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r));
    const prpos *p = &T.pos;
    if (p->phase == PP_SHOW) { snprintf(m, (size_t)cap, "h"); return 1; }
    int c[PR_MAX], n = 0;
    for (int i = 0; i < p->n; i++)
        if (pr_legal(p, PA_TURN | i)) c[n++] = i;
    if (!n) return 0;
    snprintf(m, (size_t)cap, "t%d", c[ai_rand_n(r, n)]);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a game of bots at these levels (one a seat) to the end: every move legal; the moves (pairs turned) */
static int bots_game(int np, const int *lv, uint32_t seed, int *ok)
{
    const bot *b = bot_find("pairs");
    pr_new(&T, variant, np, seed);
    for (int k = 0; k < 4000 && T.pos.phase != PP_OVER; k++) {
        char m[BOT_MOVE_CAP] = "";
        ai_set_seed(ai_mix(seed, (uint64_t)k));
        int s = T.pos.turn;
        if (!b->think(s, lv[s], m, sizeof m) || !one(m)) { *ok = 0; return 0; }
    }
    *ok &= T.pos.phase == PP_OVER;
    return T.pos.moves;
}

int main(int argc, char **argv)
{
    char name[160];
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    const bot *b = bot_find("pairs");
    check("registered: bot_find(\"pairs\")", b && b->think && b->actor && b->play == pairs_show);

    {   /* the cards never turned up never matter: the same moves with their pictures swapped round */
        int same = 1, tried = 0;
        ai_rng r;
        ai_seed(&r, 4);
        for (int k = 0; k < 60; k++) {
            variant = k % PR_VARIANTS;
            fresh();
            for (int i = 0, n = 4 + k % 9; i < n && T.pos.phase != PP_OVER; i++) {
                char m[8];
                random_move(&r, m, sizeof m);
                one(m);
            }
            if (T.pos.phase != PP_PLAY) continue;
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                char m1[BOT_MOVE_CAP] = "", m2[BOT_MOVE_CAP] = "";
                prgame keep = T;
                ai_set_seed(21);
                b->think(T.pos.turn, lv, m1, sizeof m1);
                int u[PR_MAX], nu = 0;          /* the unseen cards' pictures, turned round one place */
                for (int c = 0; c < T.pos.n; c++)
                    if (!T.pos.seen[c]) u[nu++] = c;
                if (nu > 1) {
                    uint8_t first = T.pos.pic[u[0]];
                    for (int i = 0; i + 1 < nu; i++) T.pos.pic[u[i]] = T.pos.pic[u[i + 1]];
                    T.pos.pic[u[nu - 1]] = first;
                }
                ai_set_seed(21);
                b->think(T.pos.turn, lv, m2, sizeof m2);
                same &= !strcmp(m1, m2) && m1[0];
                tried++;
                T = keep;
            }
        }
        snprintf(name, sizeof name, "the moves never depend on a card never turned up (%d positions)", tried);
        check(name, same && tried > 60);
    }
    {   /* a pair seen: hard takes it, both cards */
        variant = PR_MEDIUM;
        fresh();
        int a = -1, c2 = -1;
        for (int i = 0; i < T.pos.n && c2 < 0; i++)
            for (int j = i + 1; j < T.pos.n; j++)
                if (T.pos.pic[i] == T.pos.pic[j]) { a = i, c2 = j; break; }
        int other = a == 0 ? 1 : 0;
        while (other == c2 || T.pos.pic[other] == T.pos.pic[a]) other++;
        char m[BOT_MOVE_CAP];
        snprintf(m, sizeof m, "t%d t%d h t%d", a, other, c2);   /* player 0 shows a, player 1 turns its match */
        one(m);
        ai_set_seed(1);
        int ok = b->think(1, AI_HARD, m, sizeof m) && one(m) && T.pos.pairs[1] == 1;   /* the match of c2: a */
        ok &= T.pos.turn == 1 && b->think(1, AI_HARD, m, sizeof m) && one(m);   /* again: a card never seen */
        ok &= T.pos.seen[PA_CARD(atoi(m + 1))] && strcmp(m + 1, "") && atoi(m + 1) != other;
        check("the match of a card seen: hard takes it, then turns a new card", ok);
    }
    {   /* alone: the pairs of cards each level turns to clear the table (hard: a perfect memory) */
        int games = full ? 100 : 20;
        for (variant = 0; variant < PR_VARIANTS; variant++) {
            double mv[AI_LEVELS] = {0};
            int ok = 1;
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                long sum = 0;
                for (int k = 0; k < games; k++) {
                    int l1[1] = {lv};
                    sum += bots_game(1, l1, 500 + (uint32_t)k, &ok);
                }
                mv[lv] = (double)sum / games;
            }
            snprintf(name, sizeof name, "alone, %d cards: easy %.1f, medium %.1f, hard %.1f moves (fewer each level)",
                     pr_cards(variant), mv[0], mv[1], mv[2]);
            check(name, ok && mv[0] > mv[1] && mv[1] > mv[2]);
        }
    }
    {   /* three and four players, bots of every level: legal to the end */
        int ok = 1;
        for (int k = 0; k < 30; k++) {
            int lv[4] = {k % 3, (k + 1) % 3, (k + 2) % 3, k % 2 ? AI_HARD : AI_EASY};
            variant = k % PR_VARIANTS;
            bots_game(3 + k % 2, lv, 900 + (uint32_t)k, &ok);
        }
        check("three and four bots: every move legal, every game ended", ok);
    }
    {   /* the miss on show: the screen turns it back, nobody acts */
        variant = PR_SMALL;
        fresh();
        int a = 0, c2 = 1;
        while (T.pos.pic[c2] == T.pos.pic[a]) c2++;
        char m[16];
        snprintf(m, sizeof m, "t%d t%d", a, c2);
        one(m);
        check("a miss on show: no actor (the screen turns the cards back)", T.pos.phase == PP_SHOW && b->actor() == -1);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "pairs", .max_plies = 2000, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 200, 75},
        {AI_MEDIUM, AI_EASY, 200, 65},
        {AI_HARD, AI_MEDIUM, 200, 55},
        {AI_HARD, AI_EASY, 200, 75},
    };
    /* the short run (a twentieth of the games: ten a pairing) checks the games; with so few, luck rules: its
     * targets are 30 points looser */
    int quick = 1;
    for (int i = 1; i < argc; i++) quick &= strcmp(argv[i], "--full") != 0;
    for (size_t i = 0; quick && i < sizeof P / sizeof P[0]; i++)
        P[i].min_pct = P[i].min_pct > 30 ? P[i].min_pct - 30 : 0;
    int bad = 0;
    for (variant = 0; variant < PR_VARIANTS; variant++) {
        printf("-- %d cards, two players\n", pr_cards(variant));
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
