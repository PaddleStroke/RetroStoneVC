/*
 * Tile Rummy's bot (src/games/tilerummy_bot.c): its registry entry, a bot that reads no other rack nor the pool (the
 * hidden tiles shuffled: the same action), the first meld, a rearrangement only hard finds (a run's end taken into a
 * group), games of two to four bots in both variants to the end, then self-play at every level for two
 * (tests/ai_selfplay.h). Headless: this file plays the screen's part (tilerummy_live(), tilerummy_show()). --full for
 * the 200 games a pairing.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"

#include "cards.h"
#include "games/tilerummy.h"

int tilb_choose(const tgame *g, int seat, int level, char *move, int cap);

static tgame T, U;
static int dealt, NP = 2, VAR = TR_ONE;
const tgame *tilerummy_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        tr_new(&T, VAR, NP, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    if (!s[0] || ((s[0] == 'a' || s[0] == '#' || s[0] == '*') && !s[1])) return 0;
    U = T;
    if (!tr_script(&U, s)) return 0;
    T = U;
    return 1;
}
int tilerummy_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != TP_OVER) return -1;
    unsigned w = tr_winners(&T.pos);
    return w == 3 ? 2 : w == 1 ? 0 : 1;
}

/* a random player: the rules' plain turn now and then (whole), else a draw */
static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    if (T.pos.phase == TP_ROUND) { snprintf(m, (size_t)cap, "n"); return 1; }
    if (tr_changed(&T)) { snprintf(m, (size_t)cap, "x"); return 1; }
    snprintf(m, (size_t)cap, "d");
    (void)r;
    return 1;
}

static int fails;
static void check(const char *what, int ok)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    fails += !ok;
}

static int tile_of(int colour, int number, int copy)
{
    for (int t = 0, k = 0; t < TR_TILES; t++)
        if (!tr_is_joker(t) && CARD_SUIT(tr_face(t)) == colour && CARD_RANK(tr_face(t)) == number &&
            k++ == copy)
            return t;
    return -1;
}

/* plays the bot's turn to its end ("e" or "d"); the rack's tiles laid */
static int whole_turn(int level)
{
    int me = T.pos.turn, before = 0, after = 0;
    for (int t = 0; t < TR_TILES; t++) before += T.pos.where[t] == TR_RACK + me;
    for (int k = 0; k < 60; k++) {
        char m[BOT_MOVE_CAP];
        if (!tilb_choose(&T, me, level, m, sizeof m) || !one(m)) {
            printf("     level %d: \"%s\" refused (check %d)\n", level, m, tr_check(&T));
            return -100;
        }
        if (!strcmp(m, "e") || !strcmp(m, "d")) break;
    }
    for (int t = 0; t < TR_TILES; t++) after += T.pos.where[t] == TR_RACK + me;
    return before - after;
}

