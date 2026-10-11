/*
 * Mystery Face: the rules' test (the 24 faces and how the questions split them, the picks, one question a turn, the
 * faces put down and undone, the guess right and wrong, whole random games from a seed). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/mysteryface.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static mfgame G;
static const mfpos *P = &G.pos;

/* every question splits the 24 usefully, and the answers tell every face apart */
static void test_faces(void)
{
    int yes[MF_QUESTIONS] = {0};
    for (int f = 0; f < MF_FACES; f++)
        for (int q = 0; q < MF_QUESTIONS; q++) yes[q] += mf_has(f, q);
    for (int q = 0; q < MF_QUESTIONS; q++) CHECK(yes[q] >= 3 && yes[q] <= 12);
    CHECK(yes[MQ_HAT] == 5 && yes[MQ_GLASSES] == 5 && yes[MQ_BEARD] == 5 && yes[MQ_MOUSTACHE] == 5);
    CHECK(yes[MQ_EARRINGS] == 5 && yes[MQ_BALD] == 4 && yes[MQ_LONG] == 7 && yes[MQ_CURLY] == 5);
    CHECK(yes[MQ_EYE_BROWN] + yes[MQ_EYE_BLUE] + yes[MQ_EYE_GREEN] == MF_FACES);
    int hair = 0;
    for (int q = MQ_BLACK; q <= MQ_WHITE; q++) hair += yes[q];
    CHECK(hair == MF_FACES);
    int skin[SKINS] = {0};
    for (int f = 0; f < MF_FACES; f++) skin[MF_FACE[f].skin]++;
    for (int k = 0; k < SKINS; k++) CHECK(skin[k] >= 4);
    int alike = 0;
    for (int f = 0; f < MF_FACES; f++)
        for (int h = f + 1; h < MF_FACES; h++) {
            int same = 1;
            for (int q = 0; q < MF_QUESTIONS; q++) same &= mf_has(f, q) == mf_has(h, q);
            alike += same;
        }
    CHECK(alike == 0);
    for (int f = 0; f < MF_FACES; f++)         /* a hat never hides a bald head */
        CHECK(!(MF_FACE[f].style == STYLE_BALD && (MF_FACE[f].has & MF_HAT)));
}

static void test_turns(void)
{
    mf_new(&G);
    CHECK(P->phase == MF_PICK && P->turn == 0);
    CHECK(!mf_play(&G, MA_END));                /* no face picked yet */
    CHECK(!mf_play(&G, MA_ASK | MQ_HAT) && !mf_play(&G, MA_FLIP | 3) && !mf_play(&G, MA_GUESS | 3));
    CHECK(mf_play(&G, MA_PICK | 4) && mf_play(&G, MA_PICK | 3) && P->secret[0] == 3);
    CHECK(mf_undo(&G) && P->secret[0] == 4);    /* the pick taken back: the first one again */
    CHECK(mf_play(&G, MA_END) && P->turn == 1 && P->phase == MF_PICK);
    CHECK(!mf_undo(&G));                        /* never past the hand-off */
    CHECK(mf_play(&G, MA_PICK | 14) && mf_play(&G, MA_END));
    CHECK(P->phase == MF_PLAY && P->turn == 0 && P->secret[0] == 4 && P->secret[1] == 14);
    CHECK(!mf_play(&G, MA_END));                /* a question first */
    CHECK(mf_play(&G, MA_FLIP | 0) && P->down[0] == 1);
    CHECK(mf_play(&G, MA_ASK | MQ_HAT) && P->asked == MQ_HAT && P->answer == 1);   /* 14 wears a hat */
    CHECK(!mf_play(&G, MA_ASK | MQ_BEARD));     /* one question a turn */
    CHECK(!mf_play(&G, MA_GUESS | 14));         /* asked: no guess this turn */
    CHECK(mf_play(&G, MA_FLIP | 1) && mf_play(&G, MA_FLIP | 2) && mf_play(&G, MA_FLIP | 2));
    CHECK(P->down[0] == 3 && mf_up(P, 0) == 22 && mf_up(P, 1) == 24);
    CHECK(mf_undo(&G) && P->down[0] == 7);      /* the last stand-up undone */
    CHECK(mf_undo(&G) && mf_undo(&G) && P->down[0] == 1);
    CHECK(mf_undo(&G) && P->down[0] == 0 && P->asked == MQ_HAT);   /* the face put down before the question */
    CHECK(!mf_undo(&G));                        /* the question stays */
    CHECK(mf_play(&G, MA_END) && P->turn == 1 && P->asked == -1);
    CHECK(!mf_undo(&G));
    CHECK(mf_play(&G, MA_ASK | MQ_EYE_BROWN) && P->answer == 1);   /* 4: brown eyes */
    CHECK(mf_play(&G, MA_ASK | MQ_BLACK) == 0);
    CHECK(mf_play(&G, MA_END) && P->questions[0] == 1 && P->questions[1] == 1);
    CHECK(mf_play(&G, MA_FLIP | 14) && !mf_play(&G, MA_GUESS | 14));   /* a face put down cannot be guessed */
    CHECK(mf_play(&G, MA_FLIP | 14) && mf_play(&G, MA_GUESS | 14));
    CHECK(P->phase == MF_OVER && P->winner == 0 && P->guess == 14);
    CHECK(!mf_undo(&G) && !mf_play(&G, MA_FLIP | 1));

    mf_new(&G);                                 /* a wrong guess loses at once */
    CHECK(mf_script(&G, "p4 e p14 e q5 e q0 e g9"));
    CHECK(P->phase == MF_OVER && P->winner == 1 && P->guess == 9);
    CHECK(P->answer == -1);
    mf_new(&G);
    CHECK(!mf_script(&G, "p4 e p14 e q5 q6"));
    mf_new(&G);
    CHECK(!mf_script(&G, "x1") && !mf_script(&G, "p99"));
}

