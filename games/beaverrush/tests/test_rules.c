/*
 * Beaver Rush: unit tests of the game rules against the tuning table (rules.c, world.c; no video, no sound).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/beaverrush/LICENSE.
 *
 * The expected curves are computed here in floating point from the REFERENCE formula (DESIGN.md: the chop
 * rate that holds the bar at score s is s / (2 + 0.15 s) per second); the integer game must follow them.
 *   test_rules [--seeds N]      (default 20000 seeds for the fairness test)
 */
#include "br.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

static double ref_rate(int level)
{
    double s = LEVEL_LOGS * level + LEVEL_MID;
    double r = s / (2 + 0.15 * s);
    if (level > LEVEL_ENDGAME) r += 0.12 * (level - LEVEL_ENDGAME);
    return r;
}

/* ---- the timer curves ------------------------------------------------------------------------------ */
static void test_curves(void)
{
    printf("timer curves (drain per level, refill)\n");
    CHECK(fabs(REFILL / (double)BAR_FULL - 7.0 / 120) < 1e-6, "refill %.5f of the bar", REFILL / (double)BAR_FULL);
    /* level 0: a full bar lasts 6 s */
    int32_t bar = BAR_FULL;
    int frames = 0;
    while (bar > 0) { bar -= drain_per_frame(0); frames++; }
    CHECK(abs(frames - BAR_FRAMES_L0) <= 1, "a full bar lasts %d frames at level 0 (360)", frames);
    int32_t prev = 0;
    printf("  level  logs   gnaws/s to hold (ref)   full bar lasts\n");
    for (int L = 0; L <= LEVEL_MAX; L++) {
        int32_t d = drain_per_frame(L);
        double rate = d * 60.0 / REFILL;
        CHECK(fabs(rate - ref_rate(L)) / ref_rate(L) < 0.01, "level %d: %.3f gnaws/s, reference %.3f", L, rate,
              ref_rate(L));
        CHECK(d > prev, "the drain grows with the level (%d: %d <= %d)", L, d, prev);
        prev = d;
        if (L <= 6 || L % 5 == 0)
            printf("  %5d  %4d   %5.2f (%5.2f)             %5.2f s\n", L, L * LEVEL_LOGS, rate, ref_rate(L),
                   BAR_FULL / (double)d / 60);
    }
    CHECK(level_of(19) == 0 && level_of(20) == 1 && level_of(299) == 14 && level_of(100000) == LEVEL_MAX, "levels");
    CHECK(fabs(ref_rate(0) - 2.857) < 0.01 && fabs(ref_rate(1) - 4.615) < 0.01 && fabs(ref_rate(2) - 5.263) < 0.01,
          "the reference points (2.86, 4.62, 5.26)");
}

/* ---- the gnaw rules -------------------------------------------------------------------------------------- */
static void clear_tree(tree *t)
{
    for (int i = 0; i < TRUNK_N; i++) memset(&t->s[i], 0, sizeof t->s[i]);
}

static int step1(world *w, int side)
{
    int press[MAX_PLAYERS] = {side, 0};
    world_step(w, press, 0);
    return w->events[0];
}

