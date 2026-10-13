/*
 * Klaverjas's bot (src/games/klaverjas_bot.c, trickbot.h): the core's rules against the game's (the legal cards,
 * the tricks' winners, points and roem, the deal's score with pit and nat, card by card over random deals), the
 * determinised deals, the view keeps to what the seat may know (the others' hidden cards swapped: the same move), the
 * trump calls (how often the bots play and make it), the cancel, then self-play at every level by sides
 * (tests/ai_selfplay.h: seats 0 and 2 one level, 1 and 3 the other). Headless: this file plays the screen's part
 * (klaverjas_live(), klaverjas_show()). --full for the 200 games and the table.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "ai_selfplay.h"
#include "ai_tricks.h"

#include "games/klaverjas.h"

void kvjb_view(const kvgame *g, int me, tb_state *v, kvgame *t);
int  kvjb_choose(const kvgame *g, int seat, int level, uint64_t seed, ai_stats *stats);
void kvjb_name(int a, char *move, int cap);
void kvjb_score(const tb_state *st, int *sc);

static kvgame T, U, S;
static int dealt;
const kvgame *klaverjas_live(void) { return &T; }

static int one(const char *s)
{
    if (!dealt) {
        if (s[0] != 'D') return 0;
        kvj_new(&T, (uint32_t)strtoul(s + 1, NULL, 10));
        dealt = 1;
        return 1;
    }
    U = T;
    if (!kvj_script(&U, s) || U.n != T.n + 1) return 0;
    T = U;
    return 1;
}
int klaverjas_show(const char *move) { return dealt && one(move); }

static void reset(void) { dealt = 0; }
static int actor(void) { return dealt ? T.pos.turn : 0; }
static int result(void)
{
    if (!dealt || T.pos.phase != KP_OVER) return -1;
    return T.pos.total[0] == T.pos.total[1] ? 2 : T.pos.total[1] > T.pos.total[0];
}

static int legal_acts(const kvpos *p, int *acts)
{
    int k = 0;
    if (kvj_legal(p, KA_PASS)) acts[k++] = KA_PASS;
    for (int s = 0; s < 4; s++)
        if (kvj_legal(p, KA_TRUMP | s)) acts[k++] = KA_TRUMP | s;
    if (kvj_legal(p, KA_NEXT)) acts[k++] = KA_NEXT;
    int pl = p->turn;
    if (p->phase == KP_PLAY)
        for (int i = 0; i < p->t.n[pl]; i++)
            if (kvj_can_play(p, p->t.hand[pl][i])) acts[k++] = KA_PLAY | p->t.hand[pl][i];
    return k;
}

static int random_move(ai_rng *r, char *m, int cap)
{
    if (!dealt) { snprintf(m, (size_t)cap, "D%u", ai_rand(r) >> 2); return 1; }
    int acts[32], k = legal_acts(&T.pos, acts);
    if (!k) return 0;
    kvjb_name(acts[ai_rand_n(r, k)], m, cap);
    return 1;
}

static void walk(ai_rng *r, int plies)
{
    for (int k = 0; k < plies && T.pos.phase != KP_OVER; k++) {
        char m[BOT_MOVE_CAP];
        random_move(r, m, sizeof m);
        one(m);
    }
}

static void deal(uint32_t seed)
{
    kvj_new(&T, seed);
    dealt = 1;
}

int main(int argc, char **argv)
{
    char name[160];
    ai_rng r;
    ai_seed(&r, 14);
    tb_state v, w;
    {   /* the core's rules are the game's */
        int same = 1, plays = 0, tricks_ok = 1, score_ok = 1, scored = 0, fresh = 1;
        for (int gn = 0; gn < 200 && same && tricks_ok && score_ok; gn++) {
            deal(500 + gn);
            fresh = 1;
            while (T.pos.phase != KP_OVER && T.pos.deal < 4 && same && tricks_ok) {
                const kvpos *p = &T.pos;
                char m[BOT_MOVE_CAP];
                if (p->phase == KP_PLAY) {
                    int pl = p->turn;
                    kvjb_view(&T, pl, &v, &S);
                    uint8_t L[TB_HAND];
                    int k = tb_legal(&v, L), want = 0;
                    for (int i = 0; i < p->t.n[pl]; i++) want += kvj_can_play(p, p->t.hand[pl][i]);
                    same = k == want;
                    for (int i = 0; i < k && same; i++) same = kvj_can_play(p, L[i]);
                    if (fresh) {
                        w = v;
                        tt_reveal(&w, &p->t.hand[0][0], TK4_HAND, p->t.n);
                        fresh = 0;
                    }
                    plays++;
                }
                random_move(&r, m, sizeof m);
                int before = p->t.tricks, was_play = p->phase == KP_PLAY;
                if (was_play) {
                    const char *s = m;
                    tb_play(&w, trick_parse(&s));
                }
                one(m);
                if (was_play && p->t.tricks != before) {
                    for (int s = 0; s < TK4_P; s++) tricks_ok &= w.won[s] == p->t.won[s] && w.pts[s] == p->t.eyes[s];
                    if (p->phase == KP_PLAY) tricks_ok &= w.turn == p->turn;
                }
                if (was_play && p->phase != KP_PLAY) {   /* the deal scored */
                    int sc[2];
                    const kvdeal *d = &p->list[p->ndeals - 1];
                    tricks_ok &= tb_over(&w);
                    kvjb_score(&w, sc);
                    score_ok &= sc[0] == d->score[0] && sc[1] == d->score[1];
                    scored++;
                }
                if (p->phase == KP_BID) fresh = 1;
            }
        }
        snprintf(name, sizeof name, "the core's legal cards = the game's (%d turns)", plays);
        tt_check(name, same);
        tt_check("a revealed deal played by the core: the same tricks, points, turns and end", tricks_ok);
        snprintf(name, sizeof name, "the core's deal score (roem, pit, nat) = the rules' (%d deals)", scored);
        tt_check(name, score_ok && scored > 300);
    }
    {   /* the determinisation */
        int ok = 1, tried = 0;
        for (int gn = 0; gn < 80 && ok; gn++) {
            deal(900 + gn);
            walk(&r, 4 + gn % 40);
            if (T.pos.phase != KP_PLAY) continue;
            kvjb_view(&T, T.pos.turn, &v, &S);
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
    const bot *b = bot_find("klaverjas");
    tt_check("registered: bot_find(\"klaverjas\")", b && b->think && b->play == klaverjas_show && b->actor);
    {   /* honest: the others' hidden cards swapped (same suits): the same move */
        int same = 1, tried = 0;
        for (int gn = 0; gn < 50; gn++) {
            deal(3000 + gn);
            walk(&r, 6 + gn % 30);
            kvpos *p = &T.pos;
            if (p->phase != KP_PLAY) continue;
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
            int m1 = kvjb_choose(&T, me, AI_MEDIUM, 77, NULL);
            kvgame keep = T;
            kvj_new(&T, keep.seed);
            for (int i = 0; i < keep.n; i++) kvj_play(&T, keep.act[i]);
            same &= m1 == kvjb_choose(&T, me, AI_MEDIUM, 77, NULL);
            tried++;
        }
        snprintf(name, sizeof name, "the others' hidden cards swapped: the same move (%d positions)", tried);
        tt_check(name, same && tried >= 20);
    }
    {   /* the calls: whole deals by the bots (easy card play) */
        int deals = 0, forced = 0, made = 0;
        for (int gn = 0; gn < 60; gn++) {
            deal(7000 + gn);
            for (int guard = 0; guard < 200 && (T.pos.phase == KP_BID || T.pos.phase == KP_PLAY); guard++) {
                const kvpos *p = &T.pos;
                forced += kvj_must_name(p);
                int a = kvjb_choose(&T, p->turn, p->phase == KP_PLAY ? AI_EASY : AI_HARD, gn * 31 + guard, NULL);
                if (!kvj_play(&T, a)) break;
            }
            const kvpos *p = &T.pos;
            if (p->phase != KP_DEAL && p->phase != KP_OVER) continue;
            deals++;
            made += p->list[p->ndeals - 1].made;
        }
        snprintf(name, sizeof name, "calls: %d forced (all passed) in %d deals, made %d", forced, deals, made);
        tt_check(name, deals >= 55 && made * 100 >= deals * 65 && forced * 2 <= deals);
    }
    {   /* cancelled: no move, at once */
        char m[BOT_MOVE_CAP] = "";
        deal(5);
        while (T.pos.phase != KP_PLAY) one(".");
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
                deal(40 + k);
                ai_seed(&r, 99 + k + 1000 * (uint64_t)retry++);
                walk(&r, 3 + k % 20 + retry % 8);
                if (T.pos.phase != KP_PLAY || T.pos.t.tricks > 4) { k--; continue; }
                ai_stats s;
                double t0 = ai_now_ms();
                kvjb_choose(&T, T.pos.turn, lv, 5, &s);
                double dt = ai_now_ms() - t0;
                ms += dt, nodes += s.nodes;
                if (dt > worst) worst = dt;
            }
            printf("     %-6s %7.0f iterations %6.1f ms (worst %6.1f)\n", sp_name(lv), nodes / npos, ms / npos, worst);
        }
    }
    if (tt_fails) return 1;

    static const sp_game G = {
        .id = "klaverjas", .max_plies = 3000, .opening = 1, .reset = reset, .actor = actor, .result = result,
        .random_move = random_move, .script = one,
    };
    static sp_pair P[] = {
        {AI_EASY, SP_RANDOM, 60, 80},
        {AI_MEDIUM, AI_EASY, 60, 52},
        {AI_HARD, AI_MEDIUM, 60, 45},
        {AI_HARD, AI_EASY, 40, 55},
    };
    return tt_run(&G, P, (int)(sizeof P / sizeof P[0]), argc, argv);
}