static uint32_t rng = 12345;
static int rnd(int n)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (int)(rng % (uint32_t)n);
}

/* whole games: players asking at random and putting down exactly the faces the answers rule out; the answers always
 * keep the secret standing, and someone always wins */
static void test_random_games(void)
{
    int games = 0, rightly = 0;
    for (int seed = 1; seed <= 300; seed++) {
        rng = (uint32_t)seed * 2654435761u + 1;
        mf_new(&G);
        CHECK(mf_play(&G, MA_PICK | rnd(MF_FACES)) && mf_play(&G, MA_END));
        CHECK(mf_play(&G, MA_PICK | rnd(MF_FACES)) && mf_play(&G, MA_END));
        int turns = 0;
        while (P->phase == MF_PLAY && turns < 200) {
            int me = P->turn, up = mf_up(P, me);
            int sure = up == 1 || (up <= 3 && rnd(3) == 0);
            if (sure) {
                int f = 0;
                while (P->down[me] >> f & 1) f++;
                CHECK(mf_play(&G, MA_GUESS | f));
                break;
            }
            int q = rnd(MF_QUESTIONS);
            CHECK(mf_play(&G, MA_ASK | q));
            for (int f = 0; f < MF_FACES; f++)
                if (!(P->down[me] >> f & 1) && mf_has(f, q) != P->answer) CHECK(mf_play(&G, MA_FLIP | f));
            CHECK(!(P->down[me] >> P->secret[!me] & 1));   /* the secret never goes down */
            if (rnd(4) == 0 && P->down[me]) {   /* a slip, undone */
                int f = 0;
                while (!(P->down[me] >> f & 1)) f++;
                uint32_t d = P->down[me];
                CHECK(mf_play(&G, MA_FLIP | f) && mf_undo(&G) && P->down[me] == d);
            }
            CHECK(mf_play(&G, MA_END));
            turns++;
        }
        CHECK(P->phase == MF_OVER && (P->winner == 0 || P->winner == 1));
        games++;
        rightly += P->winner == (P->guess == P->secret[!P->turn] ? P->turn : !P->turn);
        mfgame H;                               /* the same actions, the same position */
        mf_new(&H);
        for (int i = 0; i < G.n; i++) mf_play(&H, G.act[i]);
        CHECK(!memcmp(&H.pos, &G.pos, sizeof H.pos));
    }
    CHECK(games == 300 && rightly == 300);
}

int main(void)
{
    test_faces();
    test_turns();
    test_random_games();
    printf("mysteryface: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