static void test_gnaw(void)
{
    printf("gnawing, hits, timer\n");
    world w;
    world_init(&w, 1, 7);
    for (int i = 0; i < START_CLEAR; i++) CHECK(!w.bv[0].tr.s[i].branch && !w.bv[0].tr.s[i].gold, "clear start %d", i);
    /* no drain before the first gnaw */
    for (int i = 0; i < 600; i++) step1(&w, 0);
    CHECK(w.bv[0].bar == BAR_START && w.bv[0].state == BV_READY && !w.started, "the bar waits for the first gnaw");
    /* the first gnaw starts, gnaws, refills (capped) and drains */
    int ev = step1(&w, SIDE_R);
    CHECK((ev & EV_START) && (ev & EV_GNAW) && (ev & EV_MOVE) && w.bv[0].logs == 1 && w.bv[0].side == SIDE_R,
          "the first gnaw: ev %x logs %d", ev, w.bv[0].logs);
    CHECK(w.bv[0].bar == BAR_FULL - drain_per_frame(0), "capped refill then one frame of drain");
    /* hit A: a branch on the beaver's side drops onto its head */
    world_init(&w, 1, 7);
    clear_tree(&w.bv[0].tr);
    w.bv[0].tr.s[1].branch = SIDE_L;
    ev = step1(&w, SIDE_L);
    CHECK((ev & EV_BONK) && (ev & EV_GNAW) && w.bv[0].state == BV_BONK && w.bv[0].bonk_hit == 1 && w.over,
          "hit A: ev %x state %d", ev, w.bv[0].state);
    world_init(&w, 1, 7);
    clear_tree(&w.bv[0].tr);
    w.bv[0].tr.s[1].branch = SIDE_L;
    ev = step1(&w, SIDE_R);
    CHECK(!(ev & EV_BONK) && w.bv[0].state == BV_PLAY && w.bv[0].tr.s[0].branch == SIDE_L, "the other side is safe");
    /* hit B: walking into the branch at head height (the log is not gnawed) */
    ev = step1(&w, SIDE_L);
    CHECK((ev & EV_BONK) && !(ev & EV_GNAW) && w.bv[0].bonk_hit == 2 && w.bv[0].logs == 1, "hit B: ev %x", ev);
    /* staying under a branch gnaws it away */
    world_init(&w, 1, 7);
    clear_tree(&w.bv[0].tr);
    w.bv[0].tr.s[1].branch = SIDE_L;
    step1(&w, SIDE_R);
    ev = step1(&w, SIDE_R);
    CHECK(!(ev & EV_BONK) && w.bv[0].logs == 2 && (w.bv[0].last_gnawed & 3) == SIDE_L, "the branch goes with its log");
    /* golden log */
    world_init(&w, 1, 7);
    clear_tree(&w.bv[0].tr);
    w.bv[0].tr.s[1].gold = 1;
    step1(&w, SIDE_L);
    for (int i = 0; i < 200; i++) step1(&w, 0);
    int32_t before = w.bv[0].bar;
    ev = step1(&w, SIDE_L);
    CHECK((ev & EV_GOLD) && w.bv[0].score == 2 + GOLD_BONUS && w.bv[0].logs == 2 && w.bv[0].golds == 1,
          "gold: score %d", w.bv[0].score);
    CHECK(w.bv[0].bar == before + REFILL + GOLD_REFILL - drain_per_frame(0), "gold refill");
    /* out of breath */
    world_init(&w, 1, 7);
    clear_tree(&w.bv[0].tr);
    step1(&w, SIDE_L);
    int n = 0;
    while (w.bv[0].state == BV_PLAY && n < 1000) { step1(&w, 0); n++; }
    CHECK(w.bv[0].state == BV_SLEEP && w.over && abs(n - BAR_FRAMES_L0) <= 2, "out of breath after %d frames", n);
    /* milestones and levels */
    world_init(&w, 1, 7);
    int ms = 0, lv = 0;
    for (int i = 0; i < 2 * STAGE_LOGS; i++) {
        clear_tree(&w.bv[0].tr);
        ev = step1(&w, SIDE_L);
        ms += (ev & EV_MILESTONE) != 0;
        lv += (ev & EV_LEVEL) != 0;
    }
    CHECK(ms == 2 && lv == 2 * STAGE_LOGS / LEVEL_LOGS && world_stage(&w) == 2, "milestones %d, levels %d", ms, lv);
    CHECK(medal_of(49) == 0 && medal_of(50) == 1 && medal_of(100) == 2 && medal_of(200) == 3 && medal_of(300) == 4,
          "medals");
}

/* ---- fairness: every reachable position, every step ------------------------------------------------ */
/* the sides the beaver may stand on after gnawing with s0 at the bottom and s1 above: not into s0's branch
 * (hit B), not under s1's (hit A) */
