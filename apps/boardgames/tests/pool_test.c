/*
 * Pool: the rules' test (the racks, the physics' repeatability, a ball pocketed by a real shot, the legal break, the
 * groups, the fouls and ball in hand, the 8 (won, lost, spotted on the break), 9-ball's lowest ball first and its 9,
 * the cue ball's place in hand, the notation, undo, long random games). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pool.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static pool_pos P;

static int overlaps(const pool_pos *p)
{
    for (int i = 0; i < p->w.n; i++)
        for (int j = i + 1; j < p->w.n; j++) {
            const cue_ball *a = &p->w.b[i], *b = &p->w.b[j];
            if (a->down >= 0 || b->down >= 0) continue;
            float R = a->r + b->r - 0.05f;
            if ((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) < R * R) return 1;
        }
    return 0;
}

static void test_racks(void)
{
    pool_new(&P, POOL_EIGHT, 5);
    CHECK(P.w.n == 16 && P.brk && P.in_hand && P.turn == 0 && P.group[0] == POOL_OPEN && !overlaps(&P));
    float d = 2 * POOL_R + 0.02f;
    CHECK(fabsf(P.w.b[8].x - (POOL_FOOT_X + 2 * d * 0.8660254f)) < 0.01f && fabsf(P.w.b[8].y - POOL_H / 2) < 0.01f);
    int corner[2] = {-1, -1};                   /* the back corners: a solid and a stripe */
    for (int i = 1; i < 16; i++)
        if (P.w.b[i].x > POOL_FOOT_X + 3.5f * d * 0.8660254f) corner[P.w.b[i].y > POOL_H / 2] = i;
    CHECK(corner[0] > 0 && corner[1] > 0 && pool_group_of(corner[0]) != pool_group_of(corner[1]));
    int apex = 0;
    for (int i = 1; i < 16; i++) apex += fabsf(P.w.b[i].x - POOL_FOOT_X) < 0.01f;
    CHECK(apex == 1);
    pool_pos Q;
    pool_new(&Q, POOL_EIGHT, 6);
    CHECK(memcmp(&P.w, &Q.w, sizeof P.w) != 0);   /* another seed, another rack */
    pool_new(&P, POOL_NINE, 5);
    CHECK(P.w.n == 10 && !overlaps(&P));
    CHECK(fabsf(P.w.b[1].x - POOL_FOOT_X) < 0.01f && fabsf(P.w.b[9].x - (POOL_FOOT_X + 2 * d * 0.8660254f)) < 0.01f);
    CHECK(pool_targets(&P) == 1u << 1);
}

static void test_repeat(void)
{
    pool_pos a, b;
    cue_events ea, eb;
    pool_new(&a, POOL_EIGHT, 1);
    pool_new(&b, POOL_EIGHT, 1);
    CHECK(pool_shot(&a, 0, 1000, 1, 20, 25.3f, &ea) && pool_shot(&b, 0, 1000, 1, 20, 25.3f, &eb));
    CHECK(!memcmp(&a, &b, sizeof a) && !memcmp(&ea, &eb, sizeof ea));
    CHECK(ea.first > 0);
}

/* every object ball but `keep` gathered at the foot end */
static void clear_table(pool_pos *p, int keep)
{
    int k = 0;
    for (int i = 1; i < p->w.n; i++) {
        if (i == keep) continue;
        p->w.b[i].x = 70 + (k % 5) * 4;
        p->w.b[i].y = 10 + (k / 5) * 4;
        k++;
    }
}

static void test_pocket_shot(void)
{
    pool_new(&P, POOL_EIGHT, 3);
    P.brk = 0;
    P.in_hand = 0;
    clear_table(&P, 3);
    P.w.b[3].x = 40;                            /* the 3 near the bottom left corner, the cue ball behind it */
    P.w.b[3].y = 40;
    P.w.b[0].x = 50;
    P.w.b[0].y = 30;
    float ux = 0 - 40, uy = 50 - 40, l = sqrtf(ux * ux + uy * uy);
    ux /= l;
    uy /= l;
    float gx = 40 - ux * 2 * POOL_R, gy = 40 - uy * 2 * POOL_R;
    cue_events e;
    CHECK(pool_shot(&P, cue_angle(gx - 50, gy - 30), 450, 0, 0, 0, &e));
    CHECK(e.first == 3 && P.w.b[3].down == 3);  /* pocket 3: the bottom left */
    CHECK(P.foul == POOL_FOUL_NONE && P.turn == 0 && P.group[0] == POOL_SOLIDS && P.group[1] == POOL_STRIPES);
    CHECK(pool_left(&P, POOL_SOLIDS) == 6);
}

