/*
 * Chinese checkers' bot (src/games/halma_bot.c), two, three, four and six players: the holes it finds a marble can
 * reach against the rules' on random games, its registry entry, the start at every level for every count, a win in
 * one, the cancel; then self-play: two players through tests/ai_selfplay.h, three or more through this file's own
 * runner (the level under test in one seat, round the star from game to game, the others at another level or
 * random; its score: a win 1, a draw 1 / players). Headless: this file plays the screen's part (halma_live(),
 * halma_show()). --full for all the games and the tables; -j n games at once.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_strategy.h"

#include "games/halma.h"

int hlb_targets(const hl_game *g, int from, int *to);
int hlb_choose(const hl_game *g, int level, uint64_t seed, int *from, int *to, ai_stats *stats);

static hl_game T;                               /* the "screen's" game: the bot reads it */
static int NP = 2;
const hl_game *halma_live(void) { return &T; }

static int one(const char *s) { return !strchr(s, ' ') && hl_status(&T) == HL_PLAY && hl_script(&T, s); }
int halma_show(const char *move) { return one(move); }

static void reset(void) { hl_new_players(&T, NP); }
static int actor(void) { return hl_turn(&T); }
static int result(void)                         /* -1 playing, the winner, or NP: a draw */
{
    int s = hl_status(&T);
    return s == HL_PLAY ? -1 : s == HL_DRAW ? (NP == 2 ? 2 : NP) : hl_winner(s);
}

static int random_move(ai_rng *r, char *m, int cap)
{
    int own[HL_MARBLES], n = 0, to[HL_HOLES];
    for (int h = 0; h < HL_HOLES; h++)
        if (T.cell[h] == T.turn + 1 && hl_targets(&T, h, to)) own[n++] = h;
    if (!n || cap < 8) return 0;
    int from = own[ai_rand_n(r, n)], k = hl_targets(&T, from, to);
    char a[4], b[4];
    hl_name(from, a);
    hl_name(to[ai_rand_n(r, k)], b);
    snprintf(m, (size_t)cap, "%s-%s", a, b);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int f, t;
    return hlb_choose(&T, level, 5, &f, &t, s);
}

/* ---- three or more players: the runner ------------------------------------------------------------------------ */
typedef struct { int level, others, games, min_pct; } mp_pair;
typedef struct {
    int done, ok, won, draw, plies, moves[AI_LEVELS];
    double ms_sum[AI_LEVELS], ms_max[AI_LEVELS];
    char err[160];
} mp_result;

static void mp_play(const mp_pair *p, int k, uint64_t seed, mp_result *r)
{
    const bot *b = bot_find("halma");
    ai_rng rng;
    ai_seed(&rng, seed);
    int seat = k % NP;                          /* the level under test, round the star */
    reset();
    for (int ply = 0;; ply++) {
        int res = result();
        if (res >= 0) {
            r->won = res == seat;
            r->draw = res == NP;
            r->ok = 1;
            break;
        }
        if (ply >= 600 * NP) { snprintf(r->err, sizeof r->err, "no end after %d plies", ply); break; }
        int who = actor(), level = who == seat ? p->level : p->others;
        char m[BOT_MOVE_CAP] = "";
        if (ply < NP || level == SP_RANDOM) {
            if (!random_move(&rng, m, sizeof m)) { snprintf(r->err, sizeof r->err, "no random move"); break; }
        } else {
            ai_set_seed(ai_mix(seed, (uint64_t)ply));
            double t0 = ai_now_ms();
            int ok = b->think(who, level, m, (int)sizeof m);
            double dt = ai_now_ms() - t0;
            r->moves[level]++;
            r->ms_sum[level] += dt;
            if (dt > r->ms_max[level]) r->ms_max[level] = dt;
            if (!ok || !m[0]) {
                snprintf(r->err, sizeof r->err, "ply %d: %s gave no move", ply, sp_name(level));
                break;
            }
            if (dt > ai_level_budget(level).ms + 50) {
                snprintf(r->err, sizeof r->err, "ply %d: %s took %.0f ms", ply, sp_name(level), dt);
                break;
            }
        }
        if (!one(m)) { snprintf(r->err, sizeof r->err, "ply %d: \"%s\" refused", ply, m); break; }
        r->plies = ply + 1;
    }
    r->done = 1;
}

