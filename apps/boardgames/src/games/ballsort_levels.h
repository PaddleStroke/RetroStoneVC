/*
 * Ball Sort's levels: our own, made by tools/ballsort_gen.c (each from a theme: a shuffle, rows of every
 * colour, rainbow tubes, a mirrored rack, a Latin rack to end each chapter; the shortest way found by the
 * rules' search; six chapters of 25 from the easiest). Each: the tubes from the left ('|' between them), each
 * one's colours from the bottom (1..9 a b c); in the comment the colours, the theme, the shortest way and the
 * positions the search made; the shortest ways again in bs_level_par (the tests check them).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const char *const bs_levels[BS_LEVELS] = {
    "1212|1212||", /* 1: 2 colours, shuffle, 8 moves, 34 looked at */
    "1132|2312|2313||", /* 2: 3 colours, shuffle, 10 moves, 73 looked at */
    "1132|3213|2321||", /* 3: 3 colours, rows, 10 moves, 80 looked at */
    "2131|2323|2131||", /* 4: 3 colours, mirror, 11 moves, 67 looked at */
    "1323|2132|1123||", /* 5: 3 colours, shuffle, 10 moves, 112 looked at */
    "1212|1323|1233||", /* 6: 3 colours, shuffle, 11 moves, 74 looked at */
    "2313|1212|2313||", /* 7: 3 colours, mirror, 11 moves, 77 looked at */
    "3121|1213|2332||", /* 8: 3 colours, rows, 11 moves, 100 looked at */
    "3123|2331|1212||", /* 9: 3 colours, rows, 11 moves, 106 looked at */
    "3132|1323|1212||", /* 10: 3 colours, shuffle, 11 moves, 107 looked at */
    "3123|2121|3123||", /* 11: 3 colours, mirror, 12 moves, 77 looked at */
    "3212|2133|1321||", /* 12: 3 colours, rows, 11 moves, 134 looked at */
    "1321|3232|1321||", /* 13: 3 colours, mirror, 12 moves, 93 looked at */
    "2421|1443|2143|3123||", /* 14: 4 colours, shuffle, 14 moves, 125 looked at */
    "4121|2334|1243|3412||", /* 15: 4 colours, rows, 14 moves, 141 looked at */
    "2421|1343|1343|2421||", /* 16: 4 colours, mirror, 15 moves, 98 looked at */
    "3241|2134|1432|1432||", /* 17: 4 colours, rainbow, 15 moves, 127 looked at */
    "1231|3124|4124|3324||", /* 18: 4 colours, shuffle, 14 moves, 219 looked at */
    "4321|3241|3241|4321||", /* 19: 4 colours, mirror, 15 moves, 152 looked at */
    "2114|2424|2313|3431||", /* 20: 4 colours, shuffle, 15 moves, 164 looked at */
    "4323|3141|1414|2232||", /* 21: 4 colours, rows, 14 moves, 294 looked at */
    "3134|2412|2412|3134||", /* 22: 4 colours, mirror, 16 moves, 135 looked at */
    "2413|3412|1234|4321||", /* 23: 4 colours, rainbow, 15 moves, 246 looked at */
    "3211|2134|1423|4243||", /* 24: 4 colours, shuffle, 15 moves, 314 looked at */
    "2423|3231|4142|1314||", /* 25: 4 colours, rows, 17 moves, 780 looked at */
    "3535|4321|1132|4214|4525||", /* 26: 5 colours, shuffle, 17 moves, 251 looked at */
    "2353|3212|5124|1541|4435||", /* 27: 5 colours, rows, 17 moves, 285 looked at */
    "3123|2541|4545|2541|3123||", /* 28: 5 colours, mirror, 19 moves, 149 looked at */
    "2154|4531|3512|2341|4325||", /* 29: 5 colours, rainbow, 18 moves, 276 looked at */
    "3125|1525|5443|3434|1212||", /* 30: 5 colours, shuffle, 18 moves, 301 looked at */
    "2353|5421|1414|5421|2353||", /* 31: 5 colours, mirror, 19 moves, 232 looked at */
    "4541|2431|4153|1323|5252||", /* 32: 5 colours, shuffle, 18 moves, 378 looked at */
    "2451|5112|4323|1545|3234||", /* 33: 5 colours, rows, 18 moves, 417 looked at */
    "1352|5143|4242|5143|1352||", /* 34: 5 colours, mirror, 19 moves, 324 looked at */
    "3145|4312|1235|3245|2145||", /* 35: 5 colours, rainbow, 18 moves, 587 looked at */
    "3214|4541|2153|5352|4231||", /* 36: 5 colours, shuffle, 19 moves, 528 looked at */
    "4531|2512|3434|2512|4531||", /* 37: 5 colours, mirror, 20 moves, 355 looked at */
    "5161|2235|2436|3413|5414|6256||", /* 38: 6 colours, shuffle, 21 moves, 649 looked at */
    "1142|2465|6524|3256|4631|5313||", /* 39: 6 colours, rows, 21 moves, 663 looked at */
    "2261|3513|4654|4654|3513|2261||", /* 40: 6 colours, mirror, 23 moves, 363 looked at */
    "1365|5213|6452|1324|1645|3624||", /* 41: 6 colours, rainbow, 22 moves, 599 looked at */
    "4316|3534|2516|2154|1322|4566||", /* 42: 6 colours, shuffle, 22 moves, 703 looked at */
    "3161|4336|6253|1615|2542|5424||", /* 43: 6 colours, rows, 22 moves, 829 looked at */
    "3531|5126|4642|4642|5126|3531||", /* 44: 6 colours, mirror, 23 moves, 552 looked at */
    "1312|6524|5312|3454|5361|6264||", /* 45: 6 colours, shuffle, 22 moves, 1035 looked at */
    "2354|1654|2613|2613|1654|2354||", /* 46: 6 colours, mirror, 24 moves, 535 looked at */
    "4316|5223|2423|1543|1651|5466||", /* 47: 6 colours, shuffle, 23 moves, 868 looked at */
    "3415|5613|4125|5261|4263|6342||", /* 48: 6 colours, rainbow, 24 moves, 720 looked at */
    "1463|5624|1352|1352|5624|1463||", /* 49: 6 colours, mirror, 26 moves, 693 looked at */
    "3561|6253|1345|5124|2436|4612||", /* 50: 6 colours, showpiece, 22 moves, 436 looked at */
    "7565|6342|4247|1175|3131|6643|7252||", /* 51: 7 colours, shuffle, 23 moves, 864 looked at */
    "1527|2643|6256|7314|5771|4435|3162||", /* 52: 7 colours, rows, 24 moves, 658 looked at */
    "2563|4617|5714|2323|5714|4617|2563||", /* 53: 7 colours, mirror, 26 moves, 280 looked at */
    "2431|6271|3164|7425|6753|2753|5164||", /* 54: 7 colours, rainbow, 25 moves, 568 looked at */
    "3667|5741|2351|4242|2351|5741|3667||", /* 55: 7 colours, mirror, 26 moves, 486 looked at */
    "2764|4557|1343|1656|5276|1227|3134||", /* 56: 7 colours, shuffle, 25 moves, 936 looked at */
    "4536|5325|2114|6747|3651|7272|1463||", /* 57: 7 colours, rows, 25 moves, 965 looked at */
    "4552|1616|3747|2323|3747|1616|4552||", /* 58: 7 colours, mirror, 26 moves, 612 looked at */
    "3747|3131|6214|4726|5461|6255|3527||", /* 59: 7 colours, shuffle, 26 moves, 625 looked at */
    "3413|2257|5467|6161|5467|2257|3413||", /* 60: 7 colours, mirror, 26 moves, 901 looked at */
    "7242|3264|7153|4521|6646|3573|1571||", /* 61: 7 colours, shuffle, 26 moves, 921 looked at */
    "2714|3272|1657|6435|7161|4543|5326||", /* 62: 7 colours, rows, 26 moves, 1079 looked at */
    "5351|4276|6357|4644|1213|3126|5772||", /* 63: 7 colours, shuffle, 26 moves, 1135 looked at */
    "1753|4625|2435|4217|2461|3761|7635||", /* 64: 7 colours, rainbow, 26 moves, 1205 looked at */
    "4763|6724|7461|7215|3145|5213|2356||", /* 65: 7 colours, rainbow, 26 moves, 1320 looked at */
    "6227|1745|5431|6363|5431|1745|6227||", /* 66: 7 colours, mirror, 28 moves, 569 looked at */
    "4376|6523|4142|1625|7537|1731|4256||", /* 67: 7 colours, shuffle, 27 moves, 1023 looked at */
    "2465|7337|4251|1616|4251|7337|2465||", /* 68: 7 colours, mirror, 27 moves, 1082 looked at */
    "4721|6243|4517|1337|6156|6355|2472||", /* 69: 7 colours, shuffle, 27 moves, 1237 looked at */
    "2516|6474|7135|3232|7135|6474|2516||", /* 70: 7 colours, mirror, 28 moves, 866 looked at */
    "7267|6575|3322|4131|2746|5613|1454||", /* 71: 7 colours, rows, 26 moves, 2213 looked at */
    "6432|5352|2514|2467|7646|1733|1175||", /* 72: 7 colours, shuffle, 27 moves, 1591 looked at */
    "7621|7134|2514|6543|4267|7135|2536||", /* 73: 7 colours, rainbow, 28 moves, 1057 looked at */
    "1367|1545|3724|6262|3724|1545|1367||", /* 74: 7 colours, mirror, 28 moves, 1089 looked at */
    "3152|2376|6741|5617|4263|1534|7425||", /* 75: 7 colours, showpiece, 27 moves, 1918 looked at */
    "2778|3384|1425|1463|6254|5716|3528|8761||", /* 76: 8 colours, shuffle, 28 moves, 806 looked at */
    "3434|2147|5515|6623|4351|1768|8286|7872||", /* 77: 8 colours, rows, 27 moves, 2461 looked at */
    "8712|2735|6863|1544|1544|6863|2735|8712||", /* 78: 8 colours, mirror, 30 moves, 762 looked at */
    "3127|2128|6645|4836|5187|1683|2774|5345||", /* 79: 8 colours, shuffle, 29 moves, 2106 looked at */
    "5124|8376|5213|6123|4873|2816|7584|6475||", /* 80: 8 colours, rainbow, 31 moves, 946 looked at */
    "8876|2615|2435|1473|1473|2435|2615|8876||", /* 81: 8 colours, mirror, 31 moves, 1250 looked at */
    "6812|7248|1746|8654|3375|1515|7428|3236||", /* 82: 8 colours, shuffle, 30 moves, 2508 looked at */
    "3287|1541|6165|8714|4336|2653|7428|5872||", /* 83: 8 colours, rows, 30 moves, 2849 looked at */
    "2426|8564|7813|7351|7351|7813|8564|2426||", /* 84: 8 colours, mirror, 32 moves, 1378 looked at */
    "2685|2371|6743|5273|3816|4865|5412|4187||", /* 85: 8 colours, rainbow, 32 moves, 1626 looked at */
    "5126|2654|4643|8138|7525|1617|3247|8387||", /* 86: 8 colours, shuffle, 31 moves, 3027 looked at */
    "5612|7173|5426|8438|8438|5426|7173|5612||", /* 87: 8 colours, mirror, 33 moves, 1275 looked at */
    "6285|3968|4576|4423|1597|8737|8614|3929|2115||", /* 88: 9 colours, shuffle, 34 moves, 2750 looked at */
    "6234|7451|5789|4566|8147|3692|2373|1925|9818||", /* 89: 9 colours, rows, 34 moves, 3485 looked at */
    "2376|6941|8215|7451|3962|7891|8237|4635|8495||", /* 90: 9 colours, rainbow, 34 moves, 4311 looked at */
    "7881|3562|5492|7964|3131|7964|5492|3562|7881||", /* 91: 9 colours, mirror, 36 moves, 1740 looked at */
    "7989|3265|3469|9255|4681|4775|3416|8317|1228||", /* 92: 9 colours, shuffle, 34 moves, 5527 looked at */
    "5613|2865|2147|7489|3939|7489|2147|2865|5613||", /* 93: 9 colours, mirror, 37 moves, 1776 looked at */
    "2449|6859|1316|9645|8374|2171|5328|7265|8973||", /* 94: 9 colours, shuffle, 36 moves, 3244 looked at */
    "2715|9557|5431|6946|4124|1872|7298|8389|3663||", /* 95: 9 colours, rows, 35 moves, 6574 looked at */
    "9514|3987|7256|8463|1212|8463|7256|3987|9514||", /* 96: 9 colours, mirror, 39 moves, 1242 looked at */
    "2641|5632|9186|9426|8745|8427|3157|5913|8379||", /* 97: 9 colours, rainbow, 37 moves, 3308 looked at */
    "8529|4346|7912|7386|5151|7386|7912|4346|8529||", /* 98: 9 colours, mirror, 37 moves, 3566 looked at */
    "5638|9232|5863|1341|9781|6844|7595|1762|9724||", /* 99: 9 colours, shuffle, 36 moves, 6544 looked at */
    "7248|3769|4396|5913|9824|8135|6571|1652|2487||", /* 100: 9 colours, showpiece, 34 moves, 3900 looked at */
    "8267|42a3|9433|9451|5165|126a|a324|7699|5718|88a7||", /* 101: 10 colours, shuffle, 36 moves, 1194 looked at */
    "7251|5123|3a84|9866|a974|a974|9866|3a84|5123|7251||", /* 102: 10 colours, mirror, 38 moves, 1358 looked at */
    "4618|38aa|6929|8272|5533|a157|9395|7464|1a81|2746||", /* 103: 10 colours, rows, 35 moves, 5644 looked at */
    "4539|1597|4128|1833|6a8a|98a5|6494|767a|5223|7261||", /* 104: 10 colours, shuffle, 38 moves, 2985 looked at */
    "12a4|3983|6552|a781|9647|9647|a781|6552|3983|12a4||", /* 105: 10 colours, mirror, 40 moves, 1312 looked at */
    "5a37|a261|6217|a984|7913|7592|4319|4358|4658|86a2||", /* 106: 10 colours, rainbow, 39 moves, 2253 looked at */
    "2116|844a|4675|5a93|3881|4a52|6279|5379|a287|1936||", /* 107: 10 colours, shuffle, 38 moves, 3722 looked at */
    "515a|7235|4819|9376|2561|1622|8448|3aa3|a984|6797||", /* 108: 10 colours, rows, 37 moves, 6267 looked at */
    "9121|7839|7655|a4a4|8236|8236|a4a4|7655|7839|9121||", /* 109: 10 colours, mirror, 39 moves, 2708 looked at */
    "7167|6a94|5925|4533|2632|8497|1aa3|8a91|8872|6154||", /* 110: 10 colours, shuffle, 38 moves, 5704 looked at */
    "a575|4493|3a9a|822a|6758|7874|9296|1516|2831|1364||", /* 111: 10 colours, shuffle, 38 moves, 6672 looked at */
    "1473|6a42|2685|1795|3a89|3a89|1795|2685|6a42|1473||", /* 112: 10 colours, mirror, 41 moves, 1712 looked at */
    "621a|7396|a632|5a41|17a3|8587|2855|9968|4124|3479||", /* 113: 10 colours, rows, 38 moves, 7225 looked at */
    "1652|743a|18a4|9563|2987|2987|9563|18a4|743a|1652||", /* 114: 10 colours, mirror, 41 moves, 1995 looked at */
    "a892|7964|6942|645a|2317|53a8|2185|731a|4581|7936||", /* 115: 10 colours, rainbow, 40 moves, 3420 looked at */
    "6453|1a78|6785|2a93|4912|4912|2a93|6785|1a78|6453||", /* 116: 10 colours, mirror, 41 moves, 2444 looked at */
    "474a|1331|5479|a658|9986|5252|96a1|7387|8214|a632||", /* 117: 10 colours, shuffle, 39 moves, 6168 looked at */
    "8a15|6798|328a|2a19|3198|1476|3654|4756|5742|93a2||", /* 118: 10 colours, rainbow, 39 moves, 6226 looked at */
    "4583|6166|5253|a214|491a|5743|3a87|1a98|6279|2987||", /* 119: 10 colours, shuffle, 39 moves, 7395 looked at */
    "93a8|5749|7a42|6251|8361|8361|6251|7a42|5749|93a8||", /* 120: 10 colours, mirror, 42 moves, 2422 looked at */
    "43a2|8776|291a|1547|7281|6a69|5833|9424|a655|3198||", /* 121: 10 colours, rows, 39 moves, 9775 looked at */
    "7495|18a6|4531|4829|391a|4972|3567|5768|a321|68a2||", /* 122: 10 colours, rainbow, 39 moves, 11518 looked at */
    "8578|99a6|5325|4524|3a71|a328|367a|9781|6269|1441||", /* 123: 10 colours, shuffle, 39 moves, 13283 looked at */
    "23a6|1897|a753|4185|2946|2946|4185|a753|1897|23a6||", /* 124: 10 colours, mirror, 43 moves, 2725 looked at */
    "1728|9645|3287|7a39|6451|519a|2513|a862|4376|89a4||", /* 125: 10 colours, showpiece, 39 moves, 7007 looked at */
    "4979|b945|6a31|7736|782a|9154|4bab|311b|3a28|2652|8865||", /* 126: 11 colours, shuffle, 39 moves, 5643 looked at */
    "6492|1b6b|aa25|b957|2189|8834|93a8|567a|4746|3513|72b1||", /* 127: 11 colours, rows, 40 moves, 6450 looked at */
    "7a41|95b4|b339|8a57|2168|2626|2168|8a57|b339|95b4|7a41||", /* 128: 11 colours, mirror, 44 moves, 1759 looked at */
    "b721|2497|5ba3|3a64|8351|7ba2|4952|6b15|86a1|9738|4689||", /* 129: 11 colours, rainbow, 42 moves, 6483 looked at */
    "a233|b7a2|a118|79a9|7492|6b65|8695|5627|5434|834b|18b1||", /* 130: 11 colours, shuffle, 43 moves, 4418 looked at */
    "9832|4621|b5a7|835a|14b7|9696|14b7|835a|b5a7|4621|9832||", /* 131: 11 colours, mirror, 44 moves, 3598 looked at */
    "7352|6621|7278|9a4a|a1b1|b91a|5256|943b|7946|b858|4383||", /* 132: 11 colours, shuffle, 43 moves, 7318 looked at */
    "a7ba|2a83|bba1|9869|7496|4377|1225|5148|8952|3614|653b||", /* 133: 11 colours, rows, 42 moves, 18516 looked at */
    "486b|7641|a198|b352|3a75|2929|3a75|b352|a198|7641|486b||", /* 134: 11 colours, mirror, 46 moves, 3280 looked at */
    "951a|a268|a278|29a5|1b46|b547|2397|8634|134b|7165|38b9||", /* 135: 11 colours, rainbow, 44 moves, 9406 looked at */
    "8758|138a|6654|1941|184b|b952|b956|b67a|74aa|3272|9332||", /* 136: 11 colours, shuffle, 44 moves, 12483 looked at */
    "b489|3215|621a|9a85|637b|4747|637b|9a85|621a|3215|b489||", /* 137: 11 colours, mirror, 46 moves, 5930 looked at */
    "8445|c737|3a15|7b81|c95a|c746|bb29|242a|6858|c69b|1a29|1633||", /* 138: 12 colours, shuffle, 46 moves, 15208 looked at */
    "8552|a638|5165|3897|7c71|1ab4|491c|b2cb|2b29|c48a|97a6|6343||", /* 139: 12 colours, rows, 47 moves, 15285 looked at */
    "3b47|2152|1c76|4989|ac3b|a856|a856|ac3b|4989|1c76|2152|3b47||", /* 140: 12 colours, mirror, 49 moves, 7390 looked at */
    "8729|a432|1853|c9ba|68b3|7926|c536|b578|614c|ac71|b594|41a2||", /* 141: 12 colours, rainbow, 50 moves, 6031 looked at */
    "a8c3|2739|7bc6|25ab|6194|8451|8451|6194|25ab|7bc6|2739|a8c3||", /* 142: 12 colours, mirror, 50 moves, 6632 looked at */
    "3a97|cbc6|c593|2436|a2c6|b781|8493|2789|82a4|617b|455a|511b||", /* 143: 12 colours, shuffle, 48 moves, 18152 looked at */
    "7859|4648|9a12|7b2c|35c3|a61b|a61b|35c3|7b2c|9a12|4648|7859||", /* 144: 12 colours, mirror, 52 moves, 3799 looked at */
    "b439|2b81|6c14|771a|2a69|385b|c4c5|58a8|3a1b|c542|3967|7629||", /* 145: 12 colours, shuffle, 50 moves, 11810 looked at */
    "bc3b|5866|841c|1577|c7a9|398a|a695|7b58|92b4|43c1|6143|2a22||", /* 146: 12 colours, rows, 48 moves, 40249 looked at */
    "c3b4|8125|569b|c129|9a65|7b2a|186c|839c|36a7|1742|3748|b5a4||", /* 147: 12 colours, rainbow, 51 moves, 13315 looked at */
    "93a6|2c48|27a5|b6c5|1431|b798|b798|1431|b6c5|27a5|2c48|93a6||", /* 148: 12 colours, mirror, 53 moves, 8991 looked at */
    "3a75|c86c|2c49|2119|9654|85b8|a68b|93ba|7376|1352|a441|c7b2||", /* 149: 12 colours, shuffle, 54 moves, 5757 looked at */
    "a982|6149|5413|42ca|3cb5|7628|285b|ba36|c567|179c|8371|9ba4||", /* 150: 12 colours, showpiece, 49 moves, 37531 looked at */
};

const uint8_t bs_level_par[BS_LEVELS] = {
    8, 10, 10, 11, 10, 11, 11, 11, 11, 11, 12, 11, 12, 14, 14, 15, 15, 14, 15, 15, 14, 16, 15, 15, 17,
    17, 17, 19, 18, 18, 19, 18, 18, 19, 18, 19, 20, 21, 21, 23, 22, 22, 22, 23, 22, 24, 23, 24, 26, 22,
    23, 24, 26, 25, 26, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 28, 27, 27, 27, 28, 26, 27, 28, 28, 27,
    28, 27, 30, 29, 31, 31, 30, 30, 32, 32, 31, 33, 34, 34, 34, 36, 34, 37, 36, 35, 39, 37, 37, 36, 34,
    36, 38, 35, 38, 40, 39, 38, 37, 39, 38, 38, 41, 38, 41, 40, 41, 39, 39, 39, 42, 39, 39, 39, 43, 39,
    39, 40, 44, 42, 43, 44, 43, 42, 46, 44, 44, 46, 46, 47, 49, 50, 50, 48, 52, 50, 48, 51, 53, 54, 49,
};
