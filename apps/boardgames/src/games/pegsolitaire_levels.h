/*
 * Peg Solitaire's levels: our own positions, made by tools/pegsolitaire_gen.c (symmetric pictures solved by
 * search, positions played backwards from one marble in the goal hole, the classic full boards solved by
 * search), six chapters of 25 by the marbles
 * at the start, each from the easiest. Each: its board, its goal hole (a cell of the 9 x 9 grid), its marbles
 * (bit i: the board's hole i, in cell order) and a way to one marble in the goal; then (in the comment) the
 * marbles, how many of 300 games played at random end in the goal, its grade.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const ps_level_t ps_levels[PS_LEVELS] = {
    {PS_TRIANGLE, 20, 0xcc0ull,
     "a2c2 a1c1 c1c3"}, /* 1: triangle, 4 marbles, 231 of 300 at random, 5.1, a picture */
    {PS_ENGLISH, 23, 0x2041200ull,
     "f3f5 g5e5 d5f5"}, /* 2: English, 4 marbles, 156 of 300 at random, 6.8 */
    {PS_TRIANGLE, 38, 0x3a50ull,
     "c1e1 e1c3 c3a3 a3a1 a1c1"}, /* 3: triangle, 6 marbles, 154 of 300 at random, 8.9, a picture */
    {PS_ENGLISH, 11, 0x10800220ull,
     "d2d4 d4d6 e6c6"}, /* 4: English, 4 marbles, 77 of 300 at random, 9.8 */
    {PS_ENGLISH, 23, 0x120050200ull,
     "e1e3 d5d3 d3f3 f3f5"}, /* 5: English, 5 marbles, 82 of 300 at random, 10.6 */
    {PS_TRIANGLE, 20, 0x970ull,
     "c3a3 a3a1 a1c1 c1c3"}, /* 6: triangle, 5 marbles, 73 of 300 at random, 11.1, a picture */
    {PS_ENGLISH, 37, 0x904209ull,
     "c7c5 d5b5 b5b3 a3c3 d3b3"}, /* 7: English, 6 marbles, 74 of 300 at random, 12.0 */
    {PS_ENGLISH, 40, 0x18300ull,
     "c5e5 c4e4 e5e3"}, /* 8: English, 4 marbles, 38 of 300 at random, 12.8, a picture */
    {PS_TRIANGLE, 19, 0x1190ull,
     "b3b1 b1d1 d1b3"}, /* 9: triangle, 4 marbles, 33 of 300 at random, 13.4, a picture */
    {PS_ENGLISH, 40, 0x16112ull,
     "d7d5 c5e5 a4c4 c4e4 e5e3"}, /* 10: English, 6 marbles, 46 of 300 at random, 14.0, a picture */
    {PS_TRIANGLE, 20, 0x348aull,
     "a4a2 a2c2 d1b1 a1c1 c1c3"}, /* 11: triangle, 6 marbles, 39 of 300 at random, 14.7, a picture */
    {PS_ENGLISH, 48, 0x2030400ull,
     "e5e3 f3d3 d4d2"}, /* 12: English, 4 marbles, 22 of 300 at random, 15.1 */
    {PS_ENGLISH, 21, 0x72aull,
     "c5c7 c7e7 d5f5 e7e5 f5d5"}, /* 13: English, 6 marbles, 30 of 300 at random, 15.8, a picture */
    {PS_ENGLISH, 30, 0x10870000ull,
     "e4c4 d2d4 c4e4 f4d4"}, /* 14: English, 5 marbles, 21 of 300 at random, 16.3, a picture */
    {PS_ENGLISH, 31, 0x130a0000ull,
     "f3d3 d2d4 d4f4 g4e4"}, /* 15: English, 5 marbles, 19 of 300 at random, 16.7 */
    {PS_ENGLISH, 20, 0x408700ull,
     "d5b5 c3c5 b5d5 e5c5"}, /* 16: English, 5 marbles, 17 of 300 at random, 17.2, a picture */
    {PS_ENGLISH, 30, 0x10d80ull,
     "b5d5 d4d6 f5d5 d6d4"}, /* 17: English, 5 marbles, 14 of 300 at random, 18.0, a picture */
    {PS_ENGLISH, 21, 0x13600000ull,
     "b3d3 d2d4 f3d3 d3d5"}, /* 18: English, 5 marbles, 11 of 300 at random, 18.9, a picture */
    {PS_ENGLISH, 39, 0x10712ull,
     "d5f5 d7d5 c5e5 f5d5 d5d3"}, /* 19: English, 6 marbles, 13 of 300 at random, 19.3, a picture */
    {PS_ENGLISH, 29, 0x10808210ull,
     "d6d4 c4e4 d2d4 e4c4"}, /* 20: English, 5 marbles, 8 of 300 at random, 20.2, a picture */
    {PS_ENGLISH, 30, 0x1086c000ull,
     "b4d4 e4c4 d2d4 c4e4 f4d4"}, /* 21: English, 6 marbles, 8 of 300 at random, 21.2, a picture */
    {PS_ENGLISH, 11, 0x10d0820ull,
     "g4e4 e3e5 f5d5 d4d6 e6c6"}, /* 22: English, 6 marbles, 7 of 300 at random, 21.7 */
    {PS_ENGLISH, 20, 0x1610420ull,
     "e6e4 e4c4 b3d3 e3c3 c3c5"}, /* 23: English, 6 marbles, 5 of 300 at random, 22.9, a picture */
    {PS_ENGLISH, 29, 0x10830210ull,
     "e4c4 d6d4 c4e4 d2d4 e4c4"}, /* 24: English, 6 marbles, 2 of 300 at random, 25.9, a picture */
    {PS_TRIANGLE, 27, 0x1313ull,
     "c1c3 d2b4 a5c3 c3a3 a4a2"}, /* 25: triangle, 6 marbles, 0 of 300 at random, 30.7 */
    {PS_TRIANGLE, 19, 0x2a56ull,
     "a4c2 d2b2 a2c2 d1b3 b4b2 b1b3"}, /* 26: triangle, 7 marbles, 241 of 300 at random, 7.9, a picture */
    {PS_FRENCH, 39, 0x14a910000ull,
     "b4b2 f4f2 b2d2 d3d1 f2d2 d1d3"}, /* 27: French, 7 marbles, 28 of 300 at random, 17.1, a picture */
    {PS_ENGLISH, 40, 0xd90c1800ull,
     "f5f3 g5g3 f3d3 c2e2 c1e1 e1e3 d3f3 g3e3"}, /* 28: English, 9 marbles, 25 of 300 at random, 19.6, a picture */
    {PS_TRIANGLE6, 37, 0x7894ull,
     "b3d1 b5b3 b2b4 e2c2 d1b3 b4b2"}, /* 29: big triangle, 7 marbles, 11 of 300 at random, 20.9, a picture */
    {PS_TRIANGLE6, 29, 0x62158ull,
     "a4c4 c4c2 d2b2 d1b1 b1b3 a3c3"}, /* 30: big triangle, 7 marbles, 8 of 300 at random, 22.2, a picture */
    {PS_ENGLISH, 39, 0x11438880ull,
     "d4f4 f5f3 c3c5 b5d5 f3d3 d2d4 d5d3"}, /* 31: English, 8 marbles, 8 of 300 at random, 23.2, a picture */
    {PS_FRENCH, 30, 0x86101820ull,
     "d6d4 d3f3 f3f5 f5d5 d5d3 d2d4"}, /* 32: French, 7 marbles, 5 of 300 at random, 23.9, a picture */
    {PS_FRENCH, 40, 0x47083000ull,
     "f5d5 e3e5 e5c5 c2c4 c5c3 c3e3"}, /* 33: French, 7 marbles, 4 of 300 at random, 24.7, a picture */
    {PS_FRENCH, 30, 0x231b1880ull,
     "d5f5 b4d4 e4g4 f6f4 g4e4 e4c4 c4c2 b2d2 d2d4"}, /* 34: French, 10 marbles, 7 of 300 at random, 25.7, a picture */
    {PS_FRENCH, 30, 0x61840618ull,
     "b6d6 b5d5 d5d3 b2d2 d2d4 b3d3 d3d5 d6d4"}, /* 35: French, 9 marbles, 5 of 300 at random, 25.9, a picture */
    {PS_FRENCH, 40, 0x111e0a0800ull,
     "e3c3 c3c5 c5e5 e5e3 f3d3 e1e3 d3f3 g3e3"}, /* 36: French, 9 marbles, 4 of 300 at random, 26.7, a picture */
    {PS_FRENCH, 29, 0xb2c2c00ull,
     "c5e5 e5g5 g5g3 d4d2 g3e3 e4e2 e2c2 c2c4"}, /* 37: French, 9 marbles, 3 of 300 at random, 27.7, a picture */
    {PS_ENGLISH, 39, 0x29c44000ull,
     "c2c4 b4d4 d3d5 e2e4 f4d4 d5d3"}, /* 38: English, 7 marbles, 1 of 300 at random, 28.7, a picture */
    {PS_ENGLISH, 30, 0xc3c610ull,
     "d5f5 d4f4 f5f3 b4d4 c3e3 f3d3 d3d5 d6d4"}, /* 39: English, 9 marbles, 2 of 300 at random, 28.9, a picture */
    {PS_ENGLISH, 21, 0x2a54500ull,
     "b3b5 b5d5 e5c5 d3d5 c5e5 f3f5 f5d5"}, /* 40: English, 8 marbles, 1 of 300 at random, 29.7, a picture */
    {PS_FRENCH, 31, 0x21878608ull,
     "b5d5 d5d3 b4d4 b2b4 d3b3 b3b5 b6b4 a4c4 c4e4"}, /* 41: French, 10 marbles, 2 of 300 at random, 29.9, a picture */
    {PS_FRENCH, 30, 0x8d083420ull,
     "f5d5 d6d4 e4c4 f3d3 d3b3 c5c3 b3d3 d2d4"}, /* 42: French, 9 marbles, 1 of 300 at random, 30.7, a picture */
    {PS_FRENCH, 31, 0x7081c00ull,
     "e4e2 c3e3 e2e4 e4e6 c5e5 e6e4"}, /* 43: French, 7 marbles, 0 of 300 at random, 31.7, a picture */
    {PS_TRIANGLE6, 39, 0x21978ull,
     "a4a2 b4d2 c1c3 a2c2 d2b2 c4c2 b2d2"}, /* 44: big triangle, 8 marbles, 0 of 300 at random, 32.7, a picture */
    {PS_ENGLISH, 39, 0x10810728ull,
     "c6c4 e6e4 d4f4 d2d4 c4e4 f4d4 d5d3"}, /* 45: English, 8 marbles, 0 of 300 at random, 32.7, a picture */
    {PS_FRENCH, 21, 0x71b2200ull,
     "b5b3 f4d4 d4d2 b3d3 d2d4 c4e4 e3e5 f5d5"}, /* 46: French, 9 marbles, 0 of 300 at random, 33.7, a picture */
    {PS_TRIANGLE6, 47, 0xd231cull,
     "d3b3 e1c1 a4c2 c1a1 b5b3 d2b2 b3b1 a1c1"}, /* 47: big triangle, 9 marbles, 0 of 300 at random, 33.7 */
    {PS_ENGLISH, 39, 0x6cda8ull,
     "b5d5 e5e7 f5f3 b4d4 d4d6 c6e6 e7e5 e5e3 f3d3"}, /* 48: English, 10 marbles, 0 of 300 at random, 34.7, a picture */
    {PS_ENGLISH, 40, 0x10658720ull,
     "e6e4 c5e5 e4g4 c4e4 b3d3 d2d4 d4f4 g4e4 e5e3"}, /* 49: English, 10 marbles, 0 of 300 at random, 34.7, a picture */
    {PS_ENGLISH, 28, 0x123ec10ull,
     "f5d5 e3e5 b3b5 d4b4 a4c4 e5c5 b5d5 d6d4 d4b4"}, /* 50: English, 10 marbles, 0 of 300 at random, 34.7 */
    {PS_TRIANGLE6, 28, 0x1f6626ull,
     "c4a6 a6a4 e2c2 c1a1 a1a3 d1b3 a4c2 f1d1 d1b3 a3c3 d3b3"}, /* 51: big triangle, 12 marbles, 39 of 300 at random, 20.7, a picture */
    {PS_TRIANGLE6, 29, 0x19ed17ull,
     "a6a4 b5b3 a4c2 c3c1 a2c2 b1d1 e1c1 f1d3 d3d1 d1b1 a1c1 c1c3"}, /* 52: big triangle, 13 marbles, 9 of 300 at random, 27.7, a picture */
    {PS_TRIANGLE6, 29, 0x1c633full,
     "a5a3 b5b3 c4c2 a3c3 c2c4 d3b5 a6c4 e1c1 f1d3 c4e2 e2c2 c1c3"}, /* 53: big triangle, 13 marbles, 5 of 300 at random, 29.9, a picture */
    {PS_TRIANGLE6, 29, 0x1d651full,
     "a6c4 a5a3 c4a4 a3a1 a1c1 d1d3 e2c4 c4c2 f1d1 d1b3 a4c2 c1c3"}, /* 54: big triangle, 13 marbles, 3 of 300 at random, 31.7, a picture */
    {PS_WIEGLEB, 75, 0xb3007204100ull,
     "f7f5 g5e5 d5f5 i5g5 e2e4 g5e5 e5e3 d3f3 f3f1 f1d1"}, /* 55: Wiegleb's, 11 marbles, 1 of 300 at random, 32.7 */
    {PS_ENGLISH, 21, 0x366d8c0ull,
     "a5c5 g5e5 b4d4 e4c4 b3d3 e3c3 f3f5 f5d5 d5b5 c3c5 b5d5"}, /* 56: English, 12 marbles, 1 of 300 at random, 33.7, a picture */
    {PS_FRENCH, 20, 0x30b1f31ull,
     "c6e6 d5f5 b5d5 b4d4 d4d6 e6c6 c7c5 c3e3 e3e5 f5d5 d5b5 a5c5"}, /* 57: French, 13 marbles, 1 of 300 at random, 34.7, a picture */
    {PS_DIAMOND, 40, 0x18cc30cd80ull,
     "g7e7 c6e6 e6e4 h6f6 f6f4 f3f5 h4f4 f5f3 g3e3 e3e5 c4e4 e4e6 e7e5"}, /* 58: diamond, 14 marbles, 1 of 300 at random, 35.7, a picture */
    {PS_DIAMOND, 59, 0xca10183044ull,
     "f2d2 f6d6 e8e6 d6f6 d5f5 f6f4 f3f5 d2d4 d4f4 f5f3"}, /* 59: diamond, 11 marbles, 0 of 300 at random, 35.7 */
    {PS_TRIANGLE6, 49, 0x10799bull,
     "a5a3 c2a4 a3a5 a6a4 f1d3 a4c4 c4e2 e2c2 b2d2 c3e1"}, /* 60: big triangle, 11 marbles, 0 of 300 at random, 35.7 */
    {PS_DIAMOND, 50, 0x418783c20ull,
     "d6b6 e5c5 g5e5 e4c4 e6e4 c4c6 b6d6 d7d5 e3e5 d5f5 f6f4"}, /* 61: diamond, 12 marbles, 0 of 300 at random, 36.7, a picture */
    {PS_ENGLISH, 40, 0x1045c798ull,
     "c6e6 e6e4 b5b3 c5e5 e4g4 c4e4 b3d3 d2d4 d4f4 g4e4 e5e3"}, /* 62: English, 12 marbles, 0 of 300 at random, 36.7, a picture */
    {PS_WIEGLEB, 40, 0x2bc0e08048ull,
     "f4f6 d4f4 d8d6 d6d4 g6e6 f4h4 e6e4 i4g4 d4f4 g4e4 e3e5"}, /* 63: Wiegleb's, 12 marbles, 0 of 300 at random, 36.7 */
    {PS_TRIANGLE, 28, 0x6ff7ull,
     "a5a3 c3c1 a2a4 a4c2 b1b3 d1b1 a1c1 e1c3 b4d2 c1c3 d2b4 b4b2"}, /* 64: triangle, 13 marbles, 0 of 300 at random, 37.7, a picture */
    {PS_DIAMOND, 40, 0x4218bc7040ull,
     "f6d6 e5g5 c5e5 e5e3 h5f5 d3d5 d6d4 e2e4 d4f4 f4f6 g6e6 e7e5"}, /* 65: diamond, 13 marbles, 0 of 300 at random, 37.7, a picture */
    {PS_DIAMOND, 40, 0x4438383844ull,
     "e6g6 e8e6 d6f6 g6e6 d4d6 d6f6 e4g4 f6f4 e2e4 e5e3 g4e4 e3e5"}, /* 66: diamond, 13 marbles, 0 of 300 at random, 37.7, a picture */
    {PS_WIEGLEB, 39, 0xd0c0703148ull,
     "d7d9 e5e3 c5e5 e5e7 f7d7 d6d8 d9d7 d3d5 f3d3 d2d4 d4d6 d7d5"}, /* 67: Wiegleb's, 13 marbles, 0 of 300 at random, 37.7, a picture */
    {PS_FRENCH, 37, 0x8a1091e70ull,
     "e5e3 b5b3 b3d3 c5c7 e3c3 d1d3 e6c6 d3b3 b2b4 c7c5 d5b5 b5b3"}, /* 68: French, 13 marbles, 0 of 300 at random, 37.7 */
    {PS_ENGLISH, 39, 0x22baf92ull,
     "c5a5 a5a3 d5d3 d7d5 e5g5 g5g3 a3c3 c3c5 c5e5 g3e3 d3f3 e5e3 f3d3"}, /* 69: English, 14 marbles, 0 of 300 at random, 38.7, a picture */
    {PS_DIAMOND, 40, 0x98fc283080ull,
     "e6g6 d4d6 f4d4 c4e4 h4f4 e4g4 g3g5 f2f4 f4f6 g6e6 d6f6 f7f5 g5e5"}, /* 70: diamond, 14 marbles, 0 of 300 at random, 38.7, a picture */
    {PS_FRENCH, 30, 0x111e8b0c60ull,
     "d6f6 c5e5 b4d4 e4e6 f6d6 e3c3 b3d3 g3e3 d3f3 e1e3 f3d3 d3d5 d6d4"}, /* 71: French, 14 marbles, 0 of 300 at random, 38.7, a picture */
    {PS_WIEGLEB, 51, 0x171012c5700ull,
     "a6c6 a5c5 c5e5 c6e6 f7f5 f4f2 e6e4 d3f3 g5e5 e5e3 e2e4 f2f4 e4g4"}, /* 72: Wiegleb's, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_ENGLISH, 28, 0xb155a388ull,
     "a3a5 d5d3 d3b3 c5c3 a5c5 e2e4 d1d3 c6c4 f4d4 d4d2 b3d3 d2d4 d4b4"}, /* 73: English, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_WIEGLEB, 30, 0x53416850b0ull,
     "g4g6 f3f5 e5c5 d3d5 g6e6 f8d8 d6d4 b5d5 d4d6 d6f6 f5f7 f7d7 d8d6"}, /* 74: Wiegleb's, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_TRIANGLE, 0, 0x7ffeull,
     "a3a5 c3a3 a5c3 a2a4 d2b4 b1b3 d1b1 a1c1 c1c3 b4d2 e1c3 c3a3 a3a5"}, /* 75: triangle, 14 marbles, 0 of 300 at random, 38.7, the full board */
    {PS_TRIANGLE6, 38, 0x16d5bfull,
     "a5a3 b5d3 b4b2 d3b3 a3c3 c2c4 a2c2 d1b1 a1c1 c1c3 f1d3 d3b5 a6c4 c4c2"}, /* 76: big triangle, 15 marbles, 12 of 300 at random, 28.6, a picture */
    {PS_TRIANGLE6, 28, 0x1ffb7dull,
     "b5b3 a4a2 c3a3 d3b5 a6c4 a2a4 c2a2 e2c2 a1a3 a4a2 c1a1 a1a3 d1b3 a3c3 c4c2 f1d1 d1b3"}, /* 77: big triangle, 18 marbles, 4 of 300 at random, 35.7, a picture */
    {PS_ENGLISH, 29, 0x168c3832dull,
     "e7e5 d5f5 c3e3 e3e5 f5d5 d5b5 c7c5 b5d5 d5d3 c1c3 c4c2 e1e3 e3c3 c2c4"}, /* 78: English, 15 marbles, 0 of 300 at random, 39.7, a picture */
    {PS_FRENCH, 30, 0x88d953622ull,
     "d7d5 c5a5 d4d6 f5d5 b3d3 e3c3 f3f5 d1d3 d3b3 b3b5 a5c5 c5e5 f5d5 d6d4"}, /* 79: French, 15 marbles, 0 of 300 at random, 39.7, a picture */
    {PS_ENGLISH, 29, 0x2dcd4cb0ull,
     "g3g5 d3d5 e2e4 c2c4 e5c5 g5e5 b5d5 d6d4 e4g4 e6e4 d4f4 b4d4 g4e4 e4c4"}, /* 80: English, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_ENGLISH, 49, 0x6a27a689ull,
     "c1c3 c7c5 e5e3 d5d3 e2e4 c3e3 c5a5 f3d3 a5a3 f4d4 d4d2 a3c3 c4c2 c2e2"}, /* 81: English, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_WIEGLEB, 40, 0x736110d9c0ull,
     "d7d5 f7d7 f6h6 c4e4 e4e2 f4f2 g4g6 h6f6 f2d2 d2d4 d4d6 c6e6 f6d6 d7d5 c5e5"}, /* 82: Wiegleb's, 16 marbles, 0 of 300 at random, 40.7, a picture */
    {PS_ENGLISH, 29, 0xb87061faull,
     "d7d5 c6c4 a5c5 c5e5 e6e4 b4d4 d4f4 c2c4 a3c3 d1d3 c3e3 e2e4 f4d4 d4b4 a4c4"}, /* 83: English, 16 marbles, 0 of 300 at random, 40.7, a picture */
    {PS_FRENCH, 30, 0xab9b3aa0ull,
     "d6d4 e5g5 c4a4 e4c4 c3c5 b5d5 f3f5 g5e5 d5f5 f6f4 b2b4 a4c4 d2d4 c4e4 f4d4"}, /* 84: French, 16 marbles, 0 of 300 at random, 40.7, a picture */
    {PS_FRENCH, 33, 0x13c8931a61ull,
     "e1e3 f3d3 c4a4 d5f5 e6c6 f5f3 c7c5 c5a5 a5a3 c2e2 a3c3 c3e3 f2f4 e2e4 e4g4"}, /* 85: French, 16 marbles, 0 of 300 at random, 40.7 */
    {PS_DIAMOND, 46, 0xc785c6c05ull,
     "g6e6 c6c4 d5f5 g5g3 e4e2 g3e3 e2e4 f5f3 e9e7 d4f4 e7e5 f3f5 f5d5 d6d4 d4b4"}, /* 86: diamond, 16 marbles, 0 of 300 at random, 40.7 */
    {PS_DIAMOND, 39, 0xcd578d4c0ull,
     "e7g7 h6f6 e6g6 e5c5 f5h5 g7g5 g4g6 i5g5 g6g4 h4f4 f4d4 d4b4 c6c4 b4d4 f3d3 d3d5"}, /* 87: diamond, 17 marbles, 0 of 300 at random, 41.7, a picture */
    {PS_DIAMOND, 39, 0x4b1e101eb4ull,
     "c7e7 e7g7 d6f6 b6d6 e5e3 c3c5 e2e4 d4f4 f3f5 f5f7 g7e7 e8e6 e6c6 c6c4 b4d4 d3d5"}, /* 88: diamond, 17 marbles, 0 of 300 at random, 41.7, a picture */
    {PS_FRENCH, 30, 0x4e19778e0ull,
     "e6c6 d5d3 c4a4 c3e3 b2b4 a4c4 c1c3 c4c2 c2e2 e2e4 e4e6 f6d6 c6e6 g5e5 e6e4 f4d4"}, /* 89: French, 17 marbles, 0 of 300 at random, 41.7, a picture */
    {PS_ENGLISH, 20, 0x3343ced5ull,
     "f3d3 a5c5 e4e6 e2c2 e7e5 d5d7 d3d5 b4d4 d4d6 f5d5 c2c4 c7e7 c4c6 d5d7 e7c7 c7c5"}, /* 90: English, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_WIEGLEB, 46, 0x1929e5854c0ull,
     "i5g5 g4g6 d2d4 g6e6 e5e3 e2e4 e7e5 a4c4 d7d5 c4c6 b6d6 e5c5 e4c4 c4c6 d6b6 b6b4"}, /* 91: Wiegleb's, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_WIEGLEB, 40, 0x147ffae00ull,
     "a6a4 b6b4 c6c4 g6g4 i6g6 i5g5 f5h5 c4e4 e5e3 a4c4 f4h4 h4h6 h6f6 f6d6 d6d4 c4e4 e3e5"}, /* 92: Wiegleb's, 18 marbles, 0 of 300 at random, 42.7, a picture */
    {PS_ENGLISH, 40, 0x16e317936ull,
     "d7d5 e7e5 d5b5 a4c4 d4b4 a3c3 b5b3 b3d3 g3e3 e2e4 e4e6 g5e5 e6e4 c1c3 c3e3 e4e2 e1e3"}, /* 93: English, 18 marbles, 0 of 300 at random, 42.7, a picture */
    {PS_TRIANGLE6, 0, 0x1f7fb7ull,
     "c4a4 c1a1 a1a3 e1c1 d2b4 a4c4 a6a4 c1c3 a4c2 b2d2 d3b3 f1d3 c4e2 a3c3 e2c2 c2c4 c4a6"}, /* 94: big triangle, 18 marbles, 0 of 300 at random, 42.7 */
    {PS_FRENCH, 37, 0xdea92162eull,
     "e7c7 f3f5 f5d5 c4c6 c1c3 d6d4 e2c2 b6b4 c7c5 b2d2 d3d5 b4b2 d1d3 d3b3 d5b5 b2b4 b5b3"}, /* 95: French, 18 marbles, 0 of 300 at random, 42.7 */
    {PS_ENGLISH, 30, 0xd13fe4d3ull,
     "c7e7 a5c5 c5c3 a4c4 c4c2 e4e6 e7e5 g4e4 d4f4 a3c3 d1d3 d3b3 c1c3 b3d3 d3f3 f3f5 f5d5 d6d4"}, /* 96: English, 19 marbles, 0 of 300 at random, 43.7, a picture */
    {PS_DIAMOND, 40, 0x47cfe7c40ull,
     "d6b6 b6b4 e6e8 f6h6 h6h4 d5d3 f5f3 b4d4 e4e6 h4f4 d3d5 c5e5 e5e7 e8e6 f3f5 g5e5 e6e4 e3e5"}, /* 97: diamond, 19 marbles, 0 of 300 at random, 43.7, a picture */
    {PS_WIEGLEB, 40, 0x3e7fcf800ull,
     "d6b6 b6b4 f6h6 d5b5 a5c5 e5e3 g4g6 h6f6 e6g6 i5g5 g6g4 g4e4 d4f4 f5f3 b4d4 f3d3 d3d5 c5e5"}, /* 98: Wiegleb's, 19 marbles, 0 of 300 at random, 43.7, a picture */
    {PS_ENGLISH, 29, 0xbdee483eull,
     "g3g5 f4d4 g5e5 c2c4 d3d5 e5c5 c5c3 e7c7 b3b5 d1d3 e2e4 c7c5 c3e3 e3e5 e6e4 b5d5 d6d4 e4c4"}, /* 99: English, 19 marbles, 0 of 300 at random, 43.7 */
    {PS_ENGLISH, 32, 0x1b8cfdb84ull,
     "e1c1 g5e5 e4e6 d4d6 d3b3 e7e5 g4e4 b5d5 c1c3 b3d3 d6d4 d3d5 e5c5 e2c2 c5c3 c2c4 b4d4 d4f4"}, /* 100: English, 19 marbles, 0 of 300 at random, 43.7 */
    {PS_WIEGLEB, 40, 0x57f0e1adc0ull,
     "b6d6 d6f6 f6f8 d7f7 f8f6 g6e6 e5c5 f4f6 e6g6 h6f6 h4f4 d3d5 b4d4 d5d3 f3f5 f6f4 f4d4 d3d5 c5e5"}, /* 101: Wiegleb's, 20 marbles, 0 of 300 at random, 44.7, a picture */
    {PS_WIEGLEB, 30, 0x1367b6c3288ull,
     "e2e4 e5c5 b4b6 d3d5 a6c6 e7e5 h4f4 f4d4 d5d7 d8d6 h5f5 f5d5 d4b4 a4a6 d5b5 d6b6 a6c6 b4b6 b6d6"}, /* 102: Wiegleb's, 20 marbles, 0 of 300 at random, 44.7 */
    {PS_DIAMOND, 20, 0x14f38324c67ull,
     "d7f7 d3d5 e5c5 e9e7 c6e6 e3e5 e5g5 f7d7 e1e3 g6g4 d8d6 f3d3 g4e4 c3e3 e6c6 e3e5 b5d5 e5c5 c5c7"}, /* 103: diamond, 20 marbles, 0 of 300 at random, 44.7 */
    {PS_FRENCH, 50, 0x94baf427eull,
     "c3c5 e7e5 c6c4 b4b2 d7d5 e5c5 b6b4 c5c3 e4c4 b2d2 d3b3 d1d3 g5g3 g3e3 e3c3 c4c2 b4b2 b2d2 d2f2"}, /* 104: French, 20 marbles, 0 of 300 at random, 44.7 */
    {PS_ENGLISH, 30, 0xae3ff8eaull,
     "a5c5 c5c3 g5e5 e5e7 e7c7 c7c5 a4c4 b3d3 c5c3 d3b3 a3c3 c3c1 c1e1 e1e3 e4e2 g4e4 d4f4 g3e3 e2e4 f4d4"}, /* 105: English, 21 marbles, 0 of 300 at random, 45.7, a picture */
    {PS_WIEGLEB, 49, 0x2de3ae9dc00ull,
     "c6e6 e6e4 g6i6 b4d4 b6b4 a4c4 d4f4 g4e4 i4g4 d2d4 d4f4 f4h4 h4h6 i6g6 g6e6 f2f4 f4f6 f6d6 d6d4 c4e4"}, /* 106: Wiegleb's, 21 marbles, 0 of 300 at random, 45.7, a picture */
    {PS_ENGLISH, 18, 0x1bb6ded92ull,
     "c3a3 d7d5 e1c1 d4d6 c1c3 a3a5 f5d5 e2c2 g4e4 f3d3 c4a4 c2c4 c5c3 d3b3 a5a3 d6d4 e4c4 a3c3 c3c5 c5a5"}, /* 107: English, 21 marbles, 0 of 300 at random, 45.7 */
    {PS_WIEGLEB, 21, 0x18303f85cf3ull,
     "d6d4 b6d6 g4e4 e5e3 f5f7 d6d8 b5d5 e2e4 d5d3 d2d4 d4f4 h5f5 d9d7 f8d8 f4f6 f6f8 d7f7 f7f9 f9d9 d9d7"}, /* 108: Wiegleb's, 21 marbles, 0 of 300 at random, 45.7 */
    {PS_ENGLISH, 20, 0xeb597cdfull,
     "c7c5 d7d5 b5b3 d5b5 a5c5 g5g3 a3a5 b3d3 d4d2 f3d3 c1e1 e1e3 d3f3 g3e3 c2e2 e2e4 e4e6 e7e5 f5d5 d5b5 a5c5"}, /* 109: English, 22 marbles, 0 of 300 at random, 46.7, a picture */
    {PS_FRENCH, 13, 0x137bf3a5bull,
     "e5c5 c4a4 c7e7 e3e5 b2b4 a4c4 f5f3 g3e3 e2e4 b5d5 d5f5 c3c5 d3d5 b6d6 e7e5 e4e6 c5e5 d6f6 f6f4 g4e4 e4e6"}, /* 110: French, 22 marbles, 0 of 300 at random, 46.7 */
    {PS_WIEGLEB, 40, 0x3f705629972ull,
     "c6e6 f3f5 d2d4 d4d6 h4f4 e9e7 d7d5 f8f6 f5f3 f6h6 i6g6 d5f5 f5h5 e7e5 f2f4 i5g5 g6g4 e2e4 e5e3 g4e4 e3e5"}, /* 111: Wiegleb's, 22 marbles, 0 of 300 at random, 46.7 */
    {PS_DIAMOND, 12, 0x152dc5adc95ull,
     "d5f5 h4f4 e1e3 d3d5 e4g4 e9e7 d6d4 c4e4 f5h5 f7d7 h6f6 c7c5 f6d6 g3g5 b5d5 e3e5 d6d4 h5f5 f5d5 d4d6 d6d8"}, /* 112: diamond, 22 marbles, 0 of 300 at random, 46.7 */
    {PS_ENGLISH, 31, 0x5efafbd9ull,
     "c6e6 c5e5 a5c5 e5e3 g5e5 e6e4 c4c6 c7c5 e4e2 g3e3 c2c4 c5c3 a4c4 c4c2 a3c3 d3b3 e2e4 c1c3 b3d3 d2d4 d4f4 "
     "g4e4"}, /* 113: English, 23 marbles, 0 of 300 at random, 47.7, a picture */
    {PS_WIEGLEB, 57, 0x1af081f070feull,
     "f9d9 f6f4 f3f5 d7f7 d9d7 d5b5 f5d5 d6f6 f7f5 e4e2 g5e5 f8d8 e5c5 d8d6 b5d5 f1f3 d6d4 d2f2 d4d2 f3f1 f1d1 "
     "d1d3"}, /* 114: Wiegleb's, 23 marbles, 0 of 300 at random, 47.7 */
    {PS_ENGLISH, 21, 0x17aeeeed6ull,
     "a5a3 e4e6 g4e4 d6d4 d3d5 b3d3 e1e3 c1c3 d3b3 f3d3 b4d4 a3c3 e4c4 d2d4 c3c5 d4d6 b5d5 e7e5 d6d4 f5d5 d4d6 "
     "d7d5"}, /* 115: English, 23 marbles, 0 of 300 at random, 47.7 */
    {PS_DIAMOND, 46, 0xd87c7f3cb2ull,
     "d5d3 e6g6 g3e3 g5g7 f5d5 f2d2 c6e6 g7e7 e3e5 c4c6 d2d4 g4e4 d7f7 e5e7 e4c4 f7d7 c7c5 d8d6 c4c6 d6b6 a5c5 "
     "d5b5 b6b4"}, /* 116: diamond, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_FRENCH, 36, 0x3f79eee6aull,
     "b2b4 c4c6 c2c4 d3d1 e3e5 f2d2 d1d3 f5f3 g3e3 d5f5 g5e5 e3c3 d7d5 b5b3 e5c5 b6d6 d4b4 c3a3 e6c6 a3a5 c6c4 "
     "c4a4 a5a3"}, /* 117: French, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_ENGLISH, 58, 0xbbbdcefbull,
     "c7c5 c5c3 c2c4 g4e4 d4f4 b4d4 e6e4 e3g3 a5c5 c5e5 f5f3 d7d5 g3e3 d4f4 a3c3 e3e1 d5f5 c3e3 f5f3 e1c1 f3d3 "
     "d3d1 c1e1"}, /* 118: English, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_DIAMOND, 40, 0x14e55ff54e5ull,
     "e7g7 e9e7 d7f7 g7e7 e6e8 c5c3 a5c5 c6c4 g5g3 i5g5 g6g4 e4e6 c3c5 c5e5 e5e7 e8e6 e3c3 g3g5 g5e5 e6e4 e1e3 "
     "f3d3 c3e3 e3e5"}, /* 119: diamond, 25 marbles, 0 of 300 at random, 49.7, a picture */
    {PS_DIAMOND, 60, 0x14eec7e6cb6ull,
     "c4e4 f5h5 c6e6 h4h6 e5e7 f3f5 d3f3 f6f4 e1e3 e7g7 f3d3 c5e5 f4d4 d3d5 e5c5 b5d5 d8d6 d5d7 c7e7 e8e6 h6f6 "
     "e6g6 g7g5 g5g3"}, /* 120: diamond, 25 marbles, 0 of 300 at random, 49.7 */
    {PS_DIAMOND, 4, 0x1ca54fa79e7ull,
     "f2f4 f6f8 e1e3 d7f7 d3f3 e5c5 f4d4 b5d5 d5d7 g5e5 c4e4 e5e3 e3g3 d8d6 d6f6 g3g5 g6e6 h5f5 e9e7 e6e8 f8f6 "
     "f5f7 g7e7 e7e9"}, /* 121: diamond, 25 marbles, 0 of 300 at random, 49.7 */
    {PS_FRENCH, 21, 0x1bfbbbba72ull,
     "e5c5 e1e3 d7d5 d2d4 b2d2 d4d6 e3e5 g3e3 b5d5 f5f3 e6e4 b3b5 e3e5 d6d4 f2f4 c3c5 g4e4 d1d3 c6c4 e5e3 e3c3 "
     "d4b4 a4c4 c3c5 b5d5"}, /* 122: French, 26 marbles, 0 of 300 at random, 50.7 */
    {PS_DIAMOND, 52, 0x6e7ceef6c5ull,
     "b6d6 e7g7 d5d7 e3e5 b5d5 f5f7 h6f6 e6g6 f3f5 c4e4 e5e3 g5e5 e2e4 e4e6 g7g5 d2d4 e9e7 d4d6 d6d8 f7d7 d8d6 "
     "g4g6 d6f6 f6h6 h6h4"}, /* 123: diamond, 26 marbles, 0 of 300 at random, 50.7 */
    {PS_FRENCH, 19, 0x1efb7bbd7ull,
     "g4e4 f6d6 a5c5 c6e6 d5b5 f5d5 c3c5 a4c4 d4d6 d7d5 e3e1 e7e5 c4c6 d3d1 c7c5 b2b4 e1c1 e5e3 b4b6 d5b5 f3d3 "
     "c1c3 d3b3 b6b4 b3b5"}, /* 124: French, 26 marbles, 0 of 300 at random, 50.7 */
    {PS_TRIANGLE6, 0, 0x1ffffeull,
     "a4a6 c4a4 a6c4 a3a5 c3a3 d3b5 a2a4 a5a3 b1b3 a3c3 d2b2 d1b1 a1c1 f1d1 d1b1 b1b3 b3d3 e2c4 c4a6"}, /* 125: big triangle, 20 marbles, 0 of 300 at random, 44.7, the full board */
    {PS_ENGLISH, 30, 0x1fffffae8ull,
     "a5c5 c5e5 e5e7 g5e5 e4e6 e7e5 c4e4 a4c4 e4e6 g4e4 c3c5 c6c4 a3c3 d3b3 e3e5 e6e4 g3e3 c1c3 c4c2 e1c1 c1c3 "
     "b3d3 d2d4 d4f4 e2e4 f4d4"}, /* 126: English, 27 marbles, 0 of 300 at random, 51.7, a picture */
    {PS_DIAMOND, 49, 0x567fa65efeull,
     "d6f6 d3d5 d8d6 d5d7 g6e6 b5d5 i5g5 g4g6 e4g4 g3g5 f8d8 e2e4 b4d4 g5e5 d5f5 d4f4 f4f6 b6d6 e6c6 d8d6 f7d7 "
     "c7c5 d7d5 g6e6 c5e5 e6e4"}, /* 127: diamond, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_DIAMOND, 28, 0xcf3df2f7a5ull,
     "f5d5 f7f5 e4g4 h6f6 g5e5 i5g5 e9e7 e2e4 f2f4 b6d6 d4d2 d7f7 g4g6 d5f5 f4d4 c3c5 g7g5 e6c6 g5e5 f7f5 f5d5 "
     "c6c4 d5d3 d2d4 d4b4 b4b6"}, /* 128: diamond, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_DIAMOND, 39, 0x16b4cbe6ffaull,
     "c6e6 c4e4 e1e3 d2d4 g7g5 g4g6 e4c4 c4c6 b6d6 e6c6 e5c5 c6c4 g6e6 e7e5 e5g5 f3d3 h5f5 c7e7 c3c5 f8f6 f5f7 "
     "f7d7 d8d6 b5d5 d6d4 d3d5"}, /* 129: diamond, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_DIAMOND, 14, 0x7af6ae7577ull,
     "g3e3 d2f2 e4e2 f2d2 d2d4 d8d6 f5f3 e7e5 e9e7 h4f4 c6e6 g7g5 h5f5 d4d6 f5d5 f3f5 f5f7 d5d7 e6e8 b4b6 c7e7 "
     "c4c6 b6d6 f7d7 d6d8 d8f8"}, /* 130: diamond, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_ENGLISH, 33, 0xf7ff6bffull,
     "c5e5 c7c5 e7c7 a4c4 c4c6 c7c5 c1e1 e2c2 c2c4 a3c3 f5d5 g3g5 c4c6 a5c5 c6c4 d4d2 c4c2 f3f5 e4e2 g5e5 e1e3 "
     "c2e2 e2e4 d6d4 d4f4 e6e4 e4g4"}, /* 131: English, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_FRENCH, 24, 0x19bfbdfedaull,
     "e4c4 c4c2 a4c4 d2d4 g4e4 d5d3 b5d5 b2b4 f6d6 e4e6 d6d4 e2e4 e1c1 d3d5 g3e3 c1c3 e3e5 e6e4 b4d4 e4c4 c3c5 "
     "b6d6 g5e5 d5f5 d7d5 c5e5 e5g5"}, /* 132: French, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_WIEGLEB, 67, 0xbfd874ffcd0ull,
     "d7d5 f6d6 e8e6 i4g4 f4h4 d6f6 a5c5 f3f1 g6g4 i6g6 g6e6 d5f5 b6d6 d3f3 e6c6 h4f4 i5g5 f4f2 g5e5 c6c4 f1f3 "
     "e1e3 f3d3 d2d4 e5e3 c4e4 e4e2"}, /* 133: Wiegleb's, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_DIAMOND, 67, 0x15efd6e5eadull,
     "e3e5 c4e4 e9e7 f8f6 f5h5 d7f7 d6d4 b6d6 d3d5 f7f5 g3g5 f4f2 g6g4 d6d4 b5d5 h4f4 i5g5 f4f6 e6g6 d4f4 d5f5 "
     "e1e3 f5f3 g6g4 f2f4 g4e4 e4e2"}, /* 134: diamond, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_DIAMOND, 44, 0x176fdbbfca5ull,
     "d3f3 e9e7 i5g5 d5d3 f5d5 e7c7 c7c5 c4c6 f7f5 f4d4 a5c5 c6c4 c4e4 d5d7 h6f6 f6d6 d7d5 d2d4 d5d3 g3e3 d3f3 "
     "f5h5 h4f4 e1e3 e3g3 e4g4 g3g5 g5i5"}, /* 135: diamond, 29 marbles, 0 of 300 at random, 53.7 */
    {PS_WIEGLEB, 42, 0x1fa7e3f9b1e2ull,
     "h6f6 d6d8 f6d6 d1d3 f7f9 d4d2 f5f3 c4c6 h4f4 c6e6 f9d9 d9d7 e5c5 h5f5 b5d5 e7e5 f3d3 d2d4 e1e3 d4d6 f1f3 "
     "e5g5 e3e5 d7d5 d5f5 g5e5 f3f5 e5g5"}, /* 136: Wiegleb's, 29 marbles, 0 of 300 at random, 53.7 */
    {PS_WIEGLEB, 13, 0x1bb1bed5bcd9ull,
     "d7d5 f1f3 d2d4 d9d7 d5b5 f3d3 d3d5 h6f6 d7f7 a5c5 e5e7 b6d6 e8e6 i5g5 g5e5 d6d4 e5e7 e1e3 e3e5 c4e4 f4d4 "
     "a4c4 f7f5 c4c6 f5d5 d4d6 c6e6 e6e8"}, /* 137: Wiegleb's, 29 marbles, 0 of 300 at random, 53.7 */
    {PS_WIEGLEB, 44, 0xff7b13d37abull,
     "f3f5 f5h5 h4f4 d3f3 c5e5 f7d7 d6f6 b4d4 d1d3 d4d2 a6c6 d8d6 f3f1 c6e6 e5e3 e6g6 a5c5 d9f9 e2e4 h6f6 f9f7 "
     "f7f5 f4d4 f1d1 d1d3 d3d5 c5e5 e5g5 g5i5"}, /* 138: Wiegleb's, 30 marbles, 0 of 300 at random, 54.7 */
    {PS_WIEGLEB, 23, 0x1eadffb8713bull,
     "f1f3 f7f9 h4h6 b4b6 i4i6 i6g6 f4f2 d5b5 f5f7 g6g4 d4b4 d9d7 a4c4 b6b4 b4d4 d6f6 d4f4 g4e4 e9e7 f7f5 d7f7 "
     "d1f1 e4e2 f1f3 d2f2 f2f4 f4f6 f6f8 f9f7"}, /* 139: Wiegleb's, 30 marbles, 0 of 300 at random, 54.7 */
    {PS_ENGLISH, 30, 0x1fffd7fffull,
     "c6c4 e6c6 c7c5 e7c7 c4e4 a4c4 c4c6 c7c5 e4e6 c5e5 e6e4 a5c5 g5e5 e4e6 g4e4 e3e5 e6e4 g3e3 d3f3 c2c4 a3c3 "
     "e1e3 e4e2 c1e1 e1e3 f3d3 d3b3 c5c3 b3d3 d2d4"}, /* 140: English, 31 marbles, 0 of 300 at random, 55.7, a picture */
    {PS_WIEGLEB, 27, 0xbdddb59ff1bull,
     "f7f5 i4g4 e1e3 g6i6 f4f6 g5i5 d9d7 e9e7 d7f7 f7f5 d4f4 f4h4 i6i4 f2f4 d2d4 i4g4 d6f6 b5d5 d4d6 g4e4 c6e6 "
     "a6c6 f6d6 a4c4 f5d5 e3e5 e5c5 d6b6 c4c6 c6a6"}, /* 141: Wiegleb's, 31 marbles, 0 of 300 at random, 55.7 */
    {PS_WIEGLEB, 12, 0x1bf5abfdbcf2ull,
     "d6d8 h6f6 d2d4 b6d6 a4a6 f5f7 c4c6 e5c5 e7e5 f3d3 e4g4 f7f9 f9d9 e1e3 d4d2 d6b6 f1f3 f3d3 b5d5 a6c6 d2d4 "
     "h4f4 d4d6 c6e6 d9d7 h5f5 e5e7 d7f7 f4f6 f6f8 f8d8"}, /* 142: Wiegleb's, 32 marbles, 0 of 300 at random, 56.7 */
    {PS_WIEGLEB, 31, 0xfd7e05dffbeull,
     "f7f5 f5d5 e1e3 f3f5 h4f4 f9f7 b6b4 d3f3 e4g4 c4e4 f2f4 h6f6 d1d3 f4d4 f6f4 g4e4 e4c4 d6d4 f7d7 d3d5 e9e7 "
     "b4d4 d8d6 d5b5 d6b6 b6b4 a6a4 a4c4 c4e4 e7e5 e4e6"}, /* 143: Wiegleb's, 32 marbles, 0 of 300 at random, 56.7 */
    {PS_WIEGLEB, 53, 0x1ba5f79bfcfeull,
     "i6i4 d7d5 f9f7 c5e5 b6d6 e6c6 f5d5 d8f8 g6e6 d5d3 f1d1 i4g4 b4b6 d2d4 e3e5 g4e4 f2d2 b6d6 d1d3 f8f6 e6e8 "
     "e9e7 e4e6 d3d5 d6d4 e7e5 c4e4 e4e6 e6g6 g6i6 g5i5 i6i4"}, /* 144: Wiegleb's, 33 marbles, 0 of 300 at random, 57.7 */
    {PS_FRENCH, 40, 0x1ffffdf3ffull,
     "c7c5 d7d5 f6d6 d5d7 e7c7 b5d5 e5c5 g5e5 a4c4 c4c6 c7c5 e4c4 c4c6 b6d6 g4e4 e4e6 e6c6 b2b4 d3b3 a3c3 f3d3 "
     "d3b3 b3b5 a5c5 c6c4 c1c3 c4c2 d1d3 f2d2 c2e2 e1e3 d3f3 g3e3"}, /* 145: French, 34 marbles, 0 of 300 at random, 58.7, a picture */
    {PS_WIEGLEB, 31, 0xfeb7b7eecfeull,
     "b6d6 b4b6 f9d9 a4a6 a6c6 d6b6 d7f7 g4e4 c4c6 b6d6 f2f4 d1d3 d9d7 d3f3 e1e3 f6h6 f3d3 d6f6 d5f5 d3d5 i6g6 "
     "f6h6 f4f6 h6h4 f8d8 d8d6 f7f5 d6d4 i4g4 d4f4 g4g6 f4f6 g6e6"}, /* 146: Wiegleb's, 34 marbles, 0 of 300 at random, 58.7 */
    {PS_WIEGLEB, 44, 0x1bdfdef9f7beull,
     "a6c6 d6b6 f7d7 f9f7 d4d6 a4c4 f1d1 i4i6 b6b4 e6c6 b4d4 d8d6 d3d5 d1d3 e9e7 f2d2 g6e6 d6b6 f4f6 i6g6 d5b5 "
     "f7f5 e5g5 d2d4 b6b4 e4c4 h4f4 b4d4 e7e5 g6g4 g4e4 d4f4 f3f5 e5g5 g5i5"}, /* 147: Wiegleb's, 36 marbles, 0 of 300 at random, 60.7 */
    {PS_WIEGLEB, 13, 0x1fedf7fbfddfull,
     "d1d3 d4d2 d8f8 f3d3 i4g4 h6h4 f6h6 f4f6 h4f4 d6d8 i6g6 b6d6 b4b6 f1f3 d2d4 f3f5 c4c6 d5d7 b6d6 d7d5 f6d6 "
     "f8f6 d9d7 f9d9 d6d8 d4d6 d9d7 d7d5 f5h5 e1e3 g6e6 d5f5 i5g5 e3e5 e6e4 g5e5 e4e6 e6e8"}, /* 148: Wiegleb's, 39 marbles, 0 of 300 at random, 63.7 */
    {PS_ENGLISH, 30, 0x1fffeffffull,
     "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5 a4c4 c4c6 e4c4 g4e4 e4e6 e7e5 c3c5 c6c4 a3c3 d3b3 e2e4 "
     "g3e3 c1c3 c4c2 e1c1 c1c3 b3d3 d3f3 e5e3 f3d3 d2d4"}, /* 149: English, 32 marbles, 0 of 300 at random, 56.7, the full board */
    {PS_FRENCH, 48, 0x1fffffffdfull,
     "b6d6 e6c6 b4b6 b6d6 d5b5 d7d5 a5c5 d5b5 f5d5 d4d6 a3a5 a5c5 c4c6 c7c5 f3f5 f6f4 d3f3 b3d3 f3f5 g5e5 e4e6 "
     "e7e5 c1c3 e1e3 d3f3 g3e3 d1d3 d3f3 f2f4 g4e4 e4e6 e6c6 c6c4 c4c2 b2d2"}, /* 150: French, 36 marbles, 0 of 300 at random, 60.7, the full board */
};
