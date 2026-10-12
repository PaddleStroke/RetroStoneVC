/*
 * The Tower of Hanoi's levels: our own, made by tools/hanoi_gen.c (every position and goal peg tried, the
 * fewest moves by the rules, spread evenly from the easiest; each chapter ends with the classic tower). Each:
 * the goal peg, ':', each disc's peg from the largest; then (in the comment) its fewest moves and the discs
 * not yet on the goal peg; the fewest moves again in hn_level_par (the tests check them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const hn_levels[HN_LEVELS] = {
    "a:abc", /* 1: 2 moves, 2 discs to move */
    "a:aba", /* 2: 3 moves, 1 discs to move */
    "a:acc", /* 3: 3 moves, 2 discs to move */
    "a:cbb", /* 4: 4 moves, 3 discs to move */
    "a:cba", /* 5: 5 moves, 2 discs to move */
    "b:aca", /* 6: 5 moves, 3 discs to move */
    "a:cac", /* 7: 6 moves, 2 discs to move */
    "b:aba", /* 8: 6 moves, 2 discs to move */
    "b:abb", /* 9: 7 moves, 1 discs to move */
    "a:bbb", /* 10: 7 moves, 3 discs to move */
    "b:aaa", /* 11: 7 moves, 3 discs to move */
    "c:aaa", /* 12: 7 moves, 3 discs to move */
    "b:bbac", /* 13: 2 moves, 2 discs to move */
    "a:abcc", /* 14: 4 moves, 3 discs to move */
    "a:acbc", /* 15: 5 moves, 3 discs to move */
    "b:baba", /* 16: 6 moves, 2 discs to move */
    "a:abbb", /* 17: 7 moves, 3 discs to move */
    "a:bccc", /* 18: 8 moves, 4 discs to move */
    "a:cbbc", /* 19: 9 moves, 4 discs to move */
    "b:acba", /* 20: 10 moves, 3 discs to move */
    "a:bcbb", /* 21: 11 moves, 4 discs to move */
    "a:ccaa", /* 22: 12 moves, 2 discs to move */
    "b:abab", /* 23: 13 moves, 2 discs to move */
    "b:aaba", /* 24: 13 moves, 3 discs to move */
    "b:abbc", /* 25: 14 moves, 2 discs to move */
    "b:aaac", /* 26: 14 moves, 4 discs to move */
    "b:abba", /* 27: 15 moves, 2 discs to move */
    "a:ccba", /* 28: 15 moves, 3 discs to move */
    "a:bbcc", /* 29: 15 moves, 4 discs to move */
    "c:aaaa", /* 30: 15 moves, 4 discs to move */
    "a:aabcb", /* 31: 5 moves, 3 discs to move */
    "a:aaccc", /* 32: 7 moves, 3 discs to move */
    "a:acbaa", /* 33: 11 moves, 2 discs to move */
    "a:acaca", /* 34: 13 moves, 2 discs to move */
    "a:accbc", /* 35: 14 moves, 4 discs to move */
    "b:baaab", /* 36: 15 moves, 3 discs to move */
    "b:accca", /* 37: 17 moves, 5 discs to move */
    "a:bcbaa", /* 38: 20 moves, 3 discs to move */
    "a:bcaac", /* 39: 22 moves, 3 discs to move */
    "a:bcacc", /* 40: 23 moves, 4 discs to move */
    "a:ccaaa", /* 41: 24 moves, 2 discs to move */
    "a:babca", /* 42: 26 moves, 3 discs to move */
    "a:ccaca", /* 43: 27 moves, 3 discs to move */
    "b:abcbb", /* 44: 28 moves, 2 discs to move */
    "a:cabac", /* 45: 29 moves, 3 discs to move */
    "a:caaca", /* 46: 30 moves, 2 discs to move */
    "b:aaccb", /* 47: 30 moves, 4 discs to move */
    "a:ccbaa", /* 48: 31 moves, 3 discs to move */
    "b:abcaa", /* 49: 31 moves, 4 discs to move */
    "c:aaaaa", /* 50: 31 moves, 5 discs to move */
    "a:aabcaa", /* 51: 11 moves, 2 discs to move */
    "a:aacaaa", /* 52: 15 moves, 1 discs to move */
    "a:abccbb", /* 53: 19 moves, 5 discs to move */
    "a:abcacc", /* 54: 23 moves, 4 discs to move */
    "a:accacb", /* 55: 26 moves, 4 discs to move */
    "a:abacab", /* 56: 29 moves, 3 discs to move */
    "a:abaaaa", /* 57: 31 moves, 1 discs to move */
    "a:accccc", /* 58: 31 moves, 5 discs to move */
    "a:cbbacb", /* 59: 37 moves, 5 discs to move */
    "a:cbcaaa", /* 60: 40 moves, 3 discs to move */
    "a:bcbacc", /* 61: 43 moves, 5 discs to move */
    "b:acaaca", /* 62: 45 moves, 6 discs to move */
    "b:acaabb", /* 63: 47 moves, 4 discs to move */
    "a:caccca", /* 64: 49 moves, 4 discs to move */
    "a:babacc", /* 65: 52 moves, 4 discs to move */
    "a:ccabab", /* 66: 54 moves, 4 discs to move */
    "a:babcbb", /* 67: 55 moves, 5 discs to move */
    "a:bbcbba", /* 68: 57 moves, 5 discs to move */
    "b:abbcbc", /* 69: 59 moves, 3 discs to move */
    "a:caaacc", /* 70: 60 moves, 3 discs to move */
    "a:bacbcb", /* 71: 61 moves, 5 discs to move */
    "a:cabbba", /* 72: 62 moves, 4 discs to move */
    "a:caaabc", /* 73: 63 moves, 3 discs to move */
    "a:bbccba", /* 74: 63 moves, 5 discs to move */
    "c:aaaaaa", /* 75: 63 moves, 6 discs to move */
    "b:bbacabc", /* 76: 21 moves, 4 discs to move */
    "a:aacaaca", /* 77: 30 moves, 2 discs to move */
    "a:acbbaac", /* 78: 39 moves, 4 discs to move */
    "b:bacaccb", /* 79: 46 moves, 5 discs to move */
    "b:baabcab", /* 80: 53 moves, 4 discs to move */
    "a:abbbcab", /* 81: 58 moves, 5 discs to move */
    "b:babccac", /* 82: 61 moves, 5 discs to move */
    "b:babccca", /* 83: 63 moves, 5 discs to move */
    "a:bccbaca", /* 84: 75 moves, 5 discs to move */
    "a:cbacccc", /* 85: 80 moves, 6 discs to move */
    "a:cbacbca", /* 86: 87 moves, 5 discs to move */
    "a:cbcbcbb", /* 87: 91 moves, 7 discs to move */
    "a:bcaaacb", /* 88: 95 moves, 4 discs to move */
    "a:ccaaabb", /* 89: 99 moves, 4 discs to move */
    "a:ccacbbb", /* 90: 104 moves, 6 discs to move */
    "a:cacaacb", /* 91: 109 moves, 4 discs to move */
    "a:bbabbba", /* 92: 111 moves, 5 discs to move */
    "b:abcbbcc", /* 93: 115 moves, 4 discs to move */
    "a:cccbaab", /* 94: 118 moves, 5 discs to move */
    "a:bbbabbb", /* 95: 120 moves, 6 discs to move */
    "a:cabcbaa", /* 96: 123 moves, 4 discs to move */
    "a:baabbca", /* 97: 125 moves, 4 discs to move */
    "b:abccbab", /* 98: 126 moves, 4 discs to move */
    "a:bbcabca", /* 99: 127 moves, 5 discs to move */
    "c:aaaaaaa", /* 100: 127 moves, 7 discs to move */
};

const uint8_t hn_level_par[HN_LEVELS] = {
    2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 7, 7, 2, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 5, 7, 11, 13, 14, 15, 17, 20, 22, 23,
    24, 26, 27, 28, 29, 30, 30, 31, 31, 31, 11, 15, 19, 23, 26, 29, 31, 31, 37, 40,
    43, 45, 47, 49, 52, 54, 55, 57, 59, 60, 61, 62, 63, 63, 63, 21, 30, 39, 46, 53,
    58, 61, 63, 75, 80, 87, 91, 95, 99, 104, 109, 111, 115, 118, 120, 123, 125, 126, 127, 127,
};
