/*
 * Jelly Jam: the rules alone (src/play.c, no video, no sound, no screen width: this program links play.c with the
 * SDK's RNG only): the pair's moves, turns and wall kicks (the floor kick, the quick turn), landing and the split pair,
 * groups of 4 (not 3, not diagonal, not in the hidden row), gravity and chains, the points, the rain (amounts, the
 * offset, 5 rows at most, who gets it), losing, duels and rounds (best of 3), the brains' sanity (no suicide, they
 * fire when threatened, a strong chef beats a weak one), the layout and touch, determinism.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/jellyjam/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the SDK's RNG (play.c's only runtime calls), as sdk/src/rs_core.c: xorshift32 */
void rs_rng_seed(rs_rng *r, uint32_t s) { r->s = s ? s : 0x2545F491u; }
uint32_t rs_rng_next(rs_rng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->s = x;
}
int rs_rng_range(rs_rng *r, int n) { return n > 0 ? (int)(((uint64_t)rs_rng_next(r) * (uint32_t)n) >> 32) : 0; }

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static world W, W2;
static pin in[MAX_FIELDS];
static void idle(void) { memset(in, 0, sizeof in); }

/* a field with a pair at (col, rot), its pivot at row `row` */
static field *with_pair(int col, int rot, int row)
{
    static field f;
    field_reset(&f);
    f.state = FS_PAIR;
    f.pr.col = (int8_t)col;
    f.pr.rot = (int8_t)rot;
    f.pr.y = row * CQ;
    f.pr.c0 = 1;
    f.pr.c1 = 2;
    return &f;
}

/* a grid from text, top row first: '.' empty, R G B Y colours, o rain */
static void grid(uint8_t g[ROWS][COLS], const char *rows[], int n)
{
    memset(g, 0, sizeof(uint8_t) * ROWS * COLS);
    for (int i = 0; i < n; i++)
        for (int c = 0; c < COLS; c++) {
            char ch = rows[i][c];
            g[n - 1 - i][c] = (uint8_t)(ch == 'R' ? 1 : ch == 'G' ? 2 : ch == 'B' ? 3 : ch == 'Y' ? 4 : ch == 'o' ? RAIN : 0);
        }
}

/* run a field alone until its turn is over (back to spawning or idle) */
static void until_spawn(world *w, int i)
{
    for (int k = 0; k < 2000 && w->f[i].state != FS_SPAWN && w->f[i].state != FS_LOST; k++) world_step(w, in);
}

/* a VS world with n fields in play, the pairs not yet in (the tests set the fields) */
static void vs(world *w, int n)
{
    world_init(w, n, 77);
    world_start(w, 1);
    w->phase = PH_PLAY;
    for (int i = 0; i < n; i++) w->f[i].state = FS_CHECK;
}

