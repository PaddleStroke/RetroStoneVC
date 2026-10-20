/*
 * Goat Gorge: the rules alone (src/world.c). The movement's numbers (a full jump, a tapped one, the run, the dash, the
 * coyote time, the jump buffer, a wall jump, the corner corrections, the spikes' forgiving boxes, springs, gems,
 * crumbling blocks, platforms, wind), then every room of every chapter: it parses, and its recorded solutions
 * (levels/solutions.txt, by tests/solver.c) replay to the exit without a death, the berry's one with the berry.
 *
 *   test_rooms <levels dir> [-v]
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/goatgorge/LICENSE.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, verbose;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("  FAIL "); printf(__VA_ARGS__); printf("\n"); } \
                           else if (verbose) { printf("  ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static room R;
static world W;

static void load(const char *rows)
{
    char err[200], text[1024];
    snprintf(text, sizeof text, "room\n%s", rows);
    if (room_parse(text, &R, err, sizeof err)) { printf("  FAIL test room: %s\n", err); exit(1); }
    world_spawn(&W, &R, 0);
}
static int run(int in, int frames) { for (int i = 0; i < frames; i++) world_step(&W, &R, in); return W.h.y; }

/* an open floor: the kid at the left, the exit on the right */
static const char *FLOOR =
    "####################\n" "#...................\n" "#...................\n" "#...................\n"
    "#...................\n" "#...................\n" "#...................\n" "#...................\n"
    "#...................\n" "#...................\n" "#...................\n" "#...................\n"
    "#S..................\n" "####################\n" "####################\n";