int main(int argc, char **argv)
{
    char what[160];
    const bot *b = bot_find("tilerummy");
    check("registered: bot_find(\"tilerummy\")", b && b->think && b->actor && b->play == tilerummy_show);

    /* honest: the other racks and the pool shuffled together (each keeps its count): the same action */
    {
        int same = 1, tried = 0;
        for (int gn = 0; gn < 30; gn++) {
            NP = 2 + gn % 3;
            tr_new(&T, TR_ONE, NP, 400 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < gn % 9 * NP && T.pos.phase == TP_PLAY; k++) whole_turn(AI_HARD);
            if (T.pos.phase != TP_PLAY) continue;
            int me = T.pos.turn;
            char m0[3][BOT_MOVE_CAP], m1[BOT_MOVE_CAP];
            for (int lv = 0; lv < 3; lv++) tilb_choose(&T, me, lv, m0[lv], BOT_MOVE_CAP);
            tgame *save = malloc(sizeof *save);
            *save = T;
            uint8_t hidden[TR_TILES];
            int n = 0;
            for (int t = 0; t < TR_TILES; t++) {
                int w = T.pos.where[t];
                if (w == TR_POOL || (w >= TR_RACK && w != TR_RACK + me)) hidden[n++] = (uint8_t)t;
            }
            uint8_t perm[TR_TILES];
            memcpy(perm, hidden, (size_t)n);
            cards_shuffle(perm, n, 9u + (uint32_t)gn);
            uint8_t w0[TR_TILES];
            memcpy(w0, T.pos.where, sizeof w0);
            for (int i = 0; i < n; i++) T.pos.where[perm[i]] = w0[hidden[i]];
            for (int i = 0; i < T.pos.npool; i++)
                for (int j = 0; j < n; j++)
                    if (T.pos.pool[i] == hidden[j]) { T.pos.pool[i] = perm[j]; break; }
            T.start = T.pos;
            for (int lv = 0; lv < 3; lv++) {
                tilb_choose(&T, me, lv, m1, sizeof m1);
                if (strcmp(m0[lv], m1)) printf("     game %d level %d: %s, then %s\n", gn, lv, m0[lv], m1), same = 0;
            }
            T = *save;
            free(save);
            tried++;
        }
        snprintf(what, sizeof what, "the hidden tiles shuffled: the same action at every level (%d positions)", tried);
        check(what, same && tried > 20);
    }
    /* the first meld: r10 r11 r12 (33) laid at every level; r1 r2 r3 (6) alone is not enough: a draw */
    {
        int ok = 1;
        for (int lv = 0; lv < 3; lv++) {
            NP = 2;
            tr_new(&T, TR_ONE, 2, 77);
            dealt = 1;
            for (int t = 0; t < TR_TILES; t++)
                if (T.pos.where[t] == TR_RACK) T.pos.where[t] = TR_POOL;
            int rack[] = {tile_of(0, 10, 0), tile_of(0, 11, 0), tile_of(0, 12, 0), tile_of(1, 1, 0), tile_of(2, 5, 0)};
            for (int i = 0; i < 5; i++) T.pos.where[rack[i]] = TR_RACK;
            T.start = T.pos;
            ok &= whole_turn(lv) == 3 && T.pos.out[0];
        }
        check("the first meld of 33 laid at every level", ok);
    }
    /* a rearrangement: the table's r3 r4 r5 r6, the rack's b6 and a6: hard takes r6 into a group of sixes */
    {
        int laid[3];
        for (int lv = 0; lv < 3; lv++) {
            tr_new(&T, TR_ONE, 2, 78);
            dealt = 1;
            for (int t = 0; t < TR_TILES; t++)
                if (T.pos.where[t] < TR_POOL || T.pos.where[t] == TR_RACK) T.pos.where[t] = TR_POOL;
            for (int v = 3; v <= 6; v++) T.pos.where[tile_of(0, v, 0)] = 0;
            T.pos.nsets = 1;
            T.pos.where[tile_of(1, 6, 0)] = TR_RACK;
            T.pos.where[tile_of(2, 6, 0)] = TR_RACK;
            T.pos.where[tile_of(3, 12, 0)] = TR_RACK;
            T.pos.out[0] = 1;
            T.start = T.pos;
            laid[lv] = whole_turn(lv);
        }
        check("hard rearranges the table (a run's end into a group); easy and medium draw",
              laid[2] == 2 && laid[0] < 0 && laid[1] < 0);
    }
    /* two to four bots, both variants, to the end */
    {
        int ok = 1, games = 0;
        for (int gn = 0; gn < 12 && ok; gn++) {
            NP = 2 + gn % 3;
            VAR = gn / 3 % TR_VARIANTS;
            tr_new(&T, VAR, NP, 90 + (uint32_t)gn);
            dealt = 1;
            for (int k = 0; k < 6000 && T.pos.phase != TP_OVER; k++) {
                char m[BOT_MOVE_CAP] = "";
                ai_set_seed(ai_mix((uint64_t)gn, (uint64_t)k));
                if (!b->think(T.pos.turn, gn % 3, m, sizeof m) || !one(m)) {
                    printf("     game %d: \"%s\" refused\n", gn, m);
                    ok = 0;
                    break;
                }
            }
            ok &= T.pos.phase == TP_OVER;
            games++;
        }
        snprintf(what, sizeof what, "%d games of two to four bots, both variants, end", games);
        check(what, ok);
    }
    if (fails) return 1;

    static const sp_game G = {
        .id = "tilerummy", .max_plies = 8000, .opening = 1, .reset = reset, .actor = actor,
        .result = result, .random_move = random_move, .script = one,
    };
    static const sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 100, 85}, {AI_MEDIUM, AI_EASY, 200, 55}, {AI_HARD, AI_MEDIUM, 200, 50},
        {AI_HARD, AI_EASY, 200, 58},
    };
    NP = 2;
    VAR = TR_ONE;
    return sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
