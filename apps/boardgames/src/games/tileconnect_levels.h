/*
 * Tile Connect's levels: our own, made by tools/tileconnect_gen.c (each board played out first by the rules' generator,
 * so it always clears; chosen by how often taking pairs at random gets stuck). Each: its size, its fall, a
 * character a cell for the faces ('#' blocked), then the step of the plan that takes each tile (the tests play it).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
const tcn_lvl tcn_levels[TCN_LEVELS] = {
    {6, 4, 0, "CBLLNDDMMEPNEBOIOKCIJJPK",
     "1877944665A958B2B31200A3"}, /* 1: 6 x 4, still, 12 faces, 0 blocked, stuck 0%, search 12, plain */
    {6, 4, 0, "GE##FEFGDGFHEHHGDBFH##EB",
     "96##7832597481125034##60"}, /* 2: 6 x 4, still, 6 faces, 4 blocked, stuck 0%, search 10, notches */
    {6, 4, 0, "tjjxssA#ux#jt#zu#juAzutt",
     "4775668#95#14#39#1083022"}, /* 3: 6 x 4, still, 7 faces, 4 blocked, stuck 0%, search 10, pillars */
    {6, 4, 0, "#ssuw#yysusxuuwxAs#AsAA#",
     "#2235#446369005978#1817#"}, /* 4: 6 x 4, still, 6 faces, 4 blocked, stuck 0%, search 10, corners */
    {6, 4, 0, "nq##crnjbfcfnonbrjqo##jj",
     "14##0817560632358942##97"}, /* 5: 6 x 4, still, 8 faces, 4 blocked, stuck 0%, search 10, notches */
    {6, 4, 0, "BDCHOOD#NP#CB#HP#DKKDNMM",
     "0756117#82#50#62#9339844"}, /* 6: 6 x 4, still, 9 faces, 4 blocked, stuck 0%, search 10, pillars */
    {6, 4, 0, "#bhrf#qhfrggqekkba#ieai#",
     "#9815#685100672294#3743#"}, /* 7: 6 x 4, still, 10 faces, 4 blocked, stuck 0%, search 10, corners */
    {6, 4, 0, "DC##yDvxyBBGssvCEwwx##EG",
     "34##5328500711246998##67"}, /* 8: 6 x 4, still, 10 faces, 4 blocked, stuck 0%, search 10, notches */
    {6, 4, 0, "LKILKON#PJ#KJ#PL#MINLMOK",
     "1371349#58#08#56#2796240"}, /* 9: 6 x 4, still, 8 faces, 4 blocked, stuck 0%, search 10, pillars */
    {8, 4, 0, "#jqjklj#krpjmmqjlrpkmmoj#orrook#",
     "#A52CDA#96B24451D6BC0031#788379#"}, /* 10: 8 x 4, still, 8 faces, 4 blocked, stuck 0%, search 14, corners */
    {8, 4, 0, "HJDNDGJEOB#LJ#BBEB#LN#CCEGEJGHGO",
     "5D191A3C4B#03#882B#09#7726CDA564"}, /* 11: 8 x 4, still, 10 faces, 4 blocked, stuck 0%, search 14, pillars */
    {8, 4, 0, "#HHHByD#BEEEDAGBEHyuGAuB#uxxuEE#",
     "#5779DA#9336A04265DB4082#B118CC#"}, /* 12: 8 x 4, still, 9 faces, 4 blocked, stuck 0%, search 14, corners */
    {8, 4, 0, "uxpupkmlsx#us#kluq#kj#mruruqmmjk",
     "63050B41D3#5D#216A#BC#47979A88C2"}, /* 13: 8 x 4, still, 10 faces, 4 blocked, stuck 0%, search 14, pillars */
    {8, 4, 0, "#mkkoko#nqqolojjnmloloqp#lppqpk#",
     "#844BC5#177A6533182A6B9D#2009DC#"}, /* 14: 8 x 4, still, 8 faces, 4 blocked, stuck 0%, search 14, corners */
    {8, 4, 0, "bedbiehieiaahcabeffidcahgbabbahg",
     "28CA15415933F0E28669C0E4BD7DA7FB"}, /* 15: 8 x 4, still, 9 faces, 0 blocked, stuck 0%, search 16, plain */
    {8, 4, 0, "LMM##JLLPKKPKNOOLNKKLLJNJJK##INI",
     "399##B227C17C644361500DADB5##8A8"}, /* 16: 8 x 4, still, 8 faces, 4 blocked, stuck 0%, search 14, notches */
    {8, 4, 0, "DGGDBFFGOGCHHNDGOCBFBBNDCIFFGCIF",
     "CFF47881EDB00AC1EB9579A42653D263"}, /* 17: 8 x 4, still, 9 faces, 0 blocked, stuck 0%, search 16, plain */
    {8, 5, 0, "#zHAAHE#FvAAzvvFEtwwBAAtFFHFBzzv#tCHFCt#",
     "#DGHH79#68CCA44F932210036E7E1DA8#5BGFB5#"}, /* 18: 8 x 5, still, 10 faces, 4 blocked, stuck 0%, search 18, corners */
    {8, 5, 0, "jir##klflffiqklrjincqkrkifhchlrrljl##jrn",
     "53G##879700AH8FE53D1HBEBA9C1C466F24##2GD"}, /* 19: 8 x 5, still, 10 faces, 4 blocked, stuck 0%, search 18, notches */
    {8, 5, 0, "ztwEszwzsx#sz#EHHHHswzzwHw#AA#tyBwxAHyAB",
     "0G13501C8D#57#3FE228HC7HF4#BB#GA94D6EA69"}, /* 20: 8 x 5, still, 10 faces, 4 blocked, stuck 0%, search 18, pillars */
    {8, 5, 0, "PNOKHCKCLHOOCEKNPNN##FIKCHENOHKHLFCNCHKI",
     "48H032021BAAGIE84DD##FC76BI9H5731F69G5EC"}, /* 21: 8 x 5, still, 10 faces, 2 blocked, stuck 0%, search 19, centre */
    {8, 5, 0, "sxHxxyGxByCCGuAGHuCCABuAyyszAsuHFFHzusuG",
     "I1FAA2D1E255DCHG7CBB3E6H44I93J6F00798J8G"}, /* 22: 8 x 5, still, 11 faces, 0 blocked, stuck 0%, search 20, plain */
    {8, 5, 0, "koqmnmssso#xn#umkrrqmmsruu#xx#pmkpkpxupr",
     "4B71319AAB#H3#D64CC75592EE#H8#06FGFG8D02"}, /* 23: 8 x 5, still, 10 faces, 4 blocked, stuck 2%, search 18, pillars */
    {8, 5, 0, "agllallemonmmiiofokknigmedkkhkeofdgegkhi",
     "FGHHFEEICBA552DJ0B19A26C481973IJ08G4637D"}, /* 24: 8 x 5, still, 12 faces, 0 blocked, stuck 0%, search 20, plain */
    {8, 5, 0, "KJJKKHHCBB#II#CJKBGJJJGLBB#GC#EGBGPGCPEL",
     "9GG8844D65#11#DE95BFFE2H6A#C7#02AB3C730H"}, /* 25: 8 x 5, still, 10 faces, 4 blocked, stuck 5%, search 18, pillars */
    {8, 6, 0, "##CzCF###HzsFGB#sHsGsDBvvGvGHzzv#GDsHCs###BCGB##",
     "##3935###E9C5B6#GEA1AH6DF8D1422F#8HC47G###07B0##"}, /* 26: 8 x 6, still, 9 faces, 12 blocked, stuck 0%, search 18, octagon */
    {8, 6, 0, "#AmyAux#momyuxuutts##xytmos##AtqmqmtsAnq#qntsyx#",
     "#HF1HID#G5F1IJ77338##JB4E58##A42G0E96AC2#0C96BD#"}, /* 27: 8 x 6, still, 10 faces, 8 blocked, stuck 0%, search 20, corners+centre */
    {8, 6, 0, "wwF##HwyyCFGzzsxBxBGuGEsBxxFCCCGHwzzuEHFHFB##wFw",
     "LE1##IEJJA1600FHGH86K7DF822BA557CL44KDI9C9G##3B3"}, /* 28: 8 x 6, still, 12 faces, 4 blocked, stuck 0%, search 22, notches */
    {8, 6, 0, "knagbanjkllgbnbbpme##pankmm##pnkpmejgglngfabbflg",
     "6ICBLCFEJDDBLFGGK92##H8I611##H5JK92EA745A3800347"}, /* 29: 8 x 6, still, 11 faces, 4 blocked, stuck 0%, search 22, centre */
    {8, 6, 0, "JMKKGGKJLD#EM#IBFDIIPPIEFGGBBBMMKO#IB#KBFOLOIFOK",
     "7C225597LA#GC#J80A66BBJG044E3EKKH1#I3#98F1LDIFDH"}, /* 30: 8 x 6, still, 12 faces, 4 blocked, stuck 0%, search 22, pillars */
    {8, 6, 0, "sAACBsCxyysCCvFFBFx##EFutyy##vFyuBzxFvyxtsBBBzvE",
     "599FA5FJ22G66LEEAHC##80IKBB##10DI74CH1DJKG3374L8"}, /* 31: 8 x 6, still, 12 faces, 4 blocked, stuck 0%, search 22, centre */
    {8, 6, 0, "Css##zPDJsMsMwPxJLHMLMCxHtJzMKKDKtJtHPPKHtz##wMz",
     "A22##F180KLKLI1E0JGHJ9AE3B569448D757GCCD3B6##IHF"}, /* 32: 8 x 6, still, 13 faces, 4 blocked, stuck 0%, search 22, notches */
    {8, 6, 0, "ickcmmhkho#dd#pfhfhocipjpoggghpjpg#pa#chkkkoaaka",
     "HB381153KD#77#EILILDBHF46CG00KF46G#EJ#85992CJA2A"}, /* 33: 8 x 6, still, 12 faces, 4 blocked, stuck 0%, search 22, pillars */
    {8, 6, 0, "NGKHBGGKJNKPNFFCBGHPECMMKBONCBMGJCOEKBKGDMMDDBDM",
     "9KFBDCCF39J1HAAGDKB1ENM2JL4HG6803N4E76705M8IIL52"}, /* 34: 8 x 6, still, 13 faces, 0 blocked, stuck 0%, search 24, plain */
    {10, 5, 0, "ppvq##tovqwAmqqnlonk#AnAonokk#omlnnppolktssw##wAlw",
     "II4A##L24AB6711CG2J0#6JDECK99#K7F5533EF0L88B##HDGH"}, /* 35: 10 x 5, still, 12 faces, 6 blocked, stuck 0%, search 22, gates */
    {10, 5, 0, "#EItNyyvD#FFIGFtEvCINtFC##IDCDFtDCzzvtFt#uGuCCvuu#",
     "#H1LI3347#EE1DFGH409ILJ8##970BFKB566AGJK#CD258AC2#"}, /* 36: 10 x 5, still, 12 faces, 6 blocked, stuck 0%, search 22, corners+centre */
    {10, 5, 0, "chijjhhoijeb#beel#cebqnnbjleeojr#qrhr#rrlllrlppjpp",
     "E283GL2K83IJ#9FFC#EIJA669GB11K5M#AML7#HH4C47BD050D"}, /* 37: 10 x 5, still, 12 faces, 4 blocked, stuck 0%, search 23, pillars */
    {10, 5, 0, "njkn##pnptkksnqssxjjAtrwqjAytxkqyjjsnppArrrt##nAqw",
     "BJF0##9B98F6M01EE755G4DL1CGK8763KJCMIAAH22D4##IH3L"}, /* 38: 10 x 5, still, 12 faces, 4 blocked, stuck 0%, search 23, notches */
    {10, 5, 0, "BBPLJPJJDMLJ#EHPM#EEFDLMMPOOCCFB#HLIC#IFDHDCFDBHED",
     "22H7GH4GDF74#IE3F#IL8DBAA311JJ85#EBM6#MKC096KC50L9"}, /* 39: 10 x 5, still, 12 faces, 4 blocked, stuck 0%, search 23, pillars */
    {10, 5, 0, "#zFxxDvDz#wtEHHAAwwDvHHwtEuzBBwEFuzAEHwD#uuEzzEHA#",
     "#3I776J63#D8F22BBHCKJLLC8ME5GGDMIE49F1HK#00A45A19#"}, /* 40: 10 x 5, still, 12 faces, 4 blocked, stuck 0%, search 23, corners */
    {10, 5, 0, "AzPuGAzAAuCzzLLACACBLutu##uuLFGtGvBLsCBFGBLMBvPsBM",
     "71GNM8H67ND1HKK8A6ALF9CJ##J9F43CME2BIDL432B50EGI05"}, /* 41: 10 x 5, still, 13 faces, 2 blocked, stuck 0%, search 24, centre */
    {10, 5, 0, "JMKK##OOCMPGGNBJNECC#JHNHEKGN#LLPNNOBGOCJJJP##GKPG",
     "KG99##CCJG78E1FKLBJ2#6D1DBA5L#337IIHF5H20604##8A4E"}, /* 42: 10 x 5, still, 12 faces, 6 blocked, stuck 5%, search 22, gates */
    {10, 6, 0, "#tKKuuKDD#ytyKKFFMIwtBxy##wwIFtxMM##MFKwDDHyHIxFHF#uIBHIIxu#",
     "#D8822J99#7DGCC00PHF5E4G##BBHO5433##PMJF11I7IKNMLO#A6ELK6NA#"}, /* 43: 10 x 6, still, 12 faces, 8 blocked, stuck 0%, search 26, corners+centre */
    {10, 6, 0, "khgq##ihidkkljibnldg#lqlnanjd##jqnnaeej#qjbkkbhabkiadd##hjnd",
     "J3GK##838EJP1MOCDL4G#10L96DM4##N0A9622F#KNI55CBHIPOH77##BFAE"}, /* 44: 10 x 6, still, 12 faces, 8 blocked, stuck 0%, search 26, gates */
    {10, 6, 0, "FvzuuyEyFvxCzHzCCCtGAHzH##CyEEHHwG##xwwFAyHGCwGyxutuywxwGGFE",
     "AI711LKLAIQM04733JPREN04##JDK8GGO9##C66BEHN9M52DCFPFHOQ52RB8"}, /* 45: 10 x 6, still, 13 faces, 4 blocked, stuck 0%, search 28, centre */
    {10, 6, 0, "oijf##jgronkifkledcdonnnoeeckkgpddjjdpradkkcoleplajopc##ljll",
     "5813##1RK9HG83GAEB2B5QHQ977200RIPN66N4KOPFFCLAE4JODLIC##MDJM"}, /* 46: 10 x 6, still, 14 faces, 4 blocked, stuck 0%, search 28, notches */
    {10, 6, 0, "ELGBPBPGEICC#MJHI#BBILOFFLEFIIBIHMJOLFNNBN#LEHN#JHLNNOOEHHJE",
     "5G41D1D45E22#3ARO#BBLJI0079KLENOH3AIJK88NP#79HP#MCGQQFF6CRM6"}, /* 47: 10 x 6, still, 13 faces, 4 blocked, stuck 0%, search 28, pillars */
    {10, 6, 0, "##wvAxzA###FtyxxvxA#vFDH##tHAyzzHH##zADD#DttDDyyA###vwtyyt##",
     "##C835I3###DG975871#FDJA##GA194466##IKEE#J22BBLHK###FC0LH0##"}, /* 48: 10 x 6, still, 10 faces, 16 blocked, stuck 8%, search 22, octagon+centre */
    {10, 6, 0, "KICCOIOIEEMCJLLNNIOEJB##JF##IEMC##PK##IJLKFDDOKPDBLBKKJPBPDJ",
     "MH668DID11EF42299H8BNP##4L##7BEF##GK##7NCOL33IM0AJCPKO5GJ0A5"}, /* 49: 10 x 6, still, 13 faces, 8 blocked, stuck 8%, search 26, bars */
    {10, 6, 0, "vttt##FzEzFs#CCHH#utvuvEtxzzssvDDExuEDzBsv#EssD#DvuxEt##BDzx",
     "2ONN##6ALA6M#BBFF#J72K9LOPI800911HPJGD85M4#HCCE#D4K3G7##5EI3"}, /* 50: 10 x 6, still, 12 faces, 8 blocked, stuck 10%, search 26, pillars+notches */
    {8, 5, 1, "##vtvu###xzstAu#zusxzzxz#uyxzyA###ytty##",
     "##3734###9AC7B4#D0C82A9D#05821B###5661##"}, /* 51: 8 x 5, down, 8 faces, 12 blocked, stuck 0%, search 14, octagon */
    {8, 5, 1, "#okommn#rppkmkjkqqpkmpprjlokrlpr#joojon#",
     "#5D536G#AC913DHE00916BBFH84EA8CF#72472G#"}, /* 52: 8 x 5, down, 9 faces, 4 blocked, stuck 0%, search 18, corners */
    {8, 5, 1, "wzk##zkoootynppyrozmmsstrmtmmwnzmkt##zkz",
     "GB2##B2FF9HAC88A0957466H0E174GCDE31##53D"}, /* 53: 8 x 5, down, 11 faces, 4 blocked, stuck 0%, search 18, notches */
    {8, 5, 1, "ifjbbmlibfibbgqrmmljciqmogpocbrfpmpkkpmf",
     "74B53AGCF4C5FJ9H66GB179AEJ2E13HDI8200I8D"}, /* 54: 8 x 5, down, 13 faces, 0 blocked, stuck 0%, search 20, plain */
    {8, 5, 1, "NEM##NENEDEMCOPPODDDOCBNCDDMCMBGOLG##LEE",
     "46E##C64717GAF008122F9BCA33E9GBH85H##5DD"}, /* 55: 8 x 5, down, 10 faces, 4 blocked, stuck 2%, search 18, notches */
    {8, 5, 1, "#rfdglf#dhgmiigqrkkrmiqliibbrool#hglidd#",
     "#H2G5E2#DB57C4F9H11A74908866A330#BFECDG#"}, /* 56: 8 x 5, down, 12 faces, 4 blocked, stuck 5%, search 18, corners */
    {8, 5, 1, "MIHJJPMEKMFJMMNHDMF##PIIIHJJEEPKBHNDJEBP",
     "8652B789D4A200C5E4A##73361BIG9FDH1CEIGHF"}, /* 57: 8 x 5, down, 11 faces, 2 blocked, stuck 10%, search 19, centre */
    {8, 5, 1, "##pafg###ppqplg#aafanlbb#iqanrr###ppai##",
     "##1D29###71A789#3325C800#BA5C66###44DB##"}, /* 58: 8 x 5, down, 10 faces, 12 blocked, stuck 12%, search 18, octagon */
    {8, 6, 1, "jwv##uzjAjAuwuszvtAAutkllpkvskktpqAAvtllzqm##zjm",
     "G3D##F8EHGHF3JI8LC11JC24KB9DI927BA00L7K45A6##5E6"}, /* 59: 8 x 6, down, 13 faces, 4 blocked, stuck 2%, search 22, notches */
    {8, 6, 1, "KINDCHIIHGNCKBHPOG#KO#LIHN#HO#LHBNJDONBPKOJNIIOB",
     "63LJ7F39C0I7AHDKG0#AG#59CL#F4#5D8IBJE2HK6EB21148"}, /* 60: 8 x 6, down, 12 faces, 4 blocked, stuck 2%, search 22, bars */
    {8, 6, 1, "##gege###fhahhg#cfhdafbacfiifiba#fhdgei###ihei##",
     "##BFAF###3E422A#03E64HD90C11C8D9#H76BG8###57G5##"}, /* 61: 8 x 6, down, 9 faces, 12 blocked, stuck 5%, search 18, octagon */
    {8, 6, 1, "wytEtCEzttytzFCtJAs##sFEAII##KzBKKCCzwJBJwBBEKwJ",
     "I3C0A20KEE3C942A8HJ##J4LH77##B9GFF55KI6D81GDLB16"}, /* 62: 8 x 6, down, 13 faces, 4 blocked, stuck 5%, search 22, centre */
    {8, 6, 1, "uyCFyuzEyt#FD#zyyttCCBCxtEtEwByCDE#wE#xCzwwzFEtF",
     "BJ4FJB0K1G#FL#071G522E4AC359IE76L3#I9#A68DD8HKCH"}, /* 63: 8 x 6, down, 11 faces, 4 blocked, stuck 5%, search 22, pillars */
    {8, 6, 1, "iokrkoggdnorimgmlc#gd#cinn#lh#halilagniohignrhnr",
     "CL858G11HIG5E9A93K#AH#KC4I#FD#DJ30FJB7EL20B46276"}, /* 64: 8 x 6, down, 12 faces, 4 blocked, stuck 8%, search 22, bars */
    {8, 6, 1, "JNB##BLMBFNGCNLMGFFGFIIMKGIFINFJKBCBCOGMBOC##CGC",
     "JED##F2IF4EBK926C43BA00I7CG3G9AJ7HLHL586D51##18K"}, /* 65: 8 x 6, down, 11 faces, 4 blocked, stuck 12%, search 22, notches */
    {8, 6, 1, "#Bwsuwu#BEGsFFxsEzG##ysuxHz##wxsEyzzyyxu#FEHsFw#",
     "#FE4AEA#FH7422319C7##B10JGI##83D9BCI55J0#6HGD68#"}, /* 66: 8 x 6, down, 11 faces, 8 blocked, stuck 20%, search 20, corners+centre */
    {8, 6, 1, "##CKHD###CIJJID#KNGIGIIHKGGJIPPH#NKJNFF###KKNH##",
     "##8DGC###8E227C#1H3BF7E01F34B550#HD4A66###99AG##"}, /* 67: 8 x 6, down, 10 faces, 12 blocked, stuck 28%, search 18, octagon */
    {10, 6, 1, "DDIIGPtsHGws#HPGG#wHPOIKOwtMMKDPIOywsHEEHB#OyDB#BBHPIGPIBsGB",
     "77446AQ8C6J8#OAKK#JO3RFPRLQ99PI3F0ELNCBB2M#0EID#GG215H15DNHM"}, /* 68: 10 x 6, down, 14 faces, 4 blocked, stuck 2%, search 28, pillars */
    {10, 6, 1, "kovotswoxoyrvxqvuuskkt##qz##yyjq##qz##oyktjttwzvoyytjvjxvzrx",
     "C3N4J8P463BGL6KL008DDJ##7I##F2MK##7E##5FC1MOOPEH52B19N9AHIGA"}, /* 69: 10 x 6, down, 13 faces, 8 blocked, stuck 2%, search 4890, bars */
    {10, 6, 1, "#ecgpfcfe#hgfecbcapnkifc##kaonhaai##nnookiekcnknii#kieboaae#",
     "#L6N5383L#GN7J6P8D521M7E##CDH2GKKO##44HI1MJCEFBF99#BOAPI00A#"}, /* 70: 10 x 6, down, 12 faces, 8 blocked, stuck 8%, search 26, corners+centre */
    {10, 6, 1, "AEzszvvDusyzHHEHExtAuztt##HxwFzuEv##GEDFyxuDvDttDwsHxGEzstHD",
     "QJ9G922ELG3M77KFJDIQH866##F5NA8HKP##R4EA35LCPCI0ONB1DR4MB01O"}, /* 71: 10 x 6, down, 14 faces, 4 blocked, stuck 8%, search 28, centre */
    {10, 6, 1, "#IIzIsssz#LEIzvuBJFzsIsE##NNFzIFvE##xxBxFEszFuuuxu#BuLFJJBJ#",
     "#AAP3IBLE#KM36CJFH9EL7B0##119P7OC0##N8FNOMI6455G8J#2GK4DD2H#"}, /* 72: 10 x 6, down, 12 faces, 8 blocked, stuck 10%, search 26, corners+centre */
    {10, 6, 1, "ipgc##hjbcbbgfrrhjep#qplfbccj##lleejbek#kibcfklfcrkfqf##ddpr",
     "IN48##69LD224B556EKG#PNABM8FE##JA339LKO#1IMDHOJCF01CPH##77G0"}, /* 73: 10 x 6, down, 14 faces, 8 blocked, stuck 18%, search 26, gates */
    {10, 6, 1, "NNLHHHIHIGME#KKED#EK#GPLEMDKE##KPLJFFDD#MK#JJEN#IJLDGGJNMDIJ",
     "EEDCC0505MAL#66J4#29#M3HLG492##83HKII11#A8#OKJP#BOD7NNFPG7BF"}, /* 74: 10 x 6, down, 12 faces, 8 blocked, stuck 50%, search 26, pillars+sides */
    {10, 6, 1, "tDAy##BGEAsD#zvGA#HtDtAyyzAyCADBECvExxztsB#EvvH#DEFBFD##BzBE",
     "65JF##G9OPB5#CD97#I846J33C7FKP4GOKDH22M8BN#HAAI#L01E1L##NME0"}, /* 75: 10 x 6, down, 14 faces, 8 blocked, stuck 62%, search 372, pillars+notches */
    {8, 6, 2, "##DBMB###MMBBIG#LKDIMGKDLCFCMKNN#CICMFD###KKIK##",
     "##G2E2###118834#DHG3E469DFC576BB#FA57C9###00AH##"}, /* 76: 8 x 6, left, 10 faces, 12 blocked, stuck 0%, search 18, octagon */
    {8, 6, 2, "#nvkjyk#jjknvmmypzk##onkvjz##qqqmmvkpqon#oorppr#",
     "#ADBE9G#E70AD669F30##IHG873##JCC118BFJIH#225445#"}, /* 77: 8 x 6, left, 11 faces, 8 blocked, stuck 0%, search 20, corners+centre */
    {8, 6, 2, "OLI##IKFzzOxIIJLxLIzMzvvKOFEMEKvELOIMJDxEKD##Mxv",
     "6AD##2BL336852EF8F5I4IK0G7LC4CB01A7D9EJH1GJ##9HK"}, /* 78: 8 x 6, left, 12 faces, 4 blocked, stuck 0%, search 22, notches */
    {8, 6, 2, "##sDCE###wwvsEC#AAxvsAsAwwvvDADx#sswwAy###DDDy##",
     "##GF8B###663CB8#77A352521199H0HA#GC440E###FDDE##"}, /* 79: 8 x 6, left, 9 faces, 12 blocked, stuck 2%, search 18, octagon */
    {8, 6, 2, "#uwsuyq#AuyyrrryqsAAlrwuwwllkkzqAqvvwrwr#jzkjlk#",
     "#6DC937#E611BA53HC22JAL9KKJG88F7EHIILBD5#4F04G0#"}, /* 80: 8 x 6, left, 12 faces, 4 blocked, stuck 12%, search 22, corners */
    {8, 6, 2, "##LNNC###LBNOEP#BLBDNEPPBBBOPNLO#MDFNJC###OFMJ##",
     "##C99A###05F723#605GF2436EE74DCB#HG1D8A###B1H8##"}, /* 81: 8 x 6, left, 11 faces, 12 blocked, stuck 15%, search 18, octagon */
    {8, 6, 2, "FDE##zwDFBEGDxFFGFEvvwwxBBxsszwyBHFwssxyEHv##Dwv",
     "46A##KG54F2H6ILBHL2887GD99I00K73FJBE11D3AJC##5EC"}, /* 82: 8 x 6, left, 12 faces, 4 blocked, stuck 22%, search 22, notches */
    {8, 6, 2, "dmibbhmbcdmbdlgiohc##lgimnq##cdmoqhdhhdimnbhnbnc",
     "JCF9ALC9IJEA6342BHI##34F05G##76EBGH8118205DLKDK7"}, /* 83: 8 x 6, left, 11 faces, 4 blocked, stuck 22%, search 22, centre */
    {10, 6, 2, "#PGGPGBJJ#MBEDJBPEEJJMDE##DLLLJONN##HIPDONGEEILIDL#ILDIBHNI#",
     "#C1P81K93#AJEF9J82230AFE##M44706BN##IGCO6BP55DLDM7#HLOGKINH#"}, /* 84: 10 x 6, left, 12 faces, 8 blocked, stuck 5%, search 26, corners+centre */
    {10, 6, 2, "klamikjalkhc#imgj#gkcgkbqpjnkhjgjddjccqhhq#hhbq#nbllbaeeapee",
     "BH5JAIP5HQ8F#AJKL#KBF7IRONLDQGP74004119G89#33CO#DC22RM6EMN6E"}, /* 85: 10 x 6, left, 15 faces, 4 blocked, stuck 5%, search 28, pillars */
    {10, 6, 2, "#FDKMFFMK#NCFILHCGGDMBMN##IEIEHBLM##KKIENKKDNLDMHO#IOHLILEL#",
     "#73E0220E#1L7JIFLPP3A6A9##JD4DM6IG##CC4H1BBO98OGFN#5NMK58HK#"}, /* 86: 10 x 6, left, 13 faces, 8 blocked, stuck 10%, search 26, corners+centre */
    {10, 6, 2, "##zDHHxz###AAvvDFAu#zttzGFyAHHHEzFFzHyyy#EDDuxBxu###BvuGvx##",
     "##IK99JI###44NNKME5#FAAFLMDE11H78338HBBD#7220GCG5###C60L6J##"}, /* 87: 10 x 6, left, 13 faces, 12 blocked, stuck 12%, search 24, octagon */
    {10, 6, 2, "gcnnkgnokegnnorrookngj##ho##kemj##hg##bbcmbqqbebbcefejjfcgoe",
     "7932K72LKGJBBD55D803J4##C8##0GP4##CM##EE9PFAAFHIIO1NH66NOML1"}, /* 88: 10 x 6, left, 13 faces, 8 blocked, stuck 12%, search 26, bars */
    {10, 6, 2, "#HLHLLILI#MHCGLGILFKOOCGGCCIFCMNCDIFFIKGHNJDHHBBPD#JKGPKJJD#",
     "#KIKB8484#LO9FIREB2H557667NQ29LDNMQ00EHFODJMAA33GC#J1RG1PPC#"}, /* 89: 10 x 6, left, 14 faces, 4 blocked, stuck 12%, search 28, corners */
    {10, 6, 2, "nomz##vojmuyyvvzzoju#jllznvuk##ppjxxnom#ykyrnvmkunqrrr##qvkn",
     "P29K##I2C9M8800JKFC5#744JOHMG##337EEOFB#NLNAPIBG516DDA##6HL1"}, /* 90: 10 x 6, left, 14 faces, 8 blocked, stuck 15%, search 26, gates */
    {10, 6, 2, "qgcgmqmggqfnnnnffnnmkj##qq##mcfj##oo##ogakpfeaapoqagmmjjeppf",
     "KFOBHNLFBKD11AA55PPHG0##66##COD0##44##29EGMJ833M2NE9LC778IIJ"}, /* 91: 10 x 6, left, 12 faces, 8 blocked, stuck 15%, search 27, bars */
    {10, 6, 2, "pwpw##pvpwrwqjjmnnsztmvkvqsynkrnvkttqryksnzkrrsvvsnmtm##rksq",
     "9408##9P08F4L55KDDOGMK6CPQN7HEFI6CJMQ37RNHGE11B22BIAJA##3ROL"}, /* 92: 10 x 6, left, 13 faces, 4 blocked, stuck 22%, search 28, notches */
    {10, 7, 2, "##JvBJLv###wwACKKMB#MGtKtCMtKLGyGMGMJKAAxxwywwJABB#wvtvKBMA###tAtGBG##",
     "##9IG9DI###LLSF44CH#NBRJRFCEPD212NBM7JQ500A18A7SH3#8OEOP3M5###KQK6G6##"}, /* 93: 10 x 7, left, 13 faces, 12 blocked, stuck 15%, search 29, octagon */
    {10, 7, 2, "JJJEKEHCKFFH#PPLJ#ENFMLLFIGGILHKNOJDDGEEEKHOJODOCCKH#NHFG#DOFMKODNBBCD",
     "41192NLT2ORL#55M4#9G7C88RQFFQMVBUIDH0JKKNBSID60633ES#GVOJ#HP7CEPAUWWTA"}, /* 94: 10 x 7, left, 15 faces, 4 blocked, stuck 20%, search 33, pillars */
    {10, 7, 2, "MHCLPLMMPOHOCMvvDCsGHAwPOwHHGEFDwF##sEMMFFFLALHwDOAwPDADCDCuLCxwxFLEEu",
     "MQK5I5LMIWOWKL11XHA2OU7TJRDD29FX7V##A933FGG808QSPJURT404NPHBEN6S6VECCB"}, /* 95: 10 x 7, left, 16 faces, 2 blocked, stuck 22%, search 34, centre */
    {10, 7, 2, "OCLM##MMMFHFLFNOENHGPPJNHENHHG#JJPPBCBI#IIJHEKJPIKFNNCEDJDFLCLMC##PCMF",
     "EO0H##55H1VF0FAE93ICMMDAQ93VIC#SDBBPUPL#KK8Q2TS7LTR6642G8GRNUNJ4##7OJ1"}, /* 96: 10 x 7, left, 15 faces, 6 blocked, stuck 28%, search 291, gates */
    {10, 7, 2, "ccqc##jjqirninlqoqgglriarcbcjirllanbbbeddhrbbdoeojdnndmmraqqhhho##nadc",
     "FSJF##NNK1BI3IMJRK22MG3DG969O1BEEDQ006TP8LV447UTUO8CC7AAV5WWHLHR##Q5PS"}, /* 97: 10 x 7, left, 15 faces, 4 blocked, stuck 32%, search 33, notches */
    {10, 7, 2, "AMMxwKzKzwwt#uxNu#NxLNAAtxwOuH#yOyLMMNP#LMyvuLuKyPHv#KMyy#HvAzNHuvLLNz",
     "P116MBABAQMK#G9IG#I63UDDK9QSRH#FST7LVUE#3VFJ878CTENJ#CL44#HOP05NRO2250"}, /* 98: 10 x 7, left, 15 faces, 6 blocked, stuck 45%, search 32, pillars+sides */
    {10, 7, 2, "GuyBDvvyDvGBusFtBGFAAHGstuxxBxww##Au##CyAuEEHHEttxzxyyzECxAHHuvEGGHEyA",
     "EN4O1HL41HRON3KPSEK86MR3P2GGS0BB##I2##UF6TAAMW9770VQCCV9UQIWDTL5JJD5F8"}, /* 99: 10 x 7, left, 16 faces, 4 blocked, stuck 57%, search 35, bars */
    {10, 7, 2, "nqin##hmhohaqijjlllonfjlhhaqplfdbgddmaamdmgblaqkjanfphkkiiqdfmmp##kpqd",
     "4B54##QKO9OWB5336UU9VTR6JJDHES1CMF77I00ICNFMSW8GRDVTPQ22LL8A1NKP##GEHA"}, /* 100: 10 x 7, left, 15 faces, 4 blocked, stuck 68%, search 132, notches */
    {10, 6, 3, "wust##uxwxyuAtGHuzzH#GCAGGvHD##yyCHBGtt#wGAAsBxxHHywyx##vyDx",
     "3EP4##BAFA0E64OMB99M#KC6ODJGH##5NCL8D77#3K11P822LG0F5I##JNHI"}, /* 101: 10 x 6, centre, 14 faces, 8 blocked, stuck 0%, search 26, gates */
    {10, 6, 3, "#hlobdlkj#lleeddffmgmbeheffiikrordbhboporeribhbapj#iffkeakg#",
     "#DCGBMCQF#00341188PIPB3D499JJQHGHMA5EK6KR7ROA5EN6F#OLL27N2I#"}, /* 102: 10 x 6, centre, 15 faces, 4 blocked, stuck 0%, search 28, corners */
    {10, 6, 3, "##IBIBtN###NNNNLtvv#NBvzMMIILLOJvyzyvyLv#yLwwMMOL###JBJOOJ##",
     "##BHBHJM###N7MNIJEE#7DF655221I98FK6KAL1A#LG4433CG###0D89C0##"}, /* 103: 10 x 6, centre, 12 faces, 12 blocked, stuck 2%, search 24, octagon */
    {10, 6, 3, "tvEt##CHuCEHAAGzssHx#HttxxAHx##EHttsADD#zEuEEsszzsyFvG##Fvyv",
     "8PE8##3NK3EBFFGLDDBM#JHH77AJM##0NII9AOO#L0K1196556C24G##24CP"}, /* 104: 10 x 6, centre, 14 faces, 8 blocked, stuck 2%, search 26, gates */
    {10, 6, 3, "yjtr##mltnntnrnupumlnpAsscpemnypApmyueecreepmyaautjyey##mkkr",
     "CKA2##OIA39L92JD7D4I38M55N7B4JC8MEP1HBQN6RREP1GGHLK0Q0##OFF6"}, /* 105: 10 x 6, centre, 15 faces, 4 blocked, stuck 2%, search 28, notches */
    {10, 6, 3, "bepkldaklkgpeppaaqgggbar##bdqrfclr##ggnrebrrblmnddfeckebehhm",
     "L4AO2NPR2R7B4ABCCHFF7GP1##LNHQD8I1##MMKQ96556IJK00D98O3G3EEJ"}, /* 106: 10 x 6, centre, 15 faces, 4 blocked, stuck 8%, search 28, centre */
    {10, 6, 3, "##GyJJGO###KOyGyyGH#zHHyvyvzxxvxKKttvwKw#xHHDwttw###xGDGHx##",
     "##EK55EF###NF6IBBIJ#L116HKHL449233889GNG#2AADC77C###0MDMJ0##"}, /* 107: 10 x 6, centre, 12 faces, 12 blocked, stuck 12%, search 24, octagon */
    {10, 6, 3, "bclmlimmgcmblorhodhinijjrllhaaklneeidkagkghceqkeajrqgcqqjere",
     "32E4EJ4A92A3LFCMFIDGKJ88C66D557LKQRGIH097PMNRSHQ0BTOPNSOB1T1"}, /* 108: 10 x 6, centre, 16 faces, 0 blocked, stuck 10%, search 30, plain */
    {10, 7, 3, "##yIMOOM###DEtDtIIt#vvEEDOtyHHytDP##CPLOPtMMDxCIIL#ywPELLPI###xDwPxx##",
     "##CNJ33J###QPD29559#OOHH2BDC00K4QR##FM8BM4EELGF778#KIRPAA6N###1LI61G##"}, /* 109: 10 x 7, centre, 14 faces, 14 blocked, stuck 2%, search 28, octagon+centre */
    {10, 7, 3, "#ddomrrko#fiknmoderqfoffbhdhreligl##iogqaaflmbhppelbbieehppe#odmmnmfd#",
     "#99HE44UH#2JUVEM6P752OIIRK6K7LBNTD##NMT5AASDQR013LBFFJ88013P#OCGQVGSC#"}, /* 110: 10 x 7, centre, 16 faces, 6 blocked, stuck 10%, search 32, corners+centre */
    {10, 7, 3, "##ONsPNM###MJJsPAMP#AOxPxPzCCGxPxzOOKKCAFCMKKGzzFA#CHHHHNCF###OANFOA##",
     "##I68K65###RHH8KQ5S#QI1SOJMBPFOJ1EDD44B9NPRAAFME29#GL00LCG2###73CN73##"}, /* 111: 10 x 7, centre, 14 faces, 12 blocked, stuck 8%, search 29, octagon */
    {10, 7, 3, "#iecrcirp#oellhejhhrdhncqkkqqdaoddlfpqqejnllcffqpmaccldmadjr#mafmjaap#",
     "#0GDMD0MH#NGFFQIC112AQEWP8833L4N66VUKBBISEV9W55PKO4779ARJLC2#OTURSJTH#"}, /* 112: 10 x 7, centre, 16 faces, 4 blocked, stuck 15%, search 33, corners */
    {10, 7, 3, "EDGGJMKMHGPD#KMGM#JOCFKFMGMPONNCEEFKONPPEBBLLKGIOPJL#FKNE#IEPBLFJHLLBF",
     "HD5590W0POGD#U4FV#97JLWR4FVANQQJ66R2NKA3HII112O873TS#LUKC#8CGESBTPMMEB"}, /* 113: 10 x 7, centre, 15 faces, 4 blocked, stuck 22%, search 33, pillars */
    {10, 7, 3, "uvmp##pmxuuk#oowo#rutntkwwnAoynxqvxxnyAmttxqAAAtmxzk#ktwr#rrrAzz##yzry",
     "KON3##3NI99J#DDBC#QKPAPLSBG8C5A6EO00GR8HFF6ET22UHI4J#LUSQ#771T4M##RM15"}, /* 114: 10 x 7, centre, 15 faces, 8 blocked, stuck 25%, search 31, pillars+notches */
    {10, 7, 3, "##vGvFGB###CDwEFDvv#swEwBwuBFuGssBDDGFswwAAssCuDyu#CyBHCDxy###xvyvBH##",
     "##FQFSQ9###IP7NSP66#CON7MOL9D8RCKMGGRDKJJBB44ELA38#I2H5EA13###1020H5##"}, /* 115: 10 x 7, centre, 14 faces, 12 blocked, stuck 35%, search 30, octagon */
    {10, 7, 3, "awfficncpiccakllwewkajuggjeuprcnva##ejrerbcavgbvbwbijjlgwviabllnbjlnrw",
     "OX88WGVGIUBBKE11949EKMQ77S4QI6LVPF##NM6NDHLFPAJCJRHU55TARCWO20032ST3DX"}, /* 116: 10 x 7, centre, 16 faces, 2 blocked, stuck 35%, search 34, centre */
    {10, 7, 3, "koqq##ylquqokmmpylusvkptqpAuplotwjwkuvkoqAtjvuAvAlzjoovuljksvtzs##sttl",
     "FSV2##462W5AQMMD46PBOFRKVDUPR8AKJ0JNW3NS5UT01HC3CGELII1HGLQBO7E9##9T78"}, /* 117: 10 x 7, centre, 15 faces, 4 blocked, stuck 38%, search 33, notches */
    {12, 7, 3, "qmfejqafemffnqm#iiaa#kfrrggkkflnaciaoabkbddliqqrinhknmkddcoooqn#jjmg#hmrijrpgbnbgpgr",
     "7BJFO7LVFBJVWKc#33LY#dUPQAADDUGXTHRYbTM2M88GR00PCNZ2Wcd66H99bKN#44aE#ZaICOQ5S1X1E5SI"}, /* 118: 12 x 7, centre, 18 faces, 4 blocked, stuck 10%, search 40, pillars */
    {12, 7, 3, "#xvvuvuGGEE#wCEwzzuuyAAtEEGCxxyBwxEDHA###wD###wtzvvHuuDCFDFDAwzBHHHHCDBG#CxzAAxCvBz#",
     "#NSHFHFGGW5#UZWU11006JJEKKaZII6XMN57DO###Y9###MECBBD889RQ2Q7PYCVTLLTR2Va#4A3OPA4SX3#"}, /* 119: 12 x 7, centre, 15 faces, 10 blocked, stuck 18%, search 37, corners+bars */
    {12, 7, 3, "##PLCAAFyt###tILEEGIyut#tsEtPPPLuFIFPGyyG##KFsFPsECLIIuuFsGG#ItKCCLLAGE###CEAAACww##",
     "##FJG33C0A###9YQ88RO0KA#97IXEEUQKCSNFM55M##V4PNU7IGJSYTT4PHR#OXV11BBWHD###LD66WL22##"}, /* 120: 12 x 7, centre, 14 faces, 14 blocked, stuck 22%, search 35, octagon+centre */
    {12, 7, 3, "snlylurosxooqjzjqsqAArxnmzrttsqqrzoymj###zl###vuxxqwovmlsAlkzzxuuumrswAkjlnpxurjojpn",
     "AZ7V7BQHAR5HSPUbOLO11QRZ9UNIILCCNY5V9P###Ya###cB22STWc3K0FKEJJ8G44360TFEMaDX8G6bWMXD"}, /* 121: 12 x 7, centre, 18 faces, 6 blocked, stuck 25%, search 39, bars */
    {12, 7, 3, "ghbno##dmfgpngp#obqd#cihkqiiooajdcifhcndhiihbbfphcnekpeqqadfbbg#llpj#rrpdgmeg##fkfke",
     "ESA90##DWOEG9UY#0ALD#2KJbL88HHRTP2K6SNQZaIIJBB6GaNQVbMV55RPO11U#33MT#44YZXW7X##FCFC7"}, /* 122: 12 x 7, centre, 18 faces, 8 blocked, stuck 32%, search 38, pillars+notches */
    {12, 7, 3, "xyyww##xottysjnkokuxuqjlmmnpquqvqkryopnrrssvulrwwsokqmttnkywqmlzzpptmmwxlulnk##tunly",
     "Z326Y##0WBB2LDOTWSF0F9DAHHdEXaXN9S43KEO77bbNaA4YGLKTPUccVQM6PUCRRII8JJGZ15CdQ##85V1M"}, /* 123: 12 x 7, centre, 17 faces, 4 blocked, stuck 42%, search 40, notches */
    {12, 7, 3, "ywHzuwxsDwHBEBxCxDDFsCtwHEBCvvvFyzztuH###xy###yyDEFEFCssvvABuyvEFuDHHGAGuFEzGsuGDszz",
     "QF9aLOXTbO9D6NE1EKK7TRGF26N1VBB7SaMGL2###XS###CCbYcW3RIIHH0DPQVY3ZU44J0JZcW5A8PAU8M5"}, /* 124: 12 x 7, centre, 16 faces, 6 blocked, stuck 45%, search 13780, bars */
    {12, 7, 3, "tydhnngpgdqptyykoomddmgypkisseeiheeopyqov##mmgpmeviovqvduusmeivqkkvudtqgtytogqsultpl",
     "LIOXeeACAOVRL44JWWF88Fd6EJaGG99aX00bEIcHY##UUQC37NKHNVSDBBT37KYM55S1DZcQZ62bdMT1P2RP"}, /* 125: 12 x 7, centre, 17 faces, 2 blocked, stuck 57%, search 41, centre */
    {12, 7, 1, "#wJCwLJCCHw#PDsLCJOwsHFODMsJwLOIsMIFFF###wO###yyPLyyvuuJvPPOFIzFzIILJCvv#CHOIzvLvzH#",
     "#SRYKE4YZ1O#3F6IZ40O81A7Fa6RKE0W8aVADH###ST###MM3IBBUPPQU997DXLHLVXGQC22#CNTW5JGJ5N#"}, /* 126: 12 x 7, down, 16 faces, 10 blocked, stuck 0%, search 37, corners+bars */
    {12, 7, 1, "ymtwasmmkvwAyqysbsqaqAahjabksAmvqbekst###Ah###mmhsearrhjejbebhraevjrjqrdjebdqvhdrdyk",
     "C3R2K5738D2Fc9C5LI9K4FbZ6bS8I17D4LTEHR###1G###00NHaMOWGJXVSaQNOMTBJPVUWY6XQYUBZAPAcE"}, /* 127: 12 x 7, down, 16 faces, 6 blocked, stuck 0%, search 39, bars */
    {12, 7, 1, "#LKEDJKKAAM#xyyIDAEwIEwDxHExBFwMMOMJKJBIAFEHwwDDxJxzDIyOwMBAxAOzJIyBBOBE#JKzILMzzzK#",
     "#bGFIRGQdAP#BLLaIdT2aS3CBYFXWZ2cPV8RQNW9UZTYE3MC5NX7MKHOE81A5UO6JKH00V1S#J469bcDD74#"}, /* 128: 12 x 7, down, 16 faces, 4 blocked, stuck 2%, search 40, corners */
    {12, 7, 1, "nkjmqlqdohjlbmhbabnjqcheeiffberrfchhdoocm##fnanqigcqmddaocfeidfpbaajagihkpnnbgkdgkcq",
     "VKBPI7TaJDB7ZeDRXZSWTC1NQd66RQ55EC1UA88GP##EScVM4YGIe00HJ9bN4AbLOcHWX3dUKLFFOY2a329M"}, /* 129: 12 x 7, down, 18 faces, 2 blocked, stuck 2%, search 41, centre */
    {12, 7, 1, "xtHBF##GHuuCDBCtFGHxBxHzsxGBtGEzvAEE#uuGvDEwuAG#ywyvEzwwHBwzBCyvEyyxzDsxyuDzs##tsCHw",
     "SX8bA##G8CCEKOEXA2PSFRPNWZGbV21NUB00#66aUK15HBa#JMQDcYT59FTYOIQDc33R7LWZJHL74##V4I9M"}, /* 130: 12 x 7, down, 16 faces, 6 blocked, stuck 8%, search 39, gates */
    {12, 7, 1, "xtxddjatfstytxh#eqmr#sAfmxqiihmjmqqfgrshgheatAqjtiiqgggfexrygrs#jjjm#xssaaemyfdfhydh",
     "d2d55ZOTWQKE23B#HSIc#QG6Y3R44NIMYS96UDCBFNXbKG9MT00RF11WXJcEUDC#88ZV#J77bOHVAPaPLAaL"}, /* 131: 12 x 7, down, 16 faces, 4 blocked, stuck 10%, search 40, pillars */
    {12, 7, 2, "#vBPyvPMMKv#PCCKKLvLtBDCHNvtKFvFuFHyHNyNF##uyHtJPzFtNIHyHPPIzFyuNINLCCJI#IuJuLuJIDC#",
     "#7YTXCTLLb7#Bccb3DCDFYVUONIF38IP5PQKONSW8##5XAR2BEJRMHQKA110EJS6MHWaUG20#49Z6a9Z4VG#"}, /* 132: 12 x 7, left, 17 faces, 6 blocked, stuck 5%, search 39, corners+centre */
    {12, 7, 2, "zjzqigybizjqtbn#knuj#yzntrnogiAqyyww#rrigzugkjj#kkvozeeguvmqvii#vAmA#jkuytbcrctkbAyg",
     "B2bCQMIUQB2CFU9#ZWLE#IaWFX9Y8DRKAAJJ#VVD8a4cHE7#11OYb55c4GSKO66#GTSR#7HL3P0NXNPZ0T3M"}, /* 133: 12 x 7, left, 19 faces, 6 blocked, stuck 10%, search 39, pillars+sides */
    {12, 7, 2, "yxzpkuzojouwjjs#knoA#ppxvjppxxAwymmuoowAumAwAyowywrsqkklqqlksvs#qpqv#jnqnlrnlrrkvjmA",
     "ICBTZ0BAcA0N889#bRVL#STCQcHH66d1U22KOOYdKXL1GUVNIYDJ544P77PZ9QJ#5SWM#3RWaEDaEFFbM3XG"}, /* 134: 12 x 7, left, 17 faces, 4 blocked, stuck 22%, search 40, pillars */
    {12, 7, 2, "qxxxt##xfvxslcctmbbfchhverqmghczrarAwqqgqgvrwahmAllagavwwhwAAlfwttdibezsibfdf##mqhxf",
     "NP0PA##09FE6RDDAXdd9UO5FaZcWITUH414BYcV8VIJZL15XQ2Rb8bJ77OLBQ2SY33MGKaH6GKCMS##WNTEC"}, /* 135: 12 x 7, left, 20 faces, 4 blocked, stuck 35%, search 40, notches */
    {12, 7, 2, "MGAEPEGDKuDPBBJuPvAKJuLsMHPKJBEEvsHHBH###JL###EsxPxwwGJKAsPEOOMMuAJuLDLuKxGKxIDLssIL",
     "VIUN8NIXW2X7QQEZ8SUWb2L9V06FER55SYPPR0###bL###49C6CAAa1FcY74MMBBZc1JTOTJ3Da3DGOHKKGH"}, /* 136: 12 x 7, left, 18 faces, 6 blocked, stuck 40%, search 39, bars */
    {12, 7, 2, "kAmdm##dbiqmoiikbyhhyfiqkxfxxklloxbq#fttdbdyhht#fwrrxikkbryqxwApArAimtpddhror##hbooo",
     "AQ0D0##D7OGJUCCT7N11NaO9AIX66TVVUZLG#aSSbBMP55R#X4KKZY22BcP9I4QHW8WYJRHMb3cF8##3LEEF"}, /* 137: 12 x 7, left, 17 faces, 6 blocked, stuck 50%, search 39, gates */
    {12, 8, 3, "##wswFEzyz###JBsFPPCyAA#CJJCPCCFCPvPBvxxNGNFGvJvsByGGGwwyzzuKKusvAAGJJPu#zzxxvIBxIE###uFwwEFxE##",
     "##8T87cL2L###AWT755V2HH#CAfCZIIJVZOEbOPPFDFJSaYadWMD4400MBB6KKUdQ11SYfE6#99NNQebGec###UXRR3XG3##"}, /* 138: 12 x 8, centre, 18 faces, 12 blocked, stuck 8%, search 42, octagon */
    {12, 8, 3, "##dvhjjenz###hjdnxevzbb#sxjbxgwdwtvgshdbg##viihnsgntj##vvthxsddaesyyhjgn#asgyiizzff###ewywtnii##",
     "##ScRCCNY5###b0XYQNF522#KU0JUZWSWHFdTRXJZ##9GGPDKdBaV##9cHPQ6338ET11bVMD#86MOLL44II###E7O7aBAA##"}, /* 139: 12 x 8, centre, 17 faces, 16 blocked, stuck 12%, search 40, octagon+centre */
    {12, 8, 3, "#spqpzqmAmA#zoowqyryzApoqnkknyswwxxory###wp###AAkz###zt###mpktznntvoplyArtxuutssxluy#tourvmsuus#",
     "#cdSATS7575#W11bXBaKTQAMXUJJUBcFF22MaK###bd###LLGW###DZ###NHGZDCC8RYH4PQO0I338EEI49P#0Y9ORN6VV6#"}, /* 140: 12 x 8, centre, 17 faces, 16 blocked, stuck 18%, search 40, corners+bars */
    {12, 8, 3, "AHPxEEJPDyxyIAwIzLJtPPDvNsK#NNsL#MvsszvzvvsMNuwCKuuHzKKtuCttxHI#HDAM#DFMuMAAEKttEMzFzsxIDDvKuAFF",
     "ZcQDLLJQCGDGAZgTIYJh66CMVWU#EE9Y#OMW5RaI889dVPgbUKKeRXXhPb774cT#ejiB#jfO2diSNH33NB1f154AFFaH2S00"}, /* 141: 12 x 8, centre, 21 faces, 4 blocked, stuck 22%, search 46, pillars */
    {12, 8, 3, "fpelb##qaoelfbhnnghcklloffe#hghc#hdqqmdkarajkrmmmbbjbiqjfrrlijb#rdpi#cleqinnpkpifacddpoqp##oihdr",
     "PdVEB##fgeXE8BR113CDT00eP8V#R3CD#baY6GUWhZh4TO22GAAJKQY4FSO5cJK#ZU7L#95X6QMMIW7LFg9HadNfI##NcbHS"}, /* 142: 12 x 8, centre, 18 faces, 8 blocked, stuck 30%, search 44, pillars+notches */
    {12, 8, 3, "ofanhoAmaAxweebnnxxwwbbvshbkoiivwggvxkshp##mhAbnlaape##wvAffppgnhhkzzrrbrgarrskmwonslksmvsaverkf",
     "EgPZXE5eP5Kcjj0AA99c288VNU0bfDDa2LLVKbNUi##eXY3Z4MMId##TaY11iIRS77BFFQQ3GRJOO6BWTfSh4HhWC6JCdGHg"}, /* 143: 12 x 8, centre, 20 faces, 4 blocked, stuck 32%, search 46, centre */
    {12, 8, 1, "xosto##nqomolAkntlsprrmmxwwkssysmomjovkqwwyntjlzvwwApsmprlrvlnnvputtrqrzyvnkjjuytujyvqjlk##ykuzz",
     "SihVM##IeMEG8JAPVjTQLLEFS33WhNcTgGFZiCAeBBcI6ZY7C11JaNgQ2jDX8P4daKbb25D7fX4WHHKf60OUd5OYR##UR099"}, /* 144: 12 x 8, down, 18 faces, 4 blocked, stuck 8%, search 46, notches */
    {12, 8, 1, "bjieuquqvcioAvnrzzhhhcibbwwrnvchjbAoAn###iw###cbbt###gv###ponewvppvoegihtgdhgnnoppiecgeAercrgdpo",
     "YcOdP3P35ABbEfVH11X77AOQT00HV5aecYNSEI###BW###MQTL###Z2###DGIdWfFU2SRZ8XL4Ke9CCbUD8Ra46N6JMJ9KFG"}, /* 145: 12 x 8, down, 19 faces, 12 blocked, stuck 12%, search 42, bars */
    {12, 8, 1, "EywAM##BAFEKPsyBEsEAFECMPCJ#CAJA#KtwssxOBCAHKPEIwCKOBtDDCJxKwHs#ItPx#JJvItswtJDDxxIxItKIP##vyPwy",
     "SCIMJ##LMHW81TCLSTWPHGZJ1FU#NReP#EbY22a4ANRVEcGgIFO4A5DDZXf8YV9#g5ca#Xedh39QbU77KfhKB3OB0##d60Q6"}, /* 146: 12 x 8, down, 19 faces, 8 blocked, stuck 15%, search 44, pillars+notches */
    {12, 8, 1, "knknbbeqpprekaibhhegmjrpbjj#qjkg#deahqqdgginikfmnemeaorcidfpkrd#qaci#dcmnhjqbponrgimcdjbcphmrgch",
     "bKbGII4PSSLc9eFB0041ZXLRB22#PXi1#fcga33f66TjdiOVKUAUeCYhdHOR9Y7#MgJT#HhZGE5MW8CjQNFVJ75WD8EAQNDa"}, /* 147: 12 x 8, down, 17 faces, 4 blocked, stuck 15%, search 46, pillars */
    {12, 8, 1, "#HwANtCHAzH#uMwttJFIPDAzFOLMINzuFxFPOOHPM##EDwtPuEACC##AswxPJIxLONJsACzPJJxtMNJsItzz#IDNDuDIDNs#",
     "#53DKZT6DV6#2h3HHOWeR7MaXGEJeKVQW8XAGS5RJ##d71cA2dMBB##If18NO49ESgPCITaN009chbPCLZYY#4UbUQFLFgf#"}, /* 148: 12 x 8, down, 19 faces, 8 blocked, stuck 25%, search 44, corners+centre */
    {12, 8, 1, "yhxdtytimpxhdhdnmodkkkpAwiynttifaanAmmyin##oAkwkwioss##wAakwbmeebllwfnjjbmhiolxoddatbobhleetxnbh",
     "MFDJPMIegjDZJFfigbfULLjRHG15cPedYYiR991G5##XQ8VUHWbhh##2Qa8VCB33S662dNAAEBTWOK7X00aICOETK44c7NSZ"}, /* 149: 12 x 8, down, 20 faces, 4 blocked, stuck 30%, search 46, centre */
    {12, 8, 1, "BxMCGBMMyxuuysIMDBsuJJutPKH#tzxy#JPy#DHtCJxysBP##BGDJGCDDBt#Cyv#zGJv#GuPPuvsxGKvCCPHAHvAsDxIsttv",
     "R6baC0GbL6NfhXUGD0cZQ5NFIe2#JBHE#5YL#d2JK7VEcWY##RTdQ1K33WF#ahS#B178#Tf9IZSXHCe8AA9MgM4gPDVUPOO4"}, /* 150: 12 x 8, down, 18 faces, 8 blocked, stuck 65%, search 50, pillars+sides */
};