static int safe_sides(const seg *s0, const seg *s1)
{
    int m = 0;
    for (int p = SIDE_L; p <= SIDE_R; p++)
        if (s0->branch != p && s1->branch != p) m |= 1 << p;
    return m;
}

static void test_fairness(int seeds)
{
    printf("generator fairness: %d seeds x 1000 gnaws (half of them with stolen branches)\n", seeds);
    int bad = 0, unfair = 0, stolen = 0;
    seg lb = {SIDE_L, 0, 0, 0}, rb = {SIDE_R, 0, 0, 0}, no = {0, 0, 0, 0};
    CHECK(safe_sides(&lb, &rb) == 0 && safe_sides(&lb, &lb) == 1 << SIDE_R && safe_sides(&no, &rb) == 1 << SIDE_L,
          "the checker itself: L then R is impossible, L then L or empty then R is not");
    for (int k = 0; k < seeds; k++) {
        tree t;
        tree_init(&t, (uint32_t)(k * 2654435761u + 1));
        if (k & 2) tree_reroll(&t, (uint32_t)k * 40503u + 99);
        rs_rng r;
        rs_rng_seed(&r, (uint32_t)k + 12345);
        int alive = 1 << SIDE_L;                 /* the beaver starts on the left */
        for (int i = 0; i < 1000 && alive; i++) {
            if (!trunk_fair(t.s, TRUNK_N)) { unfair++; break; }
            /* the reachable positions: from any live one, any safe side (the move is free) */
            int next = safe_sides(&t.s[0], &t.s[1]);
            if (!next) { bad++; if (bad < 5) printf("  seed %d: no safe gnaw at %d\n", k, i); }
            alive = next;
            if ((k & 1) && rs_rng_range(&r, 50) == 0 && tree_insert_branch(&t, CHIP_SLOT, 1 + rs_rng_range(&r, 2)) >= 0)
                stolen++;
            tree_pop(&t);
        }
    }
    CHECK(unfair == 0, "%d trunks broke the rule (opposite branches in adjacent segments)", unfair);
    CHECK(bad == 0, "%d impossible sequences", bad);
    CHECK(stolen > seeds / 2, "stolen branches inserted: %d", stolen);
    printf("  %d sequences, no impossible step, %d stolen branches inserted\n", seeds, stolen);
    /* the insertion itself: never on a golden log, never next to the other side */
    tree t;
    tree_init(&t, 5);
    for (int i = 0; i < TRUNK_N; i++) memset(&t.s[i], 0, sizeof t.s[i]);
    t.s[7].branch = SIDE_L;
    t.s[8].gold = 1;
    t.s[10].branch = SIDE_L;
    int at = tree_insert_branch(&t, CHIP_SLOT, SIDE_R);
    CHECK(at == 9 && t.s[9].branch == SIDE_L && t.s[9].stolen, "stolen branch at %d side %d", at, t.s[9].branch);
    at = tree_insert_branch(&t, CHIP_SLOT, SIDE_R);
    CHECK(at == 11 && t.s[11].branch == SIDE_L, "next one at %d (on the left: the branch below)", at);
}