/* the rules after a shot given by its events (the world at rest: the balls pocketed marked down) */
static void after(pool_pos *p, int first, const int *pot, int n, uint32_t rail, uint32_t rail_after)
{
    cue_events e;
    cue_events_clear(&e);
    e.first = first;
    e.rail = rail;
    e.rail_after = rail_after;
    for (int k = 0; k < n; k++) {
        p->w.b[pot[k]].down = 0;
        e.pot[e.npot++] = pot[k];
    }
    pool_apply(p, &e);
}

static void test_break(void)
{
    pool_new(&P, POOL_EIGHT, 2);                /* four balls at a cushion: legal, nothing in: the turn passes */
    after(&P, 1, NULL, 0, 0x1Eu, 0x1Eu);
    CHECK(P.foul == POOL_FOUL_NONE && P.turn == 1 && !P.in_hand && !P.brk && P.group[0] == POOL_OPEN);
    pool_new(&P, POOL_EIGHT, 2);                /* three: an illegal break, ball in hand */
    after(&P, 1, NULL, 0, 0x0Eu, 0x0Eu);
    CHECK(P.foul == POOL_FOUL_BREAK && P.turn == 1 && P.in_hand && !P.brk);
    pool_new(&P, POOL_EIGHT, 2);                /* a ball in: the same player again, the table still open */
    int p3[] = {3};
    after(&P, 1, p3, 1, 0, 0);
    CHECK(P.foul == POOL_FOUL_NONE && P.turn == 0 && P.group[0] == POOL_OPEN && pool_targets(&P) >> 9 & 1);
    pool_new(&P, POOL_EIGHT, 2);                /* the 8 on the break: spotted, nobody loses */
    int p8[] = {8};
    after(&P, 1, p8, 1, 0, 0);
    CHECK(P.status == POOL_PLAY && P.w.b[8].down < 0 && P.turn == 1 && !overlaps(&P));
    pool_new(&P, POOL_EIGHT, 2);                /* the 8 and the cue ball on the break: still spotted, a foul */
    int p80[] = {8, 0};
    after(&P, 1, p80, 2, 0, 0);
    CHECK(P.status == POOL_PLAY && P.w.b[8].down < 0 && P.foul == POOL_FOUL_SCRATCH && P.in_hand && P.turn == 1);
    pool_new(&P, POOL_EIGHT, 2);                /* the break from behind the head string only */
    CHECK(pool_place_ok(&P, 20, 25) && !pool_place_ok(&P, 30, 25) && !pool_shot(&P, 0, 900, 1, 40, 25, NULL));
    pool_new(&P, POOL_NINE, 2);                 /* 9-ball: the 1 first on the break too */
    after(&P, 2, NULL, 0, 0x3Eu, 0x3Eu);
    CHECK(P.foul == POOL_FOUL_WRONG && P.in_hand && P.turn == 1);
}

static void open_table(pool_pos *p, int v)
{
    pool_new(p, v, 4);
    p->brk = 0;
    p->in_hand = 0;
}

