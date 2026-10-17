/*
 * Pool's bot (src/games/pool_bot.c): its registry entry, its shots legal at every level in both games, the cancel,
 * an easy pot taken, then self-play (tests/ai_selfplay.h): 8-ball, every level against random and against each other.
 * Headless: this file plays the screen's part (pool_live(), pool_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/pool.h"

static pool_game T;
static int variant;
const pool_game *pool_live(void) { return &T; }

static int one(const char *s)
{
    int ang, pow, placed, n;
    float x, y;
    if (!pool_parse(s, &ang, &pow, &placed, &x, &y, &n) || s[n]) return 0;
    return pool_play(&T, ang, pow, placed, x, y, NULL);
}
int pool_show(const char *move) { return one(move); }

static void reset(void) { pool_game_new(&T, variant, 12345); }
static int actor(void) { return pool_now(&T)->turn; }
static int result(void)
{
    int s = pool_now(&T)->status;
    return s == POOL_PLAY ? -1 : s == POOL_WIN0 ? 0 : 1;
}

/* a shot at random, the cue ball left where it is */
static int random_move(ai_rng *r, char *m, int cap)
{
    int ang = ai_rand_n(r, 36000), pow = 100 + ai_rand_n(r, 900);
    snprintf(m, (size_t)cap, "%d:%d", ang, pow);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

int main(int argc, char **argv)
{
    char name[160];
    const bot *b = bot_find("pool");
    check("registered: bot_find(\"pool\")", b && b->think && b->play == pool_show);
    for (variant = 0; variant < 2; variant++)
        for (int l = AI_EASY; l <= AI_HARD; l++) {   /* the break, then a few shots: every move accepted */
            reset();
            int ok = 1;
            double worst = 0;
            for (int k = 0; k < 6 && pool_now(&T)->status == POOL_PLAY; k++) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix(77, (uint64_t)k));
                double t0 = ai_now_ms();
                int got = b->think(pool_now(&T)->turn, l, m, sizeof m);
                if (ai_now_ms() - t0 > worst) worst = ai_now_ms() - t0;
                ok &= got && one(m);
            }
            snprintf(name, sizeof name, "%s, %s: legal shots (worst %.0f ms)", variant ? "9-ball" : "8-ball",
                     sp_name(l), worst);
            check(name, ok);
        }
    variant = 0;
    {
        char m[BOT_MOVE_CAP] = "";
        reset();
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && ai_now_ms() - t0 < 50);
    }
    {   /* an easy pot on an open table: hard pockets a ball and keeps the turn */
        int all = 1;
        for (int l = AI_HARD; l <= AI_HARD; l++) {
            reset();
            pool_pos *p = &T.pos[0];
            p->brk = p->in_hand = 0;
            for (int i = 1; i < p->w.n; i++) {
                p->w.b[i].x = 70 + (i % 5) * 4;
                p->w.b[i].y = 8 + (i / 5) * 4;
            }
            p->w.b[3].x = 8;
            p->w.b[3].y = 44;
            p->w.b[0].x = 20;
            p->w.b[0].y = 32;
            char m[BOT_MOVE_CAP] = "";
            ai_set_seed(5);
            int ok = b->think(0, l, m, sizeof m) && one(m) && pool_now(&T)->turn == 0 && !pool_now(&T)->foul;
            if (!ok) printf("     %s played %s: the 3 %s, turn %d, foul %d\n", sp_name(l), m,
                            pool_now(&T)->w.b[3].down >= 0 ? "in" : "not in", pool_now(&T)->turn, pool_now(&T)->foul);
            all &= ok;
        }
        check("an easy pot: hard makes it", all);
    }
    if (fails) return 1;
    static const sp_game G = {
        .id = "pool", .max_plies = 600, .opening = 0, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const sp_pair Pp[] = {
        {AI_EASY, SP_RANDOM, 40, 75},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 75},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_MEDIUM, AI_MEDIUM, 20, 0},
        {AI_HARD, AI_HARD, 20, 0},
    };
    return sp_run(&G, Pp, (int)(sizeof Pp / sizeof Pp[0]), argc, argv);
}
