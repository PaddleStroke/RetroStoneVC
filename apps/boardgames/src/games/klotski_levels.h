/*
 * Klotski's levels: our own, made by tools/klotski_gen.c (blocks laid by chance, every position of their family
 * and its fewest moves found by the rules, six chapters of 25 from the easiest). Each: the blocks (B the great
 * block, T standing, W lying, S small; their top left cell, row then column), then (in the comment) its fewest
 * moves and its family's size; the fewest moves again in klt_level_par (the tests check them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const klt_levels[KLT_LEVELS] = {
    "S00 T01 W02 S10 T12 W20 B30 T32 T33", /* 1: 3 moves, a family of 10 */
    "T00 T01 T02 T03 S20 W21 B30 S32 T33", /* 2: 3 moves, a family of 52 */
    "S00 S01 T02 S03 W10 S13 B21 T23 T30 S41 S42", /* 3: 4 moves, a family of 29 */
    "W00 T02 T03 S10 S11 W22 T30 T31 B32", /* 4: 3 moves, a family of 84 */
    "W00 W02 W10 S12 S13 S20 W21 B30 S32 S42 S43", /* 5: 3 moves, a family of 127 */
    "W00 W02 S10 W11 T13 W21 S30 T31 B32", /* 6: 3 moves, a family of 168 */
    "W00 S03 T10 S11 W12 B21 T23 T30 S41 S43", /* 7: 5 moves, a family of 38 */
    "T00 W02 W11 T13 S20 W21 B30 S32 T33", /* 8: 5 moves, a family of 53 */
    "S00 T02 S03 T10 T11 W22 B30 T32 T33", /* 9: 5 moves, a family of 71 */
    "T00 W01 T11 S13 T20 S22 S23 T31 B32 S40", /* 10: 5 moves, a family of 99 */
    "W00 T03 S10 T11 T12 T23 B30 T32 S43", /* 11: 6 moves, a family of 54 */
    "S01 T02 T03 T10 T11 W22 B30 S32 T33", /* 12: 6 moves, a family of 75 */
    "W00 T02 T03 W10 W20 B30 W32 S42 S43", /* 13: 3 moves, a family of 1398 */
    "S00 T01 T02 T03 S10 T21 W22 T30 B32", /* 14: 5 moves, a family of 359 */
    "W01 T03 S10 W11 T20 S21 W22 T31 B32", /* 15: 4 moves, a family of 1192 */
    "W00 W02 S10 W11 S13 B20 W22 S32 S33 S40 S41", /* 16: 7 moves, a family of 127 */
    "S01 W02 S10 T12 T13 W20 T30 T31 B32", /* 17: 8 moves, a family of 84 */
    "W00 W02 W10 W12 S22 S23 T30 T31 B32", /* 18: 7 moves, a family of 260 */
    "T01 W02 T10 W12 S22 S23 T30 T31 B32", /* 19: 7 moves, a family of 453 */
    "W00 T02 S03 T10 T11 T23 B30 T32 S43", /* 20: 6 moves, a family of 1532 */
    "T00 S01 T02 T03 W21 S23 B30 S32 T33 S42", /* 21: 9 moves, a family of 248 */
    "W00 S02 T03 W10 T20 W21 T31 B32 S40", /* 22: 8 moves, a family of 1192 */
    "S00 S01 S02 T10 T11 T12 S23 B30 S32 T33 S42", /* 23: 5 moves, a family of 43704 */
    "S00 W01 S03 W10 W12 S20 S21 B22 W30 S40 S42", /* 24: 7 moves, a family of 23704 */
    "W00 W02 S10 W12 S20 T22 S23 B30 T33 S42", /* 25: 7 moves, a family of 81462 */
    "T00 T01 T12 T13 B20 W32 S40 S41 W42", /* 26: 11 moves, a family of 30 */
    "W00 W02 S10 W11 B20 S22 W32 S40 S41 S42 S43", /* 27: 10 moves, a family of 127 */
    "S00 S01 W02 W10 T12 T13 T21 T30 B32", /* 28: 11 moves, a family of 84 */
    "W00 W02 S10 W12 B20 S22 W32 S40 S41 S42 S43", /* 29: 11 moves, a family of 127 */
    "T01 W02 T10 S12 S13 T21 S22 T30 B32 S41", /* 30: 12 moves, a family of 99 */
    "S01 T02 S03 T10 T13 W21 T30 T31 B32", /* 31: 11 moves, a family of 359 */
    "T00 T01 W02 W12 B20 S22 T32 T33 S40", /* 32: 13 moves, a family of 102 */
    "W01 T03 W10 T12 S21 S23 B30 T32 T33", /* 33: 12 moves, a family of 453 */
    "T00 S01 W02 T12 W20 S23 T30 T31 B32", /* 34: 13 moves, a family of 272 */
    "W00 W02 W11 T13 B20 T32 T33 S40 S41", /* 35: 13 moves, a family of 578 */
    "W00 T02 T03 W20 B22 T30 T31 S42 S43", /* 36: 14 moves, a family of 453 */
    "T00 T01 W02 T12 B20 T32 T33 S40 S41", /* 37: 13 moves, a family of 1532 */
    "S00 W01 T03 T10 W11 B22 T30 T31 S42", /* 38: 17 moves, a family of 81 */
    "S00 S02 T03 T10 T11 W22 T30 T31 B32", /* 39: 16 moves, a family of 359 */
    "W00 S02 S03 T10 B11 T13 T30 S32 T33 S42", /* 40: 12 moves, a family of 25955 */
    "S00 T01 S02 S03 T10 B12 T30 T31 S32 S42 S43", /* 41: 12 moves, a family of 43704 */
    "W00 W02 T10 S11 B12 T21 T30 S32 S33 S41", /* 42: 12 moves, a family of 81340 */
    "S00 S01 S02 S03 W10 B12 W20 W30 W32 S42 S43", /* 43: 14 moves, a family of 23704 */
    "T00 S02 T03 T11 S12 T20 W22 T31 B32", /* 44: 19 moves, a family of 359 */
    "W00 W02 B10 W12 W22 S30 T31 S32 S40 S43", /* 45: 15 moves, a family of 28832 */
    "S00 W01 S03 W10 B12 S21 S30 W31 S33 W40 S43", /* 46: 16 moves, a family of 23704 */
    "W00 W02 W10 B12 S20 S21 S30 T32 S33 S41 S43", /* 47: 15 moves, a family of 105064 */
    "T00 T01 T02 S03 T13 B21 S30 S33 S40 W42", /* 48: 17 moves, a family of 25955 */
    "S00 S01 W02 T10 T11 B12 W30 S32 W41 S43", /* 49: 17 moves, a family of 81462 */
    "S00 S01 W02 W10 B12 T20 T21 S33 W40 S43", /* 50: 18 moves, a family of 81462 */
    "W00 B02 W10 W20 S23 W30 S32 W40 W42", /* 51: 22 moves, a family of 508 */
    "W00 B02 W10 W20 S22 S23 W31 W40 W42", /* 52: 24 moves, a family of 508 */
    "T00 S01 S02 T03 B11 T20 T23 S31 W40 S43", /* 53: 20 moves, a family of 25955 */
    "B00 W02 S12 S13 W20 W22 S31 W32 S41 S42 S43", /* 54: 21 moves, a family of 23704 */
    "T00 T01 S03 T12 W20 S23 T30 T31 B32", /* 55: 26 moves, a family of 359 */
    "W00 B02 T10 T11 S22 S23 T32 T33 S40 S41", /* 56: 22 moves, a family of 25955 */
    "S01 B02 T10 S11 S21 T22 T23 T30 S31 S41 S43", /* 57: 22 moves, a family of 43704 */
    "W00 B02 W10 W20 W22 S31 T32 S33 S40 S43", /* 58: 23 moves, a family of 28832 */
    "W00 B02 W10 S20 S22 S23 W30 S32 W40 W42", /* 59: 25 moves, a family of 7888 */
    "T00 S01 W02 B11 S13 T20 S31 S32 T33 W40", /* 60: 23 moves, a family of 81340 */
    "T00 T01 S02 S03 B12 T20 T21 W32 S42 S43", /* 61: 25 moves, a family of 25955 */
    "B00 S02 T03 S20 S21 S22 T23 S30 T31 T32 S43", /* 62: 25 moves, a family of 43704 */
    "W00 S02 T03 B11 S20 T23 T30 S32 S41 W42", /* 63: 25 moves, a family of 81340 */
    "B00 W02 W12 W21 T23 W30 T32 S41 S43", /* 64: 30 moves, a family of 1398 */
    "W00 W02 W10 B12 S20 S30 W31 W40 W42", /* 65: 32 moves, a family of 508 */
    "W00 B02 T11 T20 S22 S31 T32 T33 S40 S41", /* 66: 28 moves, a family of 25955 */
    "W00 B02 W10 S20 S21 T23 W30 T32 W40", /* 67: 32 moves, a family of 1398 */
    "W00 S02 T03 B10 S12 T22 S23 W30 W41 S43", /* 68: 28 moves, a family of 81462 */
    "B00 S02 S03 W12 W20 S22 S23 W30 S32 S40 W41", /* 69: 30 moves, a family of 23704 */
    "B00 W12 S20 S21 W22 S30 S31 S32 T33 W40 S42", /* 70: 29 moves, a family of 105064 */
    "S00 W01 S10 T11 B12 S20 T30 S31 W32 W42", /* 71: 30 moves, a family of 81462 */
    "S01 S02 T03 B10 T12 S23 W30 T32 T33 S41", /* 72: 32 moves, a family of 25955 */
    "S00 S01 B02 W10 S21 W22 S30 W31 S33 S41 W42", /* 73: 33 moves, a family of 23704 */
    "B00 S03 T12 S13 S20 S21 S30 S31 W32 W40 W42", /* 74: 32 moves, a family of 105064 */
    "W00 S03 T10 S11 B12 S30 S31 T32 T33 W40", /* 75: 33 moves, a family of 81340 */
    "S00 S01 W02 B10 W12 S22 S23 W30 W32 W40", /* 76: 35 moves, a family of 7888 */
    "W01 S03 W11 S13 W20 W22 W30 B32 W40", /* 77: 39 moves, a family of 508 */
    "W00 W02 B11 S13 S20 W30 S32 S33 W40 W42", /* 78: 37 moves, a family of 7888 */
    "S00 W01 S03 S10 T11 B12 S30 T32 T33 W40", /* 79: 35 moves, a family of 81340 */
    "W00 W02 S10 B11 S13 S20 S30 S31 W32 S40 W42", /* 80: 37 moves, a family of 23704 */
    "S00 S01 W02 W10 B12 W20 W32 W40 W42", /* 81: 42 moves, a family of 508 */
    "W01 S03 B10 T12 T13 T30 T32 S33 S41 S43", /* 82: 38 moves, a family of 25955 */
    "T00 S02 S03 B11 T13 S30 T31 T32 S33 S40 S43", /* 83: 38 moves, a family of 43704 */
    "W00 W02 S10 T11 B12 S20 W31 S40 W41 S43", /* 84: 39 moves, a family of 28832 */
    "S00 T01 W02 S10 B12 S20 S21 S30 W31 W41 S43", /* 85: 38 moves, a family of 105064 */
    "W00 B02 T10 S11 T21 W22 S32 S33 W41 S43", /* 86: 39 moves, a family of 81462 */
    "W01 S03 S10 S11 B12 S20 S30 S31 W32 W40 W42", /* 87: 41 moves, a family of 23704 */
    "S00 W01 S11 B12 W20 W30 W32 W40 W42", /* 88: 46 moves, a family of 508 */
    "S00 B01 T03 S20 T21 W22 T30 S32 T33 S41", /* 89: 42 moves, a family of 25955 */
    "T00 S01 S02 T03 B11 S20 S30 T31 T32 S33 S43", /* 90: 42 moves, a family of 43704 */
    "B00 S02 S03 W12 T20 S21 T22 T31 T33 S40", /* 91: 43 moves, a family of 25955 */
    "B00 W02 S12 W20 S22 T23 W30 T32 W40", /* 92: 47 moves, a family of 1398 */
    "W00 B02 S10 S11 T20 T22 S31 T33 S40 W41", /* 93: 43 moves, a family of 81340 */
    "S00 W01 S03 B10 S12 S13 W22 W31 S33 S40 W41", /* 94: 45 moves, a family of 23704 */
    "B00 S02 S03 S12 S13 T20 W22 W31 S40 W41 S43", /* 95: 44 moves, a family of 105064 */
    "B00 S02 S03 W12 W20 S22 T23 W31 W40 S42", /* 96: 46 moves, a family of 28832 */
    "T01 B02 T10 S21 S22 S23 S31 T32 T33 W40", /* 97: 47 moves, a family of 25955 */
    "W00 S10 S11 B12 W20 W30 W32 S40 W41 S43", /* 98: 49 moves, a family of 7888 */
    "S00 B01 T03 S10 W20 S22 T23 W30 S32 W42", /* 99: 47 moves, a family of 81462 */
    "S00 S01 W02 B10 T12 S30 W31 T33 W40 S42", /* 100: 48 moves, a family of 81462 */
    "B00 W02 T13 W20 W30 T32 S33 W40 S43", /* 101: 51 moves, a family of 1398 */
    "W00 S02 S10 B11 S13 S23 W30 W32 S40 W41 S43", /* 102: 50 moves, a family of 23704 */
    "S01 S02 S10 S11 B12 W20 W30 W32 S40 W41 S43", /* 103: 51 moves, a family of 23704 */
    "W00 B02 S10 S11 W20 W22 S30 W31 W41 S43", /* 104: 53 moves, a family of 7888 */
    "W00 B02 T10 S11 S21 W22 W30 S33 W40 S43", /* 105: 52 moves, a family of 28832 */
    "B00 S02 S03 S12 S13 W20 W22 W31 S40 W41 S43", /* 106: 53 moves, a family of 23704 */
    "S01 B02 S10 S11 W20 T22 S23 W30 W40 S42 S43", /* 107: 52 moves, a family of 105064 */
    "W01 T10 S11 B12 S21 S30 W31 S33 W40 W42", /* 108: 54 moves, a family of 28832 */
    "B00 S03 S12 S13 W21 S23 W30 W32 S40 W41 S43", /* 109: 55 moves, a family of 23704 */
    "S00 S01 S02 S03 B10 W12 W22 T30 W31 S33 S43", /* 110: 54 moves, a family of 105064 */
    "B00 W02 T12 S13 S20 S21 S30 W31 W40 W42", /* 111: 56 moves, a family of 28832 */
    "B00 T02 T03 T20 T22 S23 S33 S40 S41 W42", /* 112: 57 moves, a family of 25955 */
    "S00 B01 T03 S10 S20 W21 T23 T30 T32 S41", /* 113: 58 moves, a family of 25955 */
    "S00 S01 W02 B10 T12 T13 W30 W32 W41", /* 114: 62 moves, a family of 1398 */
    "S00 B01 S03 T13 W20 S22 S31 W32 S40 W41 S43", /* 115: 58 moves, a family of 105064 */
    "S00 S01 W02 S10 B11 T20 T23 S31 W40 W42", /* 116: 59 moves, a family of 81462 */
    "W00 B02 T10 S11 S21 W22 T31 T33 S40 S42", /* 117: 60 moves, a family of 81340 */
    "B00 W02 S12 S20 S21 W22 W30 T33 W40 S42", /* 118: 62 moves, a family of 28832 */
    "T00 B01 S03 T13 W20 S30 T31 S32 T33 S42", /* 119: 63 moves, a family of 25955 */
    "W00 T02 T03 B10 S30 S31 W32 W40 W42", /* 120: 67 moves, a family of 1398 */
    "B00 W02 T13 S20 W21 S30 W31 T33 W40", /* 121: 68 moves, a family of 1398 */
    "W01 T03 B10 T12 S30 S31 W32 W40 W42", /* 122: 69 moves, a family of 1398 */
    "T00 T01 B02 S20 S21 S22 W30 T32 T33 S40", /* 123: 67 moves, a family of 25955 */
    "B00 S02 T03 T20 W21 T23 T31 S32 S40 S43", /* 124: 68 moves, a family of 25955 */
    "B02 W10 T20 W21 S23 S31 W32 S40 S41 W42", /* 125: 69 moves, a family of 28832 */
    "T00 S02 S03 B12 W20 S30 S31 W32 W40 W42", /* 126: 70 moves, a family of 28832 */
    "T00 T01 B02 W20 T23 S30 S31 T32 S40 S43", /* 127: 72 moves, a family of 25955 */
    "B00 S02 T03 S12 W20 S22 T23 T30 W31 S43", /* 128: 72 moves, a family of 81340 */
    "B00 W02 S12 T13 S20 W21 S30 T31 T32 S43", /* 129: 74 moves, a family of 81340 */
    "T00 B01 S03 S13 W20 W22 W31 S40 S41 W42", /* 130: 77 moves, a family of 28832 */
    "S00 B01 S10 T13 W20 W30 W32 S40 S41 W42", /* 131: 79 moves, a family of 28832 */
    "T00 B01 T03 T20 W21 T23 S40 S41 S42 S43", /* 132: 81 moves, a family of 25955 */
    "S00 B01 T03 T10 W22 S30 T31 S32 T33 S40", /* 133: 83 moves, a family of 25955 */
    "T00 B01 S03 T13 W20 T22 T30 S33 S42 S43", /* 134: 85 moves, a family of 25955 */
    "T00 S01 B02 S11 W20 S22 T23 W30 S32 W41", /* 135: 85 moves, a family of 81462 */
    "T00 B01 T03 W20 S22 S23 W30 T32 S33 S41", /* 136: 87 moves, a family of 81340 */
    "T00 S01 T03 B11 S20 S30 W31 S33 W40 W42", /* 137: 89 moves, a family of 81462 */
    "S00 T01 B02 S10 T22 S23 W30 T33 W40 S42", /* 138: 91 moves, a family of 81340 */
    "B00 T02 T03 S20 W21 W30 T33 S40 S41 S42", /* 139: 93 moves, a family of 81340 */
    "T01 S02 T10 B12 S21 S30 W31 T33 S40 W41", /* 140: 96 moves, a family of 81340 */
    "B00 S02 T03 S20 W21 S23 T30 W32 W41 S43", /* 141: 99 moves, a family of 81462 */
    "S00 B02 T10 S11 T21 W22 T30 S32 S33 W42", /* 142: 103 moves, a family of 81340 */
    "T00 B02 S11 S20 S21 W22 W30 T32 W40 S43", /* 143: 107 moves, a family of 81462 */
    "S00 S01 T02 T03 B10 S23 W30 T33 S40 W41", /* 144: 111 moves, a family of 81340 */
    "T00 S01 B02 T11 S20 S23 T30 S31 W32 W42", /* 145: 115 moves, a family of 81340 */
    "B00 S02 S13 S20 W21 S23 T30 W31 T33 W41", /* 146: 118 moves, a family of 81462 */
    "B00 S03 S12 T13 W20 S22 S31 T32 T33 W40", /* 147: 122 moves, a family of 81340 */
    "S00 B01 S03 T13 S20 W21 S30 T31 W32 W42", /* 148: 126 moves, a family of 81462 */
    "S00 S02 T10 S11 B12 T21 T30 W32 W41 S43", /* 149: 130 moves, a family of 81340 */
    "T00 S01 B02 W20 T22 S23 W30 W40 S42 S43", /* 150: 134 moves, a family of 81462 */
};

