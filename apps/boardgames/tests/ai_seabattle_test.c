/*
 * Sea Battle's bot (src/games/seabattle_bot.c): its registry entry, moves that never depend on the other fleet's
 * hidden ships, a hit followed up, the shots a level needs to sink a fleet, then self-play in every variant
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (seabattle_live(), seabattle_show()). --full for
 * the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/seabattle.h"

static sbgame T;                                /* the "screen's" game: the bot reads it */
static int variant;
static uint32_t deal = 1;
static int pending;                             /* a new game, dealt at its first move from the game's seed */
static void deal_now(uint64_t seed);
const sbgame *seabattle_live(void) { deal_now(ai_session_seed()); return &T; }
int seabattle_show(const char *move) { return sb_script(&T, move); }

static int one(const char *m) { return sb_script(&T, m); }
static void deal_now(uint64_t seed)
{
    if (pending) sb_new(&T, variant, (uint32_t)seed);
    pending = 0;
}
static void reset(void) { pending = 1; }          /* (the harness sets the game's seed after it) */
static void fresh(void) { pending = 1; deal_now(deal++ * 2654435761u); }   /* a game at once (the checks) */
static int actor(void) { return pending ? 0 : T.pos.turn; }
static int result(void) { return pending ? -1 : T.pos.phase == SP_OVER ? T.pos.winner : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    deal_now(ai_rand(r));
    const sbpos *p = &T.pos;
    if (p->phase == SP_PLACE) { snprintf(m, (size_t)cap, "ok"); return 1; }
    int n = sb_shots(p), c[SB_MAXSHIPS], k = 0;
    while (k < n) {
        int cell = ai_rand_n(r, SB_CELLS), dup = 0;
        for (int i = 0; i < k; i++) dup |= c[i] == cell;
        if (!dup && p->shot[!p->turn][cell] == SHOT_NONE) c[k++] = cell;
    }
    m[0] = 0;
    for (int i = 0; i < n; i++)
        snprintf(m + strlen(m), (size_t)cap - strlen(m), "%s%c%d", i ? "+" : "", 'a' + c[i] % SB_N, c[i] / SB_N + 1);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* one level's shots to sink a fleet alone (player 1 never fires: the turn comes back to player 0 at once) */
static double shots_to_sink(int v, int level, int games, int *worst)
{
    const bot *b = bot_find("seabattle");
    long total = 0;
    *worst = 0;
    for (int k = 0; k < games; k++) {
        variant = v;
        deal = 9000 + (uint32_t)k;
        fresh();
        sb_script(&T, "ok ok");
        ai_rng r;
        ai_seed(&r, (uint64_t)k);
        int shots = 0;
        while (T.pos.phase == SP_SHOOT) {
            char m[BOT_MOVE_CAP] = "";
            ai_set_seed(ai_mix((uint64_t)k, (uint64_t)shots));
            if (level == SP_RANDOM) random_move(&r, m, sizeof m);
            else b->think(0, level, m, sizeof m);
            shots += sb_shots(&T.pos);
            if (!one(m)) { printf("FAIL refused \"%s\"\n", m); fails++; break; }
            if (T.pos.phase == SP_SHOOT) T.pos.turn = 0;
        }
        total += shots;
        if (shots > *worst) *worst = shots;
    }
    return (double)total / games;
}

int main(int argc, char **argv)
{
    char name[160];
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    const bot *b = bot_find("seabattle");
    check("registered: bot_find(\"seabattle\")", b && b->think && b->play == seabattle_show);

    {   /* placing: every level's fleet is valid and ready (the move fits the worker's buffer) */
        int ok = 1;
        for (int v = 0; v < SB_VARIANTS; v++)
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                variant = v;
                fresh();
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(5 + (uint64_t)lv);
                ok &= b->think(0, lv, m, sizeof m) && one(m) && T.pos.turn == 1 && sb_valid(&T.pos, T.pos.ship[0]);
                if (v < SB_PAPER && lv > AI_EASY) {   /* the modern fleet: no two ships touching */
                    sbpos t = T.pos;
                    t.apart = 1;
                    ok &= sb_valid(&t, T.pos.ship[0]);
                }
            }
        check("every level places a valid fleet (modern, medium and hard: ships apart)", ok);
    }
    {   /* the other fleet's hidden ships never matter: the same move with them moved (none sunk yet) */
        int same = 1, tried = 0;
        for (int v = 0; v < SB_VARIANTS; v++)
            for (int k = 0; k < 6; k++) {
                variant = v;
                fresh();
                sb_script(&T, "ok ok");
                for (int i = 0; i < 6; i++) sb_script(&T, "*");
                for (int i = 0; i < T.pos.nships; i++)
                    if (sb_sunk(&T.pos, 1, i)) goto next;
                for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                    char m1[BOT_MOVE_CAP] = "", m2[BOT_MOVE_CAP] = "";
                    sbgame keep = T;
                    ai_set_seed(11);
                    b->think(0, lv, m1, sizeof m1);
                    /* another fleet for player 1 on the squares not shot (the hits stay hits: the same sea seen) */
                    uint32_t s = 77 + (uint32_t)k;
                    for (int tries = 0; tries < 20000; tries++) {
                        sbship f[SB_MAXSHIPS];
                        sb_random(&T.pos, &s, f);
                        int fits = 1;
                        for (int c = 0; c < SB_CELLS && fits; c++)
                            if (T.pos.shot[1][c] != SHOT_NONE)
                                fits = (sb_ship_at(&T.pos, f, c) >= 0) == (T.pos.shot[1][c] == SHOT_HIT);
                        if (fits) { memcpy(T.pos.ship[1], f, sizeof f); break; }
                    }
                    ai_set_seed(11);
                    b->think(0, lv, m2, sizeof m2);
                    same &= !strcmp(m1, m2) && m1[0];
                    tried++;
                    T = keep;
                }
            next:;
            }
        snprintf(name, sizeof name, "the moves never depend on the other fleet's hidden ships (%d positions)", tried);
        check(name, same && tried > 20);
    }
    {   /* a lone hit: medium and hard fire next to it */
        int ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) {
            variant = SB_CLASSIC;
            fresh();
            sb_script(&T, "ok ok");
            int hit = -1;
            for (int c = 0; c < SB_CELLS && hit < 0; c++)
                if (sb_ship_at(&T.pos, T.pos.ship[1], c) >= 0 && c % SB_N > 0 && c % SB_N < 9 && c / SB_N > 0 &&
                    c / SB_N < 9)
                    hit = c;
            char m[BOT_MOVE_CAP];
            snprintf(m, sizeof m, "%c%d *", 'a' + hit % SB_N, hit / SB_N + 1);
            one(m);
            ai_set_seed(3);
            b->think(0, lv, m, sizeof m);
            int x = m[0] - 'a', y = atoi(m + 1) - 1, d = abs(x - hit % SB_N) + abs(y - hit / SB_N);
            ok &= d == 1;
        }
        check("after a lone hit, medium and hard fire next to it", ok);
    }
    {   /* the shots to sink a fleet alone: each level fewer than the one below */
        int games = full ? 100 : 16;
        printf("     shots to sink the fleet alone (%d fleets), average and worst:\n", games);
        for (int v = 0; v < SB_VARIANTS; v++) {
            double s[AI_LEVELS + 1];
            int w[AI_LEVELS + 1];
            for (int lv = SP_RANDOM; lv <= AI_HARD; lv++) s[lv + 1] = shots_to_sink(v, lv, games, &w[lv + 1]);
            printf("     variant %d: random %.1f (%d), easy %.1f (%d), medium %.1f (%d), hard %.1f (%d)\n", v, s[0],
                   w[0], s[1], w[1], s[2], w[2], s[3], w[3]);
            snprintf(name, sizeof name, "variant %d: random > easy%s in shots", v, full ? " > medium > hard" : ", hard");
            check(name, s[0] > s[1] && s[0] > s[3] && (!full || (s[1] > s[2] && s[2] > s[3])));
        }
    }
    {   /* cancelled: no move */
        char m[BOT_MOVE_CAP] = "";
        variant = SB_CLASSIC;
        fresh();
        sb_script(&T, "ok ok");
        ai_cancel();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        check("cancelled: no move", !ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "seabattle", .max_plies = 2 * SB_CELLS + 4, .opening = 0, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    /* the paper fleet is the luckiest (ten ships, four of one square), above all in salvos: the levels lie closer */
    static sp_pair P[SB_VARIANTS][4] = {
        {{AI_EASY, SP_RANDOM, 100, 80}, {AI_MEDIUM, AI_EASY, 100, 60}, {AI_HARD, AI_MEDIUM, 100, 55},
         {AI_HARD, AI_EASY, 100, 70}},
        {{AI_EASY, SP_RANDOM, 100, 80}, {AI_MEDIUM, AI_EASY, 100, 60}, {AI_HARD, AI_MEDIUM, 100, 55},
         {AI_HARD, AI_EASY, 100, 70}},
        {{AI_EASY, SP_RANDOM, 100, 70}, {AI_MEDIUM, AI_EASY, 100, 60}, {AI_HARD, AI_MEDIUM, 100, 42},
         {AI_HARD, AI_EASY, 100, 70}},
        {{AI_EASY, SP_RANDOM, 100, 70}, {AI_MEDIUM, AI_EASY, 100, 60}, {AI_HARD, AI_MEDIUM, 100, 42},
         {AI_HARD, AI_EASY, 100, 70}},
    };
    for (int v = 0; !full && v < SB_VARIANTS; v++)   /* the short run: five games a pairing, luck rules */
        for (int i = 0; i < 4; i++) P[v][i].min_pct = P[v][i].min_pct > 30 ? P[v][i].min_pct - 30 : 0;
    int bad = 0;
    for (variant = 0; variant < SB_VARIANTS; variant++) {
        printf("-- variant %d\n", variant);
        bad |= sp_run(&G, P[variant], 4, argc, argv);
    }
    return bad;
}