/* ---- statistics per stage ---------------------------------------------------------------------------- */
static void test_stats(void)
{
    enum { SEEDS = 2000, N = 600 };
    printf("generator statistics per stage (%d seeds)\n", SEEDS);
    int br[12] = {0}, cnt[12] = {0}, zig[12] = {0}, gold = 0, gold_gap_min = 1 << 30, maxe = 0, maxs = 0;
    for (int k = 0; k < SEEDS; k++) {
        gen g;
        gen_init(&g, (uint32_t)k * 7919u + 3);
        seg s[N];
        for (int i = 0; i < N; i++) s[i] = gen_next(&g);
        int last_gold = -1, e = 0, same = 0;
        for (int i = 0; i < N; i++) {
            int st = i / STAGE_LOGS;
            cnt[st]++;
            br[st] += s[i].branch != 0;
            if (i >= START_CLEAR) {
                e = s[i].branch ? 0 : e + 1;
                same = s[i].branch ? (i && s[i - 1].branch == s[i].branch ? same + 1 : 1) : 0;
                if (e > maxe) maxe = e;
                if (same > maxs) maxs = same;
            }
            if (s[i].gold) {
                CHECK(!s[i].branch, "a golden log has no branch");
                if (last_gold >= 0 && i - last_gold < gold_gap_min) gold_gap_min = i - last_gold;
                last_gold = i;
                gold++;
            }
            /* a zig-zag: X . Y . X with X != Y */
            if (i >= 4 && s[i].branch && s[i - 2].branch && s[i - 4].branch && !s[i - 1].branch && !s[i - 3].branch &&
                s[i].branch != s[i - 2].branch && s[i - 2].branch != s[i - 4].branch)
                zig[st]++;
        }
    }
    printf("  stage  branches  zig-zags/1000\n");
    for (int st = 0; st < N / STAGE_LOGS; st++)
        printf("  %5d  %5.1f%%   %6.1f\n", st, 100.0 * br[st] / cnt[st], 1000.0 * zig[st] / cnt[st]);
    printf("  golden logs: 1 per %.1f segments, closest %d apart; longest empty run %d, same-side run %d\n",
           (double)SEEDS * N / gold, gold_gap_min, maxe, maxs);
    CHECK(br[0] * 100 > cnt[0] * 30 && br[0] * 100 < cnt[0] * 45, "stage 0 branch share near the reference's 40%%");
    CHECK(br[6] > br[0], "denser later");
    CHECK(zig[4] > 3 * zig[0], "zig-zag runs are planned from stage 2 (chance ones before)");
    CHECK(zig[4] * 1000 > cnt[4] * 20, "zig-zag runs from stage 2");
    CHECK(gold_gap_min > GOLD_SPACING, "golden logs %d apart", gold_gap_min);
    CHECK(maxe <= MAX_EMPTY_RUN + 1 && maxs <= MAX_SAME_RUN, "runs: empty %d, same side %d", maxe, maxs);
}

/* ---- determinism --------------------------------------------------------------------------------------- */
static uint32_t run_hash(uint32_t seed, int players, int rate)
{
    world w;
    world_init(&w, players, seed);
    uint32_t h = 2166136261u;
    for (int f = 0; f < 6000 && !w.over; f++) {
        int press[MAX_PLAYERS] = {0, 0};
        for (int p = 0; p < players; p++)
            if (f % (rate + p) == 0) {
                int m = safe_sides(&w.bv[p].tr.s[0], &w.bv[p].tr.s[1]);
                press[p] = (m & (1 << w.bv[p].side)) ? w.bv[p].side : other_side(w.bv[p].side);
            }
        world_step(&w, press, f == 0 ? seed * 3 + 1 : 0);
        for (int p = 0; p < players; p++)
            h = (h ^ (uint32_t)(w.bv[p].bar + w.bv[p].score * 131 + w.bv[p].side)) * 16777619u;
    }
    return h ^ (uint32_t)w.bv[0].score;
}

static int perfect_player(int frames_per_gnaw, int *logs)
{
    world w;
    world_init(&w, 1, 4242);
    int f;
    for (f = 0; f < 60 * 600 && !w.over; f++) {
        int press[MAX_PLAYERS] = {0, 0};
        if (f % frames_per_gnaw == 0) {
            int m = safe_sides(&w.bv[0].tr.s[0], &w.bv[0].tr.s[1]);
            press[0] = (m & (1 << w.bv[0].side)) ? w.bv[0].side : other_side(w.bv[0].side);
        }
        world_step(&w, press, 0);
    }
    *logs = w.bv[0].logs;
    return w.bv[0].state;
}