static void movement(void)
{
    /* the full jump: about 3 cells; a tap: about one */
    load(FLOOR);
    run(0, 4);
    int y0 = W.h.y, top = y0, frames_up = 0;
    for (int i = 0; i < 60; i++) {
        world_step(&W, &R, IN_JUMP);
        if (W.h.y < top) { top = W.h.y; frames_up = i + 1; }
    }
    printf("  full jump: %d px in %d frames\n", y0 - top, frames_up);
    CHECK(y0 - top >= 44 && y0 - top <= 52, "a held jump rises 44..52 px (%d)", y0 - top);
    load(FLOOR);
    run(0, 4);
    y0 = W.h.y; top = y0;
    world_step(&W, &R, IN_JUMP);
    world_step(&W, &R, IN_JUMP);
    for (int i = 0; i < 40; i++) { world_step(&W, &R, 0); if (W.h.y < top) top = W.h.y; }
    printf("  tapped jump (2 frames): %d px\n", y0 - top);
    CHECK(y0 - top >= 8 && y0 - top <= 16, "a tapped jump rises 8..16 px (%d)", y0 - top);
    load(FLOOR);
    run(0, 4);
    y0 = W.h.y; top = y0;
    for (int i = 0; i < 6; i++) world_step(&W, &R, IN_JUMP);
    for (int i = 0; i < 40; i++) { world_step(&W, &R, 0); if (W.h.y < top) top = W.h.y; }
    printf("  jump held 6 frames: %d px\n", y0 - top);
    CHECK(y0 - top > 16 && y0 - top < 44, "a jump held 6 frames goes between (%d)", y0 - top);
    /* the run: full speed in 6-7 frames, a stop in 5 */
    load(FLOOR);
    int f = 0;
    while (W.h.vx < MAX_RUN && f < 30) { world_step(&W, &R, IN_RIGHT); f++; }
    CHECK(f >= 5 && f <= 8, "full speed in %d frames", f);
    f = 0;
    while (W.h.vx > 0 && f < 30) { world_step(&W, &R, 0); f++; }
    CHECK(f >= 3 && f <= 6, "a stop in %d frames", f);
    /* the dash: 64 px of straight line after a 3-frame freeze */
    load(FLOOR);
    run(0, 4);
    int x0 = W.h.x;
    world_step(&W, &R, IN_DASH | IN_RIGHT);
    CHECK(W.h.freeze_t == DASH_FREEZE && (W.events & EV_DASH), "the dash starts with its freeze frame");
    for (int i = 0; i < DASH_FREEZE; i++) world_step(&W, &R, IN_RIGHT);
    CHECK(W.h.x == x0 && W.h.dash_t == DASH_FRAMES, "the freeze holds the kid still (%d)", W.h.x - x0);
    for (int i = 0; i < DASH_FRAMES; i++) world_step(&W, &R, IN_RIGHT);
    printf("  dash: %d px\n", W.h.x - x0);
    CHECK(W.h.x - x0 >= 62 && W.h.x - x0 <= 68, "a dash goes 62..68 px (%d)", W.h.x - x0);
    CHECK(!W.h.dash_avail || W.h.grounded, "the dash is used");
    /* the dash's direction is read at the end of the freeze */
    load(FLOOR);
    run(0, 4);
    y0 = W.h.y;
    world_step(&W, &R, IN_DASH);
    world_step(&W, &R, 0);
    world_step(&W, &R, 0);
    world_step(&W, &R, IN_UP);
    for (int i = 0; i < DASH_FRAMES; i++) world_step(&W, &R, IN_UP);
    CHECK(y0 - W.h.y >= 60, "an Up pressed during the freeze dashes up (%d px)", y0 - W.h.y);
    /* the dash comes back on landing */
    for (int i = 0; i < 80; i++) world_step(&W, &R, 0);
    CHECK(W.h.dash_avail && W.h.grounded, "landing refills the dash");
    /* coyote time: a jump 5 frames after walking off a ledge still works, 8 frames after does not */
    static const char *LEDGE =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#S..................\n" "#####...............\n" "#...................\n"
        "#...................\n" "#...................\n" "####################\n";
    for (int late = 3; late <= 9; late += 6) {
        load(LEDGE);
        int off = 0;
        for (int i = 0; i < 200 && !off; i++) { world_step(&W, &R, IN_RIGHT); if (!W.h.grounded && W.h.vy >= 0 && W.h.x > 5 * CELL - HB_W) off = 1; }
        for (int i = 0; i < late - 1; i++) world_step(&W, &R, IN_RIGHT);
        world_step(&W, &R, IN_RIGHT | IN_JUMP);
        int jumped = (W.events & EV_JUMP) != 0;
        if (late == 3) CHECK(jumped, "coyote: a jump %d frames off the ledge works", late);
        else CHECK(!jumped, "coyote: a jump %d frames off the ledge does not", late);
    }
    /* the jump buffer: a press 5 frames before landing jumps on the landing */
    load(FLOOR);
    run(0, 4);
    for (int i = 0; i < 12; i++) world_step(&W, &R, IN_JUMP);
    int land = 0;
    for (int i = 0; i < 80 && !land; i++) {
        /* when the kid is 4..5 frames above the floor: a press */
        world_step(&W, &R, 0);
        if (W.h.vy > 0 && W.h.y + HB_H >= 13 * CELL - 12) land = 1;
    }
    world_step(&W, &R, IN_JUMP);
    int jumped = 0;
    for (int i = 0; i < 8 && !jumped; i++) { world_step(&W, &R, IN_JUMP); jumped = (W.events & EV_JUMP) != 0; }
    CHECK(jumped, "jump buffer: a press just before the landing jumps on it");
}

static void walls_and_corners(void)
{
    /* a chimney: wall jumps between two walls climb it */
    static const char *CHIMNEY =
        "######.#############\n" "######.#############\n" "######.......#######\n" "######.......#######\n"
        "######.......#######\n" "######.......#######\n" "######.......#######\n" "######.......#######\n"
        "######.......#######\n" "######.......#######\n" "######.......#######\n" "######.......#######\n"
        "######S......#######\n" "####################\n" "####################\n";
    load(CHIMNEY);
    run(IN_LEFT, 5);
    world_step(&W, &R, IN_JUMP | IN_LEFT);
    for (int i = 0; i < 14; i++) world_step(&W, &R, IN_JUMP | IN_LEFT);
    for (int i = 0; i < 30 && !(W.events & EV_WALLJUMP); i++) {
        world_step(&W, &R, IN_LEFT);
        world_step(&W, &R, IN_LEFT | IN_JUMP);
    }
    CHECK((W.events & EV_WALLJUMP) && W.h.vx > 0, "a wall jump pushes away from the wall");
    CHECK(W.h.wall_lock > 0, "... and holds the input away a moment");
    /* sliding down a wall is slower than falling */
    load(CHIMNEY);
    run(IN_LEFT, 5);
    for (int i = 0; i < 14; i++) world_step(&W, &R, IN_JUMP | IN_LEFT);
    for (int i = 0; i < 25; i++) world_step(&W, &R, IN_LEFT);
    CHECK(W.h.sliding && W.h.vy <= WALL_SLIDE, "pushing into a wall slides down it (vy %d)", W.h.vy);
    /* corner correction: a jump 3 px into a ceiling's corner slides around it */
    static const char *CORNER =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#....###############\n" "#...................\n" "#...................\n"
        "#S..................\n" "####################\n" "####################\n";
    load(CORNER);
    W.h.x = (int16_t)(5 * CELL - HB_W + 3);             /* 3 px under the ceiling's corner */
    int y0 = W.h.y;
    for (int i = 0; i < 20; i++) world_step(&W, &R, IN_JUMP);
    CHECK(W.h.y < 9 * CELL + 6 && W.h.x <= 5 * CELL - HB_W, "corner correction: the jump slides off the corner (rose %d px)", y0 - W.h.y);
    load(CORNER);
    W.h.x = (int16_t)(5 * CELL - HB_W + 6);             /* 6 px under it: a bonk */
    for (int i = 0; i < 20; i++) world_step(&W, &R, IN_JUMP);
    CHECK(W.h.y > 10 * CELL, "6 px under the corner: a bonk");
}

