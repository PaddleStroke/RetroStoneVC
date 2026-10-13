/*
 * Fox and geese's bot (src/games/foxgeese_bot.c), both sides: its registry entry, its notation (a fox's chain of
 * jumps is one turn: "a4xc4xe4"), positions each side must get right, the cancel, then self-play at every level
 * (tests/ai_selfplay.h: each pairing plays both sides, the geese in even games for a). Headless: this file plays the
 * screen's part (foxgeese_live(), foxgeese_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_capture.h"

#include "games/foxgeese.h"

static fggame T;                                /* the "screen's" game: the bot reads it */
const fggame *foxgeese_live(void) { return &T; }

static int result(void)
{
    switch (fg_status(&T)) {
    case FS_PLAY: return -1;
    case FS_TRAPPED: return 0;
    case FS_FEW: case FS_STUCK: return 1;
    default: return 2;
    }
}

/* one whole turn, as the screen's script() takes it (a chain that stops before its end is refused) */
static int one(const char *s)
{
    int side = fg_pos(&T)->side;
    if (result() >= 0 || strchr(s, ' ') || !fg_script(&T, s)) return 0;
    return fg_pos(&T)->side != side || result() >= 0;
}
int foxgeese_show(const char *move) { return one(move); }

static void reset(void) { fg_new(&T); }
static int actor(void) { return fg_pos(&T)->side; }

static int random_move(ai_rng *r, char *m, int cap)
{
    fgpos p = *fg_pos(&T);
    int side = p.side, k = 0;
    do {                                        /* a whole turn: a fox's jumps on to the chain's end */
        fgmove l[FG_MAX_MOVES];
        int n = fg_legal(&p, l);
        if (!n || k + 8 > cap) return 0;
        fgmove x = l[ai_rand_n(r, n)];
        if (!k) k += snprintf(m + k, (size_t)(cap - k), "%c%c", 'a' + fg_x[x.from], '1' + fg_y[x.from]);
        k += snprintf(m + k, (size_t)(cap - k), "%c%c%c", x.over >= 0 ? 'x' : '-', 'a' + fg_x[x.to], '1' + fg_y[x.to]);
        fg_make(&p, x);
    } while (p.side == side && p.geese >= FG_FOX_WINS);
    return 1;
}

static int choose(int level, ai_stats *s)
{
    int mv[40];
    return fgb_turn(&T, level, 5, mv, 40, s) > 0;
}

/* a position: geese on points ("c1 d1"), the fox's point, side to move */
static void setup(const char *geese, const char *fox, int side)
{
    fg_new(&T);
    fgpos *p = &T.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->geese = 0;
    for (const char *s = geese; *s; s++)
        if (*s >= 'a' && *s <= 'g') {
            p->at[fg_point(s)] = FG_GOOSE;
            p->geese++;
            s++;
        }
    p->fox = (int8_t)fg_point(fox);
    p->at[p->fox] = FG_FOX;
    p->side = (uint8_t)side;
}

static int fox_jumps(void)
{
    fgpos q = *fg_pos(&T);
    q.side = FG_FOX;
    fgmove l[FG_MAX_MOVES];
    int n = fg_legal(&q, l), k = 0;
    for (int i = 0; i < n; i++) k += l[i].over >= 0;
    return k;
}

int main(int argc, char **argv)
{
    static const sp_game G = {
        .id = "foxgeese", .max_plies = 1200, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    cp_basics("foxgeese", &G, foxgeese_show);
    int all = 1;
    for (int l = AI_EASY; l <= AI_HARD; l++) {
        setup("b4 d4 c1 d1 e1 c2 d2 e2 a3 b3 g3", "a4", FG_FOX);
        all &= !strcmp(cp_move("foxgeese", FG_FOX, l), "a4xc4xe4");
    }
    cp_check("the fox: a chain of two jumps, every level takes it whole (a4xc4xe4)", all);
    all = 1;
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* the fox on d4 jumps d3 to d2: a goose fills d2 */
        setup("c1 d1 e1 c2 e2 a3 b3 c3 d3 e3 f3 g3", "d4", FG_GOOSE);
        const char *m = cp_move("foxgeese", FG_GOOSE, l);
        all &= m[0] && one(m) && !fox_jumps();
    }
    cp_check("the geese: a goose hanging, medium and hard cover it", all);
    cp_levels(argc, argv, &G, 8, choose);
    if (cp_fails) return 1;

    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 75},
        {AI_MEDIUM, AI_EASY, 40, 60},
        {AI_HARD, AI_EASY, 40, 70},
        {AI_HARD, AI_MEDIUM, 40, 55},
        {AI_EASY, AI_EASY, 10, 0},
        {AI_MEDIUM, AI_MEDIUM, 10, 0},
        {AI_HARD, AI_HARD, 10, 0},
    };
    return cp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
