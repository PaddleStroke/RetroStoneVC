/*
 * Carrom: the rules' test (the opening circle, the physics' repeatability, a coin pocketed by a real shot, the striker
 * pocketed (a coin back, or owed), the queen and her cover, the last coin, the end, four players in teams, the
 * notation, undo, long random games keeping every piece). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/carrom.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static crm_pos P;

static int overlaps(const crm_pos *p)
{
    for (int i = 0; i < p->w.n; i++)
        for (int j = i + 1; j < p->w.n; j++) {
            const cue_ball *a = &p->w.b[i], *b = &p->w.b[j];
            if (a->down >= 0 || b->down >= 0 || i == CRM_STRIKER || j == CRM_STRIKER) continue;
            float R = a->r + b->r - 0.05f;
            if ((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) < R * R) return 1;
        }
    return 0;
}

static void test_new(void)
{
    crm_new(&P, 2);
    CHECK(P.w.n == CRM_N && P.turn == 0 && P.status == CRM_PLAY && P.queen == CRM_Q_BOARD);
    CHECK(P.w.b[CRM_QUEEN].x == 50 && P.w.b[CRM_QUEEN].y == 50);
    CHECK(!overlaps(&P));
    int w = 0, b = 0, near = 1;
    for (int i = 1; i < CRM_QUEEN; i++) {
        w += crm_colour(i) == 0;
        b += crm_colour(i) == 1;
        float dx = P.w.b[i].x - 50, dy = P.w.b[i].y - 50;
        near &= dx * dx + dy * dy < 11.5f * 11.5f;   /* inside the middle circle */
    }
    CHECK(w == 9 && b == 9 && near);
    CHECK(crm_pocketed(&P, 0) == 0 && crm_pocketed(&P, 1) == 0);
}

/* the same shot, the same result, to the last bit */
static void test_repeat(void)
{
    crm_pos a, b;
    crm_new(&a, 2);
    crm_new(&b, 2);
    cue_events ea, eb;
    CHECK(crm_shot(&a, 120, 26800, 1000, &ea) && crm_shot(&b, 120, 26800, 1000, &eb));
    CHECK(!memcmp(&a, &b, sizeof a) && !memcmp(&ea, &eb, sizeof ea));
    CHECK(ea.first > 0 && ea.steps > 100);
    crm_new(&b, 2);
    crm_shot(&b, 121, 26800, 1000, NULL);
    CHECK(memcmp(&a.w, &b.w, sizeof a.w) != 0);
}

/* every coin but `keep` out of the way, along the top edge */
static void clear_board(crm_pos *p, int keep)
{
    int k = 0;
    for (int i = 1; i < CRM_N; i++) {
        if (i == keep) continue;
        p->w.b[i].x = 30 + (k % 9) * 5;
        p->w.b[i].y = 12 + (k / 9) * 5;
        k++;
    }
}

/* a real shot: a white coin straight into the top left hole; the striker set where the shot is straight */
static void test_pocket_shot(void)
{
    crm_new(&P, 2);
    clear_board(&P, CRM_WHITE);
    P.w.b[CRM_WHITE].x = 18;
    P.w.b[CRM_WHITE].y = 30;
    float hx = CRM_HOLE_AT, hy = CRM_HOLE_AT, cx = 18, cy = 30;
    float ux = hx - cx, uy = hy - cy, l = sqrtf(ux * ux + uy * uy);
    ux /= l;
    uy /= l;
    float R = CRM_COIN_R + CRM_STRIKER_R, gx = cx - ux * R, gy = cy - uy * R, t = (gy - CRM_BASE) / uy;
    float sx = gx - t * ux;
    int pos = (int)lroundf((sx - 50) / CRM_BASE_HALF * 1000);
    CHECK(pos > -1000 && pos < 1000);
    float px, py;
    crm_spot(&P, 0, pos, &px, &py);
    cue_events e;
    CHECK(crm_shot(&P, pos, cue_angle(gx - px, gy - py), 650, &e));
    CHECK(e.first == CRM_WHITE);
    CHECK(P.w.b[CRM_WHITE].down == 0);          /* in hole 0, the top left */
    CHECK(crm_pocketed(&P, 0) == 1 && P.turn == 0 && !P.foul);   /* a coin of yours: shoot again */
    CHECK(P.w.b[CRM_STRIKER].down < 0 && fabsf(P.w.b[CRM_STRIKER].y - CRM_BASE) < 0.01f);
    /* a black coin pocketed by white: the turn passes, black has it */
    crm_new(&P, 2);
    clear_board(&P, CRM_BLACK);
    P.w.b[CRM_BLACK].x = 18;
    P.w.b[CRM_BLACK].y = 30;
    CHECK(crm_shot(&P, pos, cue_angle(gx - px, gy - py), 650, &e));
    CHECK(crm_pocketed(&P, 1) == 1 && crm_pocketed(&P, 0) == 0 && P.turn == 1);
    /* the striker's place must be free; out of range or a coin on it: refused */
    crm_new(&P, 2);
    P.w.b[CRM_WHITE].x = 50;
    P.w.b[CRM_WHITE].y = CRM_BASE;
    CHECK(!crm_spot_free(&P, 0, 0) && crm_spot_free(&P, 0, 800) && !crm_shot(&P, 0, 27000, 500, NULL));
    CHECK(!crm_shot(&P, 1001, 27000, 500, NULL) && !crm_shot(&P, 800, 27000, 0, NULL));
    CHECK(crm_free_pos(&P, 0, 0) != 0 && crm_spot_free(&P, 0, crm_free_pos(&P, 0, 0)));
}

