/*
 * Chess's bot (src/games/chess_bot.c): its registry entry, positions it must solve in every variant (a mate, a free
 * queen, the hill, the eighth rank, the third check, a blast, a drop), the cancel, the levels' work, then self-play in
 * each of the nine variants at every level (tests/ai_selfplay.h). Headless: this file plays the screen's part
 * (chess_live(), chess_show(), with chess_ui.c's notation). --full for the 200 games a pairing and the tables.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "games/chess.h"

int chb_choose(const void *root, int level, uint64_t seed, char *out, ai_stats *stats);
void chb_root(const cgame *g, void *root);

enum { V_STANDARD, V_960, V_CRAZYHOUSE, V_THREECHECK, V_HILL, V_LOSING, V_ATOMIC, V_RACING, V_HORDE, VARIANTS };
static const char *const NAMES[VARIANTS] = {"standard", "chess960", "crazyhouse", "three-check", "king of the hill",
                                            "losing chess", "atomic", "racing kings", "horde"};

static cgame T;                                 /* the "screen's" game: the bot reads it */
static int variant, games_begun;
const cgame *chess_live(void) { return &T; }

/* one move as chess_ui.c's script() takes it: "e2e4", "e7e8q", the king onto its rook, "N@f3" */
static int one(const char *s)
{
    cmove l[MAX_MOVES];
    if (game_status(&T) != ST_PLAY) return 0;
    int n = chess_legal(game_pos(&T), l);
    if (s[0] && s[1] == '@') {
        const char *t = strchr("PNBRQ", s[0]);
        if (!t || !*t || strlen(s) != 4) return 0;
        int to = (s[2] - 'a') + (s[3] - '1') * 8;
        for (int i = 0; i < n; i++)
            if (l[i].drop == (int)(t - "PNBRQ") + PAWN && l[i].to == to) return game_play(&T, l[i]);
        return 0;
    }
    if (strlen(s) < 4 || strlen(s) > 5) return 0;
    int from = (s[0] - 'a') + (s[1] - '1') * 8, to = (s[2] - 'a') + (s[3] - '1') * 8;
    int want = s[4] == 'n' ? KNIGHT : s[4] == 'r' ? ROOK : s[4] == 'b' ? BISHOP : s[4] == 'k' ? KING : QUEEN;
    for (int i = 0; i < n; i++)
        if (l[i].from == from && l[i].to == to && !l[i].drop && (!l[i].promo || l[i].promo == want))
            return game_play(&T, l[i]);
    return 0;
}
int chess_show(const char *move) { return one(move); }

static void start(int v)
{
    if (v == V_960) game_new960(&T, (games_begun * 389 + 17) % 960);
    else if (v >= V_CRAZYHOUSE) game_new_rules(&T, CR_CRAZYHOUSE + v - V_CRAZYHOUSE);
    else game_new(&T);
}
static void reset(void) { start(variant); games_begun++; }
static int actor(void) { return game_pos(&T)->side; }

static int result(void)
{
    const cpos *p = game_pos(&T);
    switch (game_status(&T)) {
    case ST_PLAY: return -1;
    case ST_CHECKMATE: case ST_CHECKS: case ST_HILL: case ST_EXPLODED: case ST_HORDE: return p->side == WHITE;
    case ST_ANTI: return p->side == BLACK;
    case ST_RACE: {
        int b = chess_king(p, BLACK);
        return b >= 0 && RANK_OF(b) == 7;
    }
    default: return 2;
    }
}

static void notation(cmove m, char *out)
{
    if (m.drop) {
        sprintf(out, "%c@%c%c", "?PNBRQ"[m.drop], 'a' + FILE_OF(m.to), '1' + RANK_OF(m.to));
        return;
    }
    sprintf(out, "%c%c%c%c", 'a' + FILE_OF(m.from), '1' + RANK_OF(m.from), 'a' + FILE_OF(m.to), '1' + RANK_OF(m.to));
    if (m.promo) { out[4] = "??nbrqk"[m.promo]; out[5] = 0; }
}

static int random_move(ai_rng *r, char *m, int cap)
{
    cmove l[MAX_MOVES];
    int n = chess_legal(game_pos(&T), l);
    if (!n || cap < 6) return 0;
    notation(l[ai_rand_n(r, n)], m);
    return 1;
}

static int fails;
static void check(const char *name, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", name);
    fails += !ok;
}

/* the position from a FEN under the variant's rules, the moves before it none */
static void setup(int rules, const char *fen)
{
    T.ply = 0;
    chess_from_fen(&T.pos[0], fen);
    T.pos[0].rules = (uint8_t)rules;
}

/* every level plays one of the moves (space-separated) from the position */
static void solve(const char *what, int rules, const char *fen, const char *want, int from_level)
{
    const bot *b = bot_find("chess");
    for (int l = from_level; l <= AI_HARD; l++) {
        char m[BOT_MOVE_CAP] = "", name[200];
        setup(rules, fen);
        int ok = b->think(game_pos(&T)->side, l, m, sizeof m), hit = 0;
        for (const char *w = want; ok && *w; w += strcspn(w, " "), w += *w == ' ') {
            size_t k = strcspn(w, " ");
            hit |= strlen(m) == k && !strncmp(m, w, k);
        }
        snprintf(name, sizeof name, "%s (%s: %s)", what, sp_name(l), m);
        check(name, ok && hit && one(m));
    }
}

