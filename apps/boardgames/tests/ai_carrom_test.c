/*
 * Carrom's bot (src/games/carrom_bot.c): its registry entry, its shots legal at every level (two players and four),
 * the cancel, an easy coin pocketed, then self-play (tests/ai_selfplay.h): every level against random and against each
 * other. Headless: this file plays the screen's part (carrom_live(), carrom_show()). --full for the 200 games.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/carrom.h"

static crm_game T;
static int players = 2;
const crm_game *carrom_live(void) { return &T; }

static int one(const char *s)
{
    int pos, ang, pow, n;
    if (!crm_parse(s, &pos, &ang, &pow, &n) || s[n]) return 0;
    return crm_play(&T, pos, ang, pow, NULL);
}
int carrom_show(const char *move) { return one(move); }

static void reset(void) { crm_game_new(&T, players); }
static int actor(void) { return crm_now(&T)->turn; }
static int result(void)
{
    int s = crm_now(&T)->status;
    return s == CRM_PLAY ? -1 : s == CRM_WIN0 ? 0 : 1;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    const crm_pos *p = crm_now(&T);
    int pos = crm_free_pos(p, p->turn, ai_rand_n(r, 2001) - 1000);
    snprintf(m, (size_t)cap, "%d:%d:%d", pos, ai_rand_n(r, 36000), 100 + ai_rand_n(r, 900));
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
    const bot *b = bot_find("carrom");
    check("registered: bot_find(\"carrom\")", b && b->think && b->play == carrom_show);
    for (players = 2; players <= 4; players += 2)
        for (int l = AI_EASY; l <= AI_HARD; l++) {
            reset();
            int ok = 1;
            double worst = 0;
            for (int k = 0; k < 8 && crm_now(&T)->status == CRM_PLAY; k++) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix(31, (uint64_t)k));
                double t0 = ai_now_ms();
                int got = b->think(crm_now(&T)->turn, l, m, sizeof m);
                if (ai_now_ms() - t0 > worst) worst = ai_now_ms() - t0;
                ok &= got && one(m);
            }
            snprintf(name, sizeof name, "%d players, %s: legal shots (worst %.0f ms)", players, sp_name(l), worst);
            check(name, ok);
        }
    players = 2;
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
    {   /* a white coin alone near a hole: hard pockets it */
        int all = 1;
        for (int l = AI_HARD; l <= AI_HARD; l++) {
            reset();
            crm_pos *p = &T.pos[0];
            for (int i = 1, k = 0; i < CRM_N; i++, k++) {
                p->w.b[i].x = 30 + (k % 9) * 5;
                p->w.b[i].y = 12 + (k / 9) * 5;
            }
            p->w.b[CRM_WHITE].x = 18;
            p->w.b[CRM_WHITE].y = 30;
            char m[BOT_MOVE_CAP] = "";
            ai_set_seed(9);
            int ok = b->think(0, l, m, sizeof m) && one(m) && crm_pocketed(crm_now(&T), 0) >= 1;
            if (!ok) printf("     %s played %s: %d pocketed, foul %d\n", sp_name(l), m, crm_pocketed(crm_now(&T), 0),
                            crm_now(&T)->foul);
            all &= ok;
        }
        check("an easy coin: hard pockets it", all);
    }
    if (fails) return 1;
    static const sp_game G = {
        .id = "carrom", .max_plies = 900, .opening = 0, .reset = reset, .actor = actor, .result = result,
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