static void test_groups(void)
{
    open_table(&P, POOL_EIGHT);                 /* the first ball pocketed names the groups */
    int p10_3[] = {10, 3};
    after(&P, 10, p10_3, 2, 0, 0);
    CHECK(P.group[0] == POOL_STRIPES && P.group[1] == POOL_SOLIDS && P.turn == 0 && P.foul == POOL_FOUL_NONE);
    CHECK(pool_targets(&P) == ((1u << 9 | 1u << 11 | 1u << 12 | 1u << 13 | 1u << 14 | 1u << 15)));
    int p5[] = {5};
    after(&P, 9, p5, 1, 0, 0);                  /* struck a stripe, pocketed only a solid: the turn passes */
    CHECK(P.turn == 1 && P.foul == POOL_FOUL_NONE);
    after(&P, 9, NULL, 0, 1u << 9, 1u << 9);    /* solids struck a stripe first: a foul, ball in hand */
    CHECK(P.foul == POOL_FOUL_WRONG && P.turn == 0 && P.in_hand);
    open_table(&P, POOL_EIGHT);                 /* the 8 first on an open table: a foul */
    after(&P, 8, NULL, 0, 1u << 8, 1u << 8);
    CHECK(P.foul == POOL_FOUL_WRONG && P.turn == 1 && P.in_hand);
    open_table(&P, POOL_EIGHT);                 /* a foul names no group */
    int p2_0[] = {2, 0};
    after(&P, 2, p2_0, 2, 0, 0);
    CHECK(P.group[0] == POOL_OPEN && P.foul == POOL_FOUL_SCRATCH && P.turn == 1 && P.in_hand);
    CHECK(P.w.b[0].down < 0 && P.w.b[2].down >= 0 && !overlaps(&P));   /* the cue ball back; the 2 stays down */
    open_table(&P, POOL_EIGHT);                 /* nothing struck */
    after(&P, -1, NULL, 0, 1, 1);
    CHECK(P.foul == POOL_FOUL_MISS && P.turn == 1);
    open_table(&P, POOL_EIGHT);                 /* struck, but nothing at a cushion and nothing in */
    after(&P, 4, NULL, 0, 0, 0);
    CHECK(P.foul == POOL_FOUL_RAIL && P.in_hand);
    open_table(&P, POOL_EIGHT);                 /* the cue ball at a cushion after the contact is enough */
    after(&P, 4, NULL, 0, 1, 1);
    CHECK(P.foul == POOL_FOUL_NONE && !P.in_hand && P.turn == 1);
}

static void test_eight(void)
{
    int p8[] = {8}, p80[] = {8, 0};
    open_table(&P, POOL_EIGHT);                 /* the group cleared, the 8 in: won */
    P.group[0] = POOL_SOLIDS;
    P.group[1] = POOL_STRIPES;
    for (int i = 1; i <= 7; i++) P.w.b[i].down = 0;
    CHECK(pool_targets(&P) == 1u << 8);
    after(&P, 8, p8, 1, 0, 0);
    CHECK(P.status == POOL_WIN0);
    CHECK(!pool_shot(&P, 0, 500, 0, 0, 0, NULL));
    open_table(&P, POOL_EIGHT);                 /* the 8 with the cue ball: lost */
    P.group[0] = POOL_SOLIDS;
    P.group[1] = POOL_STRIPES;
    for (int i = 1; i <= 7; i++) P.w.b[i].down = 0;
    after(&P, 8, p80, 2, 0, 0);
    CHECK(P.status == POOL_WIN1);
    open_table(&P, POOL_EIGHT);                 /* the 8 early: lost */
    P.group[0] = POOL_SOLIDS;
    P.group[1] = POOL_STRIPES;
    int p2_8[] = {2, 8};
    after(&P, 2, p2_8, 2, 0, 0);
    CHECK(P.status == POOL_WIN1);
    open_table(&P, POOL_EIGHT);                 /* the last ball of the group and the 8 in one shot: lost (not
                                                 * cleared before it) */
    P.group[0] = POOL_SOLIDS;
    P.group[1] = POOL_STRIPES;
    for (int i = 1; i <= 6; i++) P.w.b[i].down = 0;
    int p7_8[] = {7, 8};
    after(&P, 7, p7_8, 2, 0, 0);
    CHECK(P.status == POOL_WIN1);
    open_table(&P, POOL_EIGHT);                 /* the second player wins the same way */
    P.turn = 1;
    P.group[0] = POOL_SOLIDS;
    P.group[1] = POOL_STRIPES;
    for (int i = 9; i <= 15; i++) P.w.b[i].down = 0;
    after(&P, 8, p8, 1, 0, 0);
    CHECK(P.status == POOL_WIN1);
}

