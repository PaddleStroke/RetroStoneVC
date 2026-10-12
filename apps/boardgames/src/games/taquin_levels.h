/*
 * The 15 Puzzle's levels: our own, made by tools/taquin_gen.c (trays slid out of order by chance, the exact
 * par found by the rules' search, six chapters of 25 from the easiest). Each: 'n' numbered tiles or 'p' cut
 * from the picture, then the rows (tiles 1..9 a..o, '.' the gap); in the comment its par, the search's first
 * bound and the positions it looked at; the pars again in tq_level_par (the tests search them again).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const tq_levels[TQ_LEVELS] = {
    "n:12.|453|786", /* 1: par 2, bound 2, 2 looked at */
    "n:.13|425|786", /* 2: par 4, bound 4, 4 looked at */
    "n:412|.53|786", /* 3: par 5, bound 5, 5 looked at */
    "n:253|1.6|478", /* 4: par 6, bound 6, 6 looked at */
    "n:152|483|76.", /* 5: par 6, bound 6, 6 looked at */
    "n:253|176|4.8", /* 6: par 7, bound 7, 7 looked at */
    "n:1.5|432|786", /* 7: par 7, bound 7, 8 looked at */
    "n:236|1.5|478", /* 8: par 8, bound 8, 8 looked at */
    "n:.42|513|786", /* 9: par 8, bound 8, 11 looked at */
    "n:235|18.|476", /* 10: par 9, bound 9, 9 looked at */
    "n:162|453|.78", /* 11: par 8, bound 6, 12 looked at */
    "n:123|5.7|486", /* 12: par 8, bound 6, 15 looked at */
    "n:253|7.6|148", /* 13: par 10, bound 10, 21 looked at */
    "n:123|587|4.6", /* 14: par 9, bound 7, 17 looked at */
    "n:415|3.2|786", /* 15: par 10, bound 10, 14 looked at */
    "n:162|.38|475", /* 16: par 11, bound 11, 11 looked at */
    "n:.52|136|478", /* 17: par 10, bound 8, 23 looked at */
    "n:253|1.7|486", /* 18: par 10, bound 8, 27 looked at */
    "n:3.5|126|478", /* 19: par 11, bound 9, 21 looked at */
    "n:162|748|.35", /* 20: par 12, bound 12, 20 looked at */
    "n:412|638|.75", /* 21: par 12, bound 12, 12 looked at */
    "n:123|86.|457", /* 22: par 11, bound 7, 36 looked at */
    "n:135|7.6|248", /* 23: par 12, bound 10, 30 looked at */
    "n:12.|468|735", /* 24: par 12, bound 8, 43 looked at */
    "n:.12|436|578", /* 25: par 12, bound 8, 43 looked at */
    "p:182|6.3|475", /* 26: par 12, bound 10, 20 looked at */
    "p:.13|478|625", /* 27: par 14, bound 12, 25 looked at */
    "p:.43|271|586", /* 28: par 14, bound 12, 36 looked at */
    "p:628|13.|475", /* 29: par 15, bound 13, 19 looked at */
    "p:125|463|7.8", /* 30: par 13, bound 5, 132 looked at */
    "p:123|85.|647", /* 31: par 15, bound 9, 112 looked at */
    "p:126|438|5.7", /* 32: par 15, bound 9, 121 looked at */
    "p:71.|465|832", /* 33: par 18, bound 16, 36 looked at */
    "p:842|136|5.7", /* 34: par 17, bound 13, 123 looked at */
    "p:563|128|4.7", /* 35: par 17, bound 11, 238 looked at */
    "p:136|475|28.", /* 36: par 16, bound 8, 187 looked at */
    "p:182|756|34.", /* 37: par 18, bound 12, 150 looked at */
    "p:642|178|5.3", /* 38: par 19, bound 15, 201 looked at */
    "p:874|651|.32", /* 39: par 22, bound 22, 22 looked at */
    "p:631|.25|478", /* 40: par 19, bound 13, 182 looked at */
    "p:853|1.7|246", /* 41: par 20, bound 14, 507 looked at */
    "p:417|532|.68", /* 42: par 20, bound 14, 172 looked at */
    "p:741|38.|526", /* 43: par 21, bound 17, 128 looked at */
    "p:628|715|4.3", /* 44: par 21, bound 15, 360 looked at */
    "p:.76|132|458", /* 45: par 20, bound 12, 577 looked at */
    "p:.25|743|618", /* 46: par 20, bound 12, 561 looked at */
    "p:1.6|384|725", /* 47: par 21, bound 13, 417 looked at */
    "p:5.3|148|672", /* 48: par 21, bound 13, 520 looked at */
    "p:427|1.6|583", /* 49: par 22, bound 14, 416 looked at */
    "p:523|86.|471", /* 50: par 21, bound 11, 768 looked at */
    "n:.234|1a78|65bc|9def", /* 51: par 10, bound 10, 10 looked at */
    "n:2534|1a68|9e7c|.dbf", /* 52: par 11, bound 11, 11 looked at */
    "n:2634|1.78|5dac|e9bf", /* 53: par 12, bound 12, 15 looked at */
    "n:5.24|6137|9bc8|daef", /* 54: par 13, bound 13, 22 looked at */
    "n:1234|5678|ed.a|9bfc", /* 55: par 12, bound 10, 20 looked at */
    "n:1348|527c|6.bf|9dae", /* 56: par 15, bound 15, 15 looked at */
    "n:6248|1a53|9.7b|defc", /* 57: par 15, bound 13, 40 looked at */
    "n:53.8|9147|62ab|defc", /* 58: par 16, bound 16, 19 looked at */
    "n:238.|164c|597f|deab", /* 59: par 17, bound 17, 27 looked at */
    "n:1834|526c|9a.f|de7b", /* 60: par 16, bound 12, 65 looked at */
    "n:6234|1578|9abc|def.", /* 61: par 14, bound 4, 84 looked at */
    "n:1263|5b.4|ad87|9ecf", /* 62: par 19, bound 17, 40 looked at */
    "n:1364|5278|d9b.|aefc", /* 63: par 17, bound 9, 163 looked at */
    "n:1364|58ab|d92c|e.7f", /* 64: par 20, bound 18, 91 looked at */
    "n:1283|6a4e|5.b7|9dfc", /* 65: par 21, bound 17, 227 looked at */
    "n:7b34|15a2|9.68|defc", /* 66: par 21, bound 17, 136 looked at */
    "n:6514|3f27|d9b8|.aec", /* 67: par 23, bound 21, 141 looked at */
    "n:1734|5a82|9.c6|debf", /* 68: par 21, bound 13, 543 looked at */
    "n:3248|1.7c|69fe|5dab", /* 69: par 24, bound 22, 71 looked at */
    "n:d513|a624|.b78|e9fc", /* 70: par 24, bound 20, 230 looked at */
    "n:9623|1874|d5ac|e.bf", /* 71: par 24, bound 20, 542 looked at */
    "n:1487|326b|59.c|deaf", /* 72: par 24, bound 18, 398 looked at */
    "n:6523|9b17|.ea4|dfc8", /* 73: par 26, bound 22, 396 looked at */
    "n:283.|1da4|657b|9efc", /* 74: par 25, bound 19, 638 looked at */
    "n:5634|2.78|eadc|19bf", /* 75: par 26, bound 18, 2528 looked at */
    "p:1274|3.98|6eaf|5dcb", /* 76: par 24, bound 20, 285 looked at */
    "p:1834|56bc|d2.f|a97e", /* 77: par 26, bound 22, 596 looked at */
    "p:3574|1628|e.fb|ad9c", /* 78: par 25, bound 19, 574 looked at */
    "p:a534|2678|dbcf|19e.", /* 79: par 26, bound 20, 1229 looked at */
    "p:3274|58cf|91.e|d6ba", /* 80: par 28, bound 24, 806 looked at */
    "p:5217|a638|9b.4|defc", /* 81: par 26, bound 16, 3309 looked at */
    "p:5124|da67|f.38|c9eb", /* 82: par 29, bound 23, 770 looked at */
    "p:9548|d13a|6f2b|.7ec", /* 83: par 31, bound 27, 51 looked at */
    "p:1382|5674|9bce|d.af", /* 84: par 28, bound 16, 5292 looked at */
    "p:5137|ea84|2.fc|96bd", /* 85: par 31, bound 25, 1657 looked at */
    "p:268b|1a35|9f4.|dec7", /* 86: par 31, bound 23, 7501 looked at */
    "p:5.72|e193|ab64|cd8f", /* 87: par 33, bound 29, 937 looked at */
    "p:1478|526c|a.eb|9fd3", /* 88: par 31, bound 21, 25808 looked at */
    "p:587c|2.1f|63b4|9dae", /* 89: par 34, bound 30, 1763 looked at */
    "p:6513|c.74|d2b8|9aef", /* 90: par 32, bound 22, 18939 looked at */
    "p:5248|693e|df.1|a7cb", /* 91: par 34, bound 28, 6173 looked at */
    "p:74d2|163.|5ab8|9efc", /* 92: par 32, bound 20, 25103 looked at */
    "p:12.8|a6c3|d4bf|5e97", /* 93: par 34, bound 24, 6933 looked at */
    "p:26b3|19e7|a8c4|5.df", /* 94: par 34, bound 24, 6220 looked at */
    "p:518c|324f|976e|.adb", /* 95: par 35, bound 27, 5012 looked at */
    "p:628.|3a4c|97fb|15de", /* 96: par 35, bound 27, 4771 looked at */
    "p:27a3|698c|15bf|d4.e", /* 97: par 35, bound 25, 25489 looked at */
    "p:96e4|25f7|13.b|dac8", /* 98: par 36, bound 26, 21961 looked at */
    "p:6137|2a84|95bd|ecf.", /* 99: par 34, bound 20, 20845 looked at */
    "p:2583|6.7c|e1af|9d4b", /* 100: par 36, bound 24, 61931 looked at */
    "n:2.58|1c4f|d6a7|b93e", /* 101: par 35, bound 29, 5864 looked at */
    "n:9763|d148|.5fc|ea2b", /* 102: par 34, bound 24, 11693 looked at */
    "n:614b|7a.8|e53c|29df", /* 103: par 35, bound 25, 4962 looked at */
    "n:6.47|b53c|192e|da8f", /* 104: par 35, bound 25, 10231 looked at */
    "n:24a8|637f|.d1b|95ec", /* 105: par 36, bound 26, 15384 looked at */
    "n:2.36|9417|5dcf|eba8", /* 106: par 37, bound 27, 14686 looked at */
    "n:1.54|96d7|ae28|cb3f", /* 107: par 37, bound 25, 42291 looked at */
    "n:7836|21ec|ab.f|594d", /* 108: par 40, bound 32, 16266 looked at */
    "n:12c.|386a|9457|dbef", /* 109: par 39, bound 27, 362836 looked at */
    "n:15a2|37.4|89ef|d6cb", /* 110: par 39, bound 27, 164917 looked at */
    "n:d6.3|eb17|2f4c|598a", /* 111: par 42, bound 34, 17373 looked at */
    "n:.5a1|2738|b46f|9dce", /* 112: par 40, bound 28, 48866 looked at */
    "n:7a12|d963|.e8c|bf54", /* 113: par 42, bound 32, 20748 looked at */
    "n:a327|65d4|.1c8|9efb", /* 114: par 40, bound 26, 658183 looked at */
    "n:ba37|2d45|6.c1|98ef", /* 115: par 43, bound 33, 89052 looked at */
    "n:51f6|a43c|d827|9b.e", /* 116: par 43, bound 33, 400178 looked at */
    "n:132b|a.98|6d4c|5fe7", /* 117: par 42, bound 28, 1476256 looked at */
    "n:14b9|8527|d36e|c.af", /* 118: par 44, bound 34, 188248 looked at */
    "n:9d14|f.6e|a753|cb28", /* 119: par 46, bound 38, 27737 looked at */
    "n:1358|6.7f|2c9a|deb4", /* 120: par 42, bound 26, 1562115 looked at */
    "n:124a|9c.8|fdb3|5e67", /* 121: par 43, bound 27, 434461 looked at */
    "n:78a4|95f1|e3.2|cd6b", /* 122: par 46, bound 36, 127107 looked at */
    "n:b2f8|1d7.|3654|e9ca", /* 123: par 46, bound 34, 532481 looked at */
    "n:d547|21a8|9b3e|6cf.", /* 124: par 44, bound 28, 1069345 looked at */
    "n:32.6|498c|daf5|be17", /* 125: par 46, bound 32, 103873 looked at */
    "n:13845|6cd9a|b2.ef|l7nio|hgmkj", /* 126: par 26, bound 24, 196 looked at */
    "p:12459|683fa|b7cdj|mhioe|gl.nk", /* 127: par 26, bound 22, 170 looked at */
    "n:12345|6e7af|h.89j|gbdik|lcmno", /* 128: par 27, bound 21, 721 looked at */
    "p:1245a|6739e|gbikd|.8cno|lhfmj", /* 129: par 31, bound 31, 54 looked at */
    "n:12345|c7ead|6ih8j|gbf.9|lmnok", /* 130: par 30, bound 26, 2492 looked at */
    "p:268d4|19c35|b7iea|ghkfo|l.mnj", /* 131: par 31, bound 29, 117 looked at */
    "n:86245|173.a|dhi9f|cbjek|lgmno", /* 132: par 30, bound 24, 402 looked at */
    "p:18734|6d2e5|gi.a9|cbjhf|lmnok", /* 133: par 32, bound 30, 785 looked at */
    "n:13745|628a.|bch9f|gldek|mnjio", /* 134: par 29, bound 19, 5979 looked at */
    "p:8.c74|26a39|1hde5|bgijf|lmnok", /* 135: par 33, bound 29, 552 looked at */
    "n:1c925|734.a|6d8ef|gmbhk|linjo", /* 136: par 32, bound 24, 6289 looked at */
    "p:1.g34|c2895|76jea|bhdio|lmnkf", /* 137: par 33, bound 27, 1730 looked at */
    "n:12345|b6e7a|gc9if|.md8n|hlojk", /* 138: par 33, bound 25, 8948 looked at */
    "p:61983|b2c5f|ge7a4|hmdjk|li.no", /* 139: par 36, bound 32, 536 looked at */
    "n:134a9|62dj5|c87fk|be.io|glmhn", /* 140: par 37, bound 33, 2855 looked at */
    "p:27345|g1c9a|m68df|lbheo|.inkj", /* 141: par 36, bound 28, 16191 looked at */
    "n:263f5|174ek|bd98a|gch.i|lmnjo", /* 142: par 36, bound 26, 13209 looked at */
    "p:1245f|b683e|.7cda|lik9o|hgmnj", /* 143: par 38, bound 32, 2663 looked at */
    "n:17283|b6da5|gnf4k|ic.9o|lhmje", /* 144: par 39, bound 33, 2836 looked at */
    "p:61475|b2d3e|hgi8a|mc.j9|nlokf", /* 145: par 41, bound 37, 2057 looked at */
    "n:72.45|163k9|nb8fa|cmdie|lhgjo", /* 146: par 40, bound 32, 60576 looked at */
    "p:8.234|1b9a5|h67en|gmcfj|liodk", /* 147: par 41, bound 35, 12977 looked at */
    "n:26145|c738a|hgd9f|m.lej|nbiok", /* 148: par 40, bound 30, 105603 looked at */
    "p:23845|1g7k9|ch6ea|lb.fj|dmino", /* 149: par 41, bound 31, 33374 looked at */
    "n:61235|849.o|7hdej|bcgia|lmnkf", /* 150: par 42, bound 32, 23166 looked at */
};

const uint8_t tq_level_par[TQ_LEVELS] = {
    2, 4, 5, 6, 6, 7, 7, 8, 8, 9, 8, 8, 10, 9, 10, 11, 10, 10, 11, 12, 12, 11, 12, 12, 12,
    12, 14, 14, 15, 13, 15, 15, 18, 17, 17, 16, 18, 19, 22, 19, 20, 20, 21, 21, 20, 20, 21, 21, 22, 21,
    10, 11, 12, 13, 12, 15, 15, 16, 17, 16, 14, 19, 17, 20, 21, 21, 23, 21, 24, 24, 24, 24, 26, 25, 26,
    24, 26, 25, 26, 28, 26, 29, 31, 28, 31, 31, 33, 31, 34, 32, 34, 32, 34, 34, 35, 35, 35, 36, 34, 36,
    35, 34, 35, 35, 36, 37, 37, 40, 39, 39, 42, 40, 42, 40, 43, 43, 42, 44, 46, 42, 43, 46, 46, 44, 46,
    26, 26, 27, 31, 30, 31, 30, 32, 29, 33, 32, 33, 33, 36, 37, 36, 36, 38, 39, 41, 40, 41, 40, 41, 42,
};
