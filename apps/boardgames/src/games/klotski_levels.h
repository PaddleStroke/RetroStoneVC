/*
 * Klotski's levels: our own, made by tools/klotski_gen.c (blocks laid by chance, every position of their family
 * and its fewest moves found by the rules, six chapters of 25 from the easiest). Each: the blocks (B the great
 * block, T standing, W lying, S small; their top left cell, row then column), then (in the comment) its fewest
 * moves and its family's size; the fewest moves again in klt_level_par (the tests check them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const klt_levels[KLT_LEVELS] = {
    "S00 T01 W02 S10 T12 W20 B30 T32 T33", /* 1: 3 moves, a family of 10 */
    "W01 S03 T10 S11 W12 B21 T23 T30 S41 S43", /* 2: 4 moves, a family of 38 */
    "S00 S01 T02 S03 W10 S13 B21 T30 T33 S41 S42", /* 3: 5 moves, a family of 29 */
    "T00 W01 S03 S11 W12 B21 T30 T33 S41 S42", /* 4: 5 moves, a family of 38 */
    "W00 W02 S10 W11 S13 W21 B30 S32 S33 S42 S43", /* 5: 4 moves, a family of 127 */
    "S00 W02 T10 W11 T13 B21 T30 T33 S41", /* 6: 5 moves, a family of 74 */
    "W00 W02 T10 T11 S12 T13 T22 B30 S33", /* 7: 5 moves, a family of 102 */
    "T00 T01 T02 T03 W21 T30 S31 B32 S41", /* 8: 4 moves, a family of 359 */
    "W00 W02 T10 T11 T12 B30 S32 T33 S42", /* 9: 4 moves, a family of 453 */
    "T00 T01 W02 S12 T13 B21 T30 T33 S41", /* 10: 3 moves, a family of 1532 */
    "T00 T01 T02 T03 W20 B22 W40 S42 S43", /* 11: 8 moves, a family of 30 */
    "W00 W02 T10 W11 B21 S23 T30 T33 S41", /* 12: 4 moves, a family of 1192 */
    "T00 W01 S03 W11 S13 W21 T30 T31 B32", /* 13: 8 moves, a family of 58 */
    "T00 T01 T02 T03 S20 B21 S23 T30 S41 S42", /* 14: 3 moves, a family of 7462 */
    "W00 W02 S11 S12 W20 W22 T30 T31 B32", /* 15: 7 moves, a family of 260 */
    "W00 W02 W10 W12 W20 B22 W30 S42 S43", /* 16: 7 moves, a family of 508 */
    "W00 W02 W10 W12 S20 S23 W30 B32 S40 S41", /* 17: 4 moves, a family of 7888 */
    "T00 S01 S02 T03 S11 S12 T20 B21 T23 S41 S42", /* 18: 3 moves, a family of 43704 */
    "W00 W02 W10 W12 B21 S23 S33 S40 S41 S42 S43", /* 19: 4 moves, a family of 23704 */
    "S00 W01 S03 W10 W12 S20 B21 S23 T33 S41 S42", /* 20: 3 moves, a family of 105064 */
    "T00 W01 T03 W11 B21 T23 S40 S41 S42 S43", /* 21: 4 moves, a family of 81340 */
    "T00 W01 S11 S12 T13 B21 T30 T33 S41 S42", /* 22: 6 moves, a family of 25955 */
    "W00 W02 W10 W12 B21 S30 S33 S40 S41 S42 S43", /* 23: 7 moves, a family of 23704 */
    "W00 W02 S10 B11 T13 S20 T30 T33 S41 S42", /* 24: 7 moves, a family of 81340 */
    "T00 S01 S02 T03 B11 S20 S23 T30 S31 T33 S41", /* 25: 9 moves, a family of 43704 */
    "T00 T01 W12 T20 S21 S22 S23 T31 B32 S40", /* 26: 10 moves, a family of 99 */
    "W00 S02 T10 W11 T13 T22 B30 T33 S42", /* 27: 11 moves, a family of 81 */
    "W00 W02 W10 W12 B20 S22 T23 T32 S41", /* 28: 12 moves, a family of 66 */
    "S00 S01 W10 W12 W20 W22 T30 T31 B32", /* 29: 11 moves, a family of 260 */
    "W00 W02 S10 T13 T20 W21 T31 B32 S40", /* 30: 10 moves, a family of 1192 */
    "T00 W01 S03 W11 T13 S20 T30 T31 B32", /* 31: 12 moves, a family of 272 */
    "W00 W02 W10 T12 T13 B20 W32 S40 S41", /* 32: 11 moves, a family of 1398 */
    "T00 T02 T03 S11 T20 B21 T23 S41 S42 S43", /* 33: 10 moves, a family of 7462 */
    "W00 W02 W10 B12 W30 W32 W40 S42 S43", /* 34: 14 moves, a family of 508 */
    "W00 W02 W10 W12 B21 S23 S33 S40 S41 W42", /* 35: 11 moves, a family of 7888 */
    "T00 S01 S02 T03 B11 T20 T23 S31 S32 S40 S43", /* 36: 10 moves, a family of 43704 */
    "S00 W01 S03 T10 B11 T13 T30 T33 S41 S42", /* 37: 11 moves, a family of 25955 */
    "T00 S01 S02 T03 B11 S23 T30 S32 T33 S41 S42", /* 38: 11 moves, a family of 43704 */
    "W00 W02 B11 T13 S30 T31 S32 T33 S40 S42", /* 39: 11 moves, a family of 81340 */
    "W00 W02 W10 B12 T20 S21 S31 S32 S33 S40 S43", /* 40: 11 moves, a family of 105064 */
    "W00 W02 S10 B11 T13 T20 T33 S40 S41 S42", /* 41: 12 moves, a family of 81340 */
    "S00 W01 S03 B10 W12 W30 W32 S40 S41 S42 S43", /* 42: 14 moves, a family of 23704 */
    "T00 T01 T02 T03 S20 B21 S23 S30 S33 W41", /* 43: 14 moves, a family of 25955 */
    "W01 S03 T10 B11 T13 T30 S31 S32 T33 S42", /* 44: 15 moves, a family of 25955 */
    "W00 W02 S10 B11 S20 W30 W32 W40 S42 S43", /* 45: 17 moves, a family of 7888 */
    "T00 T01 T02 T03 B21 S30 S33 S40 W41 S43", /* 46: 16 moves, a family of 25955 */
    "T00 W01 S03 B11 T13 T20 S31 S32 T33 S40", /* 47: 17 moves, a family of 25955 */
    "S00 W01 S10 B11 S20 S23 W30 W32 W40 S42 S43", /* 48: 18 moves, a family of 23704 */
    "S00 S03 T10 B11 T13 T30 S31 S32 T33 S41 S42", /* 49: 18 moves, a family of 43704 */
    "W00 W02 T10 B11 S30 T31 W32 S40 S42 S43", /* 50: 18 moves, a family of 81462 */
    "W00 W02 B11 T13 S20 W31 T33 S40 W41", /* 51: 21 moves, a family of 1398 */
    "W00 W02 S10 B11 S13 S23 S31 W32 W40 W42", /* 52: 21 moves, a family of 7888 */
    "T00 S01 S02 T03 B11 T20 T23 S31 W40 S43", /* 53: 20 moves, a family of 25955 */
    "W00 W02 S10 B11 S13 S23 S31 S32 S33 W40 W42", /* 54: 21 moves, a family of 23704 */
    "S00 B01 S03 T10 T13 S21 S22 T30 T33 S41 S42", /* 55: 21 moves, a family of 43704 */
    "S00 W01 S03 T10 B11 T13 T31 T32 S33 S43", /* 56: 22 moves, a family of 25955 */
    "W01 T03 B11 S20 S23 S30 S31 S32 S33 W40 W42", /* 57: 21 moves, a family of 105064 */
    "W00 W02 S10 B11 S13 S20 T30 W31 W41 S43", /* 58: 23 moves, a family of 28832 */
    "W00 W02 S10 B11 S13 S20 S23 S31 S32 W40 W42", /* 59: 24 moves, a family of 23704 */
    "S00 B01 S03 S10 S13 T20 T21 T22 T23 S40 S43", /* 60: 24 moves, a family of 43704 */
    "W01 S10 B11 S13 S20 S23 T30 T31 T32 T33", /* 61: 25 moves, a family of 25955 */
    "S00 B01 S03 S10 S13 S20 S23 T30 T31 T32 T33", /* 62: 25 moves, a family of 43704 */
    "W00 W02 S10 B11 T13 T20 S31 S40 W41 S43", /* 63: 25 moves, a family of 81462 */
    "B01 S03 S13 W20 W22 S30 W31 S33 S40 W41 S43", /* 64: 27 moves, a family of 23704 */
    "W00 W02 S10 B11 S20 W30 W32 W40 W42", /* 65: 32 moves, a family of 508 */
    "S00 W01 S03 B11 T13 T20 S31 S32 T33 W41", /* 66: 27 moves, a family of 81340 */
    "S00 B01 S03 S13 S21 W22 W30 W32 W40 W42", /* 67: 30 moves, a family of 7888 */
    "S00 W01 S03 T10 B11 T13 S31 S32 T33 W41", /* 68: 28 moves, a family of 81340 */
    "S00 B01 S03 T10 T13 S22 T30 S32 T33 W41", /* 69: 30 moves, a family of 25955 */
    "B01 S03 S13 W20 W22 S30 S31 S32 T33 S40 W41", /* 70: 29 moves, a family of 105064 */
    "T00 B01 T03 S20 S23 S30 T31 T32 S33 S40 S43", /* 71: 31 moves, a family of 43704 */
    "S00 B01 S03 T10 T13 S21 S22 T30 W31 T33", /* 72: 32 moves, a family of 25955 */
    "S00 B01 S03 S10 S13 T20 W21 T31 T32 T33", /* 73: 33 moves, a family of 25955 */
    "S00 B01 T03 S10 S20 S23 S30 W31 S33 W40 W42", /* 74: 32 moves, a family of 105064 */
    "W00 W02 T10 B11 T13 S30 S31 S40 W41 S43", /* 75: 34 moves, a family of 81462 */
    "W00 S02 S10 B11 S20 S23 W30 W32 W40 W42", /* 76: 35 moves, a family of 7888 */
    "S00 W01 S03 B11 S20 T23 T30 T31 T32 S43", /* 77: 35 moves, a family of 25955 */
    "S00 B01 S03 S10 S13 S20 W21 S23 W31 W40 W42", /* 78: 36 moves, a family of 23704 */
    "S00 B01 S03 T20 T21 T22 T23 S40 S41 S42 S43", /* 79: 36 moves, a family of 43704 */
    "S00 B01 S03 S10 S13 S20 W21 S23 W30 W40 W42", /* 80: 37 moves, a family of 23704 */
    "S00 B01 S03 T13 W21 S30 S31 S32 S33 W40 W42", /* 81: 36 moves, a family of 105064 */
    "T00 B01 S03 S13 T20 T23 S31 T32 W40 S43", /* 82: 38 moves, a family of 25955 */
    "B01 T03 S20 W21 S23 S30 S31 S32 S33 W40 W42", /* 83: 37 moves, a family of 105064 */
    "W00 W02 B11 T20 T23 S31 S32 S40 W41 S43", /* 84: 38 moves, a family of 81462 */
    "S00 B01 S03 S10 S13 S20 W21 S23 W31 T33 W41", /* 85: 38 moves, a family of 105064 */
    "T00 W01 S03 B11 T13 T20 S33 S40 W41 S43", /* 86: 39 moves, a family of 81340 */
    "S00 B01 S10 T20 S21 T22 T23 T31 S40 W42", /* 87: 41 moves, a family of 25955 */
    "T00 S01 S02 T03 B11 S20 T31 T32 S33 S40 S43", /* 88: 41 moves, a family of 43704 */
    "W00 W02 B11 T20 S23 W31 S33 S40 W41 S43", /* 89: 42 moves, a family of 28832 */
    "S00 W01 S03 S10 B11 S20 W30 W32 W40 W42", /* 90: 44 moves, a family of 7888 */
    "S00 B01 T03 T10 S21 W22 T30 S31 S32 T33", /* 91: 43 moves, a family of 25955 */
    "W01 S03 B11 S13 S20 S23 W30 W32 W40 W42", /* 92: 45 moves, a family of 7888 */
    "W00 W02 S10 B11 S13 T30 S31 S32 T33 W41", /* 93: 43 moves, a family of 81462 */
    "S00 B01 T10 T13 W21 T30 S31 S32 T33 S42", /* 94: 45 moves, a family of 25955 */
    "B00 W02 S12 S13 W20 T22 T23 W40 W42", /* 95: 49 moves, a family of 1398 */
    "W01 T03 B11 S20 S23 T30 S31 S32 T33 W41", /* 96: 45 moves, a family of 81340 */
    "T00 B01 S03 T13 T20 S31 S32 T33 W40 S42", /* 97: 47 moves, a family of 25955 */
    "S00 W01 S10 B11 S20 S23 W30 W32 S40 W41 S43", /* 98: 48 moves, a family of 23704 */
    "S00 B01 T03 S10 W21 T23 S31 S32 W40 W42", /* 99: 47 moves, a family of 81462 */
    "W00 W02 S10 B11 S13 T20 T23 S40 W41 S43", /* 100: 49 moves, a family of 81462 */
    "S00 B01 S10 W20 W22 W30 W32 W40 W42", /* 101: 53 moves, a family of 508 */
    "T00 B01 S03 T13 S20 S21 S22 T30 W31 T33", /* 102: 50 moves, a family of 25955 */
    "B01 S03 S13 S20 T21 W22 T30 T32 T33 S41", /* 103: 51 moves, a family of 25955 */
    "S00 B01 S10 S20 S21 W22 W30 W32 W40 W42", /* 104: 53 moves, a family of 7888 */
    "S00 W01 S03 S10 B11 S13 T30 W31 T33 W41", /* 105: 51 moves, a family of 81462 */
    "T00 B01 T03 T20 T23 S31 S32 W40 S42 S43", /* 106: 53 moves, a family of 25955 */
    "S00 B01 S10 W20 W22 W30 W32 S40 W41 S43", /* 107: 55 moves, a family of 7888 */
    "W00 T03 B11 S20 S23 T30 W31 T33 S41 S42", /* 108: 53 moves, a family of 81340 */
    "B01 S03 S13 W20 S22 S23 W30 W32 S40 W41 S43", /* 109: 55 moves, a family of 23704 */
    "S00 B01 S10 S20 W21 S23 S30 T31 W32 S40 W42", /* 110: 54 moves, a family of 105064 */
    "W01 S10 B11 S13 T20 S23 S31 W32 W40 W42", /* 111: 56 moves, a family of 28832 */
    "S00 B01 T03 S10 W20 W22 S31 S32 T33 W41", /* 112: 56 moves, a family of 81462 */
    "T00 B01 T03 S20 S23 T30 S31 S32 T33 W41", /* 113: 58 moves, a family of 25955 */
    "S00 B01 T03 T10 W21 S23 T30 S31 S32 T33", /* 114: 59 moves, a family of 25955 */
    "B01 S10 S13 W20 S22 T23 S30 W31 S40 W41 S43", /* 115: 58 moves, a family of 105064 */
    "S00 B01 T03 S10 T20 W21 T23 S31 S32 W41", /* 116: 59 moves, a family of 81340 */
    "S00 B01 S03 S10 S13 W21 T30 W31 T33 W41", /* 117: 60 moves, a family of 81462 */
    "S01 S02 T03 B11 S20 S23 W30 W32 W40 W42", /* 118: 62 moves, a family of 28832 */
    "B01 T10 T13 T21 T22 S30 S33 S40 W41 S43", /* 119: 63 moves, a family of 25955 */
    "S00 B01 T03 T10 S22 T23 T30 W31 S42 S43", /* 120: 64 moves, a family of 25955 */
    "T00 B01 T03 S20 W21 S23 T31 T32 S33 S43", /* 121: 65 moves, a family of 25955 */
    "W00 B10 T12 T13 S30 S31 W32 W40 W42", /* 122: 69 moves, a family of 1398 */
    "S00 B01 S03 T13 S21 S22 W30 W32 W40 W42", /* 123: 67 moves, a family of 28832 */
    "B01 S10 T13 T20 W21 S31 S32 T33 S40 W41", /* 124: 67 moves, a family of 81340 */
    "S00 W01 S03 T10 B11 T13 W30 W32 S40 S43", /* 125: 69 moves, a family of 81462 */
    "T00 B01 T03 S20 S23 T30 W31 T33 S41 S42", /* 126: 71 moves, a family of 25955 */
    "S00 B01 S03 T10 W21 S30 W31 S33 W40 W42", /* 127: 72 moves, a family of 28832 */
    "T00 B01 T03 S20 S21 S22 S23 W31 T33 W41", /* 128: 72 moves, a family of 81340 */
    "B01 S03 S13 T20 W21 T23 W31 S40 W41 S43", /* 129: 74 moves, a family of 81462 */
    "S00 B01 S03 S10 T13 T20 W31 T33 S40 W41", /* 130: 76 moves, a family of 81340 */
    "T00 B01 S03 S13 W20 W22 W30 W32 S40 S43", /* 131: 79 moves, a family of 28832 */
    "T00 B01 T03 T20 W21 T23 S31 S32 S40 S43", /* 132: 81 moves, a family of 25955 */
    "S00 B01 T03 S10 S20 W21 S23 T30 W31 W41", /* 133: 82 moves, a family of 81462 */
    "S00 B01 T03 S10 S20 W21 S23 T30 W31 T33", /* 134: 84 moves, a family of 81340 */
    "T00 B01 S03 T13 S20 W21 W30 W32 S40 S43", /* 135: 85 moves, a family of 81462 */
    "T00 B01 T03 S21 S22 S23 W31 S33 W40 W42", /* 136: 87 moves, a family of 81462 */
    "B01 T10 S13 W21 T23 T30 S31 T32 S41 S43", /* 137: 91 moves, a family of 25955 */
    "S00 B01 S03 T10 T13 W21 S30 S33 W40 W42", /* 138: 91 moves, a family of 81462 */
    "S00 B01 S03 S10 T13 S20 W30 T32 T33 W40", /* 139: 94 moves, a family of 81340 */
    "B01 T10 T13 W21 S30 S31 S32 S33 W40 W42", /* 140: 96 moves, a family of 81462 */
    "S00 B01 S03 T10 T21 T23 S30 S32 W40 W42", /* 141: 100 moves, a family of 81340 */
    "T00 B01 T03 W20 W22 S30 W31 S33 S40 S43", /* 142: 103 moves, a family of 81462 */
    "S00 B01 S03 T13 W20 T22 S31 T33 S40 W41", /* 143: 108 moves, a family of 81340 */
    "B00 S02 T03 S12 S20 W21 T23 W30 T32 S43", /* 144: 111 moves, a family of 81340 */
    "S00 S01 B02 S11 W21 S23 T30 W31 T33 W41", /* 145: 116 moves, a family of 81462 */
    "S00 B01 S10 S20 W21 S23 T30 W31 T33 W41", /* 146: 119 moves, a family of 81462 */
    "B01 S10 S13 T20 W21 T23 W31 S40 W41 S43", /* 147: 123 moves, a family of 81462 */
    "B00 S02 S03 S12 T13 W20 T22 S31 T33 W41", /* 148: 126 moves, a family of 81340 */
    "S00 S02 S03 T10 B12 T21 T30 W32 W41 S43", /* 149: 131 moves, a family of 81340 */
    "S00 S01 S02 T03 B10 T12 T23 S30 W31 W41", /* 150: 137 moves, a family of 81340 */
};

