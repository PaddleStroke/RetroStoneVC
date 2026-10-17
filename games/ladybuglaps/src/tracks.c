/*
 * Ladybug Laps: the tracks, made by hand. Also read by tools/make_art.py (it draws them from this file: keep one
 * entry a line, as below).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft): games/ladybuglaps/LICENSE.
 *
 * A track is a grid of 64-px pieces (CELL), each a character:
 *   .  nothing (the table, the grass...)        -  a straight, west-east      |  a straight, north-south
 *   +  a crossing (the two straights)           r  a tight corner joining east and south (like a box corner),
 *   7  west and south,  L  north and east,  J  north and west (the corner's centre is the cell's corner: radius 32)
 *   A B C D  a sweeping corner over 2 x 2 cells, in the top-left cell (the other three are #): A joins east and
 *            south, B west and south, C north and east, D north and west (radius 96, round the block's corner)
 * The band is 48 px wide (HALF_W) along the centre line. The start: a straight's cell and the direction raced;
 * the line is the cell's entry edge and the grid stands behind it. Objects, zones and leaves are in world pixels
 * (their centre); repeated objects sit at the same place modulo 64 px, so that they share their tiles.
 */
#include "game.h"

/* the objects: shape, effect, size (a circle's radius; a box's width and height) */
const obj_kind obj_kinds[OB_COUNT] = {
    /* OB_CUP      */ {SH_CIRCLE, OE_SOLID, 27, 27},
    /* OB_SUGAR    */ {SH_BOX, OE_SOLID, 16, 16},
    /* OB_TEAPOT   */ {SH_CIRCLE, OE_SOLID, 38, 38},
    /* OB_SPOON    */ {SH_BOX, OE_BUMP, 60, 10},
    /* OB_COOKIE   */ {SH_CIRCLE, OE_SOLID, 15, 15},
    /* OB_CRAYON_H */ {SH_BOX, OE_SOLID, 72, 12},
    /* OB_CRAYON_V */ {SH_BOX, OE_SOLID, 12, 72},
    /* OB_ERASER   */ {SH_BOX, OE_SOLID, 40, 24},
    /* OB_PENCIL   */ {SH_BOX, OE_BUMP, 8, 60},
    /* OB_PAINT    */ {SH_CIRCLE, OE_SOLID, 22, 22},
    /* OB_APPLE    */ {SH_CIRCLE, OE_SOLID, 17, 17},
    /* OB_SANDWICH */ {SH_BOX, OE_SOLID, 48, 36},
    /* OB_JUICE    */ {SH_CIRCLE, OE_SOLID, 14, 14},
    /* OB_BASKET   */ {SH_BOX, OE_SOLID, 80, 56},
    /* OB_TWIG     */ {SH_BOX, OE_BUMP, 10, 60},
    /* OB_LETTUCE  */ {SH_CIRCLE, OE_SOLID, 18, 18},
    /* OB_PUMPKIN  */ {SH_CIRCLE, OE_SOLID, 22, 22},
    /* OB_STONE    */ {SH_CIRCLE, OE_SOLID, 9, 9},
    /* OB_CARROT   */ {SH_CIRCLE, OE_DECO, 6, 6},
    /* OB_POT      */ {SH_CIRCLE, OE_SOLID, 22, 22},
    /* OB_SNAIL    */ {SH_CIRCLE, OE_SOLID, 11, 11},
    /* OB_CAN      */ {SH_BOX, OE_SOLID, 56, 36},
};