static void test_moves(void)
{
    /* turns: the child goes round the pivot */
    field *f = with_pair(2, 0, 11);
    CHECK(pair_turn(f, 1) && f->pr.rot == 1 && f->pr.col == 2, "clockwise: the child to the right (%d, %d)", f->pr.rot, f->pr.col);
    CHECK(pair_turn(f, 1) && f->pr.rot == 2, "clockwise again: the child below");
    CHECK(pair_turn(f, -1) && pair_turn(f, -1) && pair_turn(f, -1) && f->pr.rot == 3, "counter-clockwise: the child to the left");
    /* moves stop at the walls */
    f = with_pair(2, 0, 11);
    int n = 0;
    while (pair_move(f, 1)) n++;
    CHECK(n == 3 && f->pr.col == 5, "right to the wall: 3 steps (%d), column %d", n, f->pr.col);
    /* the wall kick: at the right wall, turning the child into the wall pushes the pair left */
    CHECK(pair_turn(f, 1) && f->pr.col == 4 && f->pr.rot == 1, "wall kick at the right wall: column %d rot %d", f->pr.col, f->pr.rot);
    f = with_pair(0, 0, 11);
    CHECK(pair_turn(f, -1) && f->pr.col == 1 && f->pr.rot == 3, "wall kick at the left wall: column %d rot %d", f->pr.col, f->pr.rot);
    /* a stack beside the pair blocks a move and kicks a turn */
    f = with_pair(2, 0, 3);
    for (int r = 0; r < 5; r++) f->g[r][3] = 4;
    CHECK(!pair_move(f, 1), "a stack higher than the pair blocks the step");
    CHECK(pair_turn(f, 1) && f->pr.col == 1 && f->pr.rot == 1, "...and a turn into it kicks the pair away (%d)", f->pr.col);
    /* the floor kick: a landed pair turning its child into the floor climbs one row */
    f = with_pair(2, 1, 0);
    CHECK(pair_turn(f, 1) && f->pr.rot == 2 && f->pr.y == CQ, "floor kick: the pair climbs (y %d)", f->pr.y);
    /* the quick turn: in a well one column wide, the first press fails, a second quick one flips the pair */
    f = with_pair(2, 0, 9);
    for (int r = 0; r < 11; r++) f->g[r][1] = f->g[r][3] = 3;
    for (int r = 0; r < 8; r++) f->g[r][2] = 4;
    CHECK(!pair_turn(f, 1) && f->pr.rot == 0 && f->pr.qturn > 0, "in a well: the turn is blocked");
    CHECK(pair_turn(f, 1) && f->pr.rot == 2 && f->pr.col == 2, "...a second press flips the pair (rot %d)", f->pr.rot);
    f->pr.qturn = 0;
    f = with_pair(2, 0, 9);
    for (int r = 0; r < 11; r++) f->g[r][1] = f->g[r][3] = 3;
    for (int r = 0; r < 8; r++) f->g[r][2] = 4;
    pair_turn(f, 1);
    f->pr.qturn = 0;                                /* the second press too late */
    CHECK(!pair_turn(f, 1) && f->pr.rot == 0, "...not when the second press is too late");
    /* the quick flip at a wall asks for the floor of a child off the field: no column 6 (or -1) is read */
    f = with_pair(5, 1, 0);
    for (int r = 0; r < 11; r++) f->g[r][0] = 2;
    CHECK(pair_floor(f, &f->pr) == 0, "a child off the field: only the pivot's column counts (%d)", pair_floor(f, &f->pr));
    f = with_pair(0, 1, 12);
    f->pr.y = 13 * CQ - 20;
    for (int r = 0; r < 12; r++) f->g[r][0] = f->g[r][1] = (uint8_t)(1 + r % 2);
    CHECK(!pair_turn(f, -1) && !pair_turn(f, -1) && f->pr.rot == 1 && f->pr.col == 0,
          "at the top by the left wall: no turn up, no flip off the field (rot %d col %d)", f->pr.rot, f->pr.col);
    /* turning round and round on the floor (each floor kick lifts the pair a row) cannot hold the pair up for ever */
    world_init(&W, 2, 5);
    world_start(&W, 1);
    W.phase = PH_PLAY;
    W.f[1].state = FS_IDLE;
    f = &W.f[0];
    f->state = FS_PAIR;
    f->pr.col = 2;
    f->pr.rot = 1;
    f->pr.c0 = 1;
    f->pr.c1 = 2;
    int k = 0;
    for (; k < 5000 && f->state == FS_PAIR; k++) {
        idle();
        in[0].cw = k % 2 == 0;
        world_step(&W, in);
    }
    CHECK(f->state != FS_PAIR && k < (LOCK_RESETS + 2) * (CQ / FALL_START + LOCK_FRAMES + 4),
          "spinning on the floor: the pair lands all the same (%d frames)", k);
}

