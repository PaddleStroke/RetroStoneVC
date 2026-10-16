/*
 * Tile Connect's levels: our own, made by tools/tileconnect_gen.c (each board played out first by the rules' generator,
 * so it always clears; chosen by how often taking pairs at random gets stuck). Each: its size, its fall, a
 * character a cell for the faces ('#' blocked), then the step of the plan that takes each tile (the tests play it).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const tcn_lvl tcn_levels[TCN_LEVELS] = {
    {6, 4, 0, "ioAAJFFPPuqJuozGzMiGLLqM",
     "1877944665A958B2B31200A3"}, /* 1: 6 x 4, still, 12 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "BBzyzzlllddByBAAzKDKlKKD",
     "55070633B22878996A1AB441"}, /* 2: 6 x 4, still, 8 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "IceecmPHrBBBJHJPBmrIsOOs",
     "953350A27411626A40798BB8"}, /* 3: 6 x 4, still, 11 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "nbibirObbriiPCnrJPbOJCrb",
     "854943159A227B8A67016B30"}, /* 4: 6 x 4, still, 8 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "AGpnnnAJpkJJGonJGknoGkkn",
     "678BA165832294B573A49001"}, /* 5: 6 x 4, still, 7 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "NDNfzdDehFFxGehzfGwAdAxw",
     "6963A8945112B45A3B078720"}, /* 6: 6 x 4, still, 12 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "EkEEBEBlvlkvwwvbxwvBBbxw",
     "0806B6B41489AA1532977532"}, /* 7: 6 x 4, still, 8 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "NNsqCsCDNCDDCDPPNDGqDGqq",
     "9156853018A430BB9476A722"}, /* 8: 6 x 4, still, 7 faces, 0 blocked, stuck 0%, search 12 */
    {6, 4, 0, "bEhfbhrroovEhEEovEhEofHH",
     "6A136177825B90B25A908344"}, /* 9: 6 x 4, still, 8 faces, 0 blocked, stuck 0%, search 12 */
    {8, 4, 0, "FKPjPdKKchgPPhPBcFKdgPccaKccKjaB",
     "653A395E0D422DF706894FCB1ECB8A17"}, /* 10: 8 x 4, still, 10 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "ADaAynDsncAcaksaDaycnaDccAAAnkac",
     "9DC9EFA3F8617B32D7E15CA084465B20"}, /* 11: 8 x 4, still, 8 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "mEFFjFFHExxjBBxxmmmBEzFHzBEaBaBF",
     "7DF36C31DEE6880074495BC1B95A2A2F"}, /* 12: 8 x 4, still, 9 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "wfCOgOCewCOcOcfzowCCOoOeggccgCwz",
     "8105790A8E93231BDCEF2D5A44667FCB"}, /* 13: 8 x 4, still, 9 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "aDdkDCFfdkkjjkjCvpkFfkCCaCCpjvzz",
     "215F16A75F8BD4B0CE8A7460299EDC33"}, /* 14: 8 x 4, still, 11 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "LELLEKIIadqdEEIIqqqvKKKILvIdvvda",
     "32EE2610C878AAD0744F99613BD5FB5C"}, /* 15: 8 x 4, still, 8 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "wAFuAAAFFkkFApANDwuuwpwkuDFkkkFN",
     "80495704DAAF7251E633628B9EDBCCF1"}, /* 16: 8 x 4, still, 8 faces, 0 blocked, stuck 0%, search 16 */
    {8, 4, 0, "KrfJrKDDrPHfHrrDrjfxjfDHjPAAJjHx",
     "1A9BA1746DCF83346EF2E9785D00B5C2"}, /* 17: 8 x 4, still, 10 faces, 0 blocked, stuck 0%, search 16 */
    {8, 5, 0, "pFaFatAoEAAAzpzPborEoFFoPbqqtrrotbrotzbz",
     "F8J8JGA5HDDABF4EI79H5110E622G330C697C4IB"}, /* 18: 8 x 5, still, 12 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "tsitLNtMHNivwHHNvivccwkwNHwNstLLvskiLNsM",
     "0H2FDB065I2EGAAB19E88JCJ45GIHF3317C9D476"}, /* 19: 8 x 5, still, 11 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "dznezAJfdwwzgzgJjJngAzexwwfgzfnxdnfdfjfJ",
     "BIJ7FE6CB11I52564GJHE27D33AHFA9D0980C48G"}, /* 20: 8 x 5, still, 11 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "xGcDwccGcbOOicibbODDGIDbcxGbGOIObBBOxwxG",
     "7D4HC22906FGA4A5EI33BJH507BEDGJI611F8C89"}, /* 21: 8 x 5, still, 10 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "kIPfffBIzBBPBkIGezPlPBcIemllIBcccmfIclcG",
     "DH4F9FCA300JCDHG734IJ1BA752261EBE5968I8G"}, /* 22: 8 x 5, still, 11 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "wrvbmwMgjvggvCCirrbvvmjiiitrgtrjwjtrwtvM",
     "1A9841DBHF773JJ0EA893450CCIGB2G56HIE62FD"}, /* 23: 8 x 5, still, 11 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "eeBEByCCeLyLynLEeeesOnLjOOOsypLCEjLCEpnn",
     "00EBEHF23DJDJ7GB3886C7A9C446HIA219GF1I55"}, /* 24: 8 x 5, still, 11 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 0, "blEwwbOpyyggOwClwOpOECwillrrpwOpbiggObgg",
     "3EDBB351229JG8CI8G46DC7FIE004751HF9A6HJA"}, /* 25: 8 x 5, still, 11 faces, 0 blocked, stuck 12%, search 20 */
    {8, 6, 0, "vbvPjwyCahaPjjyFIbyjbFwhayjIaPccIfjfcPchhCFbIaaF",
     "AJABD30K57GB1108FJCDL83I5C2ME4HHF929646I7KNLMEGN"}, /* 26: 8 x 6, still, 13 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "MHbltAyAaHMlllyjHvGADGtjGADADGDajjDvHbccAHDGGHgg",
     "JCN79KEKICJ7GGE56HF248953142M8MI00AHDNBB1DAF36LL"}, /* 27: 8 x 6, still, 14 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "HifhHihLPPfLHifHhhLhfwwkwwLEEHhiFkrrHLLzsFFkskFz",
     "0A91041622G6L49E7JF7G8NM8NFHHEJABIKKLDD35BCI5MC3"}, /* 28: 8 x 6, still, 13 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "hvjJxHAJjhGJoHyJGKKjwwnjAzAowonGGzvoyKGKwGAwxJJw",
     "BLA3I6J39BG5M6F58009NK1ADCD4EM128CL4F727NGJEIHHK"}, /* 29: 8 x 6, still, 14 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "qaAAlaleEENzazeqALqqzzaLeyNjAyllqyLKAjzzAeyKLqaa",
     "29443D36HHLJDJ6I1NGGBB9F5MLE8MAA2CNK8E7715CKFI00"}, /* 30: 8 x 6, still, 12 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "oLlxOLxoFFrbOxlyOOFFHxNyOrrNHmmbHyyrysHOLsbLPybP",
     "07KH57H033EJ58KFLM22C8BFL44BA66JC11EIGAMDGND9IN9"}, /* 31: 8 x 6, still, 14 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "pOOqiEqqzfzEdEzEeeqdfjjHOzzEuufjzqpsOifjHpEpeeqs",
     "HKNFG1AF0EJI91JI33A9744BK22LCC780DHMNGE8B5L566DM"}, /* 32: 8 x 6, still, 13 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "vLqqvHbHPPLxNxysyNdduuvsPbPbNxLLvsNsNbxbPyCCyNbP",
     "HK00N1E18GKFBFI6IL4433N68MJA2C55HDBD2ACEG9779LMJ"}, /* 33: 8 x 6, still, 13 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 0, "PnItnIIItjjJBajzfzttBCfBPBrBjCBzffrfttjPjfeJaePz",
     "2NH8N6H68EEMLKG04ICCLAF32D1DBA30471F55BJG79MK9JI"}, /* 34: 8 x 6, still, 13 faces, 0 blocked, stuck 0%, search 24 */
    {10, 5, 0, "yjryPfPyjPB#rqPqkkkyqKK#pJ#jjkqjKKjJPq#PCBfpKKrCqr",
     "7H67G9LKHBJ#6FGECAAKFD8#I0#11CM32230LM#B4J9I8D54E5"}, /* 35: 10 x 5, still, 12 faces, 4 blocked, stuck 0%, search 23 */
    {10, 5, 0, "qDowBqqArzwDNNN#AoNBrqLLAAoMMBLoLr#zwBNKrwBowwNKBo",
     "M2LDK99GNBD2A44#GLAKCM7HFFJ668HJ7N#B3815CI0E3I150E"}, /* 36: 10 x 5, still, 12 faces, 2 blocked, stuck 0%, search 24 */
    {10, 5, 0, "PgPPdPnbDgndrrgf#ldbrcgDlfhmhbrmd#ccdmrbhmmchdllrm",
     "NF6NE6HGMFHEAAC4#B930DCMI4212G0LJ#KK9153788D7JIB5L"}, /* 37: 10 x 5, still, 12 faces, 2 blocked, stuck 0%, search 24 */
    {10, 5, 0, "KegrgKnrfdKr#fqlcetKecNllqdKtnGtgrKlf#fcccngNceGtn",
     "45D2D4I2EF9K#E38L51HGBJ003F917CAMKH8N#NLB6IMJ6GCA7"}, /* 38: 10 x 5, still, 13 faces, 2 blocked, stuck 0%, search 24 */
    {10, 5, 0, "ELFLuJEkFurzu#u#xxEEFzGGJPPPkHGruG#L#zPHuFGHHLHHzG",
     "AGLGMKAEJI87I#M#11BBJ794KFF6E298D4#H#562DLC03H305C"}, /* 39: 10 x 5, still, 12 faces, 4 blocked, stuck 2%, search 23 */
    {10, 5, 0, "sOsDKDiKOyiIyOOywccjFIO#sc#sscOKiKwAAKKiiIiIFjyscc",
     "B1AH8HK81FKD4334M77INCE#BL#669E525MGG00JJD2CNIFAL9"}, /* 40: 10 x 5, still, 12 faces, 2 blocked, stuck 2%, search 24 */
    {10, 5, 0, "IvbPPbJIazgvbzcvvaIxb#bbcdae#IzdccgJaPBzBezIzIxPJJ",
     "2AE44ID26CFAJM0776G9E#JI0K35#LHKNNFD31BCB5HLMG9188"}, /* 41: 10 x 5, still, 13 faces, 2 blocked, stuck 2%, search 24 */
    {10, 5, 0, "FiCHNHFFzCii#FuppCdNi#pBeuzC#uhBdBepp#iiuCdChduBpu",
     "2G5IAIF2H5DD#F633KJAG#984LHK#LE817400#CCBM1MEJ679B"}, /* 42: 10 x 5, still, 12 faces, 4 blocked, stuck 5%, search 23 */
    {10, 6, 0, "eggttdgGglpztlvzGstdpptsvszd#Gp#GIIsGKpKGdltepPvldzzPPldPvzl",
     "MGG22RFHFJDN9J0OHKQEDI9K0COR#SI#B33CBPLPSA4QML568E7N118A5674"}, /* 43: 10 x 6, still, 13 faces, 2 blocked, stuck 0%, search 29 */
    {10, 6, 0, "vkhhhKkziKlh#xhhKoKzlvPxv#sKiivxkP#zAAisxxkossK#szzxzPqqkkPs",
     "RHQAJ4HKG46J#1QAONOK6R31M#E8PPM573#I22GD957NLL8#EIF9FB00CCBD"}, /* 44: 10 x 6, still, 13 faces, 4 blocked, stuck 2%, search 28 */
    {10, 6, 0, "cxzBeexBzBc#ucBleenuLnu#klD#zDLi#iDl#nziyLLinlik#cylLLDclciu",
     "N72CQQ7C2ON#0PO9BBDE1I0#L9M#6M1K#8GH#D6J5438IHJL#F5A34GPAFKE"}, /* 45: 10 x 6, still, 13 faces, 6 blocked, stuck 5%, search 27 */
    {10, 6, 0, "bpsggEgbpsppxuBhEcghubsu#hMucBbpsxb#EhuoMBBhbgOeEgOpchcuEeEo",
     "GJ9O7B7LP9AJNKRSBMOIHL3K#S8DMRGP3NC#5IHF866QCE425E4A1Q1D020F"}, /* 46: 10 x 6, still, 14 faces, 2 blocked, stuck 5%, search 29 */
    {10, 6, 0, "EccqkaaajuJj#xyu#accqfqExqE#jfkf#keElujfJjc#jal#fuyeckkaekfe",
     "8AAKBOPHDQ2J#EGQ#H991018EKI#DCB0#MLIFN6C2J7#6PF#4NG5733O5M4L"}, /* 47: 10 x 6, still, 13 faces, 6 blocked, stuck 15%, search 46 */
    {10, 6, 0, "ccowojozgvcfp#z#cwgqppvp#covoIcwzzf#oIvvjjzP#P#jvwjgIgHHIzqj",
     "13BDBFIG4M1QN#G#KO4PNHLH#K9LI83DCCQ#9822AA5E#E#JMOF7076605PJ"}, /* 48: 10 x 6, still, 13 faces, 6 blocked, stuck 32%, search 27 */
    {10, 6, 0, "swDIInnIIsonnEoI#I#jcccoAAE#sjcD#KjnsKKjE#E#owwKKvnivEiwwEKw",
     "7QML911597J44KPL#5#G6IIJCCO#A26M#HGFA332K#O#PEE88NFDN0DBB0HQ"}, /* 49: 10 x 6, still, 13 faces, 6 blocked, stuck 38%, search 27 */
    {10, 6, 0, "zLggsuuEsucgLg#Og#LzOLN#cczfHHunNnnO#NHHcu#fu#ttsEOnNfsfgOOz",
     "E8750I4P04HQL5#N7#8B1L3#66EK22DA3AON#GJJHI#CD#99MP1OGCMKQFFB"}, /* 50: 10 x 6, still, 13 faces, 6 blocked, stuck 52%, search 27 */
    {8, 5, 1, "FccNNIcFqdcqaMggvMvvltaNNMItIMcdvlcNNIpp",
     "385E7H53FI8FGB220944DJG769CJCBAI0DAE6H11"}, /* 51: 8 x 5, down, 13 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 1, "AOytAuOyygxxkgOsgaOuiuxkytgiJkxJaeeMMsuk",
     "8H5G8D654B22AB6EIJHC9C014GI97A07JFF33ED1"}, /* 52: 8 x 5, down, 14 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 1, "gzuznPdPvnugzPuuAhAOOdhAzzzggPAwvOPdwPOd",
     "G86FI1294I6GF100DJDHE2JB833559BC4EA7CAH7"}, /* 53: 8 x 5, down, 11 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 1, "OwOhhtwIpzzAztGhMBBtOOMIpAhtIGMzMuIruxxr",
     "IHIBC5H60GJAJ59C1FF822760AB8D97G13D43EE4"}, /* 54: 8 x 5, down, 14 faces, 0 blocked, stuck 0%, search 20 */
    {8, 5, 1, "qzMNzxizaqqqNaaMzMrzxOOzaOrfMINNMxMIiOxf",
     "G087A9F0E11G7228JIBJ9DDAE5BCI6HH3436F54C"}, /* 55: 8 x 5, down, 11 faces, 0 blocked, stuck 2%, search 20 */
    {8, 5, 1, "ADGDMrACAelDMlGumeeumbbrAeDqqDACmmDflAlf",
     "1D6B0817GJAF0A6I322IE448CJD55BC73EFH9G9H"}, /* 56: 8 x 5, down, 13 faces, 0 blocked, stuck 5%, search 20 */
    {8, 5, 1, "zdttzMHdhtzMwKwGhczctKAHtAAAAMGdtGAdHMHG",
     "F21184E2CI84737GCHFHI3BA6JB55DG960J9ADE0"}, /* 57: 8 x 5, down, 11 faces, 0 blocked, stuck 8%, search 20 */
    {8, 5, 1, "GcPqAiKiPcAGkGGokkkqqqkPcGcGKkPouuPPuuDD",
     "543D6AJA376CB88122F00DFH457CJBE19IHE9IGG"}, /* 58: 8 x 5, down, 11 faces, 0 blocked, stuck 8%, search 20 */
    {8, 6, 1, "BMIxOaIfBFAMOEIciAcMIakkBBoEMFEfiFoxAcEifFAcicfc",
     "7CKJ58F17IMB5GF2HMLBK844009GCI61HD9JNA6E3DNLEA32"}, /* 59: 8 x 6, down, 14 faces, 0 blocked, stuck 0%, search 24 */
    {8, 6, 1, "oAiuIiAIiHiuJArIzHsIzJNIjoAIsoGGjzrrrGNGiorrizJJ",
     "NFEK49H4GA9KDHJ23AL1MDC208F1LNB503IJIBC5E866GM77"}, /* 60: 8 x 6, down, 13 faces, 0 blocked, stuck 2%, search 24 */
    {8, 6, 1, "JqoayudyMq#ynKuyJoJonunyJaauonKdMqyao#ojfqqnnfqj",
     "D02CGJ5AH0#A39L8727M3L68DCEJM695HBGEK#K4IB1FFI14"}, /* 61: 8 x 6, down, 12 faces, 2 blocked, stuck 5%, search 23 */
    {8, 6, 1, "MFarKKrJMaPIMI#rJJcJIIMcMPcaooecM#rJfoeJfoffoaoF",
     "4E7H88F61JALMI#FKD0DLIMB1A07C5GB4#HK35G69C392J2E"}, /* 62: 8 x 6, down, 12 faces, 2 blocked, stuck 8%, search 23 */
    {8, 6, 1, "mrmnzkrGmipp#nzhltprGhnnitmmrzrkmzp#izrIlGGkizkI",
     "L1LCEG1D7AMI#CEB9KM5FB00AK225J367HI#8J349FDG8H64"}, /* 63: 8 x 6, down, 12 faces, 2 blocked, stuck 8%, search 23 */
    {8, 6, 1, "CFLCDleAbACjDjGlFLDmyFFmGLDGybdeGALdAddCbCbbbGGC",
     "LGNJ6CIAK7LF6F5CG09M422ME09D4K3IE7NBAB38HJH11D58"}, /* 64: 8 x 6, down, 13 faces, 0 blocked, stuck 8%, search 25 */
    {8, 6, 1, "imIOgigxsNmNfdcOffmsc#mxIx#HHfHINsmxiNHdIsgffgmi",
     "37FC4340LG7GDIECHHJLE#J0A8#22D9FK168MK9IA15BB56M"}, /* 65: 8 x 6, down, 12 faces, 2 blocked, stuck 12%, search 23 */
    {8, 6, 1, "zwGbcwglzuguIbIgybgmGuMuwtlmGGMlytltzbztDDGcwGtt",
     "FGL6EGMBFJHJCNCM36HK8D0D571K8L0B371I9N9I22AE5A44"}, /* 66: 8 x 6, down, 14 faces, 0 blocked, stuck 15%, search 24 */
    {8, 6, 1, "xxKxDiarGiKDIICrzxfLIyxLGKzzLiILxfzLKyfBifaLffBC",
     "4HGL5NI308F52C93KHE62BL10F7K68C14E7DGBJANMIDJMA9"}, /* 67: 8 x 6, down, 14 faces, 0 blocked, stuck 20%, search 24 */
    {10, 6, 1, "LzggLcllLLcqlc#lzymHPycwcLlzmlqwzNggLzyByNBNc#HwmyPNqyqDwmDz",
     "5RFF5ABBDJAMK6#9RGOIH7NP6D98OKMEL233J81S74S2N#IEC1H4QGQ0PC0L"}, /* 68: 10 x 6, down, 14 faces, 2 blocked, stuck 5%, search 29 */
    {10, 6, 1, "FmowFOAuwwwhmh#uAOuwvppvpuAI#Am#pOvoIoOxoOhhv#FOmwpAoFopAExE",
     "EAGJELHMBBJNAN#2H5M3ODC8D2P1#7R#CF8Q1Q54KL66O#9FR3IPG9KI7040"}, /* 69: 10 x 6, down, 13 faces, 4 blocked, stuck 10%, search 28 */
    {10, 6, 1, "hECJgJJGCwgEtJz#mEteeE#JJegetzetzggoo#zogwGw#ePPEEdwhdGbmGbo",
     "MCH2K77FHD9CP2E#AIGNBI#448KNGLBPE3311#LR9JFD#8OO55QJMQ06A06R"}, /* 70: 10 x 6, down, 15 faces, 4 blocked, stuck 10%, search 28 */
    {10, 6, 1, "klFlCqCGFkvD#lGzqDmmvGhGgFFkGqvhlhMgvkqDvMggkzh#qGzgzqrrgvDk",
     "94J4MIMAJ65N#CHRINGG5FBFD887AQ3OCBEDS7PK3ELL9RO#PH101Q220SK6"}, /* 71: 10 x 6, down, 14 faces, 2 blocked, stuck 15%, search 29 */
    {10, 6, 1, "oiCeoOCOGwkBLBkkwH#xLwGOeooewGjCCKBkCieKi#CBjjxBHKOiGGjGKBKK",
     "5FCL5HQH3KBJDJBEA9#GDKRML22IA3SC7POEQ8I6F#7ONSG091M844NR60P1"}, /* 72: 10 x 6, down, 14 faces, 2 blocked, stuck 18%, search 29 */
    {10, 6, 1, "sJDaxjuJMrrjniuxur#DuiDMxx#ajMntM#jxDvDts#iJDrjvjsuuivsxJiiv",
     "236HK483CGE45AF1FG#J8A6700#HIC5O7#M1PDJO2#QNPEMDIRLL9BRKN9QB"}, /* 73: 10 x 6, down, 13 faces, 4 blocked, stuck 22%, search 28 */
    {10, 6, 1, "nLmvpLpvbmnpnf#NwNNpELeeE#wvnddpNw#vELbdEwwef#jdLwpdLeDDffjd",
     "501GC0AGD156K9#LJ44A3Q22P#JMK8HCLO#MPFD83II79#NHFO6RQ7BBEENR"}, /* 74: 10 x 6, down, 14 faces, 4 blocked, stuck 28%, search 28 */
    {10, 6, 1, "EEhHemebhHehGBB#xxLGhLGLCE#eimGLr#xCxCibGGrx#vvhihHCxmyyEHim",
     "77A1MJOR61MN0EE#DDGPA90GQK#O83P95#IFIQ8R225C#44NL6HFCJBBKHL3"}, /* 75: 10 x 6, down, 15 faces, 4 blocked, stuck 35%, search 28 */
    {8, 6, 2, "cctMhAhtHuuKAhvOHvvh#vMMAAv#tcuMMOzOOcKKBKzBvutM",
     "44A7CECAF22KEDMIFBBD#L7166M#98H1JI300855GK3GLH9J"}, /* 76: 8 x 6, left, 12 faces, 2 blocked, stuck 0%, search 23 */
    {8, 6, 2, "nPeArAeEeam#rePJJeOmOrrAmmneHHalnlcc#OEJnaaJPOAP",
     "DBJ0M0J7LIF#MHB34HGFG88966DLEEIC1CKK#A7312245A95"}, /* 77: 8 x 6, left, 13 faces, 2 blocked, stuck 0%, search 23 */
    {8, 6, 2, "bbcActAMwwqq#twmjjwmdbdmMjNNqMMmqqN#mmAANjNqJJNb",
     "CC202B0ELD44#BL6FKDIHMH6EF38G55I113#JJ998KAG77AM"}, /* 78: 8 x 6, left, 12 faces, 2 blocked, stuck 2%, search 23 */
    {8, 6, 2, "qAawwxqaqitccxcEKssAAKdmcEcAKmddAEKmFFmdKEiqtKcA",
     "J5FBB1NFJMK8816I722CL3E9D0DL39EGC0HA44AG7IMNKH65"}, /* 79: 8 x 6, left, 14 faces, 0 blocked, stuck 2%, search 24 */
    {8, 6, 2, "rMMMkjJjhjDhjJxLrxD#JpLphhhD#JMkhnDkxrrnnnAnnkAx",
     "39G9BDEL5L14DEK83K1#A787254H#AGB2IHMJ66FI0C0FMCJ"}, /* 80: 8 x 6, left, 12 faces, 2 blocked, stuck 8%, search 23 */
    {8, 6, 2, "uuwNjNxAjAxjaNwAjucucajGxxLHcHAGLuaausscNGaGajAA",
     "55CNH3G2KLGME3C2KDFD1EM744B818L7BA66A00FN9I9IHJJ"}, /* 81: 8 x 6, left, 12 faces, 0 blocked, stuck 8%, search 24 */
    {8, 6, 2, "sMfMHnnnEf#ssEOnefsEsrffEEEHdrHzetdsj#zHrfrzjtzO",
     "K9E9DAA2ME#JJIL26CKIH8BBM33F47F56G4H1#0D8C751G0L"}, /* 82: 8 x 6, left, 13 faces, 2 blocked, stuck 15%, search 23 */
    {8, 6, 2, "LLGIeBBeoawooaLLorwwGKwGrHooNererHNLLkkkNHHNGKIk",
     "H9JG70072N8CCN9H2L8MJDMAKB66F4L4KBF33551IEEIADG1"}, /* 83: 8 x 6, left, 13 faces, 0 blocked, stuck 18%, search 24 */
    {10, 6, 2, "pppLCmpsspwwpCfbImCfdsqqbb#I#Lw#b#jxLxwsdsCxbbPjLxffPLxxsfLf",
     "0FF9250MOQIRQLGJP52G16KK8J#P#9I#8#4ADHRM16LE33N4DABBN7EHOC7C"}, /* 84: 10 x 6, left, 14 faces, 4 blocked, stuck 5%, search 28 */
    {10, 6, 2, "PgbygPiyroybbMwwiOibs#cMs#inbbswPw#rnv#onvPvnrnycyswnyOrPPvw",
     "46D164H1OCQEE577LNLDG#P5B#HKFFBA2A#OKI#CJ929R0RMPQG8JMN033I8"}, /* 85: 10 x 6, left, 14 faces, 4 blocked, stuck 12%, search 28 */
    {10, 6, 2, "iDInEltIlEcdddopopDottiEEi#xnixKK#icffdxjjxzippoDlltzcctnnDt",
     "DC9801L910N22QRIRICJ55SGGS#M8DPKK#6N44QMEEPF6HHJ3AAOF77OBB3L"}, /* 86: 10 x 6, left, 16 faces, 2 blocked, stuck 12%, search 29 */
    {10, 6, 2, "PGGPIGDppIJxxOOj#jGIpvdvJCCyCIpvdILDCdyCvDI#CDfxdBJpBDJLfxpD",
     "B6IBHIL00GF22KKM#M6H394CR77N8G3C4QPL8DNJ9SQ#JS1ODAFEA5RP1OE5"}, /* 87: 10 x 6, left, 16 faces, 2 blocked, stuck 12%, search 305 */
    {10, 6, 2, "IwNeIoPeaPNwuNodzozadd#FzdzzozGuNIINo#joGejjneGyyNGyFajuanyu",
     "M5DRMNJS6J85IDG37G9622#O7300N94I8FFQK#HKPSBBLRPEAQ4EO1HC1LAC"}, /* 88: 10 x 6, left, 15 faces, 2 blocked, stuck 15%, search 29 */
    {10, 6, 2, "AjjiqjNHiHjNqccHHjxKvx#kiiPNpkvkaavPv#aAAvvKNkkccKHAKkAjHAap",
     "I33H5GPOHOMR5EE99GSJ0S#Q224RFB0QCC646#NIK77DPAALLJ1KDB8M18NF"}, /* 89: 10 x 6, left, 14 faces, 2 blocked, stuck 15%, search 65 */
    {10, 6, 2, "ybbyedbddiPiiyydEEdbyEEHDHdy#vP#OeDeOeOvOlOleoPbivbooGePOGov",
     "9GA9FPAN8B76611PJJNQRDDK0K8R#47#3I0ISFH2SE3EOLMQB4G55COMHCL2"}, /* 90: 10 x 6, left, 14 faces, 2 blocked, stuck 22%, search 30 */
    {10, 6, 2, "EuEopLommaauIIannEMnjpau#Er#nLrj#uv#rIIjrCCavmvmvarmjjMjmLLr",
     "H0QNOPNJJ2B066BGGHL1ROKE#QD#1AIM#E7#D44RI55KC979C283FFLM3AP8"}, /* 91: 10 x 6, left, 14 faces, 4 blocked, stuck 25%, search 28 */
    {10, 6, 2, "lghONzhgwzhgCPzxbNzwPbxMM#Nx#Oh#Ng#xNPPxzbohhwvzxblbCbvOowNO",
     "239OBP93QP5INEJ8CBJQEC877#6G#O5#MI#F6AAFKR4DDHLKGR20N0L14HM1"}, /* 92: 10 x 6, left, 14 faces, 4 blocked, stuck 32%, search 28 */
    {10, 7, 2, "ssifAaivwAzqwv#kslasskDAAEEwlfaaEzzIwwIIAIIAfkkxzfEppzw#lDsIqzpqlpxqEE",
     "8A7I9K7RH9OXGR#VUWK8AVQ00EEGWB44J33C225NFC5FI11SLBJT6LH#MQUNDO6DMTSXPP"}, /* 93: 10 x 7, left, 16 faces, 2 blocked, stuck 10%, search 34 */
    {10, 7, 2, "HhBwwHBjNNNE#bbHjrehwbrrEirrrhHI#ielE#wdHINNIINbjhwHhjdub#bjduojEolwhd",
     "MJ0WWS0H66GD#33SVQE7BR44D5NNQ718#5EUO#KI1A228AGRCLBMLCPTF#FHPT9VO9UKJI"}, /* 94: 10 x 7, left, 16 faces, 4 blocked, stuck 12%, search 33 */
    {10, 7, 2, "xjyxyfsjvFoIyyocscGINGGi#JjoooNvsIipJpoIisKjJ#jsIjKiiJfGfNIsFNKGiFKFfG",
     "P54P4R05UKSFNNSJ0JTQ322O#7XAAL3UBFVG7GL8HBMXW#C98CMOVWITRDQ91D6EHK61IE"}, /* 95: 10 x 7, left, 16 faces, 2 blocked, stuck 25%, search 34 */
    {10, 7, 2, "edyBmmAFrBHNHxFHeudrMDDNy#NNyAy#b#bx#x#eurHm#DFFMyeMrxMxmbbBBrurDuuxyu",
     "1EN277HSC2B5BJSDUVECI445L#00NHL#A#AM#M#UVGDR#Q99TK1IGJTPR33OOF8FQ66PK8"}, /* 96: 10 x 7, left, 15 faces, 6 blocked, stuck 30%, search 32 */
    {10, 7, 2, "LLFrPnHGObnccMtJGFffn#ctFvGbnFPHJFvHttOGGPvPrHJP#FLGvLrMwwrLcMnfMLfJnP",
     "KU6RWLVTA43E1XCG97223#1CQ5P4L7WVMQ5D88A9PJHJIDMB#6KTHUIX00RSENFONSOGFB"}, /* 97: 10 x 7, left, 16 faces, 2 blocked, stuck 40%, search 34 */
    {10, 7, 2, "kNtnbNNzbrcHtFpnnrrFHAFFeFt#tzpwwcncpFrrPH#zHAzpHPAzAttNzPPrHkennkkccc",
     "SVIJ2CCE2DWPMRTJ93354A88ORM#INTBBF9F6511UP#NGQE6GUAXQHHVX77D4LO00LSWKK"}, /* 98: 10 x 7, left, 15 faces, 2 blocked, stuck 45%, search 34 */
    {10, 7, 2, "yfyjAGHzzxdse#AseG#HddPPeGezMxxjjdoPeMoMsfxPxypppMA#oHMp#eHojGAAyAMszx",
     "4C4MOF5EEU28H#NLTF#52177HPTKAGU001SBJA638CWBWRD993O#SIQD#JI6MPVVRNQLKG"}, /* 99: 10 x 7, left, 15 faces, 4 blocked, stuck 48%, search 36 */
    {10, 7, 2, "mPwwPsyyNwMEDdEEybGNKKNGtb#s#KdMyDdtbbrGN#t#tyyGEEMmNrMDGEbNsdMsGDMKbw",
     "IUWFUMVOTFDQR911O7N088ANB7#M#GHJVR9BCCPSA#L#L55SQ4DITPE3K4206HJ6K3EG2W"}, /* 100: 10 x 7, left, 15 faces, 4 blocked, stuck 57%, search 33 */
    {10, 6, 3, "jjjHuHNNuvxxNtfHHfokv#JktPkkNNvjgJkPfd#xvHoxvgkfdNvgoHyogyoo",
     "00EJ9JIB9ORAIK8118SQ2#MQKH33FB5EPMNHCG#A26LRO7NCGF5P46D47DLS"}, /* 101: 10 x 6, centre, 15 faces, 2 blocked, stuck 0%, search 29 */
    {10, 6, 3, "HpvoooPvonNpOupp#NunLPOzokkNvzcLLoniHivDNHv#nNNnHnLLvzHDcHLz",
     "BNEIJJQEI50NCO66#AO58QCM2DDAKMGLL21H4HKR0F7#133P4P897SBRGF9S"}, /* 102: 10 x 6, centre, 15 faces, 2 blocked, stuck 5%, search 29 */
    {10, 6, 3, "DdKtAKtDjdIdaKAdfakAkdaaAup#NNIq#MKIIjpfjqjqqMtdutMaaupupqMq",
     "JBQGLQGJIB6MSPNM5ROLODFFNK3#4461#HP22I35C1CEEA7DK7HSR08089A9"}, /* 103: 10 x 6, centre, 15 faces, 2 blocked, stuck 5%, search 29 */
    {10, 6, 3, "kknivjjqqjriMMMnuikkjnrvMrttrkMuzNujdiFkrFzNujnibiuFrdMFdubd",
     "99C5BJDHHJ75336CM8E0DN7B6I44I0QMT2KRP8GELST2KRNA1AOGLPQSFO1F"}, /* 104: 10 x 6, centre, 15 faces, 0 blocked, stuck 5%, search 30 */
    {10, 6, 3, "gCbbxcxgxiFqyGIiECCxcaGIqqcFIFbcqbiDDIIIFaiiBCCBNCcciyNaaENN",
     "CMGIOLBCBT5HQAETDMPO9FAE00R636I9HGKNN2235F774114SPLRKQ8JJD8S"}, /* 105: 10 x 6, centre, 16 faces, 0 blocked, stuck 8%, search 30 */
    {10, 6, 3, "dBgPHtPHtntoBHCBCbtgtKK#KiiKbnhphpdd#hodgbgoJBddbotgboJhobgH",
     "D471C21M2F6N4MHLHK67500#SBBSRFPQPQ88#O9DEIJNAL33K95JRGAOGIEC"}, /* 106: 10 x 6, centre, 15 faces, 2 blocked, stuck 10%, search 29 */
    {10, 6, 3, "yqFnnFOFOopoEFMFfqpBoqBpMpfqqyyIIBBoFsIyppyMyHssrIrHaMEqrrsa",
     "FK69968L8TSTR747IJSPDKPH4HIJQE3AA55DL0OE223CFMG01OBMNCRQB1GN"}, /* 107: 10 x 6, centre, 16 faces, 0 blocked, stuck 15%, search 30 */
    {10, 6, 3, "LbCbgaooGgbappCwLwwGIadbbkKgogpwccwodoLKdpakLkdwFidbkIKiKFdo",
     "S9C9BDHHEBQDIICTONTELAFMQP47578R00R212S4F8APOK1N3JGMKL6J63G5"}, /* 108: 10 x 6, centre, 16 faces, 0 blocked, stuck 18%, search 30 */
    {10, 7, 3, "dFodDjdhFFHooEDjEyDhhhAjHbb#EFAOO#jH#rrrvD#AAbhEorveErEbejjoHGorydhdGd",
     "A1SA3BHQ14IEEV3BVUFW225LD77#G4588#LD#00CRF#NNMWGPCRTK6KMT99PIJS6UOQOJH"}, /* 109: 10 x 7, centre, 16 faces, 4 blocked, stuck 8%, search 33 */
    {10, 7, 3, "eceAMAecAwEBMccc#ANBm#JNtIcEbMNmBitwIwNbBBBttmwE#JJEE#imEwMJAAefttfeew",
     "EAEW79RA9CTV711J#WGVK#PLQBJS8MLKDFQ4B5G8D662234H#UPTH#F3S5MU00NIOOINRC"}, /* 110: 10 x 7, centre, 15 faces, 4 blocked, stuck 10%, search 33 */
    {10, 7, 3, "NIAnMvMeIncebA#DevvCHutvvKKDbfHNKntcKcCKnHHDDIICCtAHHKe#uuCeeuAnCcntfv",
     "RMU8CHCEM8QJXU#FE3346NPWW11FXS6RA9KVIQ4I97722GG00KD55AL#BBOJLNDTOVTPSH"}, /* 111: 10 x 7, centre, 16 faces, 2 blocked, stuck 10%, search 34 */
    {10, 7, 3, "rGttsxfxsrAoroPGlArrl#PPoxcHolmGGGPCcCCfAAmGACsm#PlmdoodttsramHHxAPHam",
     "7SRRA1G1A8MEOFQSBV87D#66FT2LEBXHHIQ42WWGVKXIK4NU#PD0C99C33NOJ055TMPLJU"}, /* 112: 10 x 7, centre, 16 faces, 2 blocked, stuck 12%, search 34 */
    {10, 7, 3, "ccutwwFtuFwwcewA#Ffctc#wgfegtFxffugfeAxecxauItj#gIxja#lvvltIIujxAujfAx",
     "084QBBKL41SS8WPR#KOGQG#PJF9ML1H226JF9RVW0HU6IAT#MDCEU#5335AIDNTV7NEO7C"}, /* 113: 10 x 7, centre, 15 faces, 4 blocked, stuck 15%, search 33 */
    {10, 7, 3, "xsjxIFdDzsDj#HHF#ABtDxkkxdzADtHJkksdzjIkItJJsJFjzBjht#HBD#IBBdjFhhBDhk",
     "2842OQWKP8L4#33Q#0FALD11DBP0KATH775BMJICOESH5SRJMFVUE#TNG#I99WVRU6NG6C"}, /* 114: 10 x 7, centre, 15 faces, 4 blocked, stuck 18%, search 33 */
    {10, 7, 3, "kCGdJvrdvkcGrjcPrfhPPhr#JckkxhPc#BkJx#jBhkmBfx#xxJCfmGCCBvGBrxPdPBdvfr",
     "9BN6JC46C9FV2LOE4TPE3U2#KO88DU3F#AHKD#L5PH0ARW#GGJBR0V11QSN5MWI7IQ7STM"}, /* 115: 10 x 7, centre, 15 faces, 4 blocked, stuck 22%, search 33 */
    {10, 7, 3, "zuiOoGuGPzA#diGLzdPyzdyy#oiiAOKxuOxddLyouzuOy#KKdoyrLzKAOt#GhhrLuAKtOK",
     "0NI3K7N72AG#MIOUAB260M11#J44G3RT85T99U6JSD85V#WQBKVLEDQCPF#OHHLESCRFPW"}, /* 116: 10 x 7, centre, 16 faces, 4 blocked, stuck 32%, search 33 */
    {10, 7, 3, "yzAIrHmrycMyIyIrdOOdeHzMIAOmddO#EEmAAe#emdcmAeEeEBOAGGyrrEemEttOrycdcB",
     "AMN6CWPCAKDB6B4XO008LWMD4JTG8O7#22HFF3#5PUKHJLV3E17N99RQQE5GVSSTXRIUI1"}, /* 117: 10 x 7, centre, 16 faces, 2 blocked, stuck 32%, search 203 */
    {12, 7, 3, "dduMndLMbrgPuGNbNLbLgg#OiGbciilrOlkku#OOLiyyOr#OhhtnchrnccPtd#gyyLnLhguKdkuGukGdrKrg",
     "MQ51dMS1RFZIX4PLPSRN00#7U4LYUET89TCC5#AANE6698#7bWOVYbFV33IOG#c22adaWcXKGDJHJDHQBKBZ"}, /* 118: 12 x 7, centre, 20 faces, 4 blocked, stuck 2%, search 248 */
    {12, 7, 3, "vvlcKMcllfltNdKyyMd#mmNtizlMzzfccyKydkkqdzdOOvKMziqqvzNNKKvlNyyq#ffvdqoONoMccOAooAMq",
     "22F6BW6FVUPJ0cB77Qe#EEYJZLVQOOUIIR4Re99MdDc55X4WLZGMbD3388bP0HHG#aaXdCNSYNKAAST11TKC"}, /* 119: 12 x 7, centre, 18 faces, 2 blocked, stuck 8%, search 41 */
    {12, 7, 3, "PoEdoMfPmMmyd#EvAAJJIIvjwyjjvIINPNEjhNJEwwojjKKNNwyoftotPPyMohMMMvvNPJ#tEtEmKmvtKJJt",
     "DHXLHBVDFBFQL#ZWaa4422W1bd55eMMcP3O1JIYZUUE77663IbdTVGEGPKQNTJNSSeAcK0#CXCO9R9A8R0Y8"}, /* 120: 12 x 7, centre, 18 faces, 2 blocked, stuck 10%, search 41 */
    {12, 7, 3, "mMnCMClCGlmwvllNNGllI#kPv#PgnCIwmsswGgPHbbHrkPLmIbNNbGbrbw#wCB#NvNgCLIssBgsrvgsgmrmw",
     "92MJ2ZAJaA9W0LLIIRPPK#d50#BQMcYUD113RQBTGGT6d5XDKV88VaF6FW#3Z7#bHbScXYCC7NE4HNESO4OU"}, /* 121: 12 x 7, centre, 19 faces, 4 blocked, stuck 12%, search 40 */
    {12, 7, 3, "rgvInNNHCnvrC#CnIwIHCnwagaKKMwqrrDgHaqgaMNmIwnvvnqDNNqmmDIqMhmmMNqMmCw#HvIhfHDMvHwfC",
     "8IBTH557EdK8W#WH2127EbeZQZAAX1aYYNI0VPQVXJDRedKBbaMJG3D6MRPcUFFcG3L6S4#0OTUC9NLO94CS"}, /* 122: 12 x 7, centre, 17 faces, 2 blocked, stuck 15%, search 333 */
    {12, 7, 3, "sHJrseaaekwwrHrKKDwss#keddH#HDDeak#OrOOLbLzwJkwxJ#bbsbxw#OdexO#ksDraaeOdDkKJKDraxxzx",
     "ELaOEZXMPKDDBLOAAVR77#K133S#SWWZM6#JBUUcIcb8a6RTF#GI4GY8#N21HJ#549QCXPN2950F0VQCHYbT"}, /* 123: 12 x 7, centre, 16 faces, 6 blocked, stuck 20%, search 39 */
    {12, 7, 3, "kimvyuKayikmlKaaafz#llzuluzl#NffzeifCCK#Nake#veuzaKhhIk#iyCNzNNt#IjkkeCmmuNyCjlujjCt",
     "6ESR9GLc9E6DaL44cbH#55N7VGNa#K00H2YbTU3#KCF2#RI7MC3AABF#YZUWM88X#BJQQI1DSOWZTJVOPP1X"}, /* 124: 12 x 7, centre, 18 faces, 6 blocked, stuck 25%, search 40 */
    {12, 7, 3, "iPcymDmtytaccatzyzOf#atyuEaEPPoBBo#fufyO#Pf#cBBNu#PBDffuucyNzPu#gBttENbczNbDgiEiiaaD",
     "aOP73Y3UNU6PSGVE7EXH#6VBFKGK44D22D#QFRNX#OH#Sbb8L#MTYRQJJIB85ML#9T001cZI5cZW9A1AaCCW"}, /* 125: 12 x 7, centre, 18 faces, 6 blocked, stuck 55%, search 39 */
    {12, 7, 1, "BMzwDtaKMaaBKwtuqNzaMKNhq#hzhpp#zz#amDhMhNplllaqP#up#mAmml#NKuquwwAzPAKhyqgMyAgDMDKq",
     "GD5SU6c10MMG9S6BQPZc01E4b#358CC#VV#AYU3D8PaHHLAbT#Ba#YFWWL#E9OQOKKFZTRI42NXJ2RX7J7IN"}, /* 126: 12 x 7, down, 19 faces, 6 blocked, stuck 0%, search 39 */
    {12, 7, 1, "xJIkKdrDJiDIdo#rkiJJkpKpxkorIpdKcpmcc#cDDIzJcJ#zhMkMIrKppKxzhDxdmzKxd#xIiokocDMhhidM",
     "4RH82MX0DC0AMV#Z8CRDcN2U4cVZHUdJ3NaKY#Y55TEW3W#BPIObTX199JSBFGQLaE1Sd#QA67O7KGbPF6LI"}, /* 127: 12 x 7, down, 16 faces, 4 blocked, stuck 0%, search 40 */
    {12, 7, 1, "tCFKAFCFFAAKhKN#KplIlMMpKINltljNIhpJhoNNCCJjCGFAjINCAjGIIlyJhooJAOoF#OyGttGOKOoltotp",
     "YaEI7BJBE73U0TP#UOSWe662IGZeYXKZWcORcF4P88QbJVd3KG4a9bNLLS5R0DFQ9HMd#H5V1ANCTCMX1DA2"}, /* 128: 12 x 7, down, 17 faces, 2 blocked, stuck 2%, search 41 */
    {12, 7, 1, "wtjcrIaqtulLrajw#pqBLurIrLccuIBI#IpiwuaMpIMliMiruLj#LiBDDcMMMaicLDB#DrwiDjjBjacpDaBu",
     "U1SDRKAW14dHTAMU#cWXG4QKTG22F5XB#BcZV7JNE5adL6LQ7HS#8YC99D6aNJYb8OC#IRVZO0MP03bEI3PF"}, /* 129: 12 x 7, down, 16 faces, 4 blocked, stuck 5%, search 40 */
    {12, 7, 1, "kagsMKsgmFxxnaCM#ECzCJPzugmgEKPC#k#aumFkEzmPaFkkk#J#ECJamPKunzJxFPC#FaPFmnnnxKxnKxKu",
     "3HA01c0JbN88UHI1#OILRVQLSJZAacQR#K#TSWXPaFbGDNKP3#6#O5VDZG94UF6MX75#ET7EWBYBM2CY9C24"}, /* 130: 12 x 7, down, 16 faces, 6 blocked, stuck 8%, search 39 */
    {12, 7, 1, "kyFFaLkznHgakJsFLgLztPoNgJg#ynOOkaKPtLgP#Kn#aosKzognFKnF#ooJHFPOatnLNLoNzGkOstasNGkJ",
     "IRTABSC3bcMBI0ZANUN3XKV5M0U#RQ11CdDKXS27#EP#dLZD9V2bTEPY#L6JcY74GFQaWa659OH48FG8WOHJ"}, /* 131: 12 x 7, down, 18 faces, 4 blocked, stuck 8%, search 40 */
    {12, 7, 2, "CmBCNlbBGhhfwaBN#lwGNlbmmCBCjaN#mhhsccB#OlON#smmGGjj#iiBbNsscffjawc#sicwlfiGGcwlwabs",
     "3IA3H1FAKGRSVWYb#1VKHXF0I626DWO#UGRc5L2#9X9O#cU088D4#CCYabTT5JS4MPL#QENZ7JEBBNZ7PMaQ"}, /* 132: 12 x 7, left, 16 faces, 6 blocked, stuck 5%, search 39 */
    {12, 7, 2, "gJtBxEtbABxEgpEoBApAFAJBwoE#dBpdH#dBattwpJrooFEEJo#tdrrd#AgrrrggowwHHxgAabHJxdwHwtHJ",
     "4JN5GDNTKSGDCd37aI1IZKcaP73#US1FO#F5Q00PdcM66ZAAJX#WLMRU#BbR224CXHH889bBQTVY9LEVEWOY"}, /* 133: 12 x 7, left, 16 faces, 4 blocked, stuck 10%, search 40 */
    {12, 7, 2, "CbDEECdllfbbLDLd#wwnfILEfLn#COOPI#OoCfnwwDdonMMbGI#iiDPO#bEGGILfLEn#OOnGCGddwfEwGdbC",
     "EQH88EWCCUQbNHNW#BBZUSY7aYA#P44MS#cGVaAKK9IG5113OJ#DD9Mc#370OJXFXLZ#6650VT22RFLRTIbP"}, /* 134: 12 x 7, left, 17 faces, 6 blocked, stuck 15%, search 39 */
    {12, 7, 2, "hfIxBfBKootIjxtbGjcB#gKov#xxj#ywhgwIOjgGBcycbttoOygovG#ogh#Ctgy#fcGvvfIxtxfvhfyvBBCy",
     "RBVTABAUJJWDcTWKGOaS#5UPX#11O#FIR5IVQcZGSHFHK33PQ8YNb9#NZL#C2Y8#4a9bE7D6267EL40XMMC0"}, /* 135: 12 x 7, left, 18 faces, 6 blocked, stuck 28%, search 39 */
    {12, 7, 2, "cNfcceMMOANcipp#tetyAArrFctFooyhAAeio#tfNOONNe#cmOphphirtmAoiorMhrFt#DOrimoMNDOmFmmi",
     "YZTYb2EEdGS91JJ#M2M4GU77Ob6OHH4W885DC#6TA00AS5#9cdIBIWXVFcUN1CRQBRKF#aLVX3NQZaLPK3PD"}, /* 136: 12 x 7, left, 17 faces, 4 blocked, stuck 30%, search 40 */
    {12, 7, 2, "wlBBDIlOElEtwmOIOAEAl#tiFKmBtCEssIwws#CtwBtiEK#DwFDeeFFNltCssl#IBCNKKsNEBKKONDONNOii",
     "13LLMETR535CaKREXJ8JO#CSFbKYHW8BBcQQ0#WHaY7SdP#I1FM22DDZO7GA0T#cVGZ99A6dVbP4NIX6N4UU"}, /* 137: 12 x 7, left, 17 faces, 4 blocked, stuck 32%, search 1000000000 */
    {12, 8, 3, "pddNwdGpsjsKggmmAAGPN#vdwH#JJNvvNNGndmJJFAPPNGmPFmHJGAnpvoosHCConnwwv#wGJB#oHHBmpvjKwFHCPsKKPCdF",
     "bFTXQTGbdOI2SSAACCGK4#PF19#DeX8804gZf6eRiNEE0WVK36jRgNZUP55I9MM7HHLLY#QWDh#7ajhVUYO213aBcdJJcBfi"}, /* 138: 12 x 8, centre, 20 faces, 4 blocked, stuck 10%, search 46 */
    {12, 8, 3, "ceonFDcFjPHjdkH#n#sfcD#DozIDDgsfczdekzI#cCHfkknCdCgcCfnn#sdsttjeAkHznAePP#joAk#P#zdPdoCACzjjPADA",
     "MQOK1UM1VTRA0JR#K#aSHU#7O84FFgaSHeCLJ84#NbXG99fcZcgNbG66#DCDPPBQIhXWfdLT5#BYIh#E#eZE0Y2d2WVA5373"}, /* 139: 12 x 8, centre, 19 faces, 8 blocked, stuck 15%, search 44 */
    {12, 8, 3, "kpOIqcqsxcyysscIs#ICaxqIy#pyDqJJEDcDvGCyDovE#OcIEJG#qqaGddJxEcGnnnndDO#xkooJEE#dDyaIoxxndGJnGOad",
     "MTdgAJAeVJcQDDHge#XWZVObQ#T3NOGGI858YaW3NfYI#R5X0hC#KKZaEEh70HCUUSSLBd#4Mf69FF#LBc1b674i2P9iPR12"}, /* 140: 12 x 8, centre, 19 faces, 6 blocked, stuck 20%, search 45 */
    {12, 8, 3, "ezLzkCCikLqiyLaCmaajoijyjpp#jxxippjoykCCd#iiN#qlLk#ldy#NmNNyoLdpCBqN#qqymllmxxekLqpNaeBIkIaoadej",
     "SIcIEDD6EcP625VdYaaQMg3eiAA#QOOHLL3M29JJK#gHX#Ph59#hKe#CYXTGUNFbdWfC#BBGR44R00SZNfbTV8W7Z71U1F8i"}, /* 141: 12 x 8, centre, 19 faces, 6 blocked, stuck 22%, search 7267 */
    {12, 8, 3, "frffeAAMzAzxx#erNrNfApzGMK#B#rnyyApdKyudnMMneejGGBxMfjBnNNjdGnneGjB#f#dzGccuAMcjez#pydjrxrcdpxzx",
     "haFFiHHgYeBSS#dab2bXWZBcJ6#D#28IIWZV67NV811QPP3UcDTJXf0QOO3GLEEdUR0#h#GYL44Neg5RiM#K7Af9T95AKCMC"}, /* 142: 12 x 8, centre, 18 faces, 6 blocked, stuck 32%, search 127 */
    {12, 8, 3, "oAyAAfihaAfCaOf#JKiaaCaCMOkfKK#KciakMfKme#cwe#hioe#iie#KewEJkoEmJ#oMJyEyAoCfeEmM#kkkyMCMwwyyomCA",
     "3GLGENTZIENSV0O#XMThIYhYD0UfHM#edbV5DOHB6#dWQ#Zb3K#226#eQW8cU1gBX#aAcR8R914fKgPA#FF5C747JJCLaPS9"}, /* 143: 12 x 8, centre, 18 faces, 8 blocked, stuck 60%, search 44 */
    {12, 8, 1, "vzzPhrHfKgrlvlm#hzmfm#HHKwgIghIzKKHIvhqwIhffmPrkKBzwDkHBPrPPPBzqqDKDDhHkvC#IBkgq#IgkvCkAAvCgwCAA",
     "Q99dPTi8fgTH4HZ#PUZ8O#SSDaLWL7XUKKcWQjeaX766OhbVDCEJ1AiYhb33dCE551fMMjcVNG#IYARe#IRF4BF00NGgJB22"}, /* 144: 12 x 8, down, 19 faces, 4 blocked, stuck 10%, search 46 */
    {12, 8, 1, "gogCgeLfocLIfww#xeuMoCebGeFrFG#PIKLCLeFGCrILMK#Fc#KcCPMMbIboLxfuF#xxooIfgcfKxCgg#bIFcPPxGfceuuPP",
     "UfFGFPIgfdhZb44#EVN6RKVOXeWLBD#AJ9IKCPWD7LJh69#0Y#Qd7ASSOZ3RCiHNB#55cc1gUTHQEG22#310Y8aiXbTeMMa8"}, /* 145: 12 x 8, down, 18 faces, 6 blocked, stuck 12%, search 45 */
    {12, 8, 1, "OMMdzAABAmPeCNn#zsdGAA#emNNB#sssGnyyOee#mAsnncGcBBPNBeGs#cCmeCnlBcm#CCzCl#PPyymP#zacyGNPadGncdyN",
     "599A0ZGDZgSOc8d#07AQHH#Og8CV#7BMQU22544#hGBLLRabDVfCYXaM#RehXcdNYWP#IIJeN#fF11PS#JTb3E6FTKEUWK36"}, /* 146: 12 x 8, down, 18 faces, 8 blocked, stuck 18%, search 48 */
    {12, 8, 1, "yEyCoywCEjwyEsK#twwCEoddK#opjtN#Ns#dtCNiPjpiNEddiKNCCPAssjtti#oi#sAKop#wipKoPtsp#PEdKByyxNpBPPwx",
     "AJAN459HcaQ5JhM#YQ9NcdUWg#4O1Xb#bV#8XHLDF1SPZfUWPMZ00F3hVa226#ID#R3gde#G6OEIKYRS#Kf8ETBBCLeT77GC"}, /* 147: 12 x 8, down, 17 faces, 8 blocked, stuck 22%, search 44 */
    {12, 8, 1, "aNDwkhhpAhahprAwhk#taOhDnFAk#NjtcFAApnwjnrjwNNnnpFfnwjrANFOjOllFlkf#cNOOpOfDl#kDwfjfafrklttplDFD",
     "4eKJaZBEhZ4gEj3JBa#LUbgWFI3D#e2LGRfhXFc1C72c6TddAIOCH17f6RQiN55V0DP#GTQbANOK0#MWHYiYUPjM899X8SVS"}, /* 148: 12 x 8, down, 17 faces, 4 blocked, stuck 22%, search 46 */
    {12, 8, 1, "KsKOciccKKeOqDliPAM#siiOJBBqMDiiJAKjsluqA#DJMsMjDqOlPP#OeejeDAjjBBsOAKAuJulp#sBcMMucqPuqllBcupDj",
     "UIEL6j6BEUHLMFSjcXP#K00Y1eaJdbDDZigAISNJi#bZdhPAVMYfTT#8RRQHFX2255K8WgWG1N49#heCOOGB7c374faC39VQ"}, /* 149: 12 x 8, down, 17 faces, 4 blocked, stuck 30%, search 47 */
    {12, 8, 1, "DokujrjjDgofrx#DEuEfnfAArxrxGkGwnurwDzl#GojADuxkxluFEAjG#EkgxlCwwollrottDCFdEzggl#ojfzdCCfzzzuEf",
     "ObH12hQ2FEb7ec#OB1V75R99efhf0HZK5IaKPid#0SJWFIcMUCXABWJZ#VMEUCL88gGGaS66PLATYjDDd#gQ3iT443jNNXYR"}, /* 150: 12 x 8, down, 20 faces, 4 blocked, stuck 65%, search 46 */
};