/* the rules after a shot whose pockets are given (the world at rest: those pieces marked down) */
static void after(crm_pos *p, const int *pot, int n, int first)
{
    cue_events e;
    cue_events_clear(&e);
    e.first = first;
    for (int k = 0; k < n; k++) {
        p->w.b[pot[k]].down = 1;
        e.pot[e.npot++] = pot[k];
    }
    crm_apply(p, &e);
}

static void test_foul(void)
{
    crm_new(&P, 2);
    int s[] = {CRM_STRIKER}, w2[] = {CRM_WHITE + 1}, ws[] = {CRM_WHITE + 2, CRM_STRIKER};
    after(&P, s, 1, -1);                        /* the striker in, nothing pocketed yet: a coin owed */
    CHECK(P.foul && P.turn == 1 && P.debt[0] == 1 && P.w.b[CRM_STRIKER].down < 0);
    P.turn = 0;
    after(&P, w2, 1, CRM_WHITE + 1);            /* the next coin of white's pays the debt: back on the board */
    CHECK(P.debt[0] == 0 && crm_pocketed(&P, 0) == 0 && P.w.b[CRM_WHITE + 1].down < 0);
    CHECK(!overlaps(&P));
    crm_new(&P, 2);
    P.w.b[CRM_WHITE + 5].down = 0;              /* one pocketed before */
    after(&P, ws, 2, CRM_WHITE + 2);            /* a coin and the striker: that coin back, and one more */
    CHECK(P.foul && crm_pocketed(&P, 0) == 0 && P.debt[0] == 0 && P.turn == 1);
    CHECK(!overlaps(&P));
    crm_new(&P, 2);
    int bs[] = {CRM_BLACK, CRM_STRIKER};
    after(&P, bs, 2, CRM_BLACK);                /* the other side's coin stays theirs */
    CHECK(crm_pocketed(&P, 1) == 1 && P.debt[0] == 1);
}

static void test_queen(void)
{
    int q[] = {CRM_QUEEN}, w[] = {CRM_WHITE}, none[] = {0}, qw[] = {CRM_QUEEN, CRM_WHITE + 3}, b[] = {CRM_BLACK};
    (void)none;
    crm_new(&P, 2);
    after(&P, q, 1, CRM_QUEEN);                 /* pocketed: she waits for her cover, the turn kept */
    CHECK(P.queen == CRM_Q_PENDING && P.queen_team == 0 && P.turn == 0);
    after(&P, w, 1, CRM_WHITE);                 /* covered */
    CHECK(P.queen == CRM_Q_COVERED && P.queen_team == 0 && P.turn == 0 && P.w.b[CRM_QUEEN].down >= 0);
    crm_new(&P, 2);
    after(&P, q, 1, CRM_QUEEN);
    after(&P, NULL, 0, CRM_WHITE);              /* the cover missed: she goes back to the middle */
    CHECK(P.queen == CRM_Q_BOARD && P.queen_team == -1 && P.w.b[CRM_QUEEN].down < 0 && P.turn == 1);
    CHECK(!overlaps(&P));
    crm_new(&P, 2);
    after(&P, q, 1, CRM_QUEEN);
    after(&P, b, 1, CRM_BLACK);                 /* only the other side's coin: no cover */
    CHECK(P.queen == CRM_Q_BOARD && P.w.b[CRM_QUEEN].down < 0 && P.turn == 1 && crm_pocketed(&P, 1) == 1);
    crm_new(&P, 2);
    after(&P, qw, 2, CRM_QUEEN);                /* the queen and a coin at once: covered at once */
    CHECK(P.queen == CRM_Q_COVERED && P.queen_team == 0 && P.turn == 0);
    crm_new(&P, 2);
    int s[] = {CRM_STRIKER};
    after(&P, q, 1, CRM_QUEEN);
    after(&P, s, 1, -1);                        /* a foul while she waits: she goes back */
    CHECK(P.queen == CRM_Q_BOARD && P.w.b[CRM_QUEEN].down < 0 && P.turn == 1);
    crm_new(&P, 2);
    int qs[] = {CRM_QUEEN, CRM_STRIKER};
    after(&P, qs, 2, CRM_QUEEN);                /* the queen with the striker: back */
    CHECK(P.queen == CRM_Q_BOARD && P.w.b[CRM_QUEEN].down < 0);
}