/* ---- 1. Tea Time: a garden table laid for tea; off the table you fall into the grass --------------------------- */
static const char *const tea_grid[] = {
    "....................",
    ".A#--------------B#.",
    ".##..............##.",
    ".|................|.",
    ".|...............rJ.",
    ".|...............|..",
    ".|...............|..",
    ".|...............L7.",
    ".|......r----7....|.",
    ".|......|....|....|.",
    ".C#.....|....|...D#.",
    ".##-----J....L---##.",
    "....................",
    "....................",
};
static const tobj tea_obj[] = {
    {OB_TEAPOT, 0, 512, 320},
    {OB_CUP, 0, 352, 256},
    {OB_CUP, 1, 800, 224},
    {OB_CUP, 2, 1056, 416},
    {OB_CUP, 1, 704, 672},
    {OB_SUGAR, 0, 664, 84},
    {OB_SUGAR, 0, 920, 112},
    {OB_SUGAR, 0, 408, 456},
    {OB_SUGAR, 0, 984, 600},
    {OB_SUGAR, 0, 1144, 216},
    {OB_COOKIE, 0, 640, 448},
    {OB_COOKIE, 1, 256, 448},
    {OB_COOKIE, 0, 320, 832},
    {OB_COOKIE, 1, 1088, 832},
    {OB_SPOON, 0, 1184, 576},
};
static const tzone tea_zone[] = {
    {SF_JAM, ZS_ELLIPSE, 1008, 736, 44, 16},
    {SF_JAM, ZS_ELLIPSE, 96, 448, 16, 42},
};
static const tleaf tea_leaf[] = {
    {LF_LEAF, 0, 1220, 740},
    {LF_LEAF2, 1, 1180, 800},
    {LF_LEAF, 3, 60, 60},
};

/* ---- 2. Crayon Desk: a figure of eight on a child's desk, round crayons and a paint pot ---------------------- */
static const char *const desk_grid[] = {
    "...................",
    "...........A#--B#..",
    "...........##..##..",
    "...........|....|..",
    "...........|....|..",
    "...........|....|..",
    "...........|...D#..",
    "...r-------+---##..",
    "...|.......|.......",
    "...|.......|.......",
    "...|.......|.......",
    "...C#.....D#.......",
    "...##-----##.......",
    "...................",
    "...................",
};
static const tobj desk_obj[] = {
    {OB_PAINT, 0, 896, 320},
    {OB_PAINT, 1, 896, 640},
    {OB_ERASER, 0, 512, 320},
    {OB_CRAYON_H, 0, 448, 192},
    {OB_CRAYON_H, 1, 384, 128},
    {OB_CRAYON_V, 2, 640, 192},
    {OB_CRAYON_V, 3, 192, 320},
    {OB_CRAYON_H, 2, 512, 640},
    {OB_CRAYON_V, 0, 960, 576},
    {OB_CRAYON_H, 3, 832, 896},
    {OB_PENCIL, 0, 576, 480},
    {OB_ERASER, 1, 1024, 768},
};
static const tzone desk_zone[] = {
    {SF_JAM, ZS_ELLIPSE, 736, 640, 16, 40},
    {SF_WATER, ZS_ELLIPSE, 1056, 288, 20, 52},
};
static const tleaf desk_leaf[] = {{LF_LEAF, 0, 0, 0}};

/* ---- 3. Picnic: round the picnic blanket on the lawn, under the tree ---------------------------------------------- */
static const char *const picnic_grid[] = {
    "........................",
    "........................",
    "....A#--------B#........",
    "....##........##........",
    "....|..........|........",
    "....|..........|........",
    "....|..........C#.......",
    "....|..........##---B#..",
    "....|...............##..",
    "....|................|..",
    "....C#...............|..",
    "....##---7..........D#..",
    ".........L----------##..",
    "........................",
    "........................",
};
static const tobj picnic_obj[] = {
    {OB_BASKET, 0, 576, 384},
    {OB_SANDWICH, 0, 768, 512},
    {OB_APPLE, 0, 448, 576},
    {OB_APPLE, 1, 1064, 424},
    {OB_JUICE, 0, 832, 320},
    {OB_JUICE, 1, 1280, 576},
    {OB_APPLE, 0, 1280, 192},
    {OB_TWIG, 0, 960, 800},
    {OB_STONE, 0, 192, 704},
    {OB_STONE, 0, 1408, 832},
    {OB_STONE, 0, 1472, 256},
};
static const tzone picnic_zone[] = {
    {SF_JAM, ZS_ELLIPSE, 288, 448, 16, 44},
};
static const tleaf picnic_leaf[] = {
    {LF_LEAF, 0, 1000, 120},
    {LF_LEAF2, 1, 1060, 170},
    {LF_LEAF, 2, 940, 190},
    {LF_LEAF2, 0, 1120, 110},
    {LF_LEAF, 1, 1380, 560},
    {LF_LEAF2, 3, 1430, 610},
};

