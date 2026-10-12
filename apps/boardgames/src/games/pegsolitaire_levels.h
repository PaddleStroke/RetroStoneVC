/*
 * Peg Solitaire's levels: our own positions, made by tools/pegsolitaire_gen.c (played backwards from one
 * marble in the goal hole, and the classic full boards solved by search), six chapters of 25 by the marbles
 * at the start, each from the easiest. Each: its board, its goal hole (a cell of the 9 x 9 grid), its marbles
 * (bit i: the board's hole i, in cell order) and a way to one marble in the goal; then (in the comment) the
 * marbles, how many of 300 games played at random end in the goal, its grade.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const ps_level_t ps_levels[PS_LEVELS] = {
    {PS_TRIANGLE, 18, 0x1014ull,
     "b4b2 c1a3"}, /* 1: triangle, 3 marbles, 300 of 300 at random, 3.0 */
    {PS_ENGLISH, 11, 0x8031ull,
     "e6c6 c7c5 c4c6"}, /* 2: English, 4 marbles, 155 of 300 at random, 6.8 */
    {PS_TRIANGLE, 18, 0x4190ull,
     "b3d1 e1c1 c1a3"}, /* 3: triangle, 4 marbles, 100 of 300 at random, 8.7 */
    {PS_ENGLISH, 49, 0x10400108ull,
     "c6c4 c4c2 c2e2"}, /* 4: English, 4 marbles, 78 of 300 at random, 9.8 */
    {PS_ENGLISH, 29, 0x6a000ull,
     "f4d4 d4b4 a4c4"}, /* 5: English, 4 marbles, 68 of 300 at random, 10.4 */
    {PS_TRIANGLE, 30, 0xcc4ull,
     "b1b3 a1a3 a3c3 b4d2"}, /* 6: triangle, 5 marbles, 74 of 300 at random, 11.0 */
    {PS_TRIANGLE, 0, 0x1320ull,
     "d2b4 c1c3 c3a5"}, /* 7: triangle, 4 marbles, 50 of 300 at random, 11.7 */
    {PS_TRIANGLE, 38, 0x48aull,
     "a4a2 a1a3 a3c1"}, /* 8: triangle, 4 marbles, 43 of 300 at random, 12.3 */
    {PS_ENGLISH, 38, 0x81810000ull,
     "d4d2 d1d3 e3c3"}, /* 9: English, 4 marbles, 35 of 300 at random, 13.2 */
    {PS_ENGLISH, 20, 0xb20200ull,
     "a3c3 c3e3 e3e5 e5c5"}, /* 10: English, 5 marbles, 40 of 300 at random, 13.6 */
    {PS_ENGLISH, 31, 0x18018400ull,
     "c4e4 c2e2 e5e3 e2e4"}, /* 11: English, 5 marbles, 36 of 300 at random, 14.1 */
    {PS_TRIANGLE, 18, 0x3348ull,
     "d1b1 d2b2 a3c1 c1a1 a1a3"}, /* 12: triangle, 6 marbles, 39 of 300 at random, 14.7 */
    {PS_TRIANGLE, 30, 0x984ull,
     "b1b3 b4b2 b2d2"}, /* 13: triangle, 4 marbles, 21 of 300 at random, 15.3 */
    {PS_ENGLISH, 47, 0x18860000ull,
     "c2e2 f4d4 d4d2 e2c2"}, /* 14: English, 5 marbles, 23 of 300 at random, 15.9 */
    {PS_TRIANGLE, 20, 0x18full,
     "b2d2 a5c3 d2b4 a3a5 a5c3"}, /* 15: triangle, 6 marbles, 26 of 300 at random, 16.4 */
    {PS_ENGLISH, 49, 0x10708008ull,
     "c3c5 c6c4 a3c3 c4c2 c2e2"}, /* 16: English, 6 marbles, 22 of 300 at random, 17.1 */
    {PS_ENGLISH, 19, 0xc60108ull,
     "c6c4 c3c5 f4d4 d3d5 d5b5"}, /* 17: English, 6 marbles, 19 of 300 at random, 17.7 */
    {PS_ENGLISH, 30, 0x1061c000ull,
     "c4e4 b3d3 d2d4 e4c4 b4d4"}, /* 18: English, 6 marbles, 17 of 300 at random, 18.2 */
    {PS_TRIANGLE, 36, 0x372ull,
     "d2b2 b2b4 c3a5 a5a3 a3a1"}, /* 19: triangle, 6 marbles, 14 of 300 at random, 19.0 */
    {PS_TRIANGLE, 37, 0x2984ull,
     "b1b3 b4b2 d1b3 b3b1"}, /* 20: triangle, 5 marbles, 9 of 300 at random, 19.7 */
    {PS_ENGLISH, 21, 0x208310ull,
     "c5c3 b3d3 d6d4 d3d5"}, /* 21: English, 5 marbles, 7 of 300 at random, 20.7 */
    {PS_ENGLISH, 58, 0x28410300ull,
     "c2c4 c5c3 d5d3 c3e3 e3e1"}, /* 22: English, 6 marbles, 7 of 300 at random, 21.7 */
    {PS_ENGLISH, 21, 0x2818100ull,
     "c5c3 c3e3 f3d3 d3d5"}, /* 23: English, 5 marbles, 4 of 300 at random, 22.7 */
    {PS_ENGLISH, 13, 0x11038200ull,
     "d5d3 d2d4 e3e5 c4e4 e4e6"}, /* 24: English, 6 marbles, 3 of 300 at random, 24.7 */
    {PS_ENGLISH, 28, 0xc588ull,
     "b5d5 e5c5 b4d4 c6c4 d4b4"}, /* 25: English, 6 marbles, 0 of 300 at random, 30.7 */
    {PS_TRIANGLE6, 38, 0x180c49ull,
     "a2c2 f1d1 d1b3 a3a5 a6a4 a4c2"}, /* 26: big triangle, 7 marbles, 106 of 300 at random, 11.5 */
    {PS_ENGLISH, 47, 0x25850800ull,
     "d4d2 f5f3 e2e4 g3e3 e4e2 e2c2"}, /* 27: English, 7 marbles, 24 of 300 at random, 17.8 */
    {PS_TRIANGLE, 20, 0x3bd2ull,
     "c1a3 d2b2 a3c1 a4c2 c1a1 d1b3 a1a3 a3c3"}, /* 28: triangle, 9 marbles, 26 of 300 at random, 19.4 */
    {PS_TRIANGLE, 29, 0x33d7ull,
     "c1e1 e1c3 a5a3 a2a4 c3a5 a5a3 c2a4 a4a2 a2c2"}, /* 29: triangle, 10 marbles, 25 of 300 at random, 20.6 */
    {PS_TRIANGLE6, 9, 0xc28e0ull,
     "a3c3 c4c2 d1b3 b2b4 e1c3 c3a5"}, /* 30: big triangle, 7 marbles, 9 of 300 at random, 21.7 */
    {PS_TRIANGLE, 38, 0x3bdaull,
     "c1c3 d2b4 a4c2 b1b3 b4b2 d1b3 a3a1 b3b1 a1c1"}, /* 31: triangle, 10 marbles, 15 of 300 at random, 22.7 */
    {PS_TRIANGLE, 28, 0x5396ull,
     "c1c3 c3a3 a4a2 a2c2 e1c3 b4d2 d2b2"}, /* 32: triangle, 8 marbles, 8 of 300 at random, 23.2 */
    {PS_TRIANGLE6, 48, 0x25a84ull,
     "c1c3 e2c4 b2b4 b4d2 b5d3 d3d1"}, /* 33: big triangle, 7 marbles, 5 of 300 at random, 23.9 */
    {PS_FRENCH, 47, 0x21386410ull,
     "f5f3 g5g3 c6c4 g3e3 e4e2 c4c2 b2d2 e2c2"}, /* 34: French, 9 marbles, 7 of 300 at random, 24.7 */
    {PS_ENGLISH, 13, 0x1007a224ull,
     "d4b4 e7e5 f4d4 d5d3 d2d4 a4c4 c4e4 e4e6"}, /* 35: English, 9 marbles, 6 of 300 at random, 25.3 */
    {PS_FRENCH, 36, 0x101161aull,
     "b6d6 b5d5 e5c5 d7d5 d5b5 b5b3 c3a3"}, /* 36: French, 8 marbles, 4 of 300 at random, 25.7 */
    {PS_ENGLISH, 47, 0x15608510ull,
     "c4c6 b3d3 d3f3 c6e6 e6e4 g3e3 e4e2 e2c2"}, /* 37: English, 9 marbles, 4 of 300 at random, 26.7 */
    {PS_FRENCH, 58, 0x1c3000c00ull,
     "c5e5 c3e3 e2e4 c2e2 e5e3 e3e1"}, /* 38: French, 7 marbles, 2 of 300 at random, 26.9 */
    {PS_FRENCH, 14, 0x3030870ull,
     "d6d4 d3d5 c3c5 d5b5 b4b6 b6d6 d6f6"}, /* 39: French, 8 marbles, 2 of 300 at random, 27.9 */
    {PS_TRIANGLE6, 39, 0xc2883ull,
     "a6a4 a4c2 e1c3 d1b3 b2b4 b4d2"}, /* 40: big triangle, 7 marbles, 1 of 300 at random, 28.7 */
    {PS_TRIANGLE, 0, 0x2b7cull,
     "b4b2 d2b4 a3a1 a1c1 c1e1 b2d2 e1c3 c3a5"}, /* 41: triangle, 9 marbles, 2 of 300 at random, 28.9 */
    {PS_ENGLISH, 33, 0x318c1200ull,
     "d2d4 d5d3 g5g3 d3f3 g3e3 e2e4 e4g4"}, /* 42: English, 8 marbles, 1 of 300 at random, 29.7 */
    {PS_ENGLISH, 11, 0x10c06b9ull,
     "g4e4 e5c5 e3e5 b5d5 e6e4 d6d4 c7c5 e4c4 c4c6"}, /* 43: English, 10 marbles, 2 of 300 at random, 29.9 */
    {PS_FRENCH, 47, 0xe4d80200ull,
     "b2b4 c2e2 f4d4 e2e4 b5b3 e4c4 a3c3 c4c2"}, /* 44: French, 9 marbles, 1 of 300 at random, 30.7 */
    {PS_TRIANGLE, 9, 0x63f6ull,
     "e1c1 c1a3 d2b2 a2c2 a4a2 b4d2 d2b2 a2c2 c2a4"}, /* 45: triangle, 10 marbles, 1 of 300 at random, 31.7 */
    {PS_FRENCH, 50, 0x12ce23000ull,
     "a3c3 e5g5 c4c2 e2e4 g5g3 g3e3 b2d2 e4e2 d2f2"}, /* 46: French, 10 marbles, 1 of 300 at random, 31.7 */
    {PS_ENGLISH, 11, 0x638d04ull,
     "f5d5 c5e5 b3d3 e4e6 e7e5 d3d5 e5c5 c4c6"}, /* 47: English, 9 marbles, 0 of 300 at random, 33.7 */
    {PS_TRIANGLE6, 10, 0xa0b95ull,
     "b3b1 a6c4 c4e2 b1d1 d1f1 f1d3 d3b3 b3b5"}, /* 48: big triangle, 9 marbles, 0 of 300 at random, 33.7 */
    {PS_TRIANGLE6, 36, 0x31bfull,
     "b3d3 b5b3 d3b5 a5a3 a6c4 a3c3 d2b2 c4c2 c2a2"}, /* 49: big triangle, 10 marbles, 0 of 300 at random, 34.7 */
    {PS_ENGLISH, 33, 0xb04a350ull,
     "c5c3 f3d3 a5a3 d6d4 d3b3 a3c3 c2c4 c4e4 e4g4"}, /* 50: English, 10 marbles, 0 of 300 at random, 34.7 */
    {PS_TRIANGLE, 28, 0x6bd7ull,
     "e1c3 a5a3 a3a1 a1c1 d1b1 c2a4 b1b3 a4c2 b4d2 d2b2"}, /* 51: triangle, 11 marbles, 21 of 300 at random, 22.3 */
    {PS_TRIANGLE, 27, 0x69f7ull,
     "e1c1 a5a3 b4d2 a2a4 c1c3 a4c2 b1b3 d2b4 b4b2 c2a2"}, /* 52: triangle, 11 marbles, 3 of 300 at random, 29.7 */
    {PS_WIEGLEB, 31, 0x12105c2c080ull,
     "e2e4 e4e6 f5h5 i6i4 f6h6 e7e5 h6h4 i4g4 g4e4 e4e6"}, /* 53: Wiegleb's, 11 marbles, 2 of 300 at random, 30.9 */
    {PS_DIAMOND, 37, 0x8a4a0019a0ull,
     "g7e7 e7c7 f2f4 g4e4 e6c6 e4c4 b4d4 d3d5 c7c5 d5b5"}, /* 54: diamond, 11 marbles, 1 of 300 at random, 32.7 */
    {PS_DIAMOND, 49, 0x758024510ull,
     "e4e2 c7c5 b5d5 c3e3 g7g5 g5g3 d5d3 d3f3 g3e3 e2e4"}, /* 55: diamond, 11 marbles, 1 of 300 at random, 32.7 */
    {PS_DIAMOND, 38, 0x4016076580ull,
     "g7e7 g6e6 c5c7 e7e5 e5e3 a5c5 e2e4 b4d4 e4c4 c4c6 c7c5"}, /* 56: diamond, 12 marbles, 1 of 300 at random, 33.7 */
    {PS_FRENCH, 4, 0x12c0182c72ull,
     "f5f3 f2f4 f4d4 d5d3 d3d1 c6c4 e1c1 c1c3 d7d5 c3c5 c5e5 e5e7"}, /* 57: French, 13 marbles, 1 of 300 at random, 34.7 */
    {PS_TRIANGLE6, 28, 0x1b79b3ull,
     "f1d3 d3b5 a6a4 a4c4 b2b4 b5d3 d3b3 e1c3 b4d2 c1c3 d2b4 b4b2 b1b3"}, /* 58: big triangle, 14 marbles, 1 of 300 at random, 35.7 */
    {PS_TRIANGLE, 30, 0x6fd6ull,
     "a1a3 e1c1 b1d1 a3c1 d1b1 d2b2 a4c2 b1b3 b4b2 b2d2"}, /* 59: triangle, 11 marbles, 0 of 300 at random, 35.7 */
    {PS_WIEGLEB, 33, 0x16b06400048ull,
     "f3f5 i5g5 d8d6 f5d5 d6d4 e2e4 d4f4 f4h4 i4g4 g4g6"}, /* 60: Wiegleb's, 11 marbles, 0 of 300 at random, 35.7 */
    {PS_DIAMOND, 68, 0x8607800ccull,
     "f5h5 f4h4 d5f5 f8f6 h4h6 e8e6 e6g6 h6f6 f6f4 f4f2"}, /* 61: diamond, 11 marbles, 0 of 300 at random, 35.7 */
    {PS_DIAMOND, 34, 0x41b0d00dull,
     "e9e7 f5d5 e7e5 h6f6 d5f5 f5f7 f8f6 i5g5 g4g6 f6h6"}, /* 62: diamond, 11 marbles, 0 of 300 at random, 35.7 */
    {PS_ENGLISH, 13, 0x2b360550ull,
     "f4d4 e2e4 e5e3 a3c3 f3d3 d3d5 c2c4 d5b5 a5c5 c4c6 c6e6"}, /* 63: English, 12 marbles, 0 of 300 at random, 36.7 */
    {PS_ENGLISH, 39, 0x338d5600ull,
     "d5f5 g4e4 e2c2 d4f4 g5e5 e3c3 f3f5 c2c4 f5d5 b4d4 d5d3"}, /* 64: English, 12 marbles, 0 of 300 at random, 36.7 */
    {PS_WIEGLEB, 50, 0x1b013680090ull,
     "d2d4 e2e4 e5c5 e4c4 b4b6 e8e6 c4c6 b6d6 d6f6 h5f5 f6f4"}, /* 65: Wiegleb's, 12 marbles, 0 of 300 at random, 36.7 */
    {PS_FRENCH, 37, 0x1963340844ull,
     "b2d2 c3e3 e7e5 e1c1 g4e4 e2c2 d5d3 d3f3 c1c3 e5e3 f3d3 d3b3"}, /* 66: French, 13 marbles, 0 of 300 at random, 37.7 */
    {PS_FRENCH, 41, 0xe121bf0ull,
     "f4f2 d3f3 a5c5 e5e7 c6e6 f6d6 f2f4 c4c6 c6e6 e7e5 d5f5 f5f3"}, /* 67: French, 13 marbles, 0 of 300 at random, 37.7 */
    {PS_ENGLISH, 12, 0x3e05cb80ull,
     "c5c3 g3e3 e3e1 b5b3 c2e2 b3d3 f5f3 d4d2 e1e3 f3d3 d2d4 d4d6"}, /* 68: English, 13 marbles, 0 of 300 at random, 37.7 */
    {PS_TRIANGLE6, 10, 0x19bd3cull,
     "a1c1 c3a5 f1d1 c1e1 e1c3 b2d2 a5a3 a2a4 b5d3 d2b4 a4c4 d3b5"}, /* 69: big triangle, 13 marbles, 0 of 300 at random, 37.7 */
    {PS_FRENCH, 23, 0x47283e23ull,
     "c2c4 e5g5 g5g3 d3f3 c4c6 g3e3 e3e5 c7e7 e5c5 c6e6 b5d5 e7e5 d5f5"}, /* 70: French, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_WIEGLEB, 29, 0x1203412031d8ull,
     "f1f3 d6f6 d4d6 d8f8 g4g6 f3f5 d6d8 f7d7 f5f7 f8f6 d8d6 g6e6 e6c6"}, /* 71: Wiegleb's, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_ENGLISH, 24, 0x16885f2aull,
     "d2d4 d4d6 c6c4 e6e4 d7d5 g5e5 d5f5 b4d4 d4f4 g3e3 f5f3 e3g3 g3g5"}, /* 72: English, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_WIEGLEB, 53, 0x1221848b452ull,
     "a4c4 b6b4 e2e4 b4d4 d4f4 d7d5 e9e7 d5f5 e7e5 f4h4 e5g5 g6g4 g4i4"}, /* 73: Wiegleb's, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_ENGLISH, 32, 0xbc785b0ull,
     "f3f5 d4b4 f5d5 c2c4 e3e5 d6d4 e6e4 d4d2 b5d5 b4d4 d5d3 d2d4 d4f4"}, /* 74: English, 14 marbles, 0 of 300 at random, 38.7 */
    {PS_TRIANGLE, 0, 0x7ffeull,
     "a3a5 c3a3 a5c3 a2a4 d2b4 b1b3 d1b1 a1c1 c1c3 b4d2 e1c3 c3a3 a3a5"}, /* 75: triangle, 14 marbles, 0 of 300 at random, 38.7, the full board */
    {PS_TRIANGLE6, 29, 0x1ebaf3ull,
     "d1b1 f1d1 a6a4 a1c1 c4e2 a3a5 c1e1 a5c3 b2b4 c3c1 e2c2 c1c3 b4d2 e1c3"}, /* 76: big triangle, 15 marbles, 12 of 300 at random, 28.6 */
    {PS_TRIANGLE6, 10, 0x1d3b9full,
     "a6c4 c4e2 b3d3 d1b3 a5a3 f1d1 e2c4 a3c3 c4c2 b1b3 c2a4 a4c4 d1d3 d3b5"}, /* 77: big triangle, 15 marbles, 1 of 300 at random, 36.7 */
    {PS_FRENCH, 10, 0x31ecc3cd2ull,
     "a3c3 f6f4 d4d6 f4d4 e2e4 d7d5 e4c4 d5b5 c3c5 g3e3 d3f3 f2f4 b5d5 e6e4 f4d4 d4d6 d6b6"}, /* 78: French, 18 marbles, 1 of 300 at random, 39.7 */
    {PS_TRIANGLE6, 48, 0x1779b7ull,
     "c3a3 a6a4 a3a5 b5d3 a5c3 b1b3 c1e1 f1d1 c3e1 e1c1 b3d1 d3f1 c1e1 f1d1"}, /* 79: big triangle, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_FRENCH, 42, 0x1309b32330ull,
     "g4e4 b3d3 b4d4 d3d5 c6e6 a5c5 c5e5 f2f4 f4d4 e6e4 d4f4 e1e3 f5f3 e3g3"}, /* 80: French, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_ENGLISH, 29, 0x19821dc3cull,
     "f5d5 b3b5 e7e5 d4b4 e1c1 b5b3 c1c3 d5f5 c6e6 g5e5 e6e4 b3d3 d2d4 e4c4"}, /* 81: English, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_DIAMOND, 23, 0x1243b46a5ull,
     "a5c5 f4f6 g6e6 e9e7 e7c7 d5b5 e5e7 c3c5 f7d7 b6d6 c7e7 b5d5 d5d7 d7f7"}, /* 82: diamond, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_DIAMOND, 52, 0x2c1c10d664ull,
     "h6f6 d7f7 e4e2 b6d6 f7f5 e5e7 e8e6 d6f6 c4e4 f6f4 d2f2 e4g4 f2f4 f4h4"}, /* 83: diamond, 15 marbles, 0 of 300 at random, 39.7 */
    {PS_DIAMOND, 48, 0x6ab0d80ca8ull,
     "f8f6 d5f5 h4h6 f5h5 h6h4 c6e6 e4g4 f6d6 h4f4 d7d5 f4f2 d2d4 d5d3 f2d2 d2d4"}, /* 84: diamond, 16 marbles, 0 of 300 at random, 40.7 */
    {PS_WIEGLEB, 3, 0xf2501c070c2ull,
     "d1f1 d6d8 e6g6 f1f3 f5h5 h4h6 f3f5 e2e4 e4e6 h6f6 e6e8 d8f8 f5f7 f7f9 f9d9"}, /* 85: Wiegleb's, 16 marbles, 0 of 300 at random, 40.7 */
    {PS_WIEGLEB, 48, 0x26c402c8768ull,
     "f8f6 d8d6 i4g4 f2f4 g4e4 a5c5 g6e6 e6c6 c6c4 e3e5 d4b4 e5c5 a6c6 c6c4 b4d4"}, /* 86: Wiegleb's, 16 marbles, 0 of 300 at random, 40.7 */
    {PS_DIAMOND, 13, 0x6661a01873ull,
     "i5g5 d2d4 g5e5 g4e4 e6c6 c7c5 e4c4 c4c6 d8d6 e2e4 e4e6 e6e8 e9e7 c6e6 e6e8"}, /* 87: diamond, 16 marbles, 0 of 300 at random, 40.7 */
    {PS_ENGLISH, 49, 0x1388cfed0ull,
     "b5b3 g4e4 e4e6 d2d4 d4b4 g5e5 a5a3 a3c3 e6e4 d6d4 e4c4 b4d4 e1e3 c2c4 c4e4 e4e2"}, /* 88: English, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_DIAMOND, 23, 0x1c7182f8e0ull,
     "d7d5 i5g5 f3f5 f6f4 e7e5 h6f6 f7f5 e5c5 e3e5 f5h5 b5d5 d5f5 g3g5 f4f6 h5f5 f5f7"}, /* 89: diamond, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_FRENCH, 28, 0x2cd89a353ull,
     "c7e7 a5c5 c6c4 f3d3 e7e5 b4d4 f5d5 c3e3 e4e2 d2b2 f2d2 d5d3 b2b4 a4c4 d2d4 d4b4"}, /* 90: French, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_FRENCH, 38, 0x49a98ab99ull,
     "g3e3 b6b4 c7c5 e4g4 f6f4 g4e4 a5a3 d5b5 e4e2 a3c3 e2c2 c3e3 c1c3 b5b3 b3d3 e3c3"}, /* 91: French, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_ENGLISH, 56, 0x1836df870ull,
     "c4c2 g4e4 g5e5 e4c4 b4d4 f3d3 e1c1 d3d5 a5a3 e5c5 c1c3 e6c6 c6c4 c4c2 a3c3 c3c1"}, /* 92: English, 17 marbles, 0 of 300 at random, 41.7 */
    {PS_WIEGLEB, 58, 0x30d00d1f09eull,
     "e6c6 f9d9 e8e6 d9d7 f5f3 f2f4 i4g4 f6d6 h6f6 c6e6 g4e4 f6d6 d7d5 e5e3 c5e5 e2e4 e5e3"}, /* 93: Wiegleb's, 18 marbles, 0 of 300 at random, 42.7 */
    {PS_WIEGLEB, 13, 0x161417ca16aull,
     "e2e4 e4c4 f8f6 f3f5 d8d6 e5e7 g6e6 d6d4 e6e8 c4c6 a5c5 g5e5 c6c4 c4e4 e9e7 e4e6 e6e8"}, /* 94: Wiegleb's, 18 marbles, 0 of 300 at random, 42.7 */
    {PS_WIEGLEB, 27, 0x331d30c2c89ull,
     "h5f5 d3d5 d9d7 f5f3 e7e5 a5c5 d5b5 b6d6 d7d5 e5c5 f3d3 f2d2 d2d4 e4c4 c4a4 c5a5 a4a6"}, /* 95: Wiegleb's, 18 marbles, 0 of 300 at random, 42.7 */
    {PS_WIEGLEB, 53, 0x265ded06140ull,
     "a4c4 e4e2 c4e4 e5e3 f6d6 f4f6 d7d5 f2f4 c5e5 e2e4 f7f5 e5g5 e4g4 g4g6 h4h6 g6i6 i6i4"}, /* 96: Wiegleb's, 18 marbles, 0 of 300 at random, 42.7 */
    {PS_WIEGLEB, 53, 0x3fc06bab800ull,
     "f2f4 e2e4 i4g4 d2d4 f4f6 c5e5 d6b6 e4c4 e6e4 b6b4 b4d4 d4f4 f4h4 f6h6 i6g6 h4h6 g6i6 i6i4"}, /* 97: Wiegleb's, 19 marbles, 0 of 300 at random, 43.7 */
    {PS_DIAMOND, 46, 0x4ed8707866ull,
     "f5d5 h4f4 e4c4 f3f5 e6c6 e2e4 g5e5 g6e6 e5e3 e3c3 d8f8 e6e8 c3c5 d5b5 f8d8 d8d6 d6b6 b6b4"}, /* 98: diamond, 19 marbles, 0 of 300 at random, 43.7 */
    {PS_DIAMOND, 68, 0x26683b5c8dull,
     "g4e4 d3f3 d6f6 e4e6 d5d3 e9e7 d2d4 f6f4 f8f6 a5c5 e7e5 c6c4 c4e4 g6e6 f4d4 e6e4 d4f4 f4f2"}, /* 99: diamond, 19 marbles, 0 of 300 at random, 43.7 */
    {PS_WIEGLEB, 40, 0xd819951b11aull,
     "a4c4 f4d4 d1d3 e1e3 h6f6 e6e4 d3d5 f7f5 c4c6 d5d7 e9e7 d8d6 c6e6 e3e5 e6e4 g5e5 e4e6 e7e5"}, /* 100: Wiegleb's, 19 marbles, 0 of 300 at random, 43.7 */
    {PS_FRENCH, 22, 0xccc17ba5bull,
     "b6d6 e5c5 b5d5 c7e7 c4e4 d6d4 e7e5 c2e2 c1e1 f5d5 a4c4 e3e5 d5f5 c4e4 e1e3 e3e5 e5g5 f3f5 g5e5"}, /* 101: French, 20 marbles, 1 of 300 at random, 41.7 */
    {PS_FRENCH, 22, 0xcdce6b07aull,
     "a3c3 c1e1 c2e2 e6e4 f3d3 e1e3 g3g5 g5e5 c3c5 e4e6 c6c4 e6c6 e3c3 b6d6 d4b4 d7d5 a4c4 c3c5 c5e5"}, /* 102: French, 20 marbles, 0 of 300 at random, 44.7 */
    {PS_ENGLISH, 36, 0x13ae73ad9ull,
     "g5e5 e1e3 c7c5 e4c4 c4c6 c2c4 f3f5 e3c3 d6d4 f5d5 d5d3 d2d4 a5c5 d4b4 b3b5 c6c4 c3c5 c5a5 a5a3"}, /* 103: English, 20 marbles, 0 of 300 at random, 44.7 */
    {PS_FRENCH, 32, 0x3663fafc0ull,
     "a5a3 e3c3 d5d3 d3b3 f2d2 f6d6 c2e2 b5d5 a3c3 d6d4 c4c2 e4c4 b4d4 g4e4 d4f4 b2d2 d2f2 f5f3 f2f4"}, /* 104: French, 20 marbles, 0 of 300 at random, 44.7 */
    {PS_WIEGLEB, 48, 0x1670ec87556ull,
     "i5g5 e6e4 b6b4 f6f8 g5e5 a4c4 d6d8 f9f7 e9e7 f7d7 d8d6 f3f5 f5d5 h4f4 d6d4 d4b4 f4d4 e2e4 e4c4 b4d4"}, /* 105: Wiegleb's, 21 marbles, 0 of 300 at random, 45.7 */
    {PS_WIEGLEB, 14, 0xb1db183306eull,
     "i6g6 d7d9 b4d4 d6f6 i4g4 f9f7 e1e3 f6f8 f4h4 g6g4 d9f9 h4f4 e3e5 f9f7 f5f3 d3d5 d5f5 f2f4 f4f6 f6f8"}, /* 106: Wiegleb's, 21 marbles, 0 of 300 at random, 45.7 */
    {PS_ENGLISH, 40, 0x153b78b7eull,
     "a3c3 e7e5 d5b5 d3d1 a5c5 d7d5 d4d6 f5d5 d6d4 e4e2 c4e4 e1e3 f4d4 f3d3 c6c4 d4d2 c4c2 c1e1 c2e2 e1e3"}, /* 107: English, 21 marbles, 0 of 300 at random, 45.7 */
    {PS_ENGLISH, 57, 0x1fa677736ull,
     "e1e3 d5f5 g5e5 e7c7 e4e2 f3f5 c2c4 c5c3 f5d5 e6c6 c7c5 c1e1 e1e3 a4c4 c5e5 b3d3 d3f3 c4e4 e5e3 f3d3 d3d1"}, /* 108: English, 22 marbles, 0 of 300 at random, 46.7 */
    {PS_WIEGLEB, 67, 0x1ca186ecd8d8ull,
     "f4d4 d1d3 c6e6 i5g5 e6e4 g6e6 g5e5 d4f4 d8d6 d6d4 f1d1 a5c5 e6e4 d4d2 f4d4 d1d3 e8e6 d3d5 c5e5 e6e4 e4e2"}, /* 109: Wiegleb's, 22 marbles, 0 of 300 at random, 46.7 */
    {PS_DIAMOND, 47, 0xee5da27ea0ull,
     "i5g5 d7d5 d4f4 g5g7 g4e4 b6b4 g7e7 e7e5 f6f4 e4g4 b4d4 e2e4 e4c4 e5c5 f2f4 d2d4 g4e4 d4b4 c6c4 b4d4 e4c4"}, /* 110: diamond, 22 marbles, 0 of 300 at random, 46.7 */
    {PS_DIAMOND, 37, 0x1cad5ea17e4ull,
     "h4f4 e1e3 e4e2 f2d2 g5e5 b6d6 f3f5 e6c6 i5g5 e5c5 e7e9 d2d4 g5e5 c4e4 g7e7 e4e6 b5d5 e6e8 e9e7 e7c7 c7c5 "
     "d5b5"}, /* 111: diamond, 23 marbles, 0 of 300 at random, 47.7 */
    {PS_WIEGLEB, 21, 0x1b9e84e97906ull,
     "e1e3 f1f3 i4i6 e5e7 i6g6 d2d4 g6e6 h4f4 e4c4 d6b6 b6b4 b4d4 f4f2 d5d3 d3f3 f2f4 f4f6 f6d6 f9d9 f7d7 d6d8 "
     "d9d7"}, /* 112: Wiegleb's, 23 marbles, 0 of 300 at random, 47.7 */
    {PS_FRENCH, 18, 0x1cf59beebull,
     "f4d4 d2b2 e2e4 b5b3 b2b4 e4c4 f6f4 e6c6 b6d6 c7e7 f3f5 d5d7 f5d5 a3a5 d3b3 e7c7 d5b5 a5c5 c4c6 c7c5 b3b5 "
     "c5a5"}, /* 113: French, 23 marbles, 0 of 300 at random, 47.7 */
    {PS_DIAMOND, 24, 0x374f2beb7ull,
     "c3c5 g4g6 e9e7 h6h4 e6e8 g6e6 d7d5 b6d6 e4g4 h4f4 f4f6 d6d4 d8f8 f6d6 f8f6 d3d5 d5f5 b5d5 f5f7 d5d7 c7e7 "
     "e7g7"}, /* 114: diamond, 23 marbles, 0 of 300 at random, 47.7 */
    {PS_FRENCH, 24, 0x11eb8ebbf6ull,
     "e1e3 f6f4 e4e2 d3d1 a5a3 b2d2 c3c5 d6f6 d4d6 a3c3 d1d3 c3e3 c6e6 e2e4 f6d6 e7c7 b5d5 d5d7 e4e6 f3f5 c7e7 "
     "e7e5 e5g5"}, /* 115: French, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_ENGLISH, 22, 0x3befecbfull,
     "c7c5 c3c1 e3c3 b3d3 d3d1 c1e1 e1e3 b5d5 e5c5 c5c3 f3d3 e7e5 c3e3 d7d5 e5g5 e3e5 d5f5 a4c4 c4e4 g5g3 e4g4 "
     "g3g5 g5e5"}, /* 116: English, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_WIEGLEB, 58, 0x1fad505c6cb4ull,
     "f9f7 d1d3 f7f5 f5d5 f1f3 i4g4 b6d6 e6c6 c6c4 f4f2 e8e6 d3f3 d4d6 f2f4 g4e4 e1e3 e6c6 e3e5 b4d4 a5c5 c6c4 "
     "c4e4 e5e3"}, /* 117: Wiegleb's, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_FRENCH, 29, 0xfd7f2b27aull,
     "c1e1 d2d4 b6b4 b3b5 d7d5 e6e4 f2d2 a3a5 e3e5 c3c5 d5d3 c2e2 e1e3 f5d5 c6c4 d3f3 a5c5 g3g5 f3f5 c4c6 g5e5 "
     "e5c5 c6c4"}, /* 118: French, 24 marbles, 0 of 300 at random, 48.7 */
    {PS_WIEGLEB, 35, 0x2dd4aacc6feull,
     "g6e6 a6c6 d4d6 f9d9 i4g4 d7d5 a5c5 e7e5 f4h4 f2f4 d9d7 f8d8 f4f6 d5b5 d2d4 d8d6 d6b6 h4h6 b6b4 a4c4 c4e4 "
     "e4e6 e6g6 g6i6"}, /* 119: Wiegleb's, 25 marbles, 0 of 300 at random, 49.7 */
    {PS_WIEGLEB, 59, 0x26f96a1bdaeull,
     "d6d4 f4f6 b6d6 f2f4 i5g5 f7f5 f9d9 e3e5 h6f6 d9d7 f5f7 g4e4 d6f6 i4g4 g4g6 e4c4 g6e6 f8f6 d7f7 e6e4 b4d4 "
     "d4f4 f7f5 f5f3"}, /* 120: Wiegleb's, 25 marbles, 0 of 300 at random, 49.7 */
    {PS_FRENCH, 58, 0x96f9dbef3ull,
     "a4c4 d6b6 b6b4 c4c6 e4g4 e5c5 c7e7 e7e5 c6c4 b3b5 c3c5 b5d5 b2d2 e5c5 d3d5 f6f4 c5e5 g4e4 e3g3 e5e3 d1d3 "
     "d3f3 g3e3 e3e1"}, /* 121: French, 25 marbles, 0 of 300 at random, 49.7 */
    {PS_DIAMOND, 76, 0xce57bbdcb5ull,
     "f2f4 d3f3 c7e7 e5c5 f7d7 h6f6 e9e7 e7e5 c5c7 c7e7 f5f7 f7d7 d7d5 f3f5 i5g5 g4g6 e5g5 b4d4 d4f4 a5c5 c5e5 "
     "g6g4 g4e4 e5e3 e3e1"}, /* 122: diamond, 26 marbles, 0 of 300 at random, 50.7 */
    {PS_DIAMOND, 13, 0x6bdcbd74f3ull,
     "d5b5 c3c5 c6c4 a5c5 d2f2 h4f4 d8d6 e4g4 c4c6 d3d5 f2f4 g4e4 e5g5 h5f5 d6d4 d4f4 f7d7 c7e7 f6d6 f4f6 g6e6 "
     "e6e8 c6e6 e9e7 e6e8"}, /* 123: diamond, 26 marbles, 0 of 300 at random, 50.7 */
    {PS_DIAMOND, 68, 0x14a5cfaecfeull,
     "e1e3 f6f4 e8e6 d6f6 c7c5 d5f5 e3e5 d8d6 g5g3 c4e4 b5d5 e4g4 g6e6 e6e4 d6d4 f8f6 d4f4 g4e4 h6h4 f6f4 e4g4 "
     "g3e3 d3f3 h4f4 f4f2"}, /* 124: diamond, 26 marbles, 0 of 300 at random, 50.7 */
    {PS_TRIANGLE6, 0, 0x1ffffeull,
     "a4a6 c4a4 a6c4 a3a5 c3a3 d3b5 a2a4 a5a3 b1b3 a3c3 d2b2 d1b1 a1c1 f1d1 d1b1 b1b3 b3d3 e2c4 c4a6"}, /* 125: big triangle, 20 marbles, 0 of 300 at random, 44.7, the full board */
    {PS_WIEGLEB, 36, 0xbc0eef87ef4ull,
     "d6d8 f6d6 f9f7 d8f8 c6e6 f3f1 f8f6 a6c6 f1d1 f6d6 d4b4 d1d3 a4c4 d6b6 b6b4 e5e3 i5g5 e2e4 g5e5 d5b5 b4d4 "
     "d3d5 e4e6 e7e5 e5c5 c5a5"}, /* 126: Wiegleb's, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_DIAMOND, 50, 0x6e7cde7d3eull,
     "c7e7 d5d7 d3d5 f4d4 d8d6 e6e4 h5f5 d5d3 g7g5 f8d8 b5d5 e3c3 g4g6 d5d7 d8d6 c6e6 e7e5 c3c5 d2f2 g6e6 f5d5 "
     "f2f4 c5e5 f4d4 e6e4 d4f4"}, /* 127: diamond, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_ENGLISH, 39, 0x1beff7f3eull,
     "a3a5 e1e3 e4e2 g3e3 e3e1 c2e2 c6c4 c3c5 e1e3 d5b5 e7c7 d3f3 a5c5 g5g3 f5d5 b3b5 d5d7 c7e7 b5d5 g3e3 e7e5 "
     "d5f5 f5f3 f3d3 d4d2 d1d3"}, /* 128: English, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_ENGLISH, 37, 0x1bb6ffbbfull,
     "c5a5 f3d3 g5e5 d5f5 c3c1 a5a3 a3c3 f5f3 e7e5 e1e3 d4f4 d7d5 d2d4 c7c5 c1e1 d5d3 g4e4 d3b3 b3b5 e4e2 e1e3 "
     "b5d5 e5c5 f3d3 c5c3 d3b3"}, /* 129: English, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_DIAMOND, 47, 0xef37bafca5ull,
     "e4g4 e2e4 e9e7 f6f4 d2d4 i5g5 g5g3 d5f5 h6f6 d7d5 f6d6 f7d7 g3e3 d5d3 d7d5 f5f3 f2f4 e4e2 b4b6 c3e3 e2e4 "
     "b6d6 f4d4 c4e4 d6d4 e4c4"}, /* 130: diamond, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_WIEGLEB, 49, 0xf62faf5e9a2ull,
     "d1f1 a4a6 f6d6 f3d3 f1f3 d4d2 b4d4 d6b6 d5d3 a6c6 h6h4 f8f6 g6e6 h4f4 e6e8 e4e6 c6c4 e9e7 e7e5 d2d4 e5g5 "
     "c4e4 f3f5 g5e5 e5e3 e2e4"}, /* 131: Wiegleb's, 27 marbles, 0 of 300 at random, 51.7 */
    {PS_FRENCH, 31, 0x1befbbaef6ull,
     "e1c1 d6d4 d3d5 f3d3 e7e5 d2d4 e4e6 b3d3 b2d2 f6d6 f5f3 f2f4 g4e4 e2c2 c6e6 c1c3 c3e3 c4c6 a4c4 d4d6 e3e5 "
     "e5e7 e7c7 c7c5 b5d5 d6d4 c4e4"}, /* 132: French, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_FRENCH, 39, 0x1fbfbabb76ull,
     "g3g5 e4g4 f2f4 e7c7 d3f3 c4c2 c7c5 e6c6 d1d3 c1c3 c6c4 b2b4 a5a3 e1e3 e5c5 g4e4 b5d5 b4d4 g5e5 d4f4 d3b3 "
     "a3c3 d5f5 f3d3 f5f3 c3e3 f3d3"}, /* 133: French, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_WIEGLEB, 46, 0x1b81d87c76ffull,
     "e6c6 f9f7 f7f5 f1f3 b6d6 e4e6 d9f9 d8f8 f9f7 b4b6 a6c6 e1e3 e7e5 f3d3 f4f6 d5b5 f7f5 f5d5 d6b6 d4d6 d7d5 "
     "d2d4 d4d6 a4a6 a6c6 d6b6 b6b4"}, /* 134: Wiegleb's, 28 marbles, 0 of 300 at random, 52.7 */
    {PS_WIEGLEB, 12, 0x36bfef465beull,
     "f4h4 f6d6 d4f4 b4d4 i4g4 a4a6 a6c6 f7d7 g4e4 c6e6 i5g5 e4c4 e6e4 d8d6 c4c6 g5e5 f2f4 f4d4 e2e4 d6b6 d4d6 "
     "e4e6 f9f7 e6c6 e9e7 f7d7 b6d6 d6d8"}, /* 135: Wiegleb's, 29 marbles, 0 of 300 at random, 53.7 */
    {PS_WIEGLEB, 39, 0x3e4f6db57f9ull,
     "a6c6 e8e6 i6i4 d7d5 f6d6 i4g4 f8f6 h6h4 c6e6 f6d6 c5a5 d4f4 e5c5 d9d7 f3f1 b4d4 f5f3 d7d5 h4f4 d5b5 a5c5 "
     "f3d3 d2f2 f1f3 d3d5 c5e5 f3f5 f5d5"}, /* 136: Wiegleb's, 29 marbles, 0 of 300 at random, 53.7 */
    {PS_FRENCH, 40, 0xbefbbfff2ull,
     "g5g3 e4g4 e2e4 d6d4 c2e2 c4c2 a5a3 a3c3 d3d5 b5b3 f2d2 d1d3 b2b4 c2c4 b4d4 c5c7 d4f4 g4e4 f6f4 e4g4 g3e3 "
     "c7e7 d5f5 d3f3 e7e5 e5g5 g5g3 g3e3"}, /* 137: French, 29 marbles, 0 of 300 at random, 53.7 */
    {PS_WIEGLEB, 46, 0xbebdbfdae58ull,
     "h6f6 d8d6 f4h4 f6f4 a4c4 d4b4 f3d3 d2d4 i4g4 e4c4 e1e3 e5e7 e8e6 g4e4 a6a4 b4d4 d4f4 c6c4 d6d4 b6b4 h5f5 "
     "c4e4 a4c4 e3e5 e6e4 f5f3 f2f4 f4d4 d4b4"}, /* 138: Wiegleb's, 30 marbles, 0 of 300 at random, 54.7 */
    {PS_DIAMOND, 60, 0x15efa7d7efcull,
     "d5b5 d7d5 f7d7 b6d6 c7e7 g5g7 e6c6 f5f7 f8f6 f3f5 h4f4 f5f7 a5c5 d3f3 e8e6 e4c4 b4d4 c6c4 g7e7 c4e4 d5f5 "
     "g3e3 e7e5 f5f3 f3d3 e5e3 d3f3 e1e3 e3g3"}, /* 139: diamond, 30 marbles, 0 of 300 at random, 54.7 */
    {PS_WIEGLEB, 13, 0x1b93bb7cfcfcull,
     "f9f7 f6f8 f1f3 f3f5 d2d4 d6f6 f5f7 d4d6 f8f6 a4a6 h5f5 f5d5 b4d4 g6e6 b6b4 e1e3 d5b5 d4f4 g4e4 e3e5 d6f6 "
     "e8e6 b4b6 f6d6 c6e6 a6c6 e5e7 d8d6 c6e6 e6e8"}, /* 140: Wiegleb's, 31 marbles, 0 of 300 at random, 55.7 */
    {PS_WIEGLEB, 36, 0x1ae3f7db799bull,
     "i6i4 b4b6 g4g6 f5d5 h6h4 i4g4 d5b5 e7e5 d9d7 d7d5 e4e6 e9e7 c4e4 e3e5 e5c5 f1d1 g4e4 e7e5 b6d6 g6e6 f2f4 "
     "e5e7 f4d4 f7d7 d7d5 c5e5 d1d3 d3d5 e5c5 c5a5"}, /* 141: Wiegleb's, 31 marbles, 0 of 300 at random, 55.7 */
    {PS_WIEGLEB, 5, 0xafef6fbf0beull,
     "d5d7 i5g5 f9f7 f5d5 h4f4 g6g4 d4d6 d2d4 f4h4 e4e2 f6f8 d7f7 e1e3 b5d5 d5d3 b4d4 f8f6 f2f4 i4g4 d3d5 i6g6 "
     "g4e4 e9e7 e3e5 e6e8 d5d7 g6e6 e5e7 d7d9 e7e9 d9f9"}, /* 142: Wiegleb's, 32 marbles, 0 of 300 at random, 56.7 */
    {PS_WIEGLEB, 34, 0xfbbc76b75fbull,
     "i6g6 d1f1 f1f3 f7f9 f6h6 d6f6 d8f8 f3f5 d4d6 b6b4 d2d4 e4e6 e6e8 e2e4 e4c4 f9f7 f6f4 b4d4 d6d8 g4e4 h5f5 "
     "d4f4 f4f6 d9d7 f6f8 i4i6 e9e7 i6g6 d7f7 f8f6 f6h6"}, /* 143: Wiegleb's, 32 marbles, 0 of 300 at random, 56.7 */
    {PS_WIEGLEB, 57, 0x1bb7f7bb6df8ull,
     "b6d6 f1f3 b4b6 d4b4 e6c6 c5e5 i6i4 d2d4 d8d6 g4g6 g6e6 d6f6 e8e6 e6g6 f8f6 e5g5 i4g4 e4c4 b6d6 h5f5 f3d3 "
     "b4d4 f5f7 e1e3 h6f6 d3d5 g4e4 e3e5 d6d4 f7f5 f5d5 d5d3"}, /* 144: Wiegleb's, 33 marbles, 0 of 300 at random, 57.7 */
    {PS_WIEGLEB, 32, 0x1b63b7bffddbull,
     "f6f8 d7f7 f8f6 c5e5 a5c5 f4h4 f6f4 d9d7 d7d5 d5b5 h6f6 f6d6 i6i4 i4g4 h5f5 e9e7 b4d4 f3d3 f1d1 e4c4 c6e6 "
     "g4e4 b6b4 b4d4 d4d2 d1d3 f5d5 f2d2 e7e5 d2d4 d5f5 d4f4 f4f6"}, /* 145: Wiegleb's, 34 marbles, 0 of 300 at random, 58.7 */
    {PS_WIEGLEB, 46, 0x1be77f7f75b9ull,
     "i6g6 f3f5 d9d7 h4f4 g6g4 g4e4 f1f3 d5d3 d7d5 f7d7 d2f2 f8d8 f2f4 d8d6 e4e2 i5g5 e1e3 a4a6 e6g6 g6g4 a6c6 "
     "d6d4 g4e4 f5d5 d4d6 b5d5 d6b6 e3e5 b4d4 e5c5 d3d5 d5b5 b6b4"}, /* 146: Wiegleb's, 34 marbles, 0 of 300 at random, 58.7 */
    {PS_WIEGLEB, 27, 0x1df7f3fefd7aull,
     "f6h6 i6g6 f4f2 f1f3 e9e7 d2f2 e6e8 c6e6 d4d2 h4f4 d1d3 b4d4 d8d6 f4f6 e5e7 c5e5 f2f4 e4e2 g6e6 e1e3 f8f6 "
     "d3d5 h5f5 e6e4 e4g4 f6f4 g4e4 e3e5 e8e6 e6c6 e5c5 c6c4 b6b4 c4a4 a4a6"}, /* 147: Wiegleb's, 36 marbles, 0 of 300 at random, 60.7 */
    {PS_WIEGLEB, 59, 0x1fef77fffcfbull,
     "g4e4 f6f4 d9f9 f9f7 d8f8 f8f6 d7f7 f7f5 f3d3 e5e3 d6f6 c6a6 i4g4 f4h4 g6g4 f1f3 d2f2 f2f4 f5f3 h4f4 f3f5 "
     "i6g6 g6e6 i5g5 g5e5 b4b6 d5b5 e6e4 d4b4 a6a4 a4c4 b6b4 e4e2 b4d4 d4d2 d2f2 d1f1 f1f3"}, /* 148: Wiegleb's, 39 marbles, 0 of 300 at random, 63.7 */
    {PS_ENGLISH, 30, 0x1fffeffffull,
     "d6d4 b5d5 c7c5 e7c7 d5b5 a5c5 e5e7 g5e5 c4c6 c7c5 a4c4 c4c6 e4c4 g4e4 e4e6 e7e5 c3c5 c6c4 a3c3 d3b3 e2e4 "
     "g3e3 c1c3 c4c2 e1c1 c1c3 b3d3 d3f3 e5e3 f3d3 d2d4"}, /* 149: English, 32 marbles, 0 of 300 at random, 56.7, the full board */
    {PS_FRENCH, 48, 0x1fffffffdfull,
     "b6d6 e6c6 b4b6 b6d6 d5b5 d7d5 a5c5 d5b5 f5d5 d4d6 a3a5 a5c5 c4c6 c7c5 f3f5 f6f4 d3f3 b3d3 f3f5 g5e5 e4e6 "
     "e7e5 c1c3 e1e3 d3f3 g3e3 d1d3 d3f3 f2f4 g4e4 e4e6 e6c6 c6c4 c4c2 b2d2"}, /* 150: French, 36 marbles, 0 of 300 at random, 60.7, the full board */
};