static void test_landing(void)
{
    /* a horizontal pair over uneven ground: it lands on the higher column, the other jelly falls on (the split) */
    world_init(&W, 2, 5);
    world_start(&W, 1);
    W.phase = PH_PLAY;
    field *f = &W.f[0];
    for (int r = 0; r < 4; r++) f->g[r][3] = 2;
    f->state = FS_PAIR;
    f->pr.col = 2;
    f->pr.rot = 1;
    f->pr.y = 11 * CQ;
    f->pr.c0 = 1;
    f->pr.c1 = 3;
    idle();
    in[0].down = 1;
    int k = 0;
    while (f->state == FS_PAIR && k++ < 200) world_step(&W, in);
    CHECK(f->g[4][3] == 3 && f->g[0][2] == 1, "the child on the stack, the pivot falls to the floor");
    CHECK(f->off[0][2] == 4 * CQ, "...it still has 4 rows to fall (%d)", f->off[0][2]);
    idle();
    until_spawn(&W, 0);
    CHECK(f->off[0][2] == 0 && f->state == FS_SPAWN, "...and lands");
    CHECK(f->score >= 7, "soft drop: a point a row (%d)", f->score);
    /* the pair waits LOCK_FRAMES on the ground, then lands by itself */
    world_init(&W, 2, 5);
    world_start(&W, 1);
    W.phase = PH_PLAY;
    f = &W.f[0];
    f->state = FS_PAIR;
    f->pr.col = 0;
    f->pr.y = 0;
    f->pr.c0 = f->pr.c1 = 1;
    idle();
    for (k = 0; k < LOCK_FRAMES - 1; k++) world_step(&W, in);
    CHECK(f->state == FS_PAIR, "on the ground: it waits");
    world_step(&W, in);
    CHECK(f->state != FS_PAIR && f->g[0][0] == 1 && f->g[1][0] == 1, "...and lands after %d frames", LOCK_FRAMES);
    /* the hidden row: a jelly above it is lost */
    uint8_t g[ROWS][COLS];
    memset(g, 0, sizeof g);
    for (int r = 0; r < ROWS - 1; r++) g[r][0] = (uint8_t)(1 + r % 2 * 2);
    grid_place(g, 0, 0, 2, 4);
    CHECK(g[ROWS - 1][0] == 2 && field_height(&W.f[0], 0) >= 0, "the pivot in the hidden row, the child above it lost");
}

static void test_groups(void)
{
    uint8_t g[ROWS][COLS];
    int pts, mel;
    const char *three[] = {"RR....", "R.....", "GGBYB."};
    grid(g, three, 3);
    CHECK(grid_resolve(g, &pts, &mel) == 0 && mel == 0, "3 touching: no melt");
    const char *four[] = {"RR....", "RR....", "GBGBY."};
    grid(g, four, 3);
    CHECK(grid_resolve(g, &pts, &mel) == 1 && mel == 4 && pts == 40, "4 touching: they melt, 40 points (%d)", pts);
    CHECK(g[0][0] == 2 && g[1][0] == 0, "...and the field is packed again");
    const char *diag[] = {"R.....", "GR....", "BGR...", "GBGRB."};
    grid(g, diag, 4);
    CHECK(grid_resolve(g, NULL, &mel) == 0, "diagonals do not touch");
    const char *rain[] = {"oo....", "RRRRo.", "oGBYB."};
    grid(g, rain, 3);
    CHECK(grid_resolve(g, NULL, &mel) == 1 && mel == 4, "rain next to a melting group...");
    int left = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) left += g[r][c] == RAIN;
    CHECK(left == 0, "...melts with it (%d left)", left);
    const char *grey[] = {"oooo..", "GBGB.."};
    grid(g, grey, 2);
    CHECK(grid_resolve(g, NULL, NULL) == 0, "4 rain jellies do not melt by themselves");
    /* the hidden row: jellies there never melt */
    memset(g, 0, sizeof g);
    for (int r = 0; r < VROWS; r++) g[r][0] = (uint8_t)(r % 2 ? 2 : 3);
    for (int r = 0; r < VROWS; r++) g[r][1] = (uint8_t)(r % 2 ? 3 : 2);
    g[VROWS][0] = g[VROWS][1] = g[VROWS][2] = 1;
    g[VROWS - 1][2] = 1;
    for (int r = 0; r < VROWS - 1; r++) g[r][2] = (uint8_t)(r % 2 ? 4 : 2);
    CHECK(grid_resolve(g, NULL, NULL) == 0, "three in the hidden row and one below: no melt");
}