static void hazards_and_parts(void)
{
    /* the spikes' box is forgiving: the kid may stand 4 px into a spike cell's side */
    static const char *SPIKES =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#S.......^^^........\n" "####################\n" "####################\n";
    load(SPIKES);
    W.h.x = (int16_t)(9 * CELL + 2 - HB_W);
    for (int i = 0; i < 10; i++) world_step(&W, &R, 0);
    CHECK(!W.h.dead_t, "2 px into a spike cell's edge: alive");
    for (int i = 0; i < 30 && !W.h.dead_t; i++) world_step(&W, &R, IN_RIGHT);
    CHECK(W.h.dead_t, "walking onto the spikes: dead");
    for (int i = 0; i < DEATH_FRAMES + 2; i++) world_step(&W, &R, 0);
    CHECK(!W.h.dead_t && W.h.x == R.start_x && W.h.y == R.start_y, "the room again from its start after the burst");
    /* a spring sends the kid about 5 cells up, and gives the dash back */
    static const char *SPRING =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#S..o...............\n" "####################\n" "####################\n";
    load(SPRING);
    W.h.dash_avail = 0;
    int y0 = W.h.y, top = y0;
    for (int i = 0; i < 80; i++) { world_step(&W, &R, i < 30 ? IN_RIGHT : 0); if (W.h.y < top) top = W.h.y; }
    printf("  spring: %d px\n", y0 - top);
    CHECK(y0 - top >= 70 && y0 - top <= 100, "a spring: 70..100 px up (%d)", y0 - top);
    /* a gem gives the dash back in the air, once, and comes back later */
    static const char *GEM =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#S....*.............\n" "####################\n" "####################\n";
    load(GEM);
    W.h.dash_avail = 0;
    W.h.refill_cool = 200;
    for (int i = 0; i < 40 && !(W.events & EV_GEM); i++) world_step(&W, &R, IN_RIGHT);
    CHECK(W.h.dash_avail && W.gem_t[0] == GEM_RESPAWN, "a gem refills the dash");
    /* crumbling blocks: stood on, they shake, fall, and come back */
    static const char *CRUMBLE =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#S..................\n" "#===................\n" "#...................\n"
        "#...................\n" "####################\n" "####################\n";
    load(CRUMBLE);
    run(0, 3);
    CHECK(W.crumble[0] > 0 && W.crumble[2] > 0, "standing on a crumbling run shakes it all");
    run(0, CRUMBLE_SHAKE + 30);
    CHECK(W.h.y > 11 * CELL, "... then it falls away under the kid");
    /* a moving platform carries its rider */
    static const char *PLAT =
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#S..................\n" "#HH......x..........\n" "#...................\n"
        "#...................\n" "####################\n" "####################\n";
    load(PLAT);
    run(0, 4);
    CHECK(W.h.riding == 1, "the kid rides the platform");
    int x0 = W.h.x;
    run(0, PLAT_PAUSE + 40);
    CHECK(W.h.x - x0 >= 36 && W.h.riding == 1, "... and goes with it (%d px)", W.h.x - x0);
    /* wind pushes the kid */
    static const char *WIND =
        "room wind=192,0\n"
        "####################\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#...................\n" "#...................\n" "#...................\n" "#...................\n"
        "#S..................\n" "####################\n" "####################\n";
    char err[200];
    room_parse(WIND, &R, err, sizeof err);
    world_spawn(&W, &R, 0);
    x0 = W.h.x;
    run(0, 60);
    CHECK(W.h.x - x0 >= 40, "the wind pushes the kid (%d px in a second)", W.h.x - x0);
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc((size_t)n + 1);
    if (fread(s, 1, (size_t)n, f) != (size_t)n) n = 0;
    s[n] = 0;
    fclose(f);
    return s;
}