static int mp_run(const mp_pair *pairs, int npairs, int argc, char **argv)
{
    if (getenv("AI_LEVELS_ONLY")) return 0;
    int full = st_full(argc, argv), jobs = (int)sysconf(_SC_NPROCESSORS_ONLN), count[16], first[16], total = 0;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-j") && i + 1 < argc) jobs = atoi(argv[++i]);
    for (int i = 0; i < npairs; i++) {
        count[i] = full ? pairs[i].games : (pairs[i].games + 19) / 20;
        first[i] = total;
        total += count[i];
    }
    mp_result *R = mmap(NULL, sizeof(mp_result) * (size_t)total, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
                        -1, 0);
    int *next = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (R == MAP_FAILED || next == MAP_FAILED) { printf("FAIL mmap\n"); return 1; }
    memset(R, 0, sizeof(mp_result) * (size_t)total);
    *next = 0;
    double t0 = ai_now_ms();
    fflush(stdout);
    for (int w = 0; w < jobs; w++) {
        pid_t pid = fork();
        if (pid < 0) break;
        if (pid) continue;
        for (;;) {
            int i = __atomic_fetch_add(next, 1, __ATOMIC_SEQ_CST);
            if (i >= total) _exit(0);
            int pi = 0;
            while (pi + 1 < npairs && i >= first[pi + 1]) pi++;
            mp_play(&pairs[pi], i - first[pi], ai_mix(0x4a1 + (uint64_t)pi * 1000 + (uint64_t)NP, (uint64_t)i), &R[i]);
        }
    }
    while (wait(NULL) > 0) {}
    int fails = 0;
    double lv_ms[AI_LEVELS] = {0}, lv_max[AI_LEVELS] = {0};
    long lv_n[AI_LEVELS] = {0};
    printf("halma, %d players: %d games (%s), %.1f s\n", NP, total, full ? "full" : "short; --full for all",
           (ai_now_ms() - t0) / 1000);
    printf("  %-16s %5s %5s %5s %6s %6s  %s\n", "level-others", "games", "won", "draw", "score", "chance", "plies");
    for (int pi = 0; pi < npairs; pi++) {
        int won = 0, draw = 0, plies = 0, bad = 0;
        for (int i = first[pi]; i < first[pi] + count[pi]; i++) {
            mp_result *r = &R[i];
            if (!r->done || !r->ok) {
                printf("FAIL halma %d players game %d: %s\n", NP, i - first[pi], r->done ? r->err : "the worker died");
                bad++;
                continue;
            }
            won += r->won;
            draw += r->draw;
            plies += r->plies;
            for (int l = 0; l < AI_LEVELS; l++) {
                lv_n[l] += r->moves[l];
                lv_ms[l] += r->ms_sum[l];
                if (r->ms_max[l] > lv_max[l]) lv_max[l] = r->ms_max[l];
            }
        }
        int n = count[pi] - bad, pct = n ? (100 * won * NP + 100 * draw) / (n * NP) : 0;
        char name[40];
        snprintf(name, sizeof name, "%s-%s", sp_name(pairs[pi].level), sp_name(pairs[pi].others));
        printf("  %-16s %5d %5d %5d %5d%% %5d%%  %.1f\n", name, count[pi], won, draw, pct, 100 / NP,
               n ? (double)plies / n : 0);
        fails += bad;
        int target = full || pairs[pi].others == SP_RANDOM ? pairs[pi].min_pct : 0;
        if (pct < target) {
            printf("FAIL halma %d players %s: scored %d%%, under %d%%\n", NP, name, pct, target);
            fails++;
        }
    }
    printf("  %-8s %8s %9s %9s\n", "level", "moves", "ms/move", "max ms");
    for (int l = 0; l < AI_LEVELS; l++)
        if (lv_n[l])
            printf("  %-8s %8ld %9.1f %9.1f   (budget %d ms)\n", sp_name(l), lv_n[l], lv_ms[l] / lv_n[l], lv_max[l],
                   ai_level_budget(l).ms);
    munmap(R, sizeof(mp_result) * (size_t)total);
    munmap(next, sizeof(int));
    printf("halma, %d players: %s\n", NP, fails ? "FAILED" : "all games legal, ended, in time");
    return fails != 0;
}