const uint8_t klt_level_par[KLT_LEVELS] = {
    3, 3, 4, 3, 3, 3, 5, 5, 5, 5, 6, 6, 3, 5, 4, 7, 8, 7, 7, 6, 9, 8, 5, 7, 7,
    11, 10, 11, 11, 12, 11, 13, 12, 13, 13, 14, 13, 17, 16, 12, 12, 12, 14, 19, 15, 16, 15, 17, 17, 18,
    22, 24, 20, 21, 26, 22, 22, 23, 25, 23, 25, 25, 25, 30, 32, 28, 32, 28, 30, 29, 30, 32, 33, 32, 33,
    35, 39, 37, 35, 37, 42, 38, 38, 39, 38, 39, 41, 46, 42, 42, 43, 47, 43, 45, 44, 46, 47, 49, 47, 48,
    51, 50, 51, 53, 52, 53, 52, 54, 55, 54, 56, 57, 58, 62, 58, 59, 60, 62, 63, 67, 68, 69, 67, 68, 69,
    70, 72, 72, 74, 77, 79, 81, 83, 85, 85, 87, 89, 91, 93, 96, 99, 103, 107, 111, 115, 118, 122, 126, 130, 134,
};

const uint32_t klt_level_family[KLT_LEVELS] = {
    10, 52, 29, 84, 127, 168, 38, 53, 71, 99, 54, 75, 1398, 359, 1192,
    127, 84, 260, 453, 1532, 248, 1192, 43704, 23704, 81462, 30, 127, 84, 127, 99,
    359, 102, 453, 272, 578, 453, 1532, 81, 359, 25955, 43704, 81340, 23704, 359, 28832,
    23704, 105064, 25955, 81462, 81462, 508, 508, 25955, 23704, 359, 25955, 43704, 28832, 7888, 81340,
    25955, 43704, 81340, 1398, 508, 25955, 1398, 81462, 23704, 105064, 81462, 25955, 23704, 105064, 81340,
    7888, 508, 7888, 81340, 23704, 508, 25955, 43704, 28832, 105064, 81462, 23704, 508, 25955, 43704,
    25955, 1398, 81340, 23704, 105064, 28832, 25955, 7888, 81462, 81462, 1398, 23704, 23704, 7888, 28832,
    23704, 105064, 28832, 23704, 105064, 28832, 25955, 25955, 1398, 105064, 81462, 81340, 28832, 25955, 1398,
    1398, 1398, 25955, 25955, 28832, 28832, 25955, 81340, 81340, 28832, 28832, 25955, 25955, 25955, 81462,
    81340, 81462, 81340, 81340, 81340, 81462, 81340, 81462, 81340, 81340, 81462, 81340, 81462, 81340, 81462,
};
