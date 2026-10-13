/*
 * Yacht's bot (src/games/yacht_bot.c), every sheet (classic, nordic, generala) and one to six players: its registry
 * entry, the throws it makes ("d?????", then "d" with "-" for the dice kept), a few choices, the cancel; alone, each
 * level's average total on each sheet (the same throws for all: the same seeds); then self-play at every level for
 * each sheet and count of players (tests/ai_selfplay.h; three players or more: the even players one level, the odd
 * ones the other). Headless: this file plays the screen's part (yacht_live(), yacht_show(): the faces of a bot's
 * throw from the session's seed). --full for the 200 games and the tables; --boxes: hard's average in each box,
 * alone (the costs in yacht_bot.c come from there).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_dice.h"

#include "games/yacht.h"

static ygame T;                                 /* the "screen's" game: the bot reads it */
static int V, NP;
const ygame *yacht_live(void) { return &T; }

static int one(const char *m)
{
    char f[BOT_MOVE_CAP];
    dice_fill(m, f, sizeof f, (uint64_t)T.n);
    return ya_script(&T, f);
}
int yacht_show(const char *move) { return one(move); }

static void reset(void) { ya_new(&T, V, NP); }
static int actor(void) { return T.pos.turn; }
static int result(void)
{
    const ypos *p = &T.pos;
    if (!p->over) return -1;
    if (p->winner >= 0) return p->winner & 1;
    unsigned m = 0;
    int best = 0;
    for (int q = 0; q < p->players; q++) best = ya_total(p, q) > best ? ya_total(p, q) : best;
    for (int q = 0; q < p->players; q++)
        if (ya_total(p, q) == best) m |= 1u << q;
    return dice_side(m);
}

static const char *NAMES[YC_N] = {"1", "2", "3", "4", "5", "6", "p", "pp", "3k", "4k", "fh", "ss", "ls", "st", "c",
                                  "y", "yy"};

static int random_move(ai_rng *r, char *m, int cap)
{
    const ypos *p = &T.pos;
    if (cap < 8) return 0;
    if (!p->throws || (p->throws < YA_THROWS && ai_rand_n(r, 3))) {
        unsigned kept = p->throws ? (unsigned)ai_rand_n(r, 31) : 0;
        m[0] = 'd';
        for (int i = 0; i < YA_DICE; i++) m[1 + i] = kept >> i & 1 ? '-' : (char)('1' + ai_rand_n(r, 6));
        m[1 + YA_DICE] = 0;
        return 1;
    }
    int cats[YA_MAX_CATS], n = ya_cats(p->variant, cats), ok[YA_MAX_CATS], k = 0;
    for (int i = 0; i < n; i++)
        if (ya_points(p, cats[i]) >= 0) ok[k++] = cats[i];
    if (!k) return 0;
    snprintf(m, (size_t)cap, "%s", NAMES[ok[ai_rand_n(r, k)]]);
    return 1;
}

/* alone: a game played out by one level; its total (-1: a move refused); box[c] gets each box's points */
static int solo(const bot *b, int level, uint64_t seed, double *worst, double *ms, long *moves, double *box)
{
    NP = 1;
    reset();
    for (int ply = 0; !T.pos.over && ply < 400; ply++) {
        char m[BOT_MOVE_CAP];
        ai_set_seed(ai_mix(seed, (uint64_t)ply));
        double t0 = ai_now_ms();
        if (!b->think(0, level, m, sizeof m) || !one(m)) return -1;
        double dt = ai_now_ms() - t0;
        *ms += dt;
        ++*moves;
        if (dt > *worst) *worst = dt;
    }
    if (box)
        for (int c = 0; c < YC_N; c++) box[c] += T.pos.box[0][c] > 0 ? T.pos.box[0][c] : 0;
    return ya_total(&T.pos, 0);
}

