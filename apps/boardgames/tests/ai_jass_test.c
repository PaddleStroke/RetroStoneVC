/*
 * Jass's bot (src/games/jass_bot.c, trickbot.h), both variants: the core's rules against the game's (the legal cards,
 * the tricks' winners and points, card by card over random deals, every game: trumps, obenabe, undeufe), the
 * determinised deals, the view keeps to what the seat may know (the others' hidden cards swapped: the same move),
 * the choice of game (how often it pushes, which games, how the choosing side fares), the cancel, then self-play at
 * every level by sides (tests/ai_selfplay.h: seats 0 and 2 one level, 1 and 3 the other). Headless: this file plays
 * the screen's part (jass_live(), jass_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/jass.h"

void jasb_view(const jsgame *g, int me, tb_state *v, jsgame *t);
int  jasb_choose(const jsgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
void jasb_name(int a, char *move, int cap);

static jsgame T, U, S;
static int VAR = JASS_1000, dealt;
const jsgame *jass_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        jas_new(&T, VAR, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!jas_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int jass_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void) { return dealt && T.pos.phase == JP_OVER ? T.pos.first : -1; }

static int legal_acts(const jspos *p, int *acts)
{
    int k = 0;
    for (int m = 0; m < JM_MODES; m++)
        if (jas_legal(p, JA_MODE | m)) acts[k++] = JA_MODE | m;
    if (jas_legal(p, JA_PUSH)) acts[k++] = JA_PUSH;
    if (jas_legal(p, JA_WEIS)) acts[k++] = JA_WEIS;
    if (jas_legal(p, JA_NEXT)) acts[k++] = JA_NEXT;
    int pl = p->turn;
    if (p->phase == JP_PLAY)
        for (int i = 0; i < p->t.n[pl]; i++)
            if (jas_can_play(p, p->t.hand[pl][i])) acts[k++] = JA_PLAY | p->t.hand[pl][i];
    return k;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int acts[32], k = legal_acts(&T.pos, acts);
    if (!k) return 0;
    jasb_name(acts[ai_rand_n(r, k)], m, cap);
    return 1;
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != JP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

static void deal(int variant, uint32_t seed)
{
    VAR = variant;
    jas_new(&T, variant, seed);
    dealt = 1;
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 13);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, fresh = 1, modes = 0;
        for (int gn = 0; gn < 240 && same && tricks_ok; gn++) {
            deal(gn & 1, 500 + gn);
            fresh = 1;
            while (T.pos.phase != JP_OVER && T.pos.deal < 3 && same && tricks_ok) {
                const jspos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == JP_PLAY) {
                    int pl = p->turn;
                    jasb_view(&T, pl, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->t.n[pl]; i++) want += jas_can_play(p, p->t.hand[pl][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = jas_can_play(p, L[i]);
                    if (fresh) {
                        w = v;
                        tt_reveal(&w, &p->t.hand[0][0], TK4_HAND, p->t.n);
                        fresh = 0;
                        modes |= 1 << p->mode;
                    }
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->t.tricks, was_play = p->phase == JP_PLAY && m[0] != 'w';
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->t.tricks != before) {
                    for (int s = 0; s < TK4_P; s++)
                        tricks_ok &= w.won[s] == p->t.won[s] && w.pts[s] == p->t.eyes[s];
                    if (p->phase == JP_PLAY) tricks_ok &= w.turn == p->turn;
                    else tricks_ok &= tb_over(&w);
                }
                if (p->phase == JP_CHOOSE) fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns, games played %#x)", plays, modes);
        tt_check(name, same && modes == 0x3f);
        tt_check("a revealed deal played by the core: the same tricks, points, turns and end", tricks_ok);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            deal(gn & 1, 900 + gn);
            walk(&r, 4 + gn % 40);
            if (T.pos.phase != JP_PLAY) continue;
            jasb_view(&T, T.pos.turn, &v, &S);
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
    const bot *b = bot_find("jass");
    tt_check("registered: bot_find(\"jass\")", b && b->think && b->play == jass_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suits): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 50; gn++) {
            deal(gn & 1, 3000 + gn);
            walk(&r, 6 + gn % 30);
            jspos *p = &T.pos;
            if (p->phase != JP_PLAY || jas_can_weis(p)) continue;
            int me = p->turn, a = (me + 1) % 4, c = (me + 3) % 4, swapped = 0;
            for (int i = 0; i < p->t.n[a] && !swapped; i++)
                for (int j = 0; j < p->t.n[c] && !swapped; j++)
                    if (CARD_SUIT(p->t.hand[a][i]) == CARD_SUIT(p->t.hand[c][j])) {
                        uint8_t t = p->t.hand[a][i];
                        p->t.hand[a][i] = p->t.hand[c][j];
                        p->t.hand[c][j] = t;
                        swapped = 1;
                    }
            if (!swapped) continue;
            int m1 = jasb_choose(&T, me, AI_MEDIUM, 77, NULL);
            jsgame keep = T;
            jas_new(&T, keep.variant, keep.seed);
            for (int i = 0; i < keep.n; i++) jas_play(&T, keep.act[i]);
            same &= m1 == jasb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* the choice: whole deals by the bots (easy card play) */
        for (int var = 0; var < JASS_VARIANTS; var++) {
            int deals = 0, pushes = 0, mode_n[JM_MODES] = {0}, side = 0;
            for (int gn = 0; gn < 50; gn++) {
                deal(var, 7000 + gn);
                for (int guard = 0; guard < 200 && (T.pos.phase == JP_CHOOSE || T.pos.phase == JP_PLAY); guard++) {
                    const jspos *p = &T.pos;
                    int a = jasb_choose(&T, p->turn, p->phase == JP_PLAY ? AI_EASY : AI_HARD, gn * 31 + guard, NULL);
                    if (!jas_play(&T, a)) break;
                }
                const jspos *p = &T.pos;
                if (p->phase != JP_DEAL && p->phase != JP_OVER) continue;
                const jsdeal *d = &p->list[p->ndeals - 1];
                deals++;
                pushes += d->pushed;
                mode_n[d->mode]++;
                side += d->eyes[d->chooser & 1] + (d->match == (d->chooser & 1) + 1 ? 100 : 0);
            }
            snprintf(name, sizeof name, "%s choices: %d pushed in %d, games %d %d %d %d o%d u%d, choosers' side %d pts",
                     var ? "2500" : "1000", pushes, deals, mode_n[0], mode_n[1], mode_n[2], mode_n[3], mode_n[4],
                     mode_n[5], deals ? side / deals : 0);
            tt_check(name, deals >= 45 && side > deals * 90);
        }
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        deal(JASS_1000, 5);
        while (T.pos.phase != JP_PLAY || jas_can_weis(&T.pos)) one(".");
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
                walk(&r, 3 + k % 20 + retry);
                if (T.pos.phase != JP_PLAY || jas_can_weis(&T.pos) || T.pos.t.tricks > 4) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                jasb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "jass", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 50},
        {AI_HARD, AI_EASY, 40, 55},
    };
    tt_targets(P, (int)(sizeof P / sizeof P[0]), argc, argv);
    printf("-- to 1000\n");
    VAR = JASS_1000;
    int bad = sp_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
    static sp_pair Q[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 40, 50},
    };
    tt_targets(Q, (int)(sizeof Q / sizeof Q[0]), argc, argv);
    printf("-- to 2500\n");
    VAR = JASS_2500;
    bad |= sp_run(&G, Q, (int)(sizeof Q / sizeof Q[0]), argc, argv);
    return bad;
}
