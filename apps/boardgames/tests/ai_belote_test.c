/*
 * Belote's bot (src/games/belote_bot.c, trickbot.h), both variants: the core's rules against the game's (the legal
 * cards, the tricks' winners and points, the deal's score, card by card over random deals), the determinised deals,
 * the view keeps to what the seat may know (the others' hidden cards swapped: the same move), the calls (how often
 * the bots take or bid, how often they make it), the cancel, then self-play at every level by sides
 * (tests/ai_selfplay.h: seats 0 and 2 one level, 1 and 3 the other). Headless: this file plays the screen's part
 * (belote_live(), belote_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/belote.h"

void belb_view(const bgame *g, int me, tb_state *v, bgame *t);
int  belb_choose(const bgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
void belb_name(int a, char *move, int cap);
void belb_score(const tb_state *st, int *sc);

static bgame T, U, S;
static int VAR = BEL_CLASSIC, dealt;
const bgame *belote_live(void) { return &T; }

/* one action of the notation, as belote_show() takes it; "D<seed>" deals the tests' game (the opening ply) */
static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        bel_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!bel_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int belote_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt ? bel_winner_team(&T.pos) : -1; }

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int acts[64], k = bel_moves(&T.pos, acts);
    if (!k) return 0;
    belb_name(acts[ai_rand_n(r, k)], m, cap);
    return 1;
}

