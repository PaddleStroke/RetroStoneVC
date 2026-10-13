/*
 * Makruk's bot (src/games/makruk_bot.c): its registry entry, a free piece, a mate in one and in two, the cancel, the
 * levels' work, then self-play at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (makruk_live(), makruk_show()). --full for the 200 games a pairing and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/makruk.h"

int mkb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void mkb_root(const mkgame *g, void *root);

static mkgame T;
const mkgame *makruk_live(void) { return &T; }

static int one(const char *s)
{
    if (mk_status(&T) != MS_PLAY || strlen(s) != 4) return 0;
    return mk_script(&T, s);
}
int makruk_show(const char *move) { return one(move); }

static void reset(void) { mk_new(&T); }
static int actor(void) { return mk_pos(&T)->side; }
static int result(void)
{
    mkstatus s = mk_status(&T);
    return s == MS_PLAY ? -1 : s == MS_CHECKMATE ? mk_pos(&T)->side == MK_WHITE : 2;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    mkmove l[MK_MAX_MOVES];
    int n = mk_legal(mk_pos(&T), l);
    if (!n || cap < 5) return 0;
    mkmove x = l[ai_rand_n(r, n)];
    sprintf(m, "%c%c%c%c", 'a' + MK_FILE(x.from), '1' + MK_RANK(x.from), 'a' + MK_FILE(x.to), '1' + MK_RANK(x.to));
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* a position from lists of pieces: K Khun, M Met, S Khon, N Ma, R Rua, P Bia ("Ke1 Rh1") */
static void setup(int side, const char *white, const char *black)
{
    T.ply = 0;
    mkpos *p = &T.pos[0];
    memset(p, 0, sizeof *p);
    p->side = (uint8_t)side;
    for (int c = 0; c < 2; c++)
        for (const char *s = c ? black : white; *s; s += s[3] ? 4 : 3) {
            int kind = (int)(strchr(" PNSRMK", s[0]) - " PNSRMK");
            p->sq[mk_square(s + 1)] = MK_PC(c, kind);
        }
}

static void solve(const char *what, int side, const char *white, const char *black, const char *want, int from)
{
    const bot *b = bot_find("makruk");
    for (int l = from; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "", name[160];
        setup(side, white, black);
        int ok = b->think(side, l, m, sizeof m);
        snprintf(name, sizeof name, "%s (%s: %s)", what, sp_name(l), m);
        check(name, ok && strstr(want, m) && one(m));
    }
}

int main(int argc, char **argv)
{
    char name[160];
    int full = 0;
    for (int i = 1; i < argc; i++) full |= !strcmp(argv[i], "--full");
    const bot *b = bot_find("makruk");
    check("registered: bot_find(\"makruk\")", b && b->think && b->play == makruk_show);
    {
        char m[BOT_MOVE_CAP] = "";
        mk_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
    }
    solve("the free Ma taken", 0, "Ka1 Rd1 Pa3", "Kh8 Nd5 Ph6", "d1d5", AI_EASY);
    solve("mate in one", 0, "Kg6 Ra1", "Kh8", "a1a8", AI_EASY);
    for (int l = AI_MEDIUM; l <= AI_HARD; l++) {   /* a mate in two (several ways): every reply then mated */
        char m[BOT_MOVE_CAP] = "";
        setup(0, "Kf6 Ra1 Rb2", "Kh8");
        int ok = b->think(0, l, m, sizeof m) && one(m), all = ok;
        mkmove r[MK_MAX_MOVES], w[MK_MAX_MOVES];
        mkpos after = *mk_pos(&T);
        int nr = mk_legal(&after, r);
        for (int i = 0; i < nr && all; i++) {
            mkpos q = after;
            mk_make(&q, r[i]);
            int nw = mk_legal(&q, w), mate = 0;
            for (int j = 0; j < nw && !mate; j++) {
                mkpos z = q;
                mkmove y[MK_MAX_MOVES];
                mk_make(&z, w[j]);
                mate = !mk_legal(&z, y) && mk_in_check(&z, z.side);
            }
            all = mate;
        }
        snprintf(name, sizeof name, "mate in two (%s: %s)", sp_name(l), m);
        check(name, ok && nr > 0 && all);
    }
    {
        char m[BOT_MOVE_CAP] = "";
        mk_new(&T);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {
        int npos = full ? 24 : 8;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, nodes = 0, depth = 0, worst = 0;
            int done = 0;
            for (int k = 0; k < npos; k++) {
                mk_new(&T);
                for (int i = 0, n = 6 + 3 * k; i < n && mk_status(&T) == MS_PLAY; i++) {
                    char m[8];
                    random_move(&r, m, 8);
                    one(m);
                }
                if (mk_status(&T) != MS_PLAY) continue;
                static uint64_t root[1024];
                mkb_root(&T, root);
                ai_stats s;
                char m[8];
                mkb_choose(root, lv, 5, m, &s);
                ms += s.ms;
                nodes += s.nodes;
                depth += s.depth;
                if (s.ms > worst) worst = s.ms;
                done++;
            }
            if (done)
                printf("     %-6s %8.0f nodes %6.1f ms (worst %6.1f) depth %4.1f\n", sp_name(lv), nodes / done,
                       ms / done, worst, depth / done);
        }
    }
    if (fails) return 1;
    static const sp_game G = {
        .id = "makruk", .max_plies = 900, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 65},
        {AI_MEDIUM, AI_EASY, 50, 50},
        {AI_HARD, AI_EASY, 50, 65},
        {AI_HARD, AI_MEDIUM, 50, 50},
    };
    if (!full) P[1].min_pct = P[3].min_pct = 0;   /* three games: the close pairings' scores are noise (--full) */
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