static void test_chains(void)
{
    uint8_t g[ROWS][COLS];
    int pts, mel;
    /* a 2-chain: the reds melt, the blues fall onto the blue and melt */
    const char *two[] = {"B.....", "R.....", "RB....", "RB....", "RB...."};
    grid(g, two, 5);
    int ch = grid_resolve(g, &pts, &mel);
    int g4[1] = {4};
    int want = melt_points(1, 4, 1, g4, 1) + melt_points(2, 4, 1, g4, 1);
    CHECK(ch == 2 && mel == 8 && pts == want, "a 2-chain: %d steps, %d jellies, %d points (want %d)", ch, mel, pts, want);
    CHECK(want == 40 + 10 * 4 * 6, "chain power 6 on the 2nd step (%d)", want);
    /* a 3-chain */
    const char *three[] = {"G.....", "B.....", "RG....", "RB....", "RB....", "RBG...", "YYG..."};
    grid(g, three, 7);
    ch = grid_resolve(g, &pts, &mel);
    CHECK(ch == 3, "a 3-chain (%d)", ch);
    /* bonuses */
    int g5[1] = {5}, g44[2] = {4, 4};
    CHECK(melt_points(1, 5, 1, g5, 1) == 10 * 5 * 2, "a group of 5: + 2");
    CHECK(melt_points(1, 8, 2, g44, 2) == 10 * 8 * 3, "two colours at once: + 3");
    CHECK(melt_points(5, 4, 1, g4, 1) == 10 * 4 * 48, "chain power 48 on the 5th step");
    /* the live field: the same chain, step by step, with the animation */
    vs(&W, 2);
    grid(W.f[0].g, two, 5);
    idle();
    until_spawn(&W, 0);
    CHECK(W.f[0].max_chain == 2 && W.f[0].score == want, "the field plays the 2-chain: chain %d, score %d", W.f[0].max_chain,
          W.f[0].score);
}

static void test_rain(void)
{
    const char *two[] = {"B.....", "R.....", "RB....", "RB....", "RB...."};
    /* a 2-chain: 280 points = 4 rain (60 each), 40 kept for later */
    vs(&W, 2);
    grid(W.f[0].g, two, 5);
    idle();
    until_spawn(&W, 0);
    CHECK(W.f[1].rain_q == 280 / RAIN_POINTS && W.f[0].carry == 280 % RAIN_POINTS, "a 2-chain sends %d rain (carry %d)",
          W.f[1].rain_q, W.f[0].carry);
    /* the offset: rain coming to you is cancelled first */
    vs(&W, 2);
    grid(W.f[0].g, two, 5);
    W.f[0].rain_q = 10;
    until_spawn(&W, 0);
    int rained = 0;
    for (int c = 0; c < COLS; c++) rained += field_height(&W.f[0], c);
    CHECK(W.f[0].rain_q == 0 && rained == 6 && W.f[1].rain_q == 0, "offset: 10 coming, 4 cancelled, the 6 left fall (%d, %d sent)",
          rained, W.f[1].rain_q);
    vs(&W, 2);
    grid(W.f[0].g, two, 5);
    W.f[0].rain_q = 3;
    until_spawn(&W, 0);
    W2 = W;
    CHECK(W.f[0].rain_q == 0 && W.f[1].rain_q == 1, "offset: 3 cancelled, the 4th sent (%d, %d)", W.f[0].rain_q, W.f[1].rain_q);
    /* the rain falls when the turn is over: 5 rows at most, the rest waits */
    vs(&W, 2);
    W.f[1].rain_q = 40;
    idle();
    until_spawn(&W, 1);
    int n = field_count(&W.f[1]), even = 1;
    for (int c = 0; c < COLS; c++) even &= field_height(&W.f[1], c) == 5;
    CHECK(n == RAIN_MAX_DROP && even && W.f[1].rain_q == 10, "40 rain: 30 fall (5 a column), 10 wait (%d, %d)", n, W.f[1].rain_q);
    vs(&W, 2);
    W.f[1].rain_q = 9;
    until_spawn(&W, 1);
    int ones = 0, twos = 0;
    for (int c = 0; c < COLS; c++) ones += field_height(&W.f[1], c) == 1, twos += field_height(&W.f[1], c) == 2;
    CHECK(ones == 3 && twos == 3, "9 rain: a row, and 3 on random columns (%d, %d)", ones, twos);
    /* 3-4 players: the rain goes to the leader (the field with the least on it, and coming) */
    vs(&W, 4);
    for (int c = 0; c < COLS; c++) W.f[1].g[0][c] = W.f[2].g[0][c] = RAIN;
    W.f[3].g[0][0] = RAIN;
    CHECK(pick_target(&W, 0) == 3, "4 players: to the emptiest field (%d)", pick_target(&W, 0));
    W.f[3].rain_q = 12;
    CHECK(pick_target(&W, 0) == 1, "...counting the rain coming to it (%d)", pick_target(&W, 0));
    W.f[1].alive = 0;
    CHECK(pick_target(&W, 0) == 2, "...never a field out (%d)", pick_target(&W, 0));
    vs(&W, 3);
    CHECK(pick_target(&W, 1) == 2 && pick_target(&W, 2) == 0, "a tie: the next field round the table");
    rain_send(&W, 1, 7);
    CHECK(W.f[2].rain_q == 7 && W.f[1].sent == 7, "rain_send: 7 to field 3");
}