static void walk(ai_rng *r, int plies)          /* random actions (not past the game's end) */
{
    for (int k = 0; k < plies && T.pos.phase != BP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

static void deal(int variant, uint32_t seed)
{
    VAR = variant;
    bel_new(&T, variant, seed);
    dealt = 1;
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 12);
    tb_state v, w;
    {   /* the core's rules are the game's: the legal cards, the tricks, the points and the deal's score */
        int same = 1, plays = 0, tricks_ok = 1, score_ok = 1, scored = 0, fresh = 1;
        for (int gn = 0; gn < 240 && same && tricks_ok && score_ok; gn++) {
            deal(gn & 1, 500 + gn);
            fresh = 1;
            while (T.pos.phase != BP_OVER && T.pos.deal < 3 && same && tricks_ok) {
                const bpos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == BP_PLAY) {
                    belb_view(&T, p->turn, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->n[p->turn]; i++) want += bel_legal(p, BA_PLAY | p->hand[p->turn][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = bel_legal(p, BA_PLAY | L[i]);
                    if (fresh) {
                        w = v;
                        tt_reveal(&w, &p->hand[0][0], BEL_HAND, p->n);
                        w.R->dealt(&w);
                        fresh = 0;
                    }
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->tricks, was_play = p->phase == BP_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && (p->tricks != before || p->phase != BP_PLAY)) {
                    int won[2] = {0, 0}, pts[2] = {0, 0};
                    for (int s = 0; s < BEL_P; s++) won[s & 1] += w.won[s], pts[s & 1] += w.pts[s];
                    if (p->phase == BP_PLAY)
                        tricks_ok &= won[0] == p->won[0] && won[1] == p->won[1] && pts[0] == p->points[0] &&
                                     pts[1] == p->points[1] && w.turn == p->turn;
                    else {
                        int sc[2];
                        tricks_ok &= tb_over(&w);
                        belb_score(&w, sc);
                        score_ok &= sc[0] == p->score[0] && sc[1] == p->score[1];
                        if (!score_ok)
                            printf("     game %d: score %d %d, the rules %d %d\n", gn, sc[0], sc[1], p->score[0],
                                   p->score[1]);
                        scored++;
                    }
                }
                if (!was_play && p->phase != BP_PLAY) fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns, both variants)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, points, turns and end", tricks_ok);
        snprintf(name, sizeof name, "the core's deal score = the rules' (%d deals)", scored);
        tt_check(name, score_ok && scored > 300);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            deal(gn & 1, 900 + gn);
            walk(&r, 4 + gn % 40);
            if (T.pos.phase != BP_PLAY) continue;
            belb_view(&T, T.pos.turn, &v, &S);
            for (int d = 0; d < 20 && ok; d++) {
                w = v;
                tb_determinise(&w, &r);
                ok &= tt_dealt_ok(&v, &w);
                tried++;
            }
        }
        snprintf(name, sizeof name, "determinised deals keep counts, voids, caps, own hand (%d)", tried);
        tt_check(name, ok && tried > 100);
    }
    const bot *b = bot_find("belote");
    tt_check("registered: bot_find(\"belote\")", b && b->think && b->play == belote_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suits): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 50; gn++) {
            deal(gn & 1, 3000 + gn);
            walk(&r, 8 + gn % 30);
            bpos *p = &T.pos;
            if (p->phase != BP_PLAY) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 3) % 4, swapped = 0;
            for (int i = 0; i < p->n[a] && !swapped; i++)
                for (int j = 0; j < p->n[c] && !swapped; j++)
                    if (CARD_SUIT(p->hand[a][i]) == CARD_SUIT(p->hand[c][j])) {
                        uint8_t t = p->hand[a][i];
                        p->hand[a][i] = p->hand[c][j];
                        p->hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            int m1 = belb_choose(&T, me, AI_MEDIUM, 77, NULL);
            bgame keep = T;
            bel_start(&T, keep.variant, keep.seed, keep.start);
            for (int i = 0; i < keep.n; i++) bel_play(&T, keep.act[i]);
            same &= m1 == belb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* the calls: whole deals by the bots (easy card play), how often they take and make it */
        for (int var = 0; var < BEL_VARIANTS; var++) {
            int deals = 0, taken = 0, made = 0, passed = 0, coinched = 0, level_sum = 0;
            for (int gn = 0; gn < 50; gn++) {
                deal(var, 7000 + gn);
                int start = T.pos.dealt;
                for (int guard = 0; guard < 400 && T.pos.phase != BP_OVER; guard++) {
                    const bpos *p = &T.pos;
                    if (p->phase == BP_DEAL) break;
                    if (p->dealt != start) { passed++; start = p->dealt; }
                    int a = belb_choose(&T, p->turn, p->phase == BP_PLAY ? AI_EASY : AI_HARD, gn * 31 + guard, NULL);
                    if (!bel_play(&T, a)) break;
                }
                const bpos *p = &T.pos;
                if (p->phase != BP_DEAL && p->phase != BP_OVER) continue;
                deals++;
                taken += p->taker != BEL_NONE;
                made += p->made;
                coinched += p->coinche > 0;
                level_sum += p->level;
            }
            if (var == BEL_CLASSIC)
                snprintf(name, sizeof name, "classic calls: %d deals passed out in %d, takers made %d of %d", passed,
                         deals + passed, made, deals);
            else
                snprintf(name, sizeof name, "coinche calls: %d passed out, mean contract %d, %d coinched, made %d of %d",
                         passed, deals ? 80 + 10 * level_sum / deals : 0, coinched, made, deals);
            tt_check(name, deals >= 45 && made * 100 >= deals * 55 && passed <= deals);
        }
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        deal(BEL_CLASSIC, 5);
        while (T.pos.phase != BP_PLAY) one(".");
        ai_cancel();
        double t0 = ai_now_ms();
        int ok = b->think(T.pos.turn, AI_HARD, m, sizeof m);
        double dt = ai_now_ms() - t0;
        ai_cancel_clear();
        tt_check("cancelled: no move, at once", !ok && dt < 50);
        tt_check("not the seat to move: no move", !b->think((T.pos.turn + 1) % 4, AI_HARD, m, sizeof m));
    }
    {   /* the levels' work */
        int npos = 6;
        for (int i = 1; i < argc; i++) npos = strcmp(argv[i], "--full") ? npos : 30;
        for (int lv = AI_EASY; lv <= AI_HARD; lv++) {
            double ms = 0, worst = 0, nodes = 0;
            int retry = 0;
            for (int k = 0; k < npos; k++) {
                deal(k & 1, 40 + k);
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 8 + k % 20 + retry);
                if (T.pos.phase != BP_PLAY || T.pos.tricks > 4) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                belb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "belote", .max_plies = 4000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 45},
        {AI_HARD, AI_EASY, 40, 55},
    };
    printf("-- classic\n");
    VAR = BEL_CLASSIC;
    int bad = tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 40, 45},
    };
    printf("-- coinche\n");
    VAR = BEL_COINCHE;
    bad |= tt_run(&G, Q, (int)(sizeof Q / sizeof Q[0]), argc, argv);
    return bad;
}
