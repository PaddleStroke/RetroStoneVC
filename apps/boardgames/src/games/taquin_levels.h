/*
 * The 15 Puzzle's levels: our own, made by tools/taquin_gen.c (trays slid out of order by chance, the exact
 * par found by the rules' search, six chapters of 25 from the easiest). Each: 'n' numbered tiles or 'p' cut
 * from the picture, then the rows (tiles 1..9 a..o, '.' the gap); in the comment its par, the search's first
 * bound and the positions it looked at; the pars again in tq_level_par (the tests search them again).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const tq_levels[TQ_LEVELS] = {
    "n:12.|453|786", /* 1: par 2, bound 2, 2 looked at */
    "n:.23|145|786", /* 2: par 4, bound 4, 4 looked at */
    "n:2.3|146|758", /* 3: par 5, bound 5, 5 looked at */
    "n:23.|146|758", /* 4: par 6, bound 6, 6 looked at */
    "n:152|743|.86", /* 5: par 6, bound 6, 6 looked at */
    "n:253|176|4.8", /* 6: par 7, bound 7, 7 looked at */
    "n:243|.16|758", /* 7: par 7, bound 7, 7 looked at */
    "n:236|1.5|478", /* 8: par 8, bound 8, 8 looked at */
    "n:413|268|75.", /* 9: par 8, bound 8, 10 looked at */
    "n:235|18.|476", /* 10: par 9, bound 9, 9 looked at */
    "n:236|148|7.5", /* 11: par 9, bound 9, 9 looked at */
    "n:152|74.|863", /* 12: par 9, bound 9, 9 looked at */
    "n:51.|263|478", /* 13: par 10, bound 10, 10 looked at */
    "n:412|8.3|576", /* 14: par 10, bound 10, 15 looked at */
    "n:.43|715|826", /* 15: par 10, bound 10, 10 looked at */
    "n:536|21.|478", /* 16: par 11, bound 11, 11 looked at */
    "n:3.6|215|478", /* 17: par 11, bound 11, 11 looked at */
    "n:236|18.|457", /* 18: par 11, bound 11, 21 looked at */
    "n:236|1.7|548", /* 19: par 12, bound 12, 13 looked at */
    "n:712|843|.65", /* 20: par 12, bound 12, 12 looked at */
    "n:2.5|136|748", /* 21: par 11, bound 9, 21 looked at */
    "n:16.|572|483", /* 22: par 12, bound 10, 15 looked at */
    "n:423|581|7.6", /* 23: par 11, bound 7, 33 looked at */
    "n:412|756|.38", /* 24: par 12, bound 8, 38 looked at */
    "n:513|468|72.", /* 25: par 12, bound 8, 37 looked at */
    "p:742|.15|863", /* 26: par 13, bound 13, 14 looked at */
    "p:741|5.3|826", /* 27: par 14, bound 12, 38 looked at */
    "p:.15|836|427", /* 28: par 14, bound 12, 23 looked at */
    "p:236|.48|517", /* 29: par 15, bound 13, 52 looked at */
    "p:36.|154|728", /* 30: par 16, bound 14, 81 looked at */
    "p:352|4.6|178", /* 31: par 16, bound 12, 122 looked at */
    "p:23.|185|647", /* 32: par 16, bound 12, 80 looked at */
    "p:284|7.3|561", /* 33: par 18, bound 16, 37 looked at */
    "p:471|832|6.5", /* 34: par 19, bound 17, 34 looked at */
    "p:415|3.8|276", /* 35: par 18, bound 14, 178 looked at */
    "p:157|23.|486", /* 36: par 17, bound 11, 138 looked at */
    "p:815|742|3.6", /* 37: par 19, bound 15, 100 looked at */
    "p:462|7.1|853", /* 38: par 18, bound 12, 147 looked at */
    "p:236|.14|875", /* 39: par 19, bound 13, 537 looked at */
    "p:354|16.|278", /* 40: par 19, bound 13, 380 looked at */
    "p:568|.32|147", /* 41: par 21, bound 17, 258 looked at */
    "p:256|18.|473", /* 42: par 19, bound 11, 484 looked at */
    "p:862|743|15.", /* 43: par 20, bound 14, 190 looked at */
    "p:628|715|4.3", /* 44: par 21, bound 15, 360 looked at */
    "p:832|54.|176", /* 45: par 21, bound 15, 619 looked at */
    "p:651|.42|873", /* 46: par 21, bound 15, 282 looked at */
    "p:326|41.|587", /* 47: par 21, bound 13, 521 looked at */
    "p:.46|217|583", /* 48: par 22, bound 16, 940 looked at */
    "p:413|.82|567", /* 49: par 21, bound 11, 1170 looked at */
    "p:263|481|5.7", /* 50: par 21, bound 11, 637 looked at */
    "n:1348|627c|5ab.|9def", /* 51: par 11, bound 11, 11 looked at */
    "n:2534|1a68|9e7c|.dbf", /* 52: par 11, bound 11, 11 looked at */
    "n:5124|6.38|9a7f|decb", /* 53: par 12, bound 12, 12 looked at */
    "n:5.24|6137|9bc8|daef", /* 54: par 13, bound 13, 22 looked at */
    "n:123.|6a74|5dc8|9ebf", /* 55: par 13, bound 11, 23 looked at */
    "n:7524|1638|.9bc|daef", /* 56: par 14, bound 12, 22 looked at */
    "n:.238|1647|59ac|debf", /* 57: par 14, bound 10, 39 looked at */
    "n:1364|9278|.a5c|debf", /* 58: par 14, bound 10, 41 looked at */
    "n:5123|.a74|db68|e9fc", /* 59: par 17, bound 15, 41 looked at */
    "n:1374|528c|6b.f|a9de", /* 60: par 18, bound 18, 34 looked at */
    "n:6148|5327|d9bc|a.ef", /* 61: par 18, bound 16, 32 looked at */
    "n:51.2|9673|ebf4|adc8", /* 62: par 20, bound 20, 28 looked at */
    "n:2347|16bf|5da.|e9c8", /* 63: par 21, bound 21, 35 looked at */
    "n:1248|d953|e67c|ba.f", /* 64: par 21, bound 19, 27 looked at */
    "n:2614|5b37|e.a8|9dfc", /* 65: par 21, bound 17, 178 looked at */
    "n:7523|9184|d6ca|ef.b", /* 66: par 23, bound 23, 27 looked at */
    "n:6514|3f27|d9b8|.aec", /* 67: par 23, bound 21, 141 looked at */
    "n:7248|5a.3|1e6b|9dfc", /* 68: par 23, bound 19, 343 looked at */
    "n:3248|1.7c|69fe|5dab", /* 69: par 24, bound 22, 71 looked at */
    "n:1642|7ba3|5d8c|e9.f", /* 70: par 25, bound 23, 149 looked at */
    "n:9623|1874|d5ac|e.bf", /* 71: par 24, bound 20, 542 looked at */
    "n:51.3|a624|7dc8|9ebf", /* 72: par 24, bound 18, 868 looked at */
    "n:5.18|6732|ea4b|9dfc", /* 73: par 25, bound 19, 495 looked at */
    "n:13.8|254f|6d7c|9eab", /* 74: par 26, bound 20, 480 looked at */
    "n:2634|1957|a.b8|dcef", /* 75: par 25, bound 15, 1525 looked at */
    "p:13b6|e524|adf7|9.c8", /* 76: par 26, bound 24, 109 looked at */
    "p:6273|a1b4|d9f8|5e.c", /* 77: par 25, bound 19, 850 looked at */
    "p:1673|9254|.dac|ef8b", /* 78: par 26, bound 20, 1729 looked at */
    "p:5123|7.84|a6db|ef9c", /* 79: par 26, bound 20, 1041 looked at */
    "p:148c|62e3|5.a7|9dfb", /* 80: par 27, bound 21, 1650 looked at */
    "p:1678|423.|a5fb|9dec", /* 81: par 28, bound 22, 2571 looked at */
    "p:5124|da67|f.38|c9eb", /* 82: par 29, bound 23, 770 looked at */
    "p:2547|1.3c|abef|9d68", /* 83: par 30, bound 24, 1487 looked at */
    "p:6548|127c|9f.e|d3ba", /* 84: par 30, bound 22, 6965 looked at */
    "p:2483|57a1|e6f.|9dcb", /* 85: par 31, bound 25, 3053 looked at */
    "p:5ca2|d913|.647|eb8f", /* 86: par 34, bound 32, 211 looked at */
    "p:34.8|62eb|91ac|57df", /* 87: par 32, bound 26, 982 looked at */
    "p:51c.|f243|96b7|dae8", /* 88: par 31, bound 21, 8481 looked at */
    "p:587c|2.1f|63b4|9dae", /* 89: par 34, bound 30, 1763 looked at */
    "p:2478|63.e|91fb|ad5c", /* 90: par 33, bound 25, 6880 looked at */
    "p:5248|693e|df.1|a7cb", /* 91: par 34, bound 28, 6173 looked at */
    "p:1a43|26c8|d5ef|9.7b", /* 92: par 34, bound 26, 14007 looked at */
    "p:7243|1ac8|.5e6|9bdf", /* 93: par 34, bound 24, 20118 looked at */
    "p:6149|a527|db.8|ef3c", /* 94: par 34, bound 24, 10832 looked at */
    "p:a1.3|f528|947c|db6e", /* 95: par 34, bound 24, 24409 looked at */
    "p:6384|527f|a9eb|1.dc", /* 96: par 34, bound 24, 22230 looked at */
    "p:a537|1e84|.926|fdcb", /* 97: par 36, bound 28, 6609 looked at */
    "p:96e4|25f7|13.b|dac8", /* 98: par 36, bound 26, 21961 looked at */
    "p:a634|72.8|f15b|de9c", /* 99: par 35, bound 23, 95041 looked at */
    "p:2583|6.7c|e1af|9d4b", /* 100: par 36, bound 24, 61931 looked at */
    "n:2.58|1c4f|d6a7|b93e", /* 101: par 35, bound 29, 5864 looked at */
    "n:a523|1647|.bfc|e98d", /* 102: par 34, bound 24, 2916 looked at */
    "n:6581|9b4.|ed2f|ac37", /* 103: par 38, bound 34, 14806 looked at */
    "n:6.47|b53c|192e|da8f", /* 104: par 35, bound 25, 10231 looked at */
    "n:62a4|d5b3|91.7|fe8c", /* 105: par 36, bound 26, 8036 looked at */
    "n:9532|8fa4|1.67|dbce", /* 106: par 39, bound 31, 31887 looked at */
    "n:6394|81ec|a5.7|2dbf", /* 107: par 38, bound 28, 27467 looked at */
    "n:3518|6a4f|2.b7|9cde", /* 108: par 39, bound 29, 27758 looked at */
    "n:63c.|5b84|172a|9dfe", /* 109: par 39, bound 27, 163061 looked at */
    "n:8356|4eb2|a7c.|19df", /* 110: par 41, bound 33, 3009 looked at */
    "n:3d56|1.4a|7f28|be9c", /* 111: par 42, bound 34, 60770 looked at */
    "n:2594|.d18|fba3|7e6c", /* 112: par 41, bound 31, 112960 looked at */
    "n:9523|46c8|e.1f|bda7", /* 113: par 41, bound 29, 194591 looked at */
    "n:6.f8|1e42|7a3d|95cb", /* 114: par 43, bound 35, 80149 looked at */
    "n:3f78|.456|21dc|eb9a", /* 115: par 43, bound 33, 99262 looked at */
    "n:84.1|62b7|daf3|5e9c", /* 116: par 42, bound 28, 334819 looked at */
    "n:514e|92.f|37bd|a6c8", /* 117: par 43, bound 31, 354459 looked at */
    "n:1465|db37|e2.c|98fa", /* 118: par 42, bound 28, 343774 looked at */
    "n:27a.|8cb9|1f35|d6e4", /* 119: par 45, bound 35, 17247 looked at */
    "n:34f5|2ba8|16cd|e.97", /* 120: par 44, bound 32, 77632 looked at */
    "n:73.9|5d46|21fb|ea8c", /* 121: par 44, bound 30, 309794 looked at */
    "n:3178|db9c|24.5|6fae", /* 122: par 46, bound 36, 401501 looked at */
    "n:5d2b|a317|e86c|9f.4", /* 123: par 45, bound 31, 310393 looked at */
    "n:852a|17b.|de94|fc36", /* 124: par 46, bound 34, 701659 looked at */
    "n:b239|68.1|5aec|df47", /* 125: par 45, bound 27, 1222498 looked at */
    "n:612.4|ba385|g7d9f|hcieo|lmnkj", /* 126: par 27, bound 25, 52 looked at */
    "p:61834|2c795|nghea|bdmjf|lio.k", /* 127: par 29, bound 29, 104 looked at */
    "n:61e45|2839.|b7dfa|hcioj|glmnk", /* 128: par 27, bound 21, 139 looked at */
    "p:23984|.675a|1cdef|bgjko|lhmin", /* 129: par 29, bound 25, 225 looked at */
    "n:61239|78n54|bdfa.|gcmei|lhokj", /* 130: par 32, bound 32, 49 looked at */
    "p:1.745|632ea|gc98f|hbjdi|lmnok", /* 131: par 29, bound 23, 5319 looked at */
    "n:26349|b17.a|ch8d5|gmief|lnjok", /* 132: par 30, bound 24, 403 looked at */
    "p:61745|3c289|gbdfa|lomei|n.hkj", /* 133: par 33, bound 33, 65 looked at */
    "n:13745|628a.|bch9f|gldek|mnjio", /* 134: par 29, bound 19, 5979 looked at */
    "p:28345|1ced9|6bi7a|gnhjf|ml.ok", /* 135: par 32, bound 26, 1817 looked at */
    "n:7185a|2ce3.|bdi4f|h6k9o|glmnj", /* 136: par 35, bound 33, 142 looked at */
    "p:1.g34|c2895|76jea|bhdio|lmnkf", /* 137: par 33, bound 27, 1730 looked at */
    "n:273d4|1958a|6cief|bhnjk|g.lmo", /* 138: par 33, bound 25, 1785 looked at */
    "p:23745|1b.ej|c68a9|mhdfk|glino", /* 139: par 35, bound 29, 3800 looked at */
    "n:2715a|6.34f|c9dej|b8hoi|glmnk", /* 140: par 36, bound 30, 2514 looked at */
    "p:61584|b2d3a|g79ce|mh.jf|nliok", /* 141: par 37, bound 31, 1997 looked at */
    "n:238f4|17a95|6bdek|jhcio|gl.mn", /* 142: par 38, bound 32, 1051 looked at */
    "p:139.5|b248f|g6caj|7mndi|lhoke", /* 143: par 39, bound 35, 5109 looked at */
    "n:.125a|b6839|hd7ie|cgf4k|lmjno", /* 144: par 38, bound 30, 4796 looked at */
    "p:1d235|6i74a|c8n9e|lb.gf|hmkoj", /* 145: par 39, bound 31, 14278 looked at */
    "n:b69c3|18e54|72df.|gimoa|lhnkj", /* 146: par 42, bound 38, 767 looked at */
    "p:b6745|.213f|gi8ae|l9ckd|mhnjo", /* 147: par 41, bound 35, 8202 looked at */
    "n:124a5|683j9|l7cde|ibhf.|gmnok", /* 148: par 39, bound 27, 44477 looked at */
    "p:723a4|1cd85|6mn9e|hlj.o|bigkf", /* 149: par 42, bound 34, 8732 looked at */
    "n:71283|c.a54|b6d9e|lgfjk|ihmno", /* 150: par 42, bound 32, 21580 looked at */
};

const uint8_t tq_level_par[TQ_LEVELS] = {
    2, 4, 5, 6, 6, 7, 7, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 11, 12, 11, 12, 12,
    13, 14, 14, 15, 16, 16, 16, 18, 19, 18, 17, 19, 18, 19, 19, 21, 19, 20, 21, 21, 21, 21, 22, 21, 21,
    11, 11, 12, 13, 13, 14, 14, 14, 17, 18, 18, 20, 21, 21, 21, 23, 23, 23, 24, 25, 24, 24, 25, 26, 25,
    26, 25, 26, 26, 27, 28, 29, 30, 30, 31, 34, 32, 31, 34, 33, 34, 34, 34, 34, 34, 34, 36, 36, 35, 36,
    35, 34, 38, 35, 36, 39, 38, 39, 39, 41, 42, 41, 41, 43, 43, 42, 43, 42, 45, 44, 44, 46, 45, 46, 45,
    27, 29, 27, 29, 32, 29, 30, 33, 29, 32, 35, 33, 33, 35, 36, 37, 38, 39, 38, 39, 42, 41, 39, 42, 42,
};