static void test_losing(void)
{
    /* a field filled up to the spawn cell loses when its next pair should come */
    vs(&W, 2);
    for (int r = 0; r <= SPAWN_ROW; r++) W.f[1].g[r][SPAWN_COL] = (uint8_t)(1 + r % 2);
    idle();
    until_spawn(&W, 1);
    world_step(&W, in);
    for (int k = 0; k < SPAWN_DELAY + 2; k++) world_step(&W, in);
    CHECK(!W.f[1].alive && W.f[1].state == FS_LOST, "full up to the spawn point: lost");
    CHECK(W.phase == PH_END && W.winner == 0 && W.f[0].wins == 1, "2 players: the last field standing wins the round");
    for (int k = 0; k < END_FRAMES + 1; k++) world_step(&W, in);
    CHECK(W.phase == PH_INTRO && W.round == 2 && field_count(&W.f[1]) == 0 && W.f[0].wins == 1, "round 2: the fields are empty again");
    W.phase = PH_PLAY;
    for (int r = 0; r <= SPAWN_ROW; r++) W.f[1].g[r][SPAWN_COL] = (uint8_t)(1 + r % 2);
    W.f[0].state = W.f[1].state = FS_SPAWN;
    for (int k = 0; k < SPAWN_DELAY + 2; k++) world_step(&W, in);
    CHECK(W.f[0].wins == 2, "P1 wins round 2");
    for (int k = 0; k < END_FRAMES + 1; k++) world_step(&W, in);
    CHECK(world_over(&W), "best of 3: 2 rounds won, the match is over");
    /* 4 players: the places */
    vs(&W, 4);
    for (int i = 1; i < 4; i++) {
        for (int r = 0; r <= SPAWN_ROW; r++) W.f[i].g[r][SPAWN_COL] = 1 + (r % 2);
        W.f[i].state = FS_SPAWN;
        W.f[i].t = (int16_t)(SPAWN_DELAY - (i == 3 ? 0 : i == 2 ? 5 : 10));
    }
    W.f[0].state = FS_IDLE;
    for (int k = 0; k < 40; k++) world_step(&W, in);
    CHECK(W.f[3].place == 4 && W.f[2].place == 3 && W.f[1].place == 2 && W.f[0].place == 1 && W.f[0].wins == 1,
          "4 players: places by elimination (%d %d %d %d)", W.f[0].place, W.f[1].place, W.f[2].place, W.f[3].place);
    /* fields out at the same moment share their place; all out at once: a draw, nobody 1st */
    vs(&W, 3);
    for (int i = 1; i < 3; i++) {
        for (int r = 0; r <= SPAWN_ROW; r++) W.f[i].g[r][SPAWN_COL] = 1 + (r % 2);
        W.f[i].state = FS_SPAWN;
        W.f[i].t = SPAWN_DELAY;
    }
    W.f[0].state = FS_IDLE;
    world_step(&W, in);
    CHECK(W.f[1].place == 3 && W.f[2].place == 3 && W.f[0].place == 1 && W.winner == 0,
          "3 players, two out at once: both 3rd (%d %d %d)", W.f[0].place, W.f[1].place, W.f[2].place);
    vs(&W, 2);
    for (int i = 0; i < 2; i++) {
        for (int r = 0; r <= SPAWN_ROW; r++) W.f[i].g[r][SPAWN_COL] = 1 + (r % 2);
        W.f[i].state = FS_SPAWN;
        W.f[i].t = SPAWN_DELAY;
    }
    world_step(&W, in);
    CHECK(W.phase == PH_END && W.winner == -1 && W.f[0].place == 2 && W.f[1].place == 2 && !W.f[0].wins && !W.f[1].wins,
          "2 players out at once: a draw (places %d %d)", W.f[0].place, W.f[1].place);
    /* a duel: the chef's field lost: the next chef, a level up, the bonus; the player's: game over */
    world_init(&W, 1, 9);
    world_start(&W, 1);
    CHECK(W.mode == MODE_DUEL && W.nf == 2 && W.f[1].kind == FK_CPU && W.f[1].level == 1, "1 player: a duel with chef 1");
    W.phase = PH_PLAY;
    for (int r = 0; r <= SPAWN_ROW; r++) W.f[1].g[r][SPAWN_COL] = 1 + (r % 2);
    W.f[1].state = FS_SPAWN;
    W.f[1].t = SPAWN_DELAY;
    W.f[0].state = FS_IDLE;
    world_step(&W, in);
    CHECK(W.phase == PH_END && W.winner == 0 && W.f[0].score == WIN_BONUS, "the chef lost: YOU WIN, +%d", W.f[0].score);
    for (int k = 0; k < END_FRAMES + 1; k++) world_step(&W, in);
    CHECK(W.level == 2 && W.phase == PH_INTRO && W.f[1].level == 2 && W.f[0].score == WIN_BONUS, "...the next chef: level %d", W.level);
    W.phase = PH_PLAY;
    for (int r = 0; r <= SPAWN_ROW; r++) W.f[0].g[r][SPAWN_COL] = 1 + (r % 2);
    W.f[0].state = FS_SPAWN;
    W.f[0].t = SPAWN_DELAY;
    W.f[1].state = FS_IDLE;
    world_step(&W, in);
    CHECK(world_over(&W) && W.lost, "the player's field lost: game over");
}