/* ---- 4. Veggie Patch: zig-zag between the rows of a vegetable patch, under the carrot tops -------------------- */
static const char *const veg_grid[] = {
    ".................",
    "..A#----------7..",
    "..##..........|..",
    "..|.r---------J..",
    "..|.|............",
    "..|.L---------7..",
    "..C#..........|..",
    "..##----------J..",
    ".................",
    ".................",
};
static const tobj veg_obj[] = {
    {OB_LETTUCE, 0, 352, 160},
    {OB_LETTUCE, 0, 544, 160},
    {OB_LETTUCE, 0, 736, 160},
    {OB_PUMPKIN, 0, 864, 416},
    {OB_PUMPKIN, 1, 448, 416},
    {OB_STONE, 0, 608, 416},
    {OB_STONE, 0, 224, 288},
    {OB_CARROT, 0, 384, 288},
    {OB_CARROT, 0, 512, 288},
    {OB_CARROT, 0, 640, 288},
    {OB_CARROT, 0, 768, 288},
    {OB_TWIG, 0, 576, 224},
    {OB_CAN, 0, 832, 576},
    {OB_STONE, 0, 352, 576},
};
static const tzone veg_zone[] = {
    {SF_JAM, ZS_ELLIPSE, 704, 352, 44, 16},
    {SF_WATER, ZS_ELLIPSE, 480, 480, 56, 18},
};
static const tleaf veg_leaf[] = {
    {LF_FROND, 0, 384, 284},            /* the carrots' tops: from the top of each carrot (OB_CARROT at y 288) */
    {LF_FROND, 1, 512, 284},
    {LF_FROND, 0, 640, 284},
    {LF_FROND, 1, 768, 284},
    {LF_LEAF2, 0, 352, 136},
    {LF_LEAF2, 3, 736, 184},
};

/* ---- 5. Rainy Patio: round the flower pots on the wet flagstones, a big puddle and a snail -------------------------- */
static const char *const patio_grid[] = {
    "..................",
    "...A#-------B#....",
    "...##.......##....",
    "..rJ.........|....",
    "..|..........|....",
    "..|..........L7...",
    "..|...........|...",
    "..|...........|...",
    "..|...........|...",
    "..|...........|...",
    "..C#.........D#...",
    "..##---------##...",
    "..................",
    "..................",
};
static const tobj patio_obj[] = {
    {OB_POT, 0, 448, 384},
    {OB_POT, 1, 704, 320},
    {OB_POT, 2, 704, 576},
    {OB_POT, 1, 1088, 192},
    {OB_SNAIL, 0, 912, 520},
    {OB_TWIG, 0, 576, 96},
    {OB_STONE, 0, 352, 576},
    {OB_STONE, 0, 1024, 448},
    {OB_CAN, 0, 1024, 832},
};
static const tzone patio_zone[] = {
    {SF_WATER, ZS_ELLIPSE, 576, 736, 76, 32},
    {SF_WATER, ZS_ELLIPSE, 160, 448, 22, 40},
};
static const tleaf patio_leaf[] = {
    {LF_LEAF, 0, 832, 96},
    {LF_LEAF2, 2, 880, 136},
    {LF_LEAF, 3, 160, 640},
    {LF_LEAF2, 1, 960, 760},
};

#define N(a) (int)(sizeof(a) / sizeof((a)[0]))
const track_def tracks[TRACKS] = {
    {tea_grid, 20, 14, 5, 1, 0, 1, 22, 22, 1258, 874, tea_obj, N(tea_obj), tea_zone, N(tea_zone), tea_leaf,
     N(tea_leaf), TH_TEA},
    {desk_grid, 19, 15, 6, 7, 0, 1, 150, 24, 1150, 920, desk_obj, N(desk_obj), desk_zone, N(desk_zone), desk_leaf, 0,
     TH_DESK},
    {picnic_grid, 24, 15, 8, 2, 0, 0, 0, 0, 0, 0, picnic_obj, N(picnic_obj), picnic_zone, N(picnic_zone), picnic_leaf,
     N(picnic_leaf), TH_PICNIC},
    {veg_grid, 17, 10, 6, 1, 0, 0, 0, 0, 0, 0, veg_obj, N(veg_obj), veg_zone, N(veg_zone), veg_leaf, N(veg_leaf),
     TH_VEG},
    {patio_grid, 18, 14, 6, 11, 0, 0, 0, 0, 0, 0, patio_obj, N(patio_obj), patio_zone, N(patio_zone), patio_leaf,
     N(patio_leaf), TH_PATIO},
};
