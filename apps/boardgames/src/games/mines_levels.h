/*
 * Mines' levels: our own, made by tools/mines_gen.c (mines laid by chance away from a first cell,
 * kept when the game's reasoner clears the field from that first area without a guess), six chapters
 * of 25 from the easiest. Each: its size, its first cell (x + y * w) and its mines (mines.h,
 * mns_encode); in the comment the mines and the reasoner's rounds: with one number alone, two numbers
 * side by side, the mines left.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
static const mns_level mns_levels[MNS_LEVELS] = {
    {5, 5, 0, "AgDAA"}, /* 1: 5x5/3, rounds 3 0 0 */
    {5, 5, 14, "AChAA"}, /* 2: 5x5/3, rounds 3 0 0 */
    {5, 5, 15, "EAgQA"}, /* 3: 5x5/3, rounds 4 0 0 */
    {5, 5, 24, "EAgQA"}, /* 4: 5x5/3, rounds 4 0 0 */
    {5, 5, 7, "AYABA"}, /* 5: 5x5/3, rounds 6 0 0 */
    {5, 6, 6, "AAKAo"}, /* 6: 5x6/4, rounds 3 0 0 */
    {5, 6, 14, "EAgQI"}, /* 7: 5x6/4, rounds 4 0 0 */
    {5, 6, 18, "gCAIg"}, /* 8: 5x6/4, rounds 5 0 0 */
    {5, 6, 26, "CAKCA"}, /* 9: 5x6/4, rounds 5 0 0 */
    {5, 6, 26, "GAUAA"}, /* 10: 5x6/4, rounds 6 0 0 */
    {6, 6, 14, "BBQAEg"}, /* 11: 6x6/5, rounds 3 0 0 */
    {6, 6, 0, "QAEhQA"}, /* 12: 6x6/5, rounds 4 0 0 */
    {6, 6, 2, "AgEACM"}, /* 13: 6x6/5, rounds 5 0 0 */
    {6, 6, 26, "AEQRgA"}, /* 14: 6x6/5, rounds 6 0 0 */
    {6, 6, 21, "AJCACQ"}, /* 15: 6x6/5, rounds 7 0 0 */
    {6, 7, 21, "MAADCQA"}, /* 16: 6x7/6, rounds 3 0 0 */
    {6, 7, 3, "BAIACIR"}, /* 17: 6x7/6, rounds 5 0 0 */
    {6, 7, 2, "AhQAEwA"}, /* 18: 6x7/6, rounds 6 0 0 */
    {6, 7, 23, "CIAAEkQ"}, /* 19: 6x7/6, rounds 6 0 0 */
    {6, 7, 0, "AAJQAjA"}, /* 20: 6x7/6, rounds 8 0 0 */
    {7, 7, 41, "QREAQEACA"}, /* 21: 7x7/7, rounds 5 0 0 */
    {7, 7, 10, "gAEAYCIIA"}, /* 22: 7x7/7, rounds 5 0 0 */
    {7, 7, 38, "CICABABBB"}, /* 23: 7x7/7, rounds 6 0 0 */
    {7, 7, 31, "CBQBgAggA"}, /* 24: 7x7/7, rounds 7 0 0 */
    {7, 7, 22, "CkAABASgA"}, /* 25: 7x7/7, rounds 10 0 0, in twins */
    {7, 8, 11, "AAEkIClAAA"}, /* 26: 7x8/8, rounds 5 1 0 */
    {7, 8, 47, "EAAIIFEAYA"}, /* 27: 7x8/8, rounds 6 1 0 */
    {7, 8, 9, "AAERJABRAA"}, /* 28: 7x8/8, rounds 7 1 0 */
    {7, 8, 25, "AAKAYAUADA"}, /* 29: 7x8/8, rounds 8 1 0 */
    {7, 8, 23, "AUACUAEQAB"}, /* 30: 7x8/8, rounds 8 1 0 */
    {7, 9, 22, "AKAACiEYgAA"}, /* 31: 7x9/9, rounds 6 1 0 */
    {7, 9, 26, "KAKAgAmAAQA"}, /* 32: 7x9/9, rounds 8 1 0 */
    {7, 9, 33, "4AQAAiAEQBA"}, /* 33: 7x9/9, rounds 10 1 0 */
    {7, 9, 4, "AEgRogASAAA"}, /* 34: 7x9/9, rounds 7 2 0 */
    {7, 9, 41, "AiAggAEQaAA"}, /* 35: 7x9/9, rounds 10 2 0 */
    {8, 9, 27, "aAQAgACggUAQ"}, /* 36: 8x9/11, rounds 6 1 0 */
    {8, 9, 62, "ECFCEQAoAIgA"}, /* 37: 8x9/11, rounds 9 1 0 */
    {8, 9, 67, "AgkCABwYCACA"}, /* 38: 8x9/11, rounds 12 1 0 */
    {8, 9, 46, "QiIARAAEKQIA"}, /* 39: 8x9/11, rounds 7 2 0 */
    {8, 9, 46, "AQAmTAQCAABg"}, /* 40: 8x9/11, rounds 14 2 0 */
    {8, 10, 43, "hBACCASAAWDAAA"}, /* 41: 8x10/12, rounds 8 2 0 */
    {8, 10, 26, "JQEEAAIIUEACgA"}, /* 42: 8x10/12, rounds 9 2 0 */
    {8, 10, 35, "UQAIAECAyQEAEA"}, /* 43: 8x10/12, rounds 11 2 0 */
    {8, 10, 9, "IBCgBogQgAAAiA"}, /* 44: 8x10/12, rounds 6 3 0 */
    {8, 10, 67, "AiCgEiARgIAABA"}, /* 45: 8x10/12, rounds 10 3 0 */
    {9, 10, 10, "IHCAoAAgERBAAAg"}, /* 46: 9x10/13, rounds 4 2 0 */
    {9, 10, 29, "IQKAAAhAEAUAkAD"}, /* 47: 9x10/13, rounds 11 2 0 */
    {9, 10, 25, "gQBAABgKBAAqAIg"}, /* 48: 9x10/13, rounds 14 2 0 */
    {9, 10, 14, "AAAIEgDBDJAkAEA"}, /* 49: 9x10/13, rounds 8 3 0 */
    {9, 10, 76, "CAoGEAQACAIYFAQ"}, /* 50: 9x10/14, rounds 8 3 0, in twins */
    {9, 10, 11, "AEAA0IAUFBQIgEA"}, /* 51: 9x10/14, rounds 11 2 0 */
    {9, 10, 25, "iCBJAECIEAAwAAD"}, /* 52: 9x10/14, rounds 14 2 0 */
    {9, 10, 43, "AEACqASAAmwQAAQ"}, /* 53: 9x10/14, rounds 9 3 0 */
    {9, 10, 24, "gsAQEAYQAAAoBEC"}, /* 54: 9x10/14, rounds 10 3 0 */
    {9, 10, 36, "AAJWBAkAQAAQEKC"}, /* 55: 9x10/14, rounds 11 4 0 */
    {9, 11, 61, "wAEQggGAAQAFBCMBA"}, /* 56: 9x11/16, rounds 9 3 0 */
    {9, 11, 83, "AYgyCDAQAoIAQIQAA"}, /* 57: 9x11/16, rounds 14 3 0 */
    {9, 11, 13, "AggCBCBECQCAUAJQE"}, /* 58: 9x11/16, rounds 15 3 0 */
    {9, 11, 67, "wBATCAIgACgAAjgAB"}, /* 59: 9x11/16, rounds 10 4 0 */
    {9, 11, 23, "IEAMCBADAgiACWACA"}, /* 60: 9x11/16, rounds 13 4 0 */
    {9, 12, 30, "yECCAAiAECgAAyAIEQ"}, /* 61: 9x12/17, rounds 7 3 0 */
    {9, 12, 14, "IARIEQgAAABgQKEMAh"}, /* 62: 9x12/17, rounds 12 3 0 */
    {9, 12, 49, "CBAaAMAIAABALBKUAA"}, /* 63: 9x12/17, rounds 12 4 0 */
    {9, 12, 77, "AUmJQAABAIBQAAAQzA"}, /* 64: 9x12/17, rounds 15 4 0 */
    {9, 12, 87, "iggQACIIAJCAQABCAq"}, /* 65: 9x12/17, rounds 9 5 0 */
    {10, 12, 106, "gUAgBigBBAABAFAAkCgh"}, /* 66: 10x12/19, rounds 9 3 0 */
    {10, 12, 13, "AAAAEAgVGAAogYAoGMQA"}, /* 67: 10x12/19, rounds 13 3 0 */
    {10, 12, 106, "gAEECCYgAIhAAAGQgCRg"}, /* 68: 10x12/19, rounds 10 4 0 */
    {10, 12, 108, "AAAEAAwJiYgIYgJCgQAA"}, /* 69: 10x12/19, rounds 10 5 0 */
    {10, 12, 16, "AAEBAwACFiBghABgEwAC"}, /* 70: 10x12/19, rounds 17 5 0 */
    {10, 13, 114, "AkAAhEDBABIQBARAAAQ4EE"}, /* 71: 10x13/20, rounds 10 3 1 */
    {10, 13, 26, "ABAhACYJAARgQiACkAADQA"}, /* 72: 10x13/20, rounds 15 4 0 */
    {10, 13, 32, "IkEAIgAQEBAEgjBAASANAA"}, /* 73: 10x13/20, rounds 19 4 0 */
    {10, 13, 16, "QgECABGAAQgAPA0EAACECA"}, /* 74: 10x13/20, rounds 12 5 0 */
    {10, 13, 77, "JJAFCAIAEBAAICABAEKAJJ"}, /* 75: 10x13/20, rounds 12 6 0, in twins */
    {10, 13, 42, "GgCBkAAIAAIABAAoExAhMC"}, /* 76: 10x13/21, rounds 13 4 0 */
    {10, 13, 48, "QJxECAAQABAAGCGAALAUBA"}, /* 77: 10x13/21, rounds 15 4 0 */
    {10, 13, 74, "AACCAIlggAAiQIABQKAY0A"}, /* 78: 10x13/21, rounds 13 5 0 */
    {10, 13, 47, "AAABwgAGUAEACBFyGAEABI"}, /* 79: 10x13/21, rounds 14 6 0 */
    {10, 13, 64, "SQAAAAZaAAAAQBBEsBgiBA"}, /* 80: 10x13/21, rounds 20 7 0 */
    {10, 14, 108, "EDAoAKBgABgQQJABACgEEARC"}, /* 81: 10x14/23, rounds 11 4 0 */
    {10, 14, 128, "CMwIAChAwkAQZwAABBACAACA"}, /* 82: 10x14/23, rounds 20 4 0 */
    {10, 14, 44, "EBMAQCEBAQkYKCABACAEwEAC"}, /* 83: 10x14/23, rounds 18 5 0 */
    {10, 14, 26, "iAAhABEAIQMAIwAECgKAIRBC"}, /* 84: 10x14/23, rounds 13 6 0 */
    {10, 14, 113, "KBEImICEBAAgAgKDJAAABgAC"}, /* 85: 10x14/23, rounds 13 7 0 */
    {11, 14, 111, "ICCCwQFACEGQEBAAACBQgAFUAE"}, /* 86: 11x14/25, rounds 13 5 0 */
    {11, 14, 105, "IIIgiAACAEgQKBSDAAxAACABYE"}, /* 87: 11x14/25, rounds 17 5 0 */
    {11, 14, 52, "QAAAgDASDQACAECIJAYBEwSKAA"}, /* 88: 11x14/25, rounds 12 6 0 */
    {11, 14, 34, "AAcAAAKCEiEABYLgABogiEAIAA"}, /* 89: 11x14/25, rounds 19 6 0 */
    {11, 14, 140, "CAAWMgRFgAABABFCxCCMACAAAA"}, /* 90: 11x14/25, rounds 20 7 0 */
    {11, 15, 123, "koQgAAQAAilCgECUwAAABAgSAhgA"}, /* 91: 11x15/27, rounds 13 5 0 */
    {11, 15, 28, "VUAACAggIBEAAACYAhiiAQFAwhAA"}, /* 92: 11x15/27, rounds 15 6 0 */
    {11, 15, 15, "AgCAAMSgAEBI8qASACAAAQCBA4AB"}, /* 93: 11x15/27, rounds 18 6 0 */
    {11, 15, 117, "ACCQCSCCIgAhCAAXgABASQCBICIA"}, /* 94: 11x15/27, rounds 19 7 0 */
    {11, 15, 67, "AAQzASBIMQBgAAEQgAAEQQsAAjEA"}, /* 95: 11x15/27, rounds 23 7 0 */
    {12, 15, 104, "JAAgIAAIyEAwgRAACwSgiJhIAQAAMA"}, /* 96: 12x15/30, rounds 15 6 0 */
    {12, 15, 118, "AAiUAgAjgAIAFiMIEAAHBACEBkEAJA"}, /* 97: 12x15/30, rounds 17 6 0 */
    {12, 15, 121, "CBUiBRASIAIIAAAASBgQAZoAABIISI"}, /* 98: 12x15/30, rounds 23 7 0 */
    {12, 15, 11, "AAAAMgYCgEJBAyIJAAQgGggAYACCAa"}, /* 99: 12x15/30, rounds 19 8 0 */
    {12, 15, 65, "EAoIBIAkAADCAgTyBAQwAAJAEgEFAI"}, /* 100: 12x15/30, rounds 19 10 0, in twins */
    {12, 16, 101, "aAQRgCCIgCCBUQAEIAAIAAABCFggiCLQ"}, /* 101: 12x16/33, rounds 22 6 0 */
    {12, 16, 75, "QIgCiwwICAgQDECAIKYIAEARQQCAAAkg"}, /* 102: 12x16/33, rounds 21 6 0 */
    {12, 16, 165, "AQAkBAQUAABDEByUAwQCSCTAIgBCgCAA"}, /* 103: 12x16/33, rounds 27 7 0 */
    {12, 16, 164, "EAoQAAIABrKDEAJAQAkAQlCCSAABAAYS"}, /* 104: 12x16/33, rounds 23 8 0 */
    {12, 16, 169, "CAggQoSAIAFTEQhEgRKSRAECAAAAAQII"}, /* 105: 12x16/33, rounds 27 9 0 */
    {12, 18, 101, "AICAIAJYBhAgMFEMAgJABACABMAQAmwCBEQE"}, /* 106: 12x18/37, rounds 16 7 0 */
    {12, 18, 198, "IAhBAAGwACIBUIEQAkSEgoABAAsBB4AACAIo"}, /* 107: 12x18/37, rounds 25 7 0 */
    {12, 18, 183, "AiIEIACQAQBBFAMJBAIEGIgACCMtAQAlCBAA"}, /* 108: 12x18/37, rounds 19 8 0 */
    {12, 18, 42, "CAJQQEBECQGICGAAADAIGVEA0AAQwAECAUQg"}, /* 109: 12x18/37, rounds 25 9 0 */
    {12, 18, 73, "ocKQggFUhgQAABQACBAHkAhAIAUCAQAAQkAA"}, /* 110: 12x18/37, rounds 22 10 0 */
    {13, 18, 72, "EoKBACUCAIIAAIQTBEwUACgCAMQAoEAABBxgACo"}, /* 111: 13x18/41, rounds 20 7 0 */
    {13, 18, 140, "FEgRAAEUABEUCCCMBEikhghAABAAgRAAQAIAiIy"}, /* 112: 13x18/41, rounds 20 8 0 */
    {13, 18, 96, "BLJQARBABMAAEIAAIoIAIDgAAUiABkgACDQBAkl"}, /* 113: 13x18/41, rounds 23 8 0 */
    {13, 18, 183, "IQBAGiABxAAIrIGAABUgAgAAFDSUBAgEAgEAABH"}, /* 114: 13x18/41, rounds 27 9 0 */
    {13, 18, 79, "ECECACAAaiEgAQAglYCkgBIAwIAAGogKBEBBAAM"}, /* 115: 13x18/41, rounds 32 11 0 */
    {13, 20, 161, "KYgICACBAIABEwSLFAAFBAoQCwAAAAIAJIBKAorAADCA"}, /* 116: 13x20/46, rounds 18 8 0 */
    {13, 20, 258, "CoAAAAxCIFEAEREGEBIAAAIwCIQCAQgBhWGggAEQEIZA"}, /* 117: 13x20/46, rounds 25 8 0 */
    {13, 20, 237, "CYAAIgAEAFhIQQIkAHGgAKEyQAkEIQoWRAABAABCEBAA"}, /* 118: 13x20/46, rounds 30 9 0 */
    {13, 20, 229, "gAAECgIAQDBFJCgwBBQAYAAQdAhhQCAgQiVAUAQCBAgA"}, /* 119: 13x20/46, rounds 30 9 0 */
    {13, 20, 178, "AgUDCQAKjIQIAIAEEQQCTAsEBAgCgAoCAIUECQDCQAQA"}, /* 120: 13x20/46, rounds 49 13 0 */
    {14, 20, 233, "EuQAEAAEDgAAYQIoAIIBAIAGJQChhgBCQDKSgCAAQAoAAJD"}, /* 121: 14x20/50, rounds 30 8 0 */
    {14, 20, 119, "gCUIEBBQADAYwEEFBACAEQAEAQSskBAABApIAEJZIACCwEA"}, /* 122: 14x20/50, rounds 34 8 0 */
    {14, 20, 142, "0gAQAEQAAxYEJkIIQGgAAQKAAAMAoQAEBQAoUghYAhCACQM"}, /* 123: 14x20/50, rounds 28 9 0 */
    {14, 20, 236, "AABIgJCgmARgQEEAAAggJQIUCCQUAKgAAECVIAiBgACRE6A"}, /* 124: 14x20/50, rounds 39 9 0 */
    {14, 20, 39, "EogLAABgKgAQAFAAhJwQFAQgAAqwAZIAAKgAQAVAIAAdQBC"}, /* 125: 14x20/50, rounds 34 11 0, in twins */
    {14, 22, 32, "AMYAKAoCAEIIoCQApdAADAAADgAAkAhViAiYDgAACEFUoGAASAQA"}, /* 126: 14x22/57, rounds 32 9 0 */
    {14, 22, 48, "QgAcwGCQAAQVgwIAAJRFICoEQAABiAiCAiGEIgQggBVgAlBAAAAC"}, /* 127: 14x22/57, rounds 39 9 0 */
    {14, 22, 51, "IsgAgAAEgAEgACMQhiggAowCAABADEEAgIZhAkSQgGBIiCBCIJoA"}, /* 128: 14x22/57, rounds 28 11 0 */
    {14, 22, 232, "AECIyMABlAQIJDiASAgAhIIAoGIoIAQghAALAmDIAAAECSCEAIEC"}, /* 129: 14x22/57, rounds 31 12 0 */
    {14, 22, 162, "ICAgpEBBAJ0JLAgABABCAQAABhMgQQJBhDCAJESiIQg6AIAAQAAC"}, /* 130: 14x22/57, rounds 34 14 0 */
    {15, 22, 261, "BAlCgSACEADIAEAAAgIqwAAgRCFgAAi0cAAoCCBACImhAQQABaEQYqE"}, /* 131: 15x22/62, rounds 36 10 0 */
    {15, 22, 298, "QJgCAgKAaCBJJMICgAAQgMRIAABDSgSABICADGICgAggCrQggAASABU"}, /* 132: 15x22/62, rounds 39 10 0 */
    {15, 22, 140, "KAAEDAwAEIAQ0gBIRkIAD4AgIJCAUBgwAEQhmAQIABEAAAloBQxWBAA"}, /* 133: 15x22/62, rounds 58 10 0 */
    {15, 22, 255, "AIgSACYSAhAQRBMBAQAoYAARAEIETgAAyCSsAJAIgwAQIACShCBAAeo"}, /* 134: 15x22/62, rounds 28 13 0 */
    {15, 22, 216, "EIQI2SEAHARCRhQCiAIAIqCIBAQQAVCYAAADASgAFAAgIUgEAESTCgA"}, /* 135: 15x22/62, rounds 42 16 0 */
    {16, 24, 44, "EYYYAiAABAEopMgwCCAAC0oBQIQqIAIJAAAREAgYBaAoAAAQJAA0MSBAIMgAwECC"}, /* 136: 16x24/72, rounds 35 11 0 */
    {16, 24, 147, "xThQAAAwIQhRZQAAgECAkCACCESIEGoKgIAAAAYDgAEICIIEEQhCiAgQhAQ0JAAw"}, /* 137: 16x24/72, rounds 36 12 0 */
    {16, 24, 270, "BAkAIAAEkkiARQAApRwAQQCRxJQCQRAkMCIEANIphQAGAEhECEABICCAEgCABKEA"}, /* 138: 16x24/72, rounds 36 12 0 */
    {16, 24, 170, "AlAEKoCAItABAFACKhskMBgQICAABFIAIEUQAImgUBICiBABFCEABSBAAABgQAJk"}, /* 139: 16x24/72, rounds 52 13 0 */
    {16, 24, 311, "AigQqgFhAQCgBEQACAlEBVCCgsMpBAAASAmIAQAAQIUlwACDAIAAAAADEQA2QpCA"}, /* 140: 16x24/72, rounds 49 15 0 */
    {16, 26, 34, "AAAIiBIhgBIAIMAiICEEAJBBGCRHwgMGAAFIiASBEQIIIICRQApACAAkQIQKdAKAoCaDQA"}, /* 141: 16x26/82, rounds 34 12 0 */
    {16, 26, 182, "YELAAAgI5rBJAikCCACJAGKMCIUAEAAAAAMkQkgAJkIJUATRKIQYgIAJgEAEAABElQGAAA"}, /* 142: 16x26/82, rounds 50 12 0 */
    {16, 26, 93, "EBIAlCYBECVILEEAAUICEINAIBJBigqAAAiZApCQiAQEAAgGAIAADiAIKohQIAohACNkAA"}, /* 143: 16x26/82, rounds 47 13 0 */
    {16, 26, 147, "AaAoIYBBlxACEBTINCCmEAgiAAAgAAjAAGZIACSSiQoUgaABCQBBBAwAAAAwCEEAKUiAYA"}, /* 144: 16x26/82, rounds 48 15 0 */
    {16, 26, 233, "AEIBIQsIQGgIAAKAEESAI0CGEEARkgEEJA2IYEAAMghZgFEAABIAVIAIAIggMCEhAABFrB"}, /* 145: 16x26/82, rounds 49 17 0 */
    {16, 30, 110, "RggIAiAJgggBgUBACIBIEAQiEigoGCEDREEDogCAhUACAIhgKimdgIAABECCIEFHUYAElUgAgNEAAkAB"}, /* 146: 16x30/99, rounds 61 14 0 */
    {16, 30, 456, "KIAIKoMSAABAGhCVACKEMQDKYEgACAAAIaoXCgAgINACyBQQhYgCAooIAAKiJCEQGDIIAJYAoAFCwABA"}, /* 147: 16x30/99, rounds 68 14 0 */
    {16, 30, 398, "BigDAAIERkAGFEBLQCIQKEQmAoIACQAwAmgkBhkAQEAOQYQIyUABEgghISIIDEAFAAAiYoWDAAIFgA0A"}, /* 148: 16x30/99, rounds 67 15 0 */
    {16, 30, 49, "iIQA0ACkIWAEAgEAQByLEQIBwAAgAAkU0EBsQNBsWACGUEAISZBAAwCBwAgAQGEQggAEiAiEEBYQIg4g"}, /* 149: 16x30/99, rounds 56 19 0 */
    {16, 30, 45, "OAASGACAAI44QEQUKgIISAYoEBDKBAwFABAiCASKUSAQRAgAoDAgUwgIFGASEEBUKCICHHEAAQAYSAAc"}, /* 150: 16x30/100, rounds 70 20 0, in twins */
};