static void test_nine(void)
{
    int p9[] = {9}, p5_9[] = {5, 9}, p3[] = {3};
    open_table(&P, POOL_NINE);
    CHECK(pool_targets(&P) == 1u << 1);
    after(&P, 3, p3, 1, 0, 0);                  /* not the lowest first: a foul, even with a ball in */
    CHECK(P.foul == POOL_FOUL_WRONG && P.turn == 1 && P.in_hand && P.w.b[3].down >= 0);
    open_table(&P, POOL_NINE);
    P.w.b[1].down = P.w.b[2].down = 0;
    CHECK(pool_targets(&P) == 1u << 3);
    after(&P, 3, p3, 1, 0, 0);                  /* the lowest in: shoot again */
    CHECK(P.foul == POOL_FOUL_NONE && P.turn == 0 && pool_targets(&P) == 1u << 4);
    open_table(&P, POOL_NINE);                  /* the 1 struck first, the 9 in by combination: won */
    after(&P, 1, p5_9, 2, 0, 0);
    CHECK(P.status == POOL_WIN0);
    open_table(&P, POOL_NINE);                  /* the 9 in on a foul: back on the foot spot */
    after(&P, 4, p9, 1, 0, 0);
    CHECK(P.status == POOL_PLAY && P.w.b[9].down < 0 && P.foul == POOL_FOUL_WRONG && P.turn == 1 && !overlaps(&P));
    pool_new(&P, POOL_NINE, 2);                 /* the 9 on a legal break: won */
    after(&P, 1, p9, 1, 0, 0);
    CHECK(P.status == POOL_WIN0);
}

static void test_place(void)
{
    open_table(&P, POOL_EIGHT);
    CHECK(!pool_place_ok(&P, 30, 25));          /* not in hand */
    P.in_hand = 1;
    CHECK(pool_place_ok(&P, 30, 25) && pool_place_ok(&P, 90, 45) && !pool_place_ok(&P, 0.5f, 25));
    CHECK(!pool_place_ok(&P, P.w.b[5].x + 1, P.w.b[5].y));   /* on a ball */
    int ang, pow, placed, n;
    float x, y;
    CHECK(pool_parse("9000:650@2000,2500 1:2", &ang, &pow, &placed, &x, &y, &n) && ang == 9000 && pow == 650 &&
          placed && fabsf(x - 20) < 1e-4f && fabsf(y - 25) < 1e-4f && n == 18);
    CHECK(pool_parse("100:7", &ang, &pow, &placed, &x, &y, &n) && !placed && n == 5);
    CHECK(!pool_parse("100", &ang, &pow, &placed, &x, &y, &n));
    static pool_game g;
    pool_game_new(&g, POOL_EIGHT, 9);
    CHECK(pool_play(&g, 0, 1000, 1, 20, 25, NULL) && g.ply == 1 && pool_undo(&g) && g.ply == 0 && !pool_undo(&g));
    CHECK(!pool_play(&g, 0, 1000, 1, 60, 25, NULL) && g.ply == 0);
}

/* long random games: no ball on another, all on the table or down, the rules never stall */
static void test_random(void)
{
    uint64_t s = 77;
    int ok = 1, shots = 0, ended = 0;
    for (int gnum = 0; gnum < 40; gnum++) {
        pool_new(&P, gnum & 1, (uint64_t)gnum);
        for (int k = 0; k < 300 && P.status == POOL_PLAY; k++) {
            int ang = (int)(cue_rand(&s) % 36000), pow = 50 + (int)(cue_rand(&s) % 951);
            float x = P.w.b[0].x, y = P.w.b[0].y;
            if (P.in_hand && cue_rand(&s) % 2) {
                x = 2 + (float)(cue_rand(&s) % 9600) / 100;
                y = 2 + (float)(cue_rand(&s) % 4600) / 100;
                if (P.brk) x = 2 + (float)(cue_rand(&s) % 2200) / 100;
                if (!pool_place_ok(&P, x, y)) { x = P.w.b[0].x; y = P.w.b[0].y; }
            }
            if (!pool_shot(&P, ang, pow, P.in_hand, x, y, NULL)) { ok = 0; break; }
            shots++;
            ok &= !overlaps(&P) && (P.w.b[0].down < 0 || P.status != POOL_PLAY);
            for (int i = 0; i < P.w.n; i++)
                ok &= P.w.b[i].down >= 0 || (P.w.b[i].x > -1 && P.w.b[i].x < 101 && P.w.b[i].y > -1 && P.w.b[i].y < 51);
        }
        ended += P.status != POOL_PLAY;
    }
    printf("     (%d random shots, %d of 40 games ended)\n", shots, ended);
    CHECK(ok && ended > 20);
}

int main(void)
{
    test_racks();
    test_repeat();
    test_pocket_shot();
    test_break();
    test_groups();
    test_eight();
    test_nine();
    test_place();
    test_random();
    printf("pool: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