const uint8_t klt_level_par[KLT_LEVELS] = {
    3, 4, 5, 5, 4, 5, 5, 4, 4, 3, 8, 4, 8, 3, 7, 7, 4, 3, 4, 3, 4, 6, 7, 7, 9,
    10, 11, 12, 11, 10, 12, 11, 10, 14, 11, 10, 11, 11, 11, 11, 12, 14, 14, 15, 17, 16, 17, 18, 18, 18,
    21, 21, 20, 21, 21, 22, 21, 23, 24, 24, 25, 25, 25, 27, 32, 27, 30, 28, 30, 29, 31, 32, 33, 32, 34,
    35, 35, 36, 36, 37, 36, 38, 37, 38, 38, 39, 41, 41, 42, 44, 43, 45, 43, 45, 49, 45, 47, 48, 47, 49,
    53, 50, 51, 53, 51, 53, 55, 53, 55, 54, 56, 56, 58, 59, 58, 59, 60, 62, 63, 64, 65, 69, 67, 67, 69,
    71, 72, 72, 74, 76, 79, 81, 82, 84, 85, 87, 91, 91, 94, 96, 100, 103, 108, 111, 116, 119, 123, 126, 131, 137,
};

const uint32_t klt_level_family[KLT_LEVELS] = {
    10, 38, 29, 38, 127, 74, 102, 359, 453, 1532, 30, 1192, 58, 7462, 260,
    508, 7888, 43704, 23704, 105064, 81340, 25955, 23704, 81340, 43704, 99, 81, 66, 260, 1192,
    272, 1398, 7462, 508, 7888, 43704, 25955, 43704, 81340, 105064, 81340, 23704, 25955, 25955, 7888,
    25955, 25955, 23704, 43704, 81462, 1398, 7888, 25955, 23704, 43704, 25955, 105064, 28832, 23704, 43704,
    25955, 43704, 81462, 23704, 508, 81340, 7888, 81340, 25955, 105064, 43704, 25955, 25955, 105064, 81462,
    7888, 25955, 23704, 43704, 23704, 105064, 25955, 105064, 81462, 105064, 81340, 25955, 43704, 28832, 7888,
    25955, 7888, 81462, 25955, 1398, 81340, 25955, 23704, 81462, 81462, 508, 25955, 25955, 7888, 81462,
    25955, 7888, 81340, 23704, 105064, 28832, 81462, 25955, 25955, 105064, 81340, 81462, 28832, 25955, 25955,
    25955, 1398, 28832, 81340, 81462, 25955, 28832, 81340, 81462, 81340, 28832, 25955, 81462, 81340, 81462,
    81462, 25955, 81462, 81340, 81462, 81340, 81462, 81340, 81340, 81462, 81462, 81462, 81340, 81340, 81340,
};
