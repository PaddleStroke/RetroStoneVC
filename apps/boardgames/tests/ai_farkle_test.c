/*
 * Farkle's bot (src/games/farkle_bot.c), one to six players: its registry entry, the throws it makes ("d??????"),
 * a few choices (banking enough to win in the final round, playing on when short of it), the cancel; alone, how
 * often each level reaches 10000 in the twenty turns (the same throws for all: the same seeds); then self-play at
 * every level for two to six players (tests/ai_selfplay.h; three players or more: the even players one level, the
 * odd ones the other). Headless: this file plays the screen's part (farkle_live(), farkle_show(): the faces of a
 * bot's throw from the session's seed). --full for the 200 games and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/farkle.h"

static fgame T;                                 /* the "screen's" game: the bot reads it */
static int NP;
const fgame *farkle_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, (uint64_t)T.n);
    return fk_script(&T, f);
}
int farkle_show(const char *move) { return one(move); }

static void reset(void) { fk_new(&T, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void)
{
    const fpos *p = &T.pos;
    if (p->phase != FK_OVER) return -1;
    unsigned m = 0;
    int w = fk_leader(p);
    for (int q = 0; q < p->players; q++)
        if (p->score[q] == p->score[w]) m |= 1u << q;
    return dice_side(m);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    const fpos *p = &T.pos;
    int len = 0;
    if (p->phase == FK_THROW) {
        m[len++] = 'd';
        for (int i = 0; i < FK_DICE; i++) m[len++] = (char)('1' + ai_rand_n(r, 6));
        m[len] = 0;
        return 1;
    }
    unsigned sets[64];
    int n = 0;
    for (unsigned s = 1; s < 1u << p->nd; s++)
        if (fk_sel_value(p, s)) sets[n++] = s;
    if (!n || cap < 20) return 0;
    unsigned s = sets[ai_rand_n(r, n)];
    m[len++] = 'a';
    for (int i = 0; i < p->nd; i++)
        if (s >> i & 1) m[len++] = (char)('0' + p->dice[i]);
    m[len++] = ' ';
    if (fk_can_bank(p, s) && ai_rand_n(r, 2)) m[len++] = 'b';
    else {
        m[len++] = 'd';
        for (int i = fk_to_throw(p, s); i > 0; i--) m[len++] = (char)('1' + ai_rand_n(r, 6));
    }
    m[len] = 0;
    return 1;
}

/* alone: a game played out by one level; 1: 10000 reached in the turns (-1: a move refused) */
static int solo(const bot *b, int level, uint64_t seed, double *worst, double *ms, long *moves)
{
    NP = 1;
    reset();
    for (int ply = 0; T.pos.phase != FK_OVER && ply < 2000; ply++) {
        char m[BOT_MOVE_CAP];
        ai_set_seed(ai_mix(seed, (uint64_t)ply));
        double t0 = ai_now_ms();
        if (!b->think(0, level, m, sizeof m) || !one(m)) return -1;
        double dt = ai_now_ms() - t0;
        *ms += dt;
        ++*moves;
        if (dt > *worst) *worst = dt;
    }
    return T.pos.winner == 0;
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv);
    const bot *b = bot_find("farkle");
    dice_check("registered: bot_find(\"farkle\")", b && b->think && b->play == farkle_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    int ok = 1;
    NP = 2;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d??????");
    dice_check("a turn's first throw: \"d??????\" at every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    {   /* 1 1 1 2 3 4 at a turn's start: hard and medium set aside the three 1s (1000) and bank them (three dice
         * would farkle 28% of the time) */
        reset();
        fk_script(&T, "d111234");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "a111 b");
        dice_check("1 1 1 2 3 4: three 1s aside, banked (medium, hard)", ok);
    }
    {   /* the final round, the last turn: player 1 has 10200, player 0 9000 and a turn of 1150 so far, one die left
         * with a 5 on it: 9000 + 1200 = 10200 only ties, so hard and medium throw on (hot dice) */
        reset();
        T.pos.score[0] = 9000;
        T.pos.score[1] = 10200;
        T.pos.final_by = 1;
        T.pos.final_left = 1;
        T.pos.phase = FK_CHOOSE;
        T.pos.turn_pts = 1150;
        T.pos.nd = 1;
        T.pos.dice[0] = 5;
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "a5 d??????");
        dice_check("the final round, a tie in reach: plays on (medium, hard)", ok);
        T.pos.turn_pts = 1200;                  /* 9000 + 1250 passes 10200: bank */
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "a5 b");
        dice_check("the final round, the lead in reach: banks (medium, hard)", ok);
    }
    {   /* not yet entered: 350 with two dice left can't be banked; hard throws them */
        reset();
        T.pos.phase = FK_CHOOSE;
        T.pos.turn_pts = 300;
        T.pos.nd = 3;
        T.pos.dice[0] = 5, T.pos.dice[1] = 2, T.pos.dice[2] = 3;
        ok = b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "a5 d??");
        dice_check("under the entry's 500: no bank, throws on", ok);
    }
    {   /* cancelled: no move, at once */
        NP = 6;
        reset();
        fk_script(&T, "d115234");
        ai_cancel();
        double t0 = ai_now_ms();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        dice_check("cancelled: no move, at once", ok && dt < 50);
    }
    {   /* alone: 10000 in twenty turns, each level */
        int n = full ? 2000 : 100, won[AI_LEVELS] = {0};
        double worst = 0, ms = 0;
        long moves = 0;
        ok = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++)
            for (int i = 0; i < n; i++) {
                int s = solo(b, lv, ai_mix(31, (uint64_t)i), &worst, &ms, &moves);
                ok &= s >= 0;
                won[lv] += s > 0;
            }
        printf("     alone, %d games each: easy %.1f%%, medium %.1f%%, hard %.1f%% reach 10000 (%.2f ms a move, worst "
               "%.1f)\n", n, 100.0 * won[0] / n, 100.0 * won[1] / n, 100.0 * won[2] / n, ms / moves, worst);
        dice_check("alone: every move legal; medium better than easy, hard better than medium",
                   ok && won[1] > won[0] && won[2] > won[1]);
    }
    if (dice_fails) return 1;

    int bad = 0;
    for (NP = 2; NP <= FK_PLAYERS; NP++) {
        if (!full && NP > 3) continue;
        printf("== %d players\n", NP);
        const sp_game G = {
            .id = "farkle", .max_plies = 4000, .opening = 0, .reset = reset, .actor = actor, .result = result,
            .random_move = random_move, .script = one,
        };
        const int f = full;                     /* the short run (10 games) only checks the play */
        const sp_pair P[] = {
            {AI_EASY, SP_RANDOM, dice_games(200), f ? 90 : 0},
            {AI_MEDIUM, AI_EASY, dice_games(200), f ? 55 : 0},
            {AI_HARD, AI_EASY, dice_games(200), f ? 55 : 0},
            {AI_HARD, AI_MEDIUM, dice_games(200), f ? 50 : 0},
        };
        bad |= sp_run(&G, P, 4, argc, argv);
    }
    return bad;
}