static void test_determinism(void)
{
    printf("determinism and pace\n");
    CHECK(run_hash(11, 1, 9) == run_hash(11, 1, 9), "the same run twice (1 player)");
    CHECK(run_hash(11, 2, 8) == run_hash(11, 2, 8), "the same run twice (versus)");
    CHECK(run_hash(11, 1, 9) != run_hash(12, 1, 9), "another seed, another run");
    tree a, b;
    tree_init(&a, 99);
    tree_init(&b, 99);
    int same = 1;
    for (int i = 0; i < 5000; i++) {
        same &= !memcmp(&a.s[0], &b.s[0], sizeof a.s[0]);
        tree_pop(&a);
        tree_pop(&b);
    }
    CHECK(same, "two trees of one seed: the same 5000 segments (versus)");
    tree_init(&a, 99);
    tree_init(&b, 99);
    tree_reroll(&b, 1234);
    CHECK(!memcmp(a.s, b.s, sizeof a.s[0] * SEG_SHOWN), "a re-roll keeps the segments on screen");
    /* a perfect player at a fixed pace: where the curve stops it */
    int rates[] = {12, 10, 8, 7};
    for (int i = 0; i < 4; i++) {
        int logs, st = perfect_player(rates[i], &logs);
        printf("  a perfect player every %2d frames (%.1f/s): %s at %d logs\n", rates[i], 60.0 / rates[i],
               st == BV_SLEEP ? "out of breath" : st == BV_BONK ? "bonked" : "still going", logs);
        CHECK(st != BV_BONK, "a perfect player is never bonked");
        if (rates[i] == 12) CHECK(logs >= 100 && logs < 250, "5/s: out between 100 and 250 (%d)", logs);
        if (rates[i] == 10) CHECK(logs >= 300 && logs < 600, "6/s: out between 300 and 600 (%d)", logs);
        if (rates[i] == 7) CHECK(logs >= 700 && logs < 1200, "8.6/s: out between 700 and 1200 (%d)", logs);
    }
}

static void test_four_players(void)
{
    world w;
    world_init(&w, 4, 19);
    CHECK(w.players == 4 && world_standing(&w) == 4, "four beavers start on equal trees");
    for (int p = 1; p < 4; p++)
        CHECK(!memcmp(&w.bv[0].tr, &w.bv[p].tr, sizeof(tree)), "P%d has the same tree", p + 1);
    w.started = 1;
    for (int p = 0; p < 4; p++) w.bv[p].state = BV_PLAY;
    w.bv[1].score = 20; w.bv[2].score = 40; w.bv[3].score = 30;
    CHECK(world_steal_target(&w, 0) == 2, "a stolen branch targets the leading rival");
    w.bv[2].state = BV_SLEEP;
    CHECK(world_steal_target(&w, 0) == 3, "an eliminated leader is skipped");
    int none[MAX_PLAYERS] = {0};
    world_step(&w, none, 0);
    CHECK(!w.over && world_standing(&w) == 3, "one elimination leaves three playing");
    w.bv[1].state = BV_BONK;
    world_step(&w, none, 0);
    CHECK(!w.over && world_standing(&w) == 2, "two eliminations leave two playing");
    w.bv[0].state = BV_SLEEP;
    world_step(&w, none, 0);
    CHECK(w.over && w.winner == 3 && w.bv[3].state == BV_WIN, "the last beaver standing wins");
    int key[MAX_PLAYERS];
    world_rank_keys(&w, key);
    CHECK(key[3] > key[0] && key[0] > key[1] && key[1] > key[2], "results rank survival before score");
    CHECK(run_hash(11, 4, 8) == run_hash(11, 4, 8), "four-player match is deterministic");
}

int main(int argc, char **argv)
{
    int seeds = 20000;
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--seeds")) seeds = atoi(argv[++i]);
    test_curves();
    test_gnaw();
    test_fairness(seeds);
    test_stats();
    test_determinism();
    test_four_players();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "all passed", checks, fails);
    return fails ? 1 : 0;
}