int main(int argc, char **argv)
{
    int full = dice_full(argc, argv), boxes = 0;
    for (int i = 1; i < argc; i++) boxes |= !strcmp(argv[i], "--boxes");
    const bot *b = bot_find("yacht");
    dice_check("registered: bot_find(\"yacht\")", b && b->think && b->play == yacht_show);
    if (!b) return 1;
    char m[BOT_MOVE_CAP];
    int ok = 1;
    V = YA_CLASSIC, NP = 2;
    reset();
    for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d?????");
    dice_check("a turn's first throw: \"d?????\" at every level", ok);
    dice_check("not its turn: no move", !b->think(1, AI_HARD, m, sizeof m));
    {   /* five sixes on the first throw: everyone scores the five of a kind */
        reset();
        ya_script(&T, "d66666");
        ok = 1;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "y");
        dice_check("five sixes: \"y\" at every level", ok);
    }
    {   /* 6 6 6 2 1 on the first throw, classic: medium and hard keep the sixes and throw the other two */
        reset();
        ya_script(&T, "d66621");
        ok = 1;
        for (int lv = AI_MEDIUM; lv <= AI_HARD; lv++) ok &= b->think(0, lv, m, sizeof m) && !strcmp(m, "d---??");
        dice_check("6 6 6 2 1: keeps the sixes (\"d---??\"), medium and hard", ok);
    }
    {   /* a large straight on the first throw: hard scores it at once */
        reset();
        ya_script(&T, "d23456");
        ok = b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "ls");
        dice_check("2 3 4 5 6: hard scores the large straight", ok);
    }
    {   /* generala: a full house on the first throw, served (35): hard takes it */
        V = YA_GENERALA;
        reset();
        ya_script(&T, "d22333");
        ok = b->think(0, AI_HARD, m, sizeof m) && !strcmp(m, "fh");
        dice_check("generala, a served full house: hard scores it", ok);
    }
    {   /* cancelled: no move, at once */
        V = YA_NORDIC, NP = 6;
        reset();
        ya_script(&T, "d12345");
        ai_cancel();
        double t0 = ai_now_ms();
        ok = !b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        dice_check("cancelled: no move, at once", ok && dt < 50);
    }
    {   /* alone: each level's average on each sheet, the same seeds for all */
        static const char *SHEET[] = {"classic", "nordic", "generala"};
        int n = full ? 1000 : 60;
        ok = 1;
        for (V = 0; V < YA_VARIANTS; V++) {
            double avg[AI_LEVELS], worst = 0, ms = 0, box[YC_N] = {0};
            long moves = 0;
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                long sum = 0;
                for (int i = 0; i < n; i++) {
                    int s = solo(b, lv, ai_mix(99, (uint64_t)i), &worst, &ms, &moves,
                                 lv == AI_HARD && boxes ? box : NULL);
                    ok &= s >= 0;
                    sum += s;
                }
                avg[lv] = (double)sum / n;
            }
            printf("     %-8s alone, %d games: easy %.1f, medium %.1f, hard %.1f (%.2f ms a move, worst %.1f)\n",
                   SHEET[V], n, avg[0], avg[1], avg[2], ms / moves, worst);
            ok &= avg[AI_MEDIUM] > avg[AI_EASY] && avg[AI_HARD] >= avg[AI_MEDIUM] + (full ? 2 : -1);
            if (boxes) {
                int cats[YA_MAX_CATS], nc = ya_cats(V, cats);
                printf("       hard's boxes:");
                for (int i = 0; i < nc; i++) printf(" %s %.1f", NAMES[cats[i]], box[cats[i]] / n);
                printf("\n");
            }
        }
        dice_check("alone: every move legal; medium scores more than easy, hard more than medium", ok);
    }
    if (dice_fails || boxes) return dice_fails != 0;

    int bad = 0, only = -1, run = 0;
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--run")) only = atoi(argv[i + 1]);
    static const char *SHEETS[] = {"classic", "nordic", "generala"};
    for (V = 0; V < YA_VARIANTS; V++)
        for (NP = 2; NP <= YA_PLAYERS; NP++, run++) {
            if ((only >= 0 && run != only) || (!full && NP > 3)) continue;
            printf("== %s, %d players\n", SHEETS[V], NP);
            const sp_game G = {
                .id = "yacht", .max_plies = 2000, .opening = 0, .reset = reset, .actor = actor, .result = result,
                .random_move = random_move, .script = one,
            };
            const int f = full;                 /* the short run (10 games) only checks the play */
            const sp_pair P[] = {
                {AI_EASY, SP_RANDOM, dice_games(200), f ? 90 : 0},
                {AI_MEDIUM, AI_EASY, dice_games(200), f ? 60 : 0},
                {AI_HARD, AI_EASY, dice_games(200), f ? 60 : 0},
                {AI_HARD, AI_MEDIUM, dice_games(200), f ? 44 : 0},   /* alone: 4 to 10 points a game more */
            };
            bad |= sp_run(&G, P, 4, argc, argv);
        }
    return bad;
}