int main(int argc, char **argv)
{
    char name[160];
    static const sp_game G = {
        .id = "halma", .max_plies = 1200, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static const int COUNTS[4] = {2, 3, 4, 6};
    {   /* the holes the bot finds a marble reaches are the rules': every marble of positions of random games */
        ai_rng r;
        ai_seed(&r, 21);
        int same = 1, marbles = 0;
        for (int gn = 0; gn < 200 && same; gn++) {
            NP = COUNTS[gn % 4];
            reset();
            for (int ply = 0; ply < 300 && result() < 0 && same; ply++) {
                for (int h = 0; h < HL_HOLES && same; h++) {
                    if (T.cell[h] != T.turn + 1) continue;
                    int a[HL_HOLES], b[HL_HOLES], na = hl_targets(&T, h, a), nb = hlb_targets(&T, h, b);
                    same = na == nb;
                    for (int i = 0; i < na && same; i++) {
                        int found = 0;
                        for (int j = 0; j < nb && !found; j++) found = a[i] == b[j];
                        same = found;
                    }
                    marbles++;
                }
                char m[16];
                if (!random_move(&r, m, sizeof m)) break;
                one(m);
            }
        }
        snprintf(name, sizeof name, "the holes a marble reaches = the rules' (%d marbles)", marbles);
        st_check(name, same);
    }
    for (int c = 0; c < 4; c++) {
        NP = COUNTS[c];
        printf("     %d players:\n", NP);
        st_basics("halma", &G, halma_show, NP);
    }
    {   /* two players: ivory's point full but a14, its last marble on e13 next to it; ebony's in the middle */
        int all = 1;
        for (int l = AI_EASY; l <= AI_HARD; l++) {
            NP = 2;
            reset();
            memset(T.cell, 0, sizeof T.cell);
            int len, k = 0;
            for (int h = 0; h < HL_HOLES; h++)
                if (hl_goal(0, h)) T.cell[h] = HL_IVORY;
            T.cell[hl_hole("a14", &len)] = HL_EMPTY;
            T.cell[hl_hole("e13", &len)] = HL_IVORY;
            for (int h = 0; h < HL_HOLES && k < HL_MARBLES; h++)
                if (hl_row(h) == 6) T.cell[h] = HL_EBONY, k++;
            const char *m = st_move("halma", 0, l);
            all &= m[0] && one(m) && result() == 0;
        }
        st_check("two players: every level fills its point when one move does", all);
    }
    for (int c = 0; c < 4; c++) {
        NP = COUNTS[c];
        printf("     %d players:\n", NP);
        st_levels(argc, argv, &G, 3 * NP, choose);
    }
    if (st_fails) return 1;

    int fails = 0;
    NP = 2;
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 40, 90},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    printf("== 2 players\n");
    fails += st_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    for (int c = 1; c < 4; c++) {
        NP = COUNTS[c];
        int fair = 100 / NP;
        const mp_pair M[] = {
            {AI_EASY, SP_RANDOM, 30, 90},
            {AI_MEDIUM, AI_EASY, 30, fair + 15},
            {AI_HARD, AI_EASY, 30, fair + 20},
            {AI_HARD, AI_MEDIUM, 30, fair - 3},     /* three or more: hard is medium without its noise */
            {AI_HARD, AI_HARD, 12, 0},
        };
        printf("== %d players\n", NP);
        fails += mp_run(M, (int)(sizeof M / sizeof M[0]), argc, argv);
    }
    return fails != 0;
}