static void test_end(void)
{
    int last[] = {CRM_WHITE + 8}, lastb[] = {CRM_BLACK + 8};
    crm_new(&P, 2);                             /* white's last coin, the queen still on the board: it goes back */
    for (int i = CRM_WHITE; i < CRM_WHITE + 8; i++) P.w.b[i].down = 0;
    after(&P, last, 1, CRM_WHITE + 8);
    CHECK(P.status == CRM_PLAY && crm_pocketed(&P, 0) == 8 && P.turn == 1);
    crm_new(&P, 2);                             /* the queen covered: the last coin wins */
    for (int i = CRM_WHITE; i < CRM_WHITE + 8; i++) P.w.b[i].down = 0;
    P.w.b[CRM_QUEEN].down = 0;
    P.queen = CRM_Q_COVERED;
    P.queen_team = 1;
    after(&P, last, 1, CRM_WHITE + 8);
    CHECK(P.status == CRM_WIN0);
    crm_new(&P, 2);                             /* white pockets black's last coin: black wins */
    for (int i = CRM_BLACK; i < CRM_BLACK + 8; i++) P.w.b[i].down = 0;
    P.w.b[CRM_QUEEN].down = 0;
    P.queen = CRM_Q_COVERED;
    P.queen_team = 0;
    after(&P, lastb, 1, CRM_BLACK + 8);
    CHECK(P.status == CRM_WIN1);
    CHECK(!crm_shot(&P, 0, 27000, 500, NULL));  /* over: no more shots */
}

static void test_four(void)
{
    crm_new(&P, 4);
    CHECK(crm_team(&P, 0) == 0 && crm_team(&P, 1) == 1 && crm_team(&P, 2) == 0 && crm_team(&P, 3) == 1);
    float x, y;
    crm_spot(&P, 1, 0, &x, &y);                 /* the right side's baseline */
    CHECK(fabsf(x - CRM_BASE) < 0.01f && fabsf(y - 50) < 0.01f);
    crm_spot(&P, 1, 1000, &x, &y);              /* their right: towards the top */
    CHECK(y < 50);
    crm_spot(&P, 2, 0, &x, &y);
    CHECK(fabsf(y - (100 - CRM_BASE)) < 0.01f);
    int order[5], k = 0;
    for (int i = 0; i < 5; i++) {
        order[k++] = P.turn;
        after(&P, NULL, 0, -1);
    }
    CHECK(order[0] == 0 && order[1] == 1 && order[2] == 2 && order[3] == 3 && order[4] == 0);
    crm_new(&P, 2);
    after(&P, NULL, 0, -1);
    CHECK(P.turn == 1);
    crm_spot(&P, 1, 0, &x, &y);                 /* two players: the second sits at the top */
    CHECK(fabsf(y - (100 - CRM_BASE)) < 0.01f);
}

static void test_notation(void)
{
    int pos, ang, pow, n;
    CHECK(crm_parse("-120:27000:650 5:1:2", &pos, &ang, &pow, &n) && pos == -120 && ang == 27000 && pow == 650 && n == 14);
    CHECK(!crm_parse("x", &pos, &ang, &pow, &n) && !crm_parse("1:2", &pos, &ang, &pow, &n));
    static crm_game g;
    crm_game_new(&g, 2);
    CHECK(crm_play(&g, 0, 27000, 900, NULL) && g.ply == 1);
    CHECK(crm_undo(&g) && g.ply == 0 && !crm_undo(&g));
    crm_pos q;
    crm_new(&q, 2);
    CHECK(!memcmp(crm_now(&g), &q, sizeof q));
}

/* long random games: every piece is on the board or pocketed, none on another, the rules never stall */
static void test_random(void)
{
    uint64_t s = 99;
    int ended = 0, shots = 0, ok = 1;
    for (int gnum = 0; gnum < 30; gnum++) {
        crm_new(&P, gnum % 3 == 2 ? 4 : 2);
        for (int k = 0; k < 400 && P.status == CRM_PLAY; k++) {
            int pos = crm_free_pos(&P, P.turn, (int)(cue_rand(&s) % 2001) - 1000);
            int ang = (int)(cue_rand(&s) % 36000), pow = 100 + (int)(cue_rand(&s) % 901);
            if (!crm_shot(&P, pos, ang, pow, NULL)) { ok = 0; break; }
            shots++;
            ok &= !overlaps(&P) && P.debt[0] >= 0 && P.debt[1] >= 0;
            for (int i = 0; i < CRM_N; i++)
                ok &= P.w.b[i].down >= 0 || (P.w.b[i].x >= 0 && P.w.b[i].x <= 100 && P.w.b[i].y >= 0 && P.w.b[i].y <= 100);
            ok &= P.queen != CRM_Q_COVERED || P.w.b[CRM_QUEEN].down >= 0;
            ok &= P.queen != CRM_Q_BOARD || P.w.b[CRM_QUEEN].down < 0;
        }
        ended += P.status != CRM_PLAY;
    }
    printf("     (%d random shots, %d of 30 games ended)\n", shots, ended);
    CHECK(ok);
}

int main(void)
{
    test_new();
    test_repeat();
    test_pocket_shot();
    test_foul();
    test_queen();
    test_end();
    test_four();
    test_notation();
    test_random();
    printf("carrom: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
