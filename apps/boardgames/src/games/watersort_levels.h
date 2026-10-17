/*
 * Water Sort's levels: our own, made by tools/watersort_gen.c (each from a theme: a shuffle, rows of every
 * colour, rainbow tubes, a mirrored rack, a Latin rack to end each chapter; the shortest way found by the
 * rules' search; six chapters of 25 from the easiest). Each: the tubes from the left ('|' between them), each
 * one's colours from the bottom (1..9 a b c); in the comment the colours, the theme, the shortest way and the
 * positions the search made; the shortest ways again in ws_level_par (the tests check them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const ws_levels[WS_LEVELS] = {
    "2121|2121||", /* 1: 2 colours, shuffle, 7 pours, 17 looked at */
    "1213|3221|3132||", /* 2: 3 colours, shuffle, 9 pours, 30 looked at */
    "3221|3131|2132||", /* 3: 3 colours, shuffle, 9 pours, 49 looked at */
    "1212|3133|2321||", /* 4: 3 colours, rows, 9 pours, 65 looked at */
    "1132|3232|1312||", /* 5: 3 colours, shuffle, 9 pours, 65 looked at */
    "2312|1123|3231||", /* 6: 3 colours, rows, 9 pours, 72 looked at */
    "2123|1331|3212||", /* 7: 3 colours, rows, 9 pours, 73 looked at */
    "1321|2113|3232||", /* 8: 3 colours, shuffle, 9 pours, 73 looked at */
    "2313|1231|3122||", /* 9: 3 colours, rows, 9 pours, 78 looked at */
    "2321|3131|2321||", /* 10: 3 colours, mirror, 10 pours, 49 looked at */
    "3231|1212|3231||", /* 11: 3 colours, mirror, 11 pours, 65 looked at */
    "1231|3232|1231||", /* 12: 3 colours, mirror, 11 pours, 72 looked at */
    "2313|1212|2313||", /* 13: 3 colours, mirror, 11 pours, 73 looked at */
    "3212|1424|1314|2334||", /* 14: 4 colours, shuffle, 12 pours, 123 looked at */
    "3242|2413|4121|1334||", /* 15: 4 colours, rows, 12 pours, 136 looked at */
    "3124|2143|2143|3124||", /* 16: 4 colours, mirror, 14 pours, 63 looked at */
    "4213|2314|1234|3142||", /* 17: 4 colours, rainbow, 13 pours, 113 looked at */
    "3121|3214|1244|3423||", /* 18: 4 colours, shuffle, 13 pours, 127 looked at */
    "2124|4313|4313|2124||", /* 19: 4 colours, mirror, 14 pours, 83 looked at */
    "3411|1412|4232|3432||", /* 20: 4 colours, shuffle, 13 pours, 173 looked at */
    "4234|2413|1322|3141||", /* 21: 4 colours, rows, 13 pours, 174 looked at */
    "1343|4212|4212|1343||", /* 22: 4 colours, mirror, 14 pours, 114 looked at */
    "3142|4132|4123|3214||", /* 23: 4 colours, rainbow, 14 pours, 130 looked at */
    "2323|4141|1234|2341||", /* 24: 4 colours, shuffle, 14 pours, 162 looked at */
    "3421|4132|2314|1243||", /* 25: 4 colours, showpiece, 13 pours, 168 looked at */
    "4213|5342|4123|1325|1455||", /* 26: 5 colours, shuffle, 16 pours, 135 looked at */
    "1214|3551|5423|4135|2342||", /* 27: 5 colours, rows, 15 pours, 218 looked at */
    "1425|5143|2323|5143|1425||", /* 28: 5 colours, mirror, 17 pours, 121 looked at */
    "4512|4123|3125|2453|3451||", /* 29: 5 colours, rainbow, 17 pours, 125 looked at */
    "2213|4525|3543|4231|1541||", /* 30: 5 colours, shuffle, 16 pours, 234 looked at */
    "3215|1542|4343|1542|3215||", /* 31: 5 colours, mirror, 17 pours, 163 looked at */
    "1541|2514|3422|5314|5323||", /* 32: 5 colours, shuffle, 16 pours, 283 looked at */
    "1234|5325|2143|4451|3512||", /* 33: 5 colours, rows, 16 pours, 317 looked at */
    "2535|3141|2424|3141|2535||", /* 34: 5 colours, mirror, 18 pours, 158 looked at */
    "3542|2451|4231|2513|1354||", /* 35: 5 colours, rainbow, 17 pours, 258 looked at */
    "5243|3421|4323|1554|2151||", /* 36: 5 colours, shuffle, 16 pours, 460 looked at */
    "5412|3412|5353|3412|5412||", /* 37: 5 colours, mirror, 18 pours, 215 looked at */
    "3162|3451|5644|6232|3164|5521||", /* 38: 6 colours, shuffle, 19 pours, 245 looked at */
    "1451|4524|3216|5642|6335|2163||", /* 39: 6 colours, rows, 18 pours, 430 looked at */
    "2533|5246|4161|4161|5246|2533||", /* 40: 6 colours, mirror, 19 pours, 298 looked at */
    "4263|4215|6512|6523|4316|3154||", /* 41: 6 colours, rainbow, 21 pours, 138 looked at */
    "1262|6253|1453|1355|6244|3641||", /* 42: 6 colours, shuffle, 19 pours, 438 looked at */
    "3642|4256|3115|3115|4256|3642||", /* 43: 6 colours, mirror, 20 pours, 287 looked at */
    "6614|2416|5232|5361|4245|1353||", /* 44: 6 colours, shuffle, 19 pours, 536 looked at */
    "2426|4565|5643|6234|1351|3112||", /* 45: 6 colours, rows, 19 pours, 556 looked at */
    "4213|6264|1535|1535|6264|4213||", /* 46: 6 colours, mirror, 21 pours, 284 looked at */
    "2314|3465|6514|6231|5342|6521||", /* 47: 6 colours, rainbow, 20 pours, 546 looked at */
    "5216|5341|5533|6242|1436|4612||", /* 48: 6 colours, shuffle, 19 pours, 872 looked at */
    "3451|2636|2154|2154|2636|3451||", /* 49: 6 colours, mirror, 22 pours, 397 looked at */
    "5216|2134|6341|1523|4652|3465||", /* 50: 6 colours, showpiece, 20 pours, 577 looked at */
    "7346|7125|2455|7361|6436|3217|1542||", /* 51: 7 colours, shuffle, 22 pours, 240 looked at */
    "7372|6551|2726|5165|1243|3417|4634||", /* 52: 7 colours, rows, 21 pours, 420 looked at */
    "5647|6217|4133|2525|4133|6217|5647||", /* 53: 7 colours, mirror, 22 pours, 289 looked at */
    "6324|7451|7314|7526|6155|3172|4263||", /* 54: 7 colours, shuffle, 23 pours, 218 looked at */
    "6145|3267|4623|7142|6357|3512|5714||", /* 55: 7 colours, rainbow, 23 pours, 257 looked at */
    "1147|7543|6235|6262|6235|7543|1147||", /* 56: 7 colours, mirror, 23 pours, 276 looked at */
    "7244|6167|3512|3535|3512|6167|7244||", /* 57: 7 colours, mirror, 23 pours, 284 looked at */
    "4665|5343|1521|7254|2417|3132|6776||", /* 58: 7 colours, rows, 21 pours, 784 looked at */
    "6673|4423|4621|6512|3517|7351|2745||", /* 59: 7 colours, shuffle, 21 pours, 805 looked at */
    "3756|4162|5246|7251|2143|7365|3471||", /* 60: 7 colours, rainbow, 23 pours, 331 looked at */
    "7671|1232|4274|4616|5543|7326|1355||", /* 61: 7 colours, shuffle, 22 pours, 569 looked at */
    "6732|1446|3715|3265|7351|6424|2571||", /* 62: 7 colours, shuffle, 22 pours, 691 looked at */
    "6421|1754|3315|5243|4672|2136|7567||", /* 63: 7 colours, rows, 22 pours, 769 looked at */
    "7764|5241|6523|1313|6523|5241|7764||", /* 64: 7 colours, mirror, 23 pours, 486 looked at */
    "4113|6252|7365|4747|7365|6252|4113||", /* 65: 7 colours, mirror, 23 pours, 516 looked at */
    "7513|4625|4751|6243|6271|5743|1263||", /* 66: 7 colours, rainbow, 23 pours, 544 looked at */
    "3141|5754|5146|3226|2375|4676|7312||", /* 67: 7 colours, shuffle, 22 pours, 865 looked at */
    "3162|7514|2356|4747|2356|7514|3162||", /* 68: 7 colours, mirror, 25 pours, 217 looked at */
    "5431|3154|2263|7625|1572|4346|6717||", /* 69: 7 colours, rows, 22 pours, 1278 looked at */
    "3614|2537|4727|6127|4521|3635|4516||", /* 70: 7 colours, shuffle, 24 pours, 507 looked at */
    "5237|7463|1264|5151|1264|7463|5237||", /* 71: 7 colours, mirror, 25 pours, 325 looked at */
    "6314|2745|7513|4265|7463|1326|5127||", /* 72: 7 colours, rainbow, 23 pours, 858 looked at */
    "6271|5754|2327|1343|2156|6157|4346||", /* 73: 7 colours, shuffle, 24 pours, 593 looked at */
    "4127|1524|3673|5656|3673|1524|4127||", /* 74: 7 colours, mirror, 25 pours, 391 looked at */
    "7524|3176|6247|5763|2415|1632|4351||", /* 75: 7 colours, showpiece, 23 pours, 925 looked at */
    "6718|8852|4423|7615|6431|8527|3541|2376||", /* 76: 8 colours, shuffle, 24 pours, 364 looked at */
    "3583|8145|7227|1774|2431|4362|6818|5656||", /* 77: 8 colours, rows, 23 pours, 729 looked at */
    "1553|4621|6287|7348|7348|6287|4621|1553||", /* 78: 8 colours, mirror, 26 pours, 254 looked at */
    "4758|3256|1738|8364|4527|3741|5162|2186||", /* 79: 8 colours, rainbow, 26 pours, 350 looked at */
    "3487|8756|2216|1525|3423|4761|1853|6874||", /* 80: 8 colours, shuffle, 26 pours, 448 looked at */
    "8561|3446|8571|2723|2723|8571|3446|8561||", /* 81: 8 colours, mirror, 27 pours, 323 looked at */
    "3181|4242|7275|3568|4588|5347|7612|6136||", /* 82: 8 colours, shuffle, 26 pours, 613 looked at */
    "5613|2151|8284|1742|4828|7437|6576|3365||", /* 83: 8 colours, rows, 25 pours, 1174 looked at */
    "1348|2768|3554|2761|2761|3554|2768|1348||", /* 84: 8 colours, mirror, 27 pours, 507 looked at */
    "1423|1475|5786|7248|5136|2685|6374|8321||", /* 85: 8 colours, rainbow, 27 pours, 534 looked at */
    "5348|4138|1368|8261|3454|7756|2721|2567||", /* 86: 8 colours, shuffle, 25 pours, 1399 looked at */
    "7325|5486|2716|1384|1384|2716|5486|7325||", /* 87: 8 colours, mirror, 28 pours, 440 looked at */
    "2714|8339|3717|9154|4149|9653|2855|8682|6762||", /* 88: 9 colours, shuffle, 29 pours, 677 looked at */
    "6714|2927|8451|1669|9382|3596|7245|5133|4878||", /* 89: 9 colours, rows, 27 pours, 1733 looked at */
    "3861|4561|9473|8925|2727|8925|9473|4561|3861||", /* 90: 9 colours, mirror, 31 pours, 380 looked at */
    "2547|5216|3468|5613|2781|3971|5698|9472|8493||", /* 91: 9 colours, rainbow, 31 pours, 411 looked at */
    "5263|3387|1428|4649|2715|9749|3681|7156|9582||", /* 92: 9 colours, shuffle, 29 pours, 1140 looked at */
    "9251|4688|5973|4132|6767|4132|5973|4688|9251||", /* 93: 9 colours, mirror, 30 pours, 880 looked at */
    "7396|6859|3636|3185|2414|7478|2952|8159|1247||", /* 94: 9 colours, shuffle, 30 pours, 949 looked at */
    "8177|9245|9836|2413|6565|2413|9836|9245|8177||", /* 95: 9 colours, mirror, 31 pours, 958 looked at */
    "4967|3575|2486|9753|1849|6211|8132|5698|7324||", /* 96: 9 colours, rows, 28 pours, 3886 looked at */
    "4165|6753|4586|4293|8917|7241|8329|9725|6183||", /* 97: 9 colours, rainbow, 31 pours, 1291 looked at */
    "3518|2824|7986|7267|1647|4595|3631|4358|9291||", /* 98: 9 colours, shuffle, 31 pours, 1330 looked at */
    "7253|1267|9849|8614|3535|8614|9849|1267|7253||", /* 99: 9 colours, mirror, 33 pours, 1029 looked at */
    "2457|9215|4192|3968|5723|1349|6831|7684|8576||", /* 100: 9 colours, showpiece, 29 pours, 1851 looked at */
    "3788|1654|4997|5a66|a394|5736|1259|1a1a|8272|2348||", /* 101: 10 colours, shuffle, 30 pours, 403 looked at */
    "3723|4a74|5191|75a9|a835|266a|1456|6348|8287|9912||", /* 102: 10 colours, rows, 29 pours, 847 looked at */
    "5182|4163|2aa6|7495|3879|3879|7495|2aa6|4163|5182||", /* 103: 10 colours, mirror, 33 pours, 299 looked at */
    "4685|6947|a673|7359|365a|a222|1421|591a|3748|1889||", /* 104: 10 colours, shuffle, 30 pours, 1320 looked at */
    "4a88|9466|8293|6919|2151|76a4|a547|3735|1822|537a||", /* 105: 10 colours, rows, 29 pours, 2173 looked at */
    "1129|6448|52a7|a628|1453|938a|6231|3798|795a|6475||", /* 106: 10 colours, shuffle, 31 pours, 892 looked at */
    "4598|a542|6127|3a71|726a|1a36|3549|9841|3829|6875||", /* 107: 10 colours, rainbow, 33 pours, 387 looked at */
    "3826|a577|629a|8149|1534|1534|8149|629a|a577|3826||", /* 108: 10 colours, mirror, 33 pours, 527 looked at */
    "5974|16a2|4763|8135|9a28|9a28|8135|4763|16a2|5974||", /* 109: 10 colours, mirror, 35 pours, 214 looked at */
    "587a|4635|a327|1416|8299|8299|1416|a327|4635|587a||", /* 110: 10 colours, mirror, 33 pours, 596 looked at */
    "3569|4232|1359|47a7|21a4|3569|8165|68a2|89a4|1778||", /* 111: 10 colours, shuffle, 33 pours, 618 looked at */
    "9845|a645|2136|87a1|4a32|1257|3279|4a78|6195|9638||", /* 112: 10 colours, rainbow, 32 pours, 1082 looked at */
    "78a5|9178|9572|4322|6195|6914|331a|8434|685a|6a72||", /* 113: 10 colours, shuffle, 32 pours, 1087 looked at */
    "8a72|39a8|a38a|2417|4646|7153|5265|9721|6599|1834||", /* 114: 10 colours, rows, 31 pours, 2391 looked at */
    "5891|48a7|7265|1634|a392|a392|1634|7265|48a7|5891||", /* 115: 10 colours, mirror, 36 pours, 260 looked at */
    "4716|854a|5a31|1287|2174|a369|6329|58a9|8234|9576||", /* 116: 10 colours, rainbow, 33 pours, 1120 looked at */
    "1545|1a47|a337|89a2|9186|6853|4272|6a94|5832|6971||", /* 117: 10 colours, shuffle, 32 pours, 1798 looked at */
    "1982|3576|945a|6817|423a|423a|6817|945a|3576|1982||", /* 118: 10 colours, mirror, 35 pours, 466 looked at */
    "297a|1837|1528|a313|2695|44aa|4869|1626|4375|5987||", /* 119: 10 colours, shuffle, 33 pours, 1370 looked at */
    "9596|6153|3a24|8681|5232|7468|28a7|4949|1715|a37a||", /* 120: 10 colours, rows, 32 pours, 2596 looked at */
    "4a64|893a|9381|2215|5767|5767|2215|9381|893a|4a64||", /* 121: 10 colours, mirror, 34 pours, 1039 looked at */
    "7961|9546|3854|a37a|a296|1267|3158|2958|148a|3724||", /* 122: 10 colours, shuffle, 34 pours, 1161 looked at */
    "1679|2569|87a6|2158|a483|a153|27a4|9148|4396|3725||", /* 123: 10 colours, rainbow, 34 pours, 1167 looked at */
    "2612|7518|39a8|9543|47a6|47a6|9543|39a8|7518|2612||", /* 124: 10 colours, mirror, 36 pours, 558 looked at */
    "7149|6724|2635|85a2|5968|3816|1273|4a51|938a|a497||", /* 125: 10 colours, showpiece, 32 pours, 7125 looked at */
    "7548|7176|5943|63ba|bba9|2241|198a|a571|9663|23b5|2848||", /* 126: 11 colours, shuffle, 33 pours, 812 looked at */
    "1698|7483|3514|b679|22a5|abab|22a5|b679|3514|7483|1698||", /* 127: 11 colours, mirror, 36 pours, 342 looked at */
    "1673|9351|88b6|2b88|6924|329b|7549|b165|47a2|5417|aa3a||", /* 128: 11 colours, rows, 32 pours, 2608 looked at */
    "2619|962b|7258|1947|3756|45b3|9a83|4b6a|8a57|b13a|4821||", /* 129: 11 colours, rainbow, 36 pours, 852 looked at */
    "3258|325b|43aa|8269|1569|1a37|4ab4|8767|1b61|b792|5948||", /* 130: 11 colours, shuffle, 37 pours, 753 looked at */
    "4968|22a1|35b9|137a|6b75|4848|6b75|137a|35b9|22a1|4968||", /* 131: 11 colours, mirror, 37 pours, 844 looked at */
    "b785|25a2|4789|546b|8b19|411a|237a|496b|35a3|2683|6791||", /* 132: 11 colours, shuffle, 36 pours, 1411 looked at */
    "b359|6262|854a|3421|2713|7674|ab38|189b|9185|49a6|5ab7||", /* 133: 11 colours, rows, 35 pours, 3359 looked at */
    "1743|1696|a297|b532|b488|5a5a|b488|b532|a297|1696|1743||", /* 134: 11 colours, mirror, 38 pours, 884 looked at */
    "2947|ba72|6851|45a1|9873|546b|6541|2a13|7938|ba86|239b||", /* 135: 11 colours, rainbow, 37 pours, 1619 looked at */
    "86a2|46b7|9896|b837|99b5|4a15|3212|87a4|27b5|343a|1561||", /* 136: 11 colours, shuffle, 36 pours, 2664 looked at */
    "2361|95a6|13b8|875a|4247|9b9b|4247|875a|13b8|95a6|2361||", /* 137: 11 colours, mirror, 39 pours, 909 looked at */
    "2299|8694|bab6|4561|c738|97ab|38a1|843b|2723|ac65|cc15|5147||", /* 138: 12 colours, shuffle, 37 pours, 2298 looked at */
    "b792|31c5|1b48|2a66|4375|9ac8|9ac8|4375|2a66|1b48|31c5|b792||", /* 139: 12 colours, mirror, 40 pours, 956 looked at */
    "ab7a|9384|bcb9|1961|5415|2143|7527|48c2|8a56|32ac|6638|c79b||", /* 140: 12 colours, rows, 37 pours, 4005 looked at */
    "6758|8c97|78b1|469c|324a|b61a|93a5|235a|643c|42b1|b289|517c||", /* 141: 12 colours, rainbow, 39 pours, 1994 looked at */
    "6523|14a3|831b|8694|c1ba|5cb9|622b|5ca7|4691|47a5|37c7|8892||", /* 142: 12 colours, shuffle, 39 pours, 2861 looked at */
    "1cb2|23a5|1c7b|7689|3a89|5446|5446|3a89|7689|1c7b|23a5|1cb2||", /* 143: 12 colours, mirror, 41 pours, 1407 looked at */
    "1a8a|6293|7bb6|5179|5974|7c5a|3a69|13c8|34b2|2268|b8c5|c414||", /* 144: 12 colours, shuffle, 39 pours, 3950 looked at */
    "9353|7a34|6817|429a|ab76|16c5|cc8c|21bb|b749|5468|3921|85a2||", /* 145: 12 colours, rows, 36 pours, 16585 looked at */
    "c86b|9a57|3851|947a|6321|c4b2|c4b2|6321|947a|3851|9a57|c86b||", /* 146: 12 colours, mirror, 44 pours, 725 looked at */
    "8476|61b9|c937|1bca|a524|b29a|3876|45c8|83a4|3512|2159|6c7b||", /* 147: 12 colours, shuffle, 40 pours, 4618 looked at */
    "b9a6|5783|196c|8a14|4235|3127|b185|2c34|96ab|bc67|795c|4a28||", /* 148: 12 colours, rainbow, 41 pours, 3825 looked at */
    "171a|a763|8c52|cb59|b823|9464|9464|b823|cb59|8c52|a763|171a||", /* 149: 12 colours, mirror, 42 pours, 4247 looked at */
    "587b|7314|c15a|8b29|2745|b461|4c83|32c6|69a2|9a38|159c|a6b7||", /* 150: 12 colours, showpiece, 38 pours, 13116 looked at */
};

const uint8_t ws_level_par[WS_LEVELS] = {
    7, 9, 9, 9, 9, 9, 9, 9, 9, 10, 11, 11, 11, 12, 12, 14, 13, 13, 14, 13, 13, 14, 14, 14, 13,
    16, 15, 17, 17, 16, 17, 16, 16, 18, 17, 16, 18, 19, 18, 19, 21, 19, 20, 19, 19, 21, 20, 19, 22, 20,
    22, 21, 22, 23, 23, 23, 23, 21, 21, 23, 22, 22, 22, 23, 23, 23, 22, 25, 22, 24, 25, 23, 24, 25, 23,
    24, 23, 26, 26, 26, 27, 26, 25, 27, 27, 25, 28, 29, 27, 31, 31, 29, 30, 30, 31, 28, 31, 31, 33, 29,
    30, 29, 33, 30, 29, 31, 33, 33, 35, 33, 33, 32, 32, 31, 36, 33, 32, 35, 33, 32, 34, 34, 34, 36, 32,
    33, 36, 32, 36, 37, 37, 36, 35, 38, 37, 36, 39, 37, 40, 37, 39, 39, 41, 39, 36, 44, 40, 41, 42, 38,
};