static void chapters(const char *dir)
{
    static room rooms[CHAPTERS][MAX_ROOMS];
    int count[CHAPTERS] = {0}, total = 0, berries = 0;
    char path[512], err[200];
    for (int c = 0; c < CHAPTERS; c++) {
        snprintf(path, sizeof path, "%s/ch%d.txt", dir, c + 1);
        char *s = read_file(path);
        if (!s) { if (c == 0) CHECK(0, "%s is missing", path); continue; }
        count[c] = chapter_parse(s, rooms[c], MAX_ROOMS, err, sizeof err);
        CHECK(count[c] > 0, "%s: %s", path, count[c] < 0 ? err : count[c] ? "parsed" : "no room");
        free(s);
        for (int i = 0; i < count[c]; i++) {
            berries += rooms[c][i].has_berry;
            /* a side that is not the way out is a wall you can see: its column all rock */
            const room *r = &rooms[c][i];
            for (int side = 0; side < 2; side++) {
                int col = side ? ROOM_W - 1 : 0, open = 0;
                if (r->exit_side == (side ? EXIT_RIGHT : EXIT_LEFT)) continue;
                for (int y = 0; y < ROOM_H; y++) open += r->cell[y][col] != C_ROCK;
                CHECK(!open, "room %d-%d: its %s side is not the way out but has %d open cells (draw the wall)", c + 1,
                      i + 1, side ? "right" : "left", open);
            }
        }
        if (count[c] > 0) total += count[c];
    }
    printf("  %d rooms, %d berries\n", total, berries);
    snprintf(path, sizeof path, "%s/solutions.txt", dir);
    char *s = read_file(path);
    CHECK(s != NULL, "%s is missing", path);
    if (!s) return;
    static uint8_t done[CHAPTERS][MAX_ROOMS][2];
    static uint8_t in[30000];
    char *line = s;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        int c, rn, frames, at = 0;
        char kind[16];
        if (sscanf(line, "%d %d %15s %d %n", &c, &rn, kind, &frames, &at) == 4 && c >= 1 && c <= CHAPTERS && rn >= 1 &&
            rn <= count[c - 1]) {
            int berry = !strcmp(kind, "berry");
            int n = sol_decode(line + at, in, (int)sizeof in);
            const room *r = &rooms[c - 1][rn - 1];
            world w;
            world_spawn(&w, r, 0);
            int ok = 0, i;
            for (i = 0; i < n; i++) {
                world_step(&w, r, in[i]);
                if (w.h.dead_t) break;
                if (w.exited) { ok = !berry || w.berry_got; break; }
            }
            CHECK(ok && n == frames, "room %d-%d: the %s solution (%d frames) replays to the exit%s", c, rn, kind, frames,
                  berry ? " with the berry" : "");
            if (ok) done[c - 1][rn - 1][berry] = 1;
        }
        line = nl ? nl + 1 : NULL;
    }
    free(s);
    for (int c = 0; c < CHAPTERS; c++)
        for (int i = 0; i < count[c]; i++) {
            CHECK(done[c][i][0], "room %d-%d has a recorded solution", c + 1, i + 1);
            if (rooms[c][i].has_berry) CHECK(done[c][i][1], "room %d-%d's berry has a recorded solution", c + 1, i + 1);
        }
}

int main(int argc, char **argv)
{
    verbose = argc > 2 && !strcmp(argv[2], "-v");
    movement();
    walls_and_corners();
    hazards_and_parts();
    chapters(argc > 1 ? argv[1] : "games/goatgorge/levels");
    if (fails) { printf("test_rooms: %d FAILED\n", fails); return 1; }
    printf("test_rooms: all passed\n");
    return 0;
}