/* a whole game between two brains (a duel of two brains in a VS world); returns the winner */
static int brain_duel(const ai_params *a, const ai_params *b, uint32_t seed, int *frames)
{
    world_init(&W2, 2, seed);
    world_start(&W2, 1);
    W2.phase = PH_PLAY;
    for (int i = 0; i < 2; i++) W2.f[i].state = FS_SPAWN;
    int k = 0;
    while (W2.phase == PH_PLAY && k < 60 * 60 * 6) {
        pin p[MAX_FIELDS];
        memset(p, 0, sizeof p);
        ai_play(&W2, 0, a, &p[0]);
        ai_play(&W2, 1, b, &p[1]);
        world_step(&W2, p);
        k++;
    }
    if (frames) *frames = k;
    return W2.phase == PH_PLAY ? -1 : W2.winner;
}

/* a brain plans its pair (one stage a frame) */
static void plan(world *w, int fi, const ai_params *P)
{
    pin p;
    w->f[fi].ai.stage = 0;
    for (int k = 0; k < 20 && !(w->f[fi].ai.stage && w->f[fi].ai.planned); k++) ai_play(w, fi, P, &p);
}

static void test_brains(void)
{
    /* no suicide: with the spawn column almost full, the brain puts the pair elsewhere */
    world_init(&W, 2, 3);
    world_start(&W, 1);
    W.phase = PH_PLAY;
    field *f = &W.f[0];
    for (int r = 0; r < 10; r++) f->g[r][SPAWN_COL] = (uint8_t)(1 + r % 2 * 2);
    f->state = FS_SPAWN;
    f->t = SPAWN_DELAY;
    idle();
    world_step(&W, in);
    ai_params P;
    ai_skill_params(0, &P);
    plan(&W, 0, &P);
    CHECK(!(f->ai.tcol == SPAWN_COL && (f->ai.trot == 0 || f->ai.trot == 2)), "no suicide: not on the full column (%d, %d)",
          f->ai.tcol, f->ai.trot);
    /* threatened (rain coming), an aware brain fires the chain it has */
    world_init(&W, 2, 3);
    world_start(&W, 1);
    W.phase = PH_PLAY;
    f = &W.f[0];
    const char *ready[] = {"RRR...", "GGBB.."};
    grid(f->g, ready, 2);
    f->rain_q = 18;
    f->state = FS_PAIR;
    f->pr.col = SPAWN_COL;
    f->pr.y = SPAWN_ROW * CQ;
    f->pr.c0 = 1;
    f->pr.c1 = 3;
    f->seq = 1;
    plan(&W, 0, &P);
    uint8_t g[ROWS][COLS];
    memcpy(g, f->g, sizeof g);
    grid_place(g, f->ai.tcol, f->ai.trot, 1, 3);
    CHECK(grid_resolve(g, NULL, NULL) >= 1, "threatened: it melts the reds (%d, %d)", f->ai.tcol, f->ai.trot);
    /* every chef places pairs sensibly: a lone chef survives a while and fires chains */
    for (int lv = 1; lv <= CHEFS; lv += 3) {
        world_init(&W, 2, 11 + (uint32_t)lv);
        world_start(&W, 1);
        W.phase = PH_PLAY;
        W.f[0].state = FS_SPAWN;
        W.f[1].state = FS_IDLE;                     /* in, but idle: the round goes on */
        int k = 0;
        while (W.f[0].alive && k < 60 * 90) {
            pin q[MAX_FIELDS];
            memset(q, 0, sizeof q);
            ai_play(&W, 0, cpu_params(lv), &q[0]);
            world_step(&W, q);
            k++;
        }
        CHECK(W.f[0].alive && W.f[0].melted >= 40, "chef %d alone for 90 s: still in (%d), %d melted, best chain %d", lv,
              W.f[0].alive, W.f[0].melted, W.f[0].max_chain);
    }
    /* the strongest chef beats the weakest one */
    int strong = 0, games = 6;
    for (int s = 0; s < games; s++) strong += brain_duel(cpu_params(CHEFS), cpu_params(1), 100 + (uint32_t)s, NULL) == 0;
    CHECK(strong >= games - 1, "chef 8 beats chef 1: %d of %d", strong, games);
    /* determinism: the same duel twice */
    int f1, f2;
    int w1 = brain_duel(cpu_params(4), cpu_params(5), 42, &f1);
    uint32_t s1 = (uint32_t)(W2.f[0].score * 31 + W2.f[1].score);
    int w2 = brain_duel(cpu_params(4), cpu_params(5), 42, &f2);
    uint32_t s2 = (uint32_t)(W2.f[0].score * 31 + W2.f[1].score);
    CHECK(w1 == w2 && f1 == f2 && s1 == s2, "determinism: the same duel twice (%d %d %d / %d %d %d)", w1, f1, s1, w2, f2, s2);
}