int main(int argc, char **argv)
{
    char name[200];
    int full = 0, only = -1;
    for (int i = 1; i < argc; i++) {
        full |= !strcmp(argv[i], "--full");
        if (!strcmp(argv[i], "--variant") && i + 1 < argc) only = atoi(argv[++i]);
    }
    const bot *b = bot_find("chess");
    check("registered: bot_find(\"chess\")", b && b->think && b->play == chess_show);

    {   /* not its turn: no move; each level opens with a legal move */
        char m[BOT_MOVE_CAP] = "";
        game_new(&T);
        check("not the seat to move: no move", !b->think(1, AI_HARD, m, sizeof m));
        for (int l = AI_EASY; l <= AI_HARD; l++) {
            game_new(&T);
            int ok = b->think(0, l, m, sizeof m);
            snprintf(name, sizeof name, "%s opens with a legal move (%s)", sp_name(l), m);
            check(name, ok && one(m));
        }
    }
    solve("the free queen taken", CR_CHESS, "rnb1kbnr/pppp1ppp/8/4p1q1/3P4/8/PPP1PPPP/RNBQKBNR w KQkq - 0 3", "c1g5",
          AI_EASY);
    solve("mate in one", CR_CHESS, "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", "h5f7",
          AI_EASY);
    {   /* Chess960's castling in the bot's notation (the king onto its rook) is the script's castling */
        int good = 0, tried = 0;
        cmove l[MAX_MOVES];
        setup(CR_CHESS, "1r4kr/8/8/8/8/8/8/1R4KR w HBhb - 0 1");
        int n = chess_legal(game_pos(&T), l);
        for (int i = 0; i < n; i++) {
            if (!(l[i].flags & MV_CASTLE)) continue;
            char m[8];
            notation(l[i], m);
            setup(CR_CHESS, "1r4kr/8/8/8/8/8/8/1R4KR w HBhb - 0 1");
            tried++;
            good += one(m) && game_pos(&T)->sq[chess_castle_king(l[i])] == PC(WHITE, KING) &&
                    game_pos(&T)->sq[chess_castle_rook(l[i])] == PC(WHITE, ROOK);
        }
        check("Chess960 castling: the king onto its rook, both ways", tried == 2 && good == 2);
    }
    solve("mate in one with two rooks", CR_CHESS, "6k1/8/6K1/8/8/8/R7/1R6 w - - 0 1", "b1b8 a2a8", AI_EASY);
    solve("mate in two (a rook's ladder)", CR_CHESS, "7k/8/8/8/8/8/R7/1R4K1 w - - 0 1", "a2a7 b1b7", AI_MEDIUM);
    solve("king of the hill: onto the hill", CR_HILL, "rnbqkbnr/pppppppp/8/8/8/3K4/PPPPPPPP/RNBQ1BNR w kq - 0 1",
          "d3d4 d3e4", AI_EASY);
    solve("racing kings: home first", CR_RACING, "8/1K6/8/8/8/8/krbnNBR1/qrbnNBRQ w - - 0 1", "b7a8 b7b8 b7c8",
          AI_EASY);
    solve("three-check: the third check", CR_THREECHECK, "rnbqkbnr/ppp2ppp/8/3pp3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3 +2+0",
          "f1b5", AI_EASY);
    solve("atomic: the king blown up", CR_ATOMIC, "rnbqkbnr/pppp1ppp/8/4p2Q/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1",
          "h5f7", AI_EASY);
    solve("crazyhouse: a drop mates", CR_CRAZYHOUSE, "6rk/6pp/8/8/8/8/8/4K3[N] w - - 0 1", "N@f7", AI_EASY);
    solve("losing chess: the forced capture", CR_ANTI, "8/8/8/3p4/4P3/8/8/8 w - - 0 1", "e4d5", AI_EASY);
    solve("horde: the last pawn taken", CR_HORDE, "4k3/8/8/8/8/3P4/2q5/8 b - - 0 1", "c2d3", AI_EASY);
    {   /* cancelled: hard gives up at once, and says so */
        char m[BOT_MOVE_CAP] = "";
        game_new(&T);
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(0, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        check("cancelled: no move, at once", !ok && dt < 50);
    }
    {   /* the levels' work on positions of random games (not checked: the calibration, alone on one core) */
        int npos = full ? 24 : 6;
        ai_rng r;
        ai_seed(&r, 1234);
        for (int v = 0; v < VARIANTS; v++) {
            if (only >= 0 && v != only) continue;
            printf("     %s\n", NAMES[v]);
            for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
                double ms = 0, nodes = 0, depth = 0, worst = 0;
                int done = 0;
                for (int k = 0; k < npos; k++) {
                    start(v);
                    for (int i = 0, n = 6 + 2 * k; i < n && game_status(&T) == ST_PLAY; i++) {
                        char m[8];
                        random_move(&r, m, 8);
                        one(m);
                    }
                    if (game_status(&T) != ST_PLAY) continue;
                    static uint64_t root[1024];
                    chb_root(&T, root);
                    ai_stats s;
                    char m[8];
                    chb_choose(root, lv, 5, m, &s);
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
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "chess", .max_plies = 900, .opening = 2, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 50, 65},
        {AI_MEDIUM, AI_EASY, 50, 50},
        {AI_HARD, AI_EASY, 50, 65},
        {AI_HARD, AI_MEDIUM, 50, 50},
    };
    if (!full) P[1].min_pct = P[3].min_pct = 0;   /* three games: the close pairings' scores are noise (--full) */
    int bad = 0;
    for (variant = 0; variant < VARIANTS; variant++) {
        if (only >= 0 && variant != only) continue;
        printf("---- %s\n", NAMES[variant]);
        fflush(stdout);
        bad |= sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    }
    return bad;
}