static void test_layout_touch(void)
{
    flayout L[MAX_FIELDS];
    for (int nf = 2; nf <= 4; nf++) {
        int n = field_layout(nf, L), ok = 1;
        for (int i = 0; i < n; i++) {
            ok &= L[i].x - 4 >= 0 && L[i].x + L[i].w + 4 <= RS_SCREEN_W && (L[i].x - 4) % 8 == 0 && (L[i].y - 4) % 8 == 0;
            ok &= L[i].w == COLS * L[i].cell && L[i].h == VROWS * L[i].cell && L[i].y + L[i].h + 4 <= RS_SCREEN_H;
            if (i) ok &= L[i].x - 4 >= L[i - 1].x + L[i - 1].w + 4;
        }
        CHECK(n == nf && ok, "%d fields side by side on the 320-px stage, frames on the 8-px grid", nf);
    }
    CHECK(L[0].cell == 12 && field_layout(2, L) == 2 && L[0].cell == 16, "16-px cells for 2 fields, 12 for 3-4");
    CHECK(touch_column(2, 0, 16) == 2 && touch_column(2, 9, 16) == 3 && touch_column(2, 7, 16) == 2, "touch: a drag of half a cell moves");
    CHECK(touch_column(2, 30, 16) == 4 && touch_column(2, -40, 16) == 0 && touch_column(2, 300, 16) == 5, "touch: column by column, clamped");
    CHECK(touch_column(3, -18, 12) == 1, "touch: 12-px cells");
}

int main(void)
{
    test_moves();
    test_landing();
    test_groups();
    test_chains();
    test_rain();
    test_losing();
    test_brains();
    test_layout_touch();
    printf("jellyjam rules: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
