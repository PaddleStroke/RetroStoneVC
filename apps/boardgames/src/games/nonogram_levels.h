/*
 * Nonograms' levels: the house's own pictures (tools/nonogram_pictures.txt, drawn for this game),
 * checked and ordered by tools/nonogram_gen.c: each one solvable line by line (one solution, never a
 * guess), by size from 5 x 5 to 15 x 15, each size from its easiest (the line solver's sweeps, the squares
 * left after the first one); six chapters of 25. Each: its size and rows ('.' empty, else a colour:
 * nonogram.h, ng_letters); in the comment its name (never shown: no words in play) and its grade.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
static const ng_picture ng_levels[NG_LEVELS] = {
    /* 1: a tree, 5x5, 1 sweep, 0 left after the first */
    {5, {"..l..", ".lll.", "lllll", "..n..", ".nnn."}},
    /* 2: a mushroom, 5x5, 1 sweep, 0 left after the first */
    {5, {".rrr.", "rrwrr", "rrrrr", "..m..", ".mmm."}},
    /* 3: a ghost, 5x5, 1 sweep, 0 left after the first */
    {5, {".www.", "w.w.w", "wwwww", "wwwww", "w.w.w"}},
    /* 4: a crown, 5x5, 1 sweep, 0 left after the first */
    {5, {".....", "y.y.y", "yyyyy", "yryry", "yyyyy"}},
    /* 5: a plus sign, 5x5, 1 sweep, 0 left after the first */
    {5, {"..r..", "..r..", "rrrrr", "..r..", "..r.."}},
    /* 6: an umbrella, 5x5, 2 sweeps, 2 left after the first */
    {5, {".rrr.", "rrrrr", "r.n.r", "..n..", ".nn.."}},
    /* 7: an apple, 5x5, 2 sweeps, 2 left after the first */
    {5, {"..nl.", "rrrrr", "rrrrr", "rrrrr", ".rrr."}},
    /* 8: a cat, 5x5, 2 sweeps, 4 left after the first */
    {5, {"k...k", "kkkkk", "k.k.k", "kkkkk", ".kkk."}},
    /* 9: a bell, 5x5, 2 sweeps, 4 left after the first */
    {5, {"..n..", ".yyy.", ".yyy.", "yyyyy", "..k.."}},
    /* 10: a flower, 5x5, 2 sweeps, 4 left after the first */
    {5, {".rrr.", "rryrr", ".rrr.", ".ll..", "..l.."}},
    /* 11: a little house, 5x5, 2 sweeps, 4 left after the first */
    {5, {"..r..", ".rrr.", "rrrrr", ".wnw.", ".wnw."}},
    /* 12: a heart, 5x5, 2 sweeps, 6 left after the first */
    {5, {".r.r.", "rrrrr", "rrrrr", ".rrr.", "..r.."}},
    /* 13: a boat, 5x5, 2 sweeps, 6 left after the first */
    {5, {"..w..", "..ww.", "..k..", "nnnnn", ".nnn."}},
    /* 14: an arrow, 5x5, 2 sweeps, 8 left after the first */
    {5, {"..b..", ".bbb.", "b.b.b", "..b..", "..b.."}},
    /* 15: an anchor, 5x5, 2 sweeps, 8 left after the first */
    {5, {"..k..", ".kkk.", "..k..", "k.k.k", ".kkk."}},
    /* 16: a sword, 5x5, 2 sweeps, 10 left after the first */
    {5, {"..s..", "..s..", "..s..", ".nnn.", "..n.."}},
    /* 17: a star, 5x5, 3 sweeps, 10 left after the first */
    {5, {"..y..", "yyyyy", ".yyy.", ".y.y.", "y...y"}},
    /* 18: a musical note, 5x5, 3 sweeps, 20 left after the first */
    {5, {"..kk.", "..k.k", "..k..", "kkk..", "kkk.."}},
    /* 19: a sun, 6x6, 1 sweep, 0 left after the first */
    {6, {"y.yy.y", ".yyyy.", "yyyyyy", "yyyyyy", ".yyyy.", "y.yy.y"}},
    /* 20: an envelope, 6x6, 1 sweep, 0 left after the first */
    {6, {"......", "wwwwww", "w.ww.w", "ww..ww", "wwwwww", "wwwwww"}},
    /* 21: a cup, 6x6, 1 sweep, 0 left after the first */
    {6, {"......", "wwww..", "wwwwnn", "wwww.n", "wwwwnn", "nnnn.."}},
    /* 22: a tulip, 6x6, 1 sweep, 0 left after the first */
    {6, {"r.rr.r", "rrrrrr", "rrrrrr", ".rrrr.", "..ll..", ".llll."}},
    /* 23: a smiley face, 6x6, 2 sweeps, 4 left after the first */
    {6, {".yyyy.", "y.yy.y", "yyyyyy", "y.yy.y", "yy..yy", ".yyyy."}},
    /* 24: a small bird, 6x6, 2 sweeps, 6 left after the first */
    {6, {"......", ".bb...", "ykbb.b", ".bbbbb", "..bbb.", "..y.y."}},
    /* 25: a leaf, 6x6, 3 sweeps, 20 left after the first */
    {6, {"...lll", "..llll", ".lllll", "lllll.", "llll..", "n....."}},
    /* 26: a moon, 6x6, 3 sweeps, 26 left after the first */
    {6, {"..yyy.", ".yy...", "yy....", "yy....", ".yy...", "..yyy."}},
    /* 27: a tent, 7x7, 2 sweeps, 2 left after the first */
    {7, {"...r...", "..rrr..", ".rrrrr.", ".rr.rr.", "rrr.rrr", "rr...rr", "lllllll"}},
    /* 28: a pear, 7x7, 2 sweeps, 4 left after the first */
    {7, {"...nl..", "..yyy..", "..yyy..", ".yyyyy.", "yyyyyyy", "yyyyyyy", ".yyyyy."}},
    /* 29: a padlock, 7x7, 2 sweeps, 6 left after the first */
    {7, {"..kkk..", ".k...k.", ".k...k.", "yyyyyyy", "yyykyyy", "yyykyyy", "yyyyyyy"}},
    /* 30: an ice-cream cone, 7x7, 2 sweeps, 12 left after the first */
    {7, {"...r...", ".ooooo.", "ooooooo", "ooooooo", ".yyyyy.", "..yyy..", "...y..."}},
    /* 31: a cactus, 7x7, 2 sweeps, 13 left after the first */
    {7, {"...l...", "..lll.l", "l.lll.l", "l.lllll", "lllll..", "..lll..", ".nnnnn."}},
    /* 32: a bottle, 7x7, 2 sweeps, 14 left after the first */
    {7, {"...n...", "...q...", "...q...", "..qqq..", ".qqqqq.", ".qwwwq.", ".qqqqq."}},
    /* 33: a pennant flag, 7x7, 2 sweeps, 18 left after the first */
    {7, {".kr....", ".krrr..", ".krrrrr", ".krrr..", ".kr....", ".k.....", "kkk...."}},
    /* 34: a lollipop, 7x7, 2 sweeps, 18 left after the first */
    {7, {"..rrr..", ".rwwwr.", ".rwrwr.", ".rwwwr.", "..rrr..", "...w...", "...w..."}},
    /* 35: a light bulb, 7x7, 2 sweeps, 18 left after the first */
    {7, {"..yyy..", ".yyyyy.", ".yyyyy.", ".yyyyy.", "..yyy..", "..kkk..", "...k..."}},
    /* 36: a boot, 8x8, 1 sweep, 0 left after the first */
    {8, {".wwww...", ".nnnn...", ".nnnn...", ".nnnn...", ".nnnnnn.", "nnnnnnnn", "nnnnnnnn", "kk.kkkkk"}},
    /* 37: a dog's face, 8x8, 2 sweeps, 4 left after the first */
    {8, {"..wwww..", "nnwwwwnn", "nnkwwknn", "nnwwwwnn", "n.wkkw.n", "n.wwww.n", "...oo...", "........"}},
    /* 38: a watering can, 8x8, 2 sweeps, 4 left after the first */
    {8, {".qqq....", "q...q..q", "qqqqq.qq", "qqqqqqq.", "qqqqqq..", "qqqqq...", "qqqqq...", "........"}},
    /* 39: a frog's face, 8x8, 2 sweeps, 4 left after the first */
    {8, {".ll..ll.", "lkwllwkl", "llllllll", "llllllll", "l.llll.l", "ll....ll", ".llllll.", "........"}},
    /* 40: a car, 8x8, 2 sweeps, 8 left after the first */
    {8, {"........", "..rrrr..", ".rsrrsr.", "rrrrrrrr", "rrrrrrry", "rkkrrkkr", ".kk..kk.", "........"}},
    /* 41: a duck, 8x8, 3 sweeps, 14 left after the first */
    {8, {".yyy....", "rykyy...", ".yyy...y", "..yyyyyy", ".yyyyyyy", ".yyyyyy.", "..yyyy..", "ssssssss"}},
    /* 42: a mouse, 8x8, 3 sweeps, 16 left after the first */
    {8, {"nn....nn", "nonnnnon", "nnnnnnnn", ".nknnkn.", ".nnnnnn.", "..nnnn..", "...kk...", "........"}},
    /* 43: a teddy bear's face, 8x8, 3 sweeps, 16 left after the first */
    {8, {"nn....nn", "nnnnnnnn", ".nnnnnn.", ".nknnkn.", ".nnmmnn.", ".nmkkmn.", ".nnmmnn.", "..nnnn.."}},
    /* 44: a rabbit, 8x8, 4 sweeps, 26 left after the first */
    {8, {"...w.w..", "...w.w..", "...wwww.", "...wwkw.", "..wwwwwo", ".wwwwww.", "wwwwwww.", ".ww.www."}},
    /* 45: a lighthouse, 10x10, 1 sweep, 0 left after the first */
    {10, {"....kk....", "...yyyy...", "...kkkk...", "...rrrr...", "...wwww...", "...rrrr...", 
          "..wwwwww..", "..rrrrrr..", "..wwwwww..", "nnnnnnnnnn"}},
    /* 46: an hourglass, 10x10, 1 sweep, 0 left after the first */
    {10, {"nnnnnnnnnn", ".ssssssss.", "..ssssss..", "...yyyy...", "....yy....", "....yy....", 
          "...syys...", "..syyyys..", ".yyyyyyyy.", "nnnnnnnnnn"}},
    /* 47: a fish, 10x10, 2 sweeps, 8 left after the first */
    {10, {"..........", "...bbbb...", ".bbbbbbb.b", "bb.bbbbbbb", "bbbbbbbbbb", "bbbbbbbbbb", 
          ".bbbbbbbbb", "..bbbbbb.b", "...bbbb...", ".........."}},
    /* 48: a penguin, 10x10, 2 sweeps, 8 left after the first */
    {10, {"...kkkk...", "..kkkkkk..", "..kwkkwk..", "..kkyykk..", ".kkwwwwkk.", "kkkwwwwkkk", 
          "k.kwwwwk.k", "..kwwwwk..", "..kkwwkk..", "..yy..yy.."}},
    /* 49: a snail, 10x10, 2 sweeps, 8 left after the first */
    {10, {"o.o.......", "o.o..nnnn.", "o.o.nyyyyn", "oooonynnyn", "oooonynnyn", ".ooonyyyyn", 
          ".oooonnnn.", ".ooooooooo", "..........", ".........."}},
    /* 50: a rabbit, 10x10, 2 sweeps, 8 left after the first */
    {10, {"..ww..ww..", "..wo..ow..", "..wo..ow..", "..ww..ww..", ".wwwwwwww.", "wwkwwwwkww", 
          "wwwwoowwww", "wwwwwwwwww", ".wwwwwwww.", "..wwwwww.."}},
    /* 51: a ladybug, 10x10, 2 sweeps, 12 left after the first */
    {10, {"..k.kk.k..", "...kkkk...", ".rrrkkrrr.", "rrkrkkrkrr", "rrrrkkrrrr", "rkrrkkrrkr", 
          "rrrrkkrrrr", ".rrrkkrrr.", "..rrkkrr..", "...rkkr..."}},
    /* 52: a candle, 10x10, 2 sweeps, 12 left after the first */
    {10, {"....yy....", "...yyyy...", "...yrry...", "....yy....", "....kk....", "..wwwwww..", 
          "..wwwwww..", "..wwwwww..", "nnnnnnnnnn", ".nnnnnnnn."}},
    /* 53: a bell, 10x10, 2 sweeps, 12 left after the first */
    {10, {"....nn....", "...yyyy...", "..yyyyyy..", "..ywyyyy..", "..ywyyyy..", ".yyyyyyyy.", 
          ".yyyyyyyy.", "yyyyyyyyyy", "....kk....", ".........."}},
    /* 54: a hot-air balloon, 10x10, 2 sweeps, 12 left after the first */
    {10, {"...ryyr...", ".rryyyyrr.", "rrryyyyrrr", "rrryyyyrrr", "rrryyyyrrr", ".rryyyyrr.", 
          "..rryyrr..", "...k..k...", "...nnnn...", "...nnnn..."}},
    /* 55: a mushroom, 10x10, 2 sweeps, 14 left after the first */
    {10, {"...rrrr...", ".rrwrrrrr.", "rrrrrrwrrr", "rwrrrrrrrr", "rrrrrwrrrr", ".rrrrrrrr.", 
          "...mmmm...", "...mmmm...", "...mmmm...", "..mmmmmm.."}},
    /* 56: a bird on a branch, 10x10, 2 sweeps, 14 left after the first */
    {10, {"..........", "......bbb.", ".....bbkby", ".....bbbb.", "..bbbbbbb.", ".bbbbssbb.", 
          "bbbbbssb..", "bb..bbb...", "nnnnnyynnn", "......nn.."}},
    /* 57: a car, 10x10, 2 sweeps, 14 left after the first */
    {10, {"..........", "...rrrrr..", "..rssrssr.", ".rsssrsssr", "rrrrrrrrrr", "yrrrrrrrrr", 
          "rrrrrrrrrr", ".rkkrrrkkr", "..kk...kk.", ".........."}},
    /* 58: a strawberry, 10x10, 2 sweeps, 16 left after the first */
    {10, {"..l.ll.l..", "...llll...", ".rrllllrr.", "rrrrrrrrrr", "ryrryrryrr", "rrrrrrrrrr", 
          ".rryrryrr.", "..rrrrrr..", "...ryrr...", "....rr...."}},
    /* 59: a bee, 10x10, 2 sweeps, 17 left after the first */
    {10, {"..........", "..ss..ss..", ".ssss.ssss", "..sssssss.", ".kkyykyyk.", "kkkyykyykk", 
          "kwkyykyyk.", ".kkyykyyk.", "...k..k...", ".........."}},
    /* 60: a cup of tea, 10x10, 2 sweeps, 19 left after the first */
    {10, {"..........", "..s.s.s...", "...s.s....", ".wwwwwww..", ".wwwwwwwww", ".wwwwwww.w", 
          ".wwwwwwwww", "..wwwww...", "...www....", ".nnnnnnnn."}},
    /* 61: a cat, 10x10, 2 sweeps, 20 left after the first */
    {10, {".k......k.", ".kk....kk.", ".kkkkkkkk.", ".k.kkkk.k.", ".kkkkkkkk.", ".kkkrrkkk.", 
          ".kkkkkkkk.", "..kkkkkk..", "...kkkk...", "..kkkkkk.."}},
    /* 62: a house, 10x10, 2 sweeps, 20 left after the first */
    {10, {"....rr....", "...rrrr...", "..rrrrrr..", ".rrrrrrrr.", "rrrrrrrrrr", ".nnnnnnnn.", 
          ".n.nnnn.n.", ".nnnnnnnn.", ".nnnn..nn.", ".nnnn..nn."}},
    /* 63: an elephant, 10x10, 2 sweeps, 23 left after the first */
    {10, {"....ssss..", "..ssssssss", ".sssssssss", "ssskssssss", "ss.sssssss", "s..sssssss", 
          "s..ss..ss.", "s..ss..ss.", "ss.ss..ss.", ".........."}},
    /* 64: a butterfly, 10x10, 2 sweeps, 24 left after the first */
    {10, {"..k....k..", "...k..k...", "ppp.kk.ppp", "pyppkkppyp", "ppppkkpppp", ".pppkkppp.", 
          "..ookkoo..", ".oookkooo.", ".oo.kk.oo.", "....kk...."}},
    /* 65: an ice-cream cone, 10x10, 2 sweeps, 28 left after the first */
    {10, {"....rr....", "..oooooo..", ".oooooooo.", ".owoooooo.", "oooooooooo", ".nnmnnmnn.", 
          "..nmnnmn..", "...nmmn...", "...nnnn...", "....nn...."}},
    /* 66: a pear, 10x10, 2 sweeps, 34 left after the first */
    {10, {".....n....", ".....nll..", "....yy....", "...yyyy...", "...yyyy...", "..yyyyyy..", 
          ".yyyyyyyy.", ".yyyyyyyy.", ".yyyyyyyy.", "..yyyyyy.."}},
    /* 67: a sailing boat, 10x10, 2 sweeps, 35 left after the first */
    {10, {"....k.....", "....kw....", "....kww...", "....kwww..", "....kwwww.", "....kwwwww", 
          "....k.....", "nnnnnnnnnn", ".nnnnnnnn.", "..nnnnnn.."}},
    /* 68: a guitar, 10x10, 2 sweeps, 40 left after the first */
    {10, {"....kk....", "....nn....", "....nn....", "....nn....", "...yyyy...", "..yyyyyy..", 
          "...ykky...", "..yykkyy..", ".yyyyyyyy.", "..yykkyy.."}},
    /* 69: an owl, 10x10, 3 sweeps, 20 left after the first */
    {10, {".n......n.", ".nn....nn.", ".nnnnnnnn.", "nwwwnnwwwn", "nwkwnnwkwn", "nwwwyywwwn", 
          ".nnnyynnn.", ".nmnmmnmn.", "..nnnnnn..", "..yy..yy.."}},
    /* 70: a turtle, 10x10, 3 sweeps, 28 left after the first */
    {10, {"..........", "...qqqq...", "..qqlqqq..", ".qlqqqlqll", ".qqqqqqqlk", "qqqqqqqqll", 
          ".ll....ll.", ".ll....ll.", "..........", ".........."}},
    /* 71: a steam engine, 10x10, 3 sweeps, 28 left after the first */
    {10, {".ww.......", "ww.ww.....", ".kk...rrrr", ".kk...rssr", "rrrrrrrssr", "rrrrrrrrrr", 
          "rrrrrrrrrr", ".kk.kk.kk.", ".kk.kk.kk.", ".........."}},
    /* 72: an apple, 10x10, 3 sweeps, 32 left after the first */
    {10, {".....n.l..", "....nlll..", "..rrnrrr..", ".rwrrrrrr.", ".wrrrrrrr.", ".rrrrrrrr.", 
          ".rrrrrrrr.", "..rrrrrr..", "..rr..rr..", ".........."}},
    /* 73: a fir tree, 10x10, 3 sweeps, 34 left after the first */
    {10, {"....ll....", "...llll...", "..llllll..", "...llll...", "..llllll..", ".llllllll.", 
          "..llllll..", ".llllllll.", "llllllllll", "....nn...."}},
    /* 74: a pineapple, 10x10, 3 sweeps, 36 left after the first */
    {10, {"..l.ll.l..", "...llll...", "....ll....", "..yyyyyy..", ".ynyynyyn.", ".yyyyyyyy.", 
          ".yynyynyy.", ".yyyyyyyy.", ".ynyynyyn.", "..yyyyyy.."}},
    /* 75: an umbrella, 10x10, 3 sweeps, 40 left after the first */
    {10, {"....k.....", "..rrwrr...", ".rrrwrrr..", "rrrrwrrrr.", "r.r.k.r.r.", "....k.....", 
          "....k.....", "....k.....", "..k.k.....", "...k......"}},
    /* 76: a dog, 10x10, 3 sweeps, 52 left after the first */
    {10, {"......kk..", ".....nnnn.", "n....nknnk", "n....nnnn.", "nn..nnnn..", ".nnnnnnn..", 
          ".nnnnnnn..", ".nnnnnnn..", ".nn...nn..", ".nn...nn.."}},
    /* 77: a cactus in a pot, 10x10, 3 sweeps, 52 left after the first */
    {10, {"....ll....", ".l..ll....", ".l..ll..l.", ".lllll..l.", "....lllll.", "....ll....", 
          ".rrrrrrrr.", "..rrrrrr..", "..rrrrrr..", "...rrrr..."}},
    /* 78: a carrot, 10x10, 3 sweeps, 54 left after the first */
    {10, {".l..ll..l.", ".ll.ll.ll.", "..llllll..", "..yyyyyy..", "..yyyyyy..", "..ynyyyy..", 
          "...yyyny..", "...yyyy...", "....yy....", "....yy...."}},
    /* 79: cherries, 10x10, 4 sweeps, 50 left after the first */
    {10, {"......nll.", ".....nn...", "...nn.n...", "...n..n...", "..n...n...", ".rr...rr..", 
          "rwrr.rwrr.", "rrrr.rrrr.", "rrrr.rrrr.", ".rr...rr.."}},
    /* 80: a key, 10x10, 4 sweeps, 50 left after the first */
    {10, {"...yyyy...", "..yy..yy..", "..y....y..", "..yy..yy..", "...yyyy...", "....yy....", 
          "....yy....", "....yyyy..", "....yy....", "....yyy..."}},
    /* 81: an anchor, 10x10, 4 sweeps, 77 left after the first */
    {10, {"...bbb....", "...b.b....", "...bbb....", ".bbbbbbb..", "....b.....", "....b.....", 
          "b...b...b.", "bb..b..bb.", ".bb.b.bb..", "..bbbbb..."}},
    /* 82: a crescent moon with a star, 10x10, 5 sweeps, 87 left after the first */
    {10, {"...yyyy...", ".yyyy.....", ".yyy......", "yyy.....w.", "yyy....www", "yyy.....w.", 
          "yyyy......", ".yyyy.....", "..yyyyyy..", "....yy...."}},
    /* 83: a camera, 12x12, 1 sweep, 0 left after the first */
    {12, {"............", ".kk....kkkk.", "kkkkkkkkkkkk", "kkkk....kkkk", "kkk.bbbb.kyk", "kk.bbsbbbb.k", 
          "kk.bbbbbb.kk", "kk.bbbbbb.kk", "kkk.bbbb.kkk", "kkkk....kkkk", "kkkkkkkkkkkk", "............"}},
    /* 84: a fish bowl, 12x12, 2 sweeps, 8 left after the first */
    {12, {"..ssssssss..", "...s....s...", "..s......s..", ".ssssssssss.", "sssssrrsssss", "ssssrrrrsrss", 
          "sssrkrrrrrss", "ssssrrrrsrss", ".ssssrrssss.", "..ssssssss..", "...nnnnnn...", "..nnnnnnnn.."}},
    /* 85: a submarine, 12x12, 2 sweeps, 14 left after the first */
    {12, {".....kk.....", "......k.....", "......k.....", "....yyyyy...", "....yyyyy...", ".yyyyyyyyyy.", 
          "yyyyyyyyyyyy", "yysyysyysyyy", "yyyyyyyyyyyy", ".yyyyyyyyyy.", "ssssssssssss", ".ss.sss.sss."}},
    /* 86: a panda's face, 12x12, 2 sweeps, 20 left after the first */
    {12, {".kk......kk.", "kkkk....kkkk", "kkkwwwwwwkkk", ".wwwwwwwwww.", "wwkkwwwwkkww", "wkkkwwwwkkkw", 
          "wk.kwwwwk.kw", "wwkkwwwwkkww", "wwwwwkkwwwww", ".wwwwwwwwww.", "..wwwkkwww..", "....wwww...."}},
    /* 87: a pumpkin, 12x12, 2 sweeps, 20 left after the first */
    {12, {"......ll....", ".....ll.....", "..yyyyyyyy..", ".yyy.yy.yyy.", "yyyy.yy.yyyy", "yyyy.yy.yyyy", 
          "yyyy.yy.yyyy", "yyyy.yy.yyyy", "yyyy.yy.yyyy", ".yyy.yy.yyy.", "..yyyyyyyy..", "............"}},
    /* 88: a whale, 12x12, 2 sweeps, 26 left after the first */
    {12, {"..s.s.......", "...s........", "...s........", ".bbbbbb.....", "bbbbbbbbb..b", "bbbbbbbbbbbb", 
          "b.bbbbbbbbbb", "bbbbbbbbbbb.", "wwwwwbbbbbb.", "wwwwwwbbbb..", "..wwwwbb....", "............"}},
    /* 89: a helicopter, 12x12, 2 sweeps, 27 left after the first */
    {12, {"kkkkkkkkkkkk", ".....k......", "...yyyyy....", "..ssyyyyy...", ".sssyyyyyy.y", ".ssyyyyyyyyy", 
          ".yyyyyyyyyyy", "..yyyyyyy..y", "...yyyyy....", "...k...k....", ".kkkkkkkkk..", "............"}},
    /* 90: an octopus, 12x12, 2 sweeps, 28 left after the first */
    {12, {"...pppppp...", "..pppppppp..", ".pppppppppp.", ".ppw.pp.wpp.", ".pppppppppp.", "..pppppppp..", 
          ".pppppppppp.", "pp.p.pp.p.pp", "p..p.pp.p..p", "p.pp.pp.pp.p", "pp..pp..p.pp", "............"}},
    /* 91: a bear's face, 12x12, 2 sweeps, 28 left after the first */
    {12, {".nn......nn.", "nnnn....nnnn", "nnnnnnnnnnnn", ".nnnnnnnnnn.", ".nnknnnnknn.", ".nnnnnnnnnn.", 
          ".nnnwwwwnnn.", "nnnwwkkwwnnn", "nnnwwwwwwnnn", ".nnnwwwwnnn.", "..nnnnnnnn..", "....nnnn...."}},
    /* 92: an aeroplane, 12x12, 2 sweeps, 28 left after the first */
    {12, {".....ss.....", "....wwww....", "....wwww....", "...bwwwwb...", ".bbbwwwwbbb.", "bbbbwwwwbbbb", 
          "bbbbwwwwbbbb", "....wwww....", "....wwww....", "...bwwwwb...", "..bbbwwbbb..", ".....ww....."}},
    /* 93: a tractor, 12x12, 2 sweeps, 37 left after the first */
    {12, {"..rrrr..k...", "..r..r..k...", "..r..r..k...", "..rrrrrrrr..", ".rrrrrrrrrr.", "rrrrrrrrrrrr", 
          ".kkkk.rrrrrr", "kkkkkkrrrrrr", "kk..kk...kk.", "kk..kk..kkkk", "kkkkkk..kkkk", ".kkkk....kk."}},
    /* 94: a crab, 12x12, 2 sweeps, 52 left after the first */
    {12, {".r.r....r.r.", ".rrr....rrr.", "..r......r..", "..r.k..k.r..", "..r.r..r.r..", ".rrrrrrrrrr.", 
          "rrrrrrrrrrrr", "rrrrrrrrrrrr", ".rrrrrrrrrr.", "r..r....r..r", ".r..r..r..r.", "r..........r"}},
    /* 95: a castle tower, 12x12, 2 sweeps, 54 left after the first */
    {12, {"......krr...", "......krrr..", "......k.....", ".ww..ww..ww.", ".wwwwwwwwww.", "..wwwwwwww..", 
          "..www..www..", "..wwwwwwww..", "..wwwwwwww..", "..wwwwwwww..", "..www..www..", "..www..www.."}},
    /* 96: a sunflower, 12x12, 3 sweeps, 28 left after the first */
    {12, {"...y.yy.y...", "..yyyyyyyy..", ".yyynnnnyyy.", "yyynnnnnnyyy", ".yynnnnnnyy.", "yyynnnnnnyyy", 
          ".yyynnnnyyy.", "..yyyyyyyy..", "...y.ll.y...", ".lll.ll.lll.", "..llllllll..", ".....ll....."}},
    /* 97: a small teapot, 12x12, 3 sweeps, 38 left after the first */
    {12, {".....kk.....", "...bbbbbb...", "..wwwwwwww..", "b..bbbbbbbbb", "bb.bbbbbbb.b", ".bbbbwwbbb.b", 
          "..bbwwwwbb.b", "..bbbwwbbbbb", "..bbbbbbbbb.", "...bbbbbbb..", "....kkkkk...", "............"}},
    /* 98: a swan, 12x12, 3 sweeps, 40 left after the first */
    {12, {"..www.......", ".wkwwyy.....", ".ww..y......", ".ww.........", "..ww........", "...ww...ww..", 
          "...ww..www..", "..wwwwwwwww.", ".wwwwwwwwww.", ".wwwwwwwwww.", "..wwwwwwww..", "ssssssssssss"}},
    /* 99: a horse's head, 12x12, 3 sweeps, 42 left after the first */
    {12, {"......k.k...", ".....nnkk...", "....nnnnkk..", "...nnknnnkk.", "..nnnnnnnkk.", ".nnnnnnnnnkk", 
          "nnnnnnnnnnkk", "nnn..nnnnnkk", "nn....nnnnnk", ".......nnnnk", ".......nnnnn", "......nnnnnn"}},
    /* 100: a windmill, 12x12, 3 sweeps, 44 left after the first */
    {12, {"ww........ww", "www......www", ".www.nn.www.", "..wwnnnnww..", "...wwkkww...", "...nwwwwn...", 
          "..wwwnnwww..", ".wwwnnnnwww.", "www.nnnn.www", "ww.nnnnnn.ww", "...nnkknn...", "..nnnkknnn.."}},
    /* 101: an acorn, 12x12, 3 sweeps, 46 left after the first */
    {12, {"......n.....", ".....nn.....", "..nnnnnnnn..", ".nnnnnnnnnn.", "nnnnnnnnnnnn", ".yyyyyyyyyy.", 
          ".yyyyyyyyyy.", ".yyyyyyyyyy.", "..yyyyyyyy..", "...yyyyyy...", "....yyyy....", ".....yy....."}},
    /* 102: a fox's face, 12x12, 3 sweeps, 48 left after the first */
    {12, {"r..........r", "rr........rr", "rwr......rwr", "rwwrrrrrrwwr", "rrrrrrrrrrrr", "rrkrrrrrrkrr", 
          "wrrrrrrrrrrw", "wwrrrrrrrrww", "wwwwrrrrwwww", "..wwwwwwww..", "...wwkkww...", ".....ww....."}},
    /* 103: a globe on a stand, 12x12, 3 sweeps, 55 left after the first */
    {12, {"...bbbbbk...", "..bbllbbb.k.", ".bllllbbbb.k", ".blllbbblb.k", ".bbllbbllb.k", ".bbbbbblll.k", 
          ".bbbbbbllb.k", "..bbbbblb.k.", "...bbbbbk...", ".....n......", "....nnn.....", "..nnnnnnn..."}},
    /* 104: a parrot, 12x12, 3 sweeps, 72 left after the first */
    {12, {"....rrrr....", "...rrrrrr...", "..kkrwkrr...", ".kkkrrrrr...", ".kk.rrrrrr..", "..k.rrrbbr..", 
          "....rrrbbbr.", "....rrybbbr.", "....ryybbb..", "..nnnkknkknn", "......bbb...", "......bb...."}},
    /* 105: a tulip in a pot, 12x12, 3 sweeps, 72 left after the first */
    {12, {"...r.rr.r...", "...rrrrrr...", "...rrrrrr...", "....rrrr....", ".....ll.....", ".l...ll...l.", 
          ".ll..ll..ll.", "..llllllll..", ".nnnnnnnnnn.", "..nnnnnnnn..", "..nnnnnnnn..", "...nnnnnn..."}},
    /* 106: a kite, 12x12, 4 sweeps, 75 left after the first */
    {12, {".....k......", "....rky.....", "...rrkyy....", "..rrrkyyy...", ".kkkkkkkkk..", "..yyykrrr...", 
          "...yykrr....", "....ykr.....", ".....k......", "......k.....", ".....oko....", "......k....."}},
    /* 107: a palm tree, 12x12, 4 sweeps, 84 left after the first */
    {12, {"..lll..lll..", ".llllllllll.", "ll..llll..ll", "l..ll.nll..l", "..ll..nn.ll.", ".l....nn..l.", 
          "......nn....", "......nn....", ".....nn.....", ".....nn.....", "..yyyyyyyy..", ".yyyyyyyyyy."}},
    /* 108: a hen, 12x12, 5 sweeps, 84 left after the first */
    {12, {"..rr........", ".rrr........", ".www.......w", "ywkw......ww", "yywww....www", ".rwwww..wwww", 
          ".rwwwwwwwww.", "..wwwwwwwww.", "..wwwwwwwww.", "...wwwwwww..", ".....y.y....", "....yy.yy..."}},
    /* 109: a bicycle, 12x12, 5 sweeps, 110 left after the first */
    {12, {"..........kk", ".kkk.....k..", "..r......r..", "..rrrrrrrr..", "..r.....rr..", "..r....r.r..", 
          "..r...rr.r..", ".kkkrr..kkk.", "k.r.k..k.r.k", "k.k.k..k.k.k", "k...k..k...k", ".kkk....kkk."}},
    /* 110: a pair of scissors, 12x12, 9 sweeps, 132 left after the first */
    {12, {".ss......ss.", ".ss......ss.", "..ss....ss..", "...ss..ss...", "....ssss....", ".....kk.....", 
          "....ssss....", "...rr..rr...", ".rrrr..rrrr.", "rr..r..r..rr", "rr..r..r..rr", ".rrrr..rrrr."}},
    /* 111: a sleeping cat, 15x15, 2 sweeps, 2 left after the first */
    {15, {"...............", "...............", "...............", ".y...y.........", ".yy.yy...yyy...", 
          ".yyyyy.yyynyny.", "yyyyyyyyyynynyy", "ykkykkyyyynynyy", "yyyoyyyyyyyyyyy", ".yyyyyyyyyyyyyy", 
          "n.yyyyyyyyyyyy.", "nyynyynyynyyyy.", "...............", "...............", "..............."}},
    /* 112: a castle, 15x15, 2 sweeps, 8 left after the first */
    {15, {".......rr......", ".......k.......", ".r.....k.....r.", "rrr..k.k.k..rrr", "rrr..kkkkk..rrr", 
          "kkk..kk.kk..kkk", "kkk..kk.kk..kkk", "kkkk.kkkkk.kkkk", "kkkkkkkkkkkkkkk", "k.kkkkkkkkkkk.k", 
          "k.kkkkk.kkkkk.k", "kkkkkk...kkkkkk", "kkkkkk...kkkkkk", "kkkkkk...kkkkkk", "lllllllllllllll"}},
    /* 113: mountains with the sun, 15x15, 2 sweeps, 18 left after the first */
    {15, {"..........yyy..", ".........yyyyy.", ".........yyyyy.", "....w.....yyy..", "...www.........", 
          "..wwbww...w....", ".bbbbbbb.www...", "bbbbbbbbbbwbb..", "bbbbbbbbbbbbbb.", "bbbbbbbbbbbbbbb", 
          "lbbbbbbbbbbbbbl", "llllbbbbbbbllll", "lllllllllllllll", "lll.sssss..llll", "lllllllllllllll"}},
    /* 114: a crown with jewels, 15x15, 2 sweeps, 20 left after the first */
    {15, {"...............", ".r.....r.....r.", ".y.....y.....y.", ".yy...yyy...yy.", ".yyy.yyyyy.yyy.", 
          ".yyyyyyyyyyyyy.", ".yyyyyyyyyyyyy.", ".yyryyybyyyryy.", ".yyyyyyyyyyyyy.", ".wwwwwwwwwwwww.", 
          "ppppppppppppppp", "ppppppppppppppp", ".ppppppppppppp.", ".y...........y.", ".y...........y."}},
    /* 115: a birthday cake, 15x15, 2 sweeps, 24 left after the first */
    {15, {"...y...y...y...", "...o...s...l...", "...o...s...l...", "...o...s...l...", "...o...s...l...", 
          "..wwwwwwwwwww..", ".wwwwwwwwwwwww.", ".owwwowwwowwwo.", ".ooooooooooooo.", ".rrrrrrrrrrrrr.", 
          ".ooooooooooooo.", ".ooooooooooooo.", ".ooooooooooooo.", "sssssssssssssss", "....sssssss...."}},
    /* 116: a cat, 15x15, 2 sweeps, 26 left after the first */
    {15, {".kk.........kk.", ".kkk.......kkk.", ".kkkk.....kkkk.", ".kkkkkkkkkkkkk.", "kkkkkkkkkkkkkkk", 
          "kkk..kkkkk..kkk", "kkk.ykkkkky.kkk", "kkkkkkkkkkkkkkk", "kkkkkkkrkkkkkkk", ".kkkk.....kkkk.", 
          "..kkkkk.kkkkk..", "...kkkkkkkkk...", ".....kkkkk.....", "...............", "..............."}},
    /* 117: an hourglass, 15x15, 2 sweeps, 26 left after the first */
    {15, {"nnnnnnnnnnnnnnn", ".n.sssssssss.n.", ".n.s.......s.n.", ".n..syyyyys..n.", ".n...syyys...n.", 
          ".n....sys....n.", ".n.....y.....n.", ".n....sys....n.", ".n...s.y.s...n.", ".n..s.yyy.s..n.", 
          ".n.s.yyyyy.s.n.", ".n.syyyyyyys.n.", ".n.syyyyyyys.n.", ".n.sssssssss.n.", "nnnnnnnnnnnnnnn"}},
    /* 118: a tortoise, 15x15, 2 sweeps, 34 left after the first */
    {15, {"...............", "...............", ".....lllll.....", "...lllnlllll...", "..llnllnllnll..", 
          ".lllnllnlllnl..", ".llnlllnllllnyy", "llllnnnnnnnnyyy", "lnnnllllllllnyk", ".lllllllllllyyy", 
          "..yy.yy..yy.yy.", "..yy.yy..yy.yy.", "..yy.yy..yy.yy.", "...............", "..............."}},
    /* 119: a butterfly, 15x15, 2 sweeps, 46 left after the first */
    {15, {"...k.......k...", "....kk...kk....", ".pppp.k.k.pppp.", "ppyypp.k.ppyypp", "pyyyyppkppyyyyp", 
          "pyykyppkppykyyp", "pyyyyppkppyyyyp", ".ppppppkpppppp.", "..pppppkppppp..", "...ppppkpppp...", 
          "..pppppkppppp..", ".ppooppkppoopp.", ".pooop.k.pooop.", "..ppp.....ppp..", "..............."}},
    /* 120: a sailing boat, 15x15, 2 sweeps, 52 left after the first */
    {15, {".......k.......", ".......kw......", "......wkww.....", ".....wwkwww....", "....wwwkwwww...", 
          "...wwwwkwwwww..", "..wwwwwkwwwwww.", ".......k.......", ".......k.......", "nnnnnnnnnnnnnnn", 
          ".nnnnnnnnnnnnn.", "..nnnnnnnnnnn..", "...............", "s.s.s.s.s.s.s.s", "sssssssssssssss"}},
    /* 121: a lighthouse, 15x15, 2 sweeps, 66 left after the first */
    {15, {".......r.......", "......rrr......", ".....rrrrr.....", "yyy..kyyyk..yyy", ".....kyyyk.....", 
          "....kkkkkkk....", ".....rrrrr.....", ".....wwwww.....", ".....rrrrr.....", "....wwwwwww....", 
          "....rrr.rrr....", "....www.www....", "...nnnnnnnnn...", "bbnnnnnnnnnnnbb", "bbbbsbbbbbbsbbb"}},
    /* 122: a rocket, 15x15, 2 sweeps, 130 left after the first */
    {15, {".......r.......", "......rrr......", "......www......", ".....wwwww.....", ".....wwsww.....", 
          ".....wsssw.....", ".....wwsww.....", ".....wwwww.....", ".....wwwww.....", "....rwwwwwr....", 
          "...rrwwwwwrr...", "..rrr.www.rrr..", ".......y.......", "......yyy......", ".......y......."}},
    /* 123: an elephant, 15x15, 3 sweeps, 38 left after the first */
    {15, {"...............", "..ssss.........", ".ssssss.ssss...", ".sksbbbsssssss.", "ssssbbbsssssss.", 
          "ss.sbbbssssssss", "ss.wbbbssssssss", "ss.w.bbsssssss.", "ss..ssssssssss.", "ss..ssssssssss.", 
          ".s..sss...sss..", "....sss...sss..", "....sss...sss..", "....sss...sss..", "..............."}},
    /* 124: a hot-air balloon, 15x15, 3 sweeps, 40 left after the first */
    {15, {".....rrrrr.....", "...yyrrrrryy...", "..ryyrrrrryyr..", ".rryyrrrrryyrr.", ".rryyrrrrryyrr.", 
          "yrryyrrrrryyrry", "yrryyrrrrryyrry", ".rryyrrrrryyrr.", "..ryyrrrrryyr..", "...yyrrrrryy...", 
          "....yrrrrry....", ".....rrrrr.....", ".....k...k.....", ".....nnnnn.....", ".....nnnnn....."}},
    /* 125: an owl, 15x15, 3 sweeps, 45 left after the first */
    {15, {"...n.......n...", "...nn.....nn...", "...nnnnnnnnn...", "..nwwwnnnwwwn..", "..nwkwnnnwkwn..", 
          "..nwwwnynwwwn..", "..nnnnnynnnnn..", "..nnmnmnmnmnn..", "..nmnmnmnmnmn..", "..nnmnmnmnmnn..", 
          "...nnnnnnnnn...", "....nnn.nnn....", "kkkkykkkkkykkkk", "........kk..ll.", ".........k.ll.."}},
    /* 126: a steam train, 15x15, 3 sweeps, 48 left after the first */
    {15, {"..ww.ww........", ".wwwwwww.......", ".kkkk..........", "..kk.....rrrrrr", "..kk..kk..r..r.", 
          "kkkykkkykkr..r.", "kkkykkkykkrrrr.", "kkkykkkykkrrrr.", "kkkykkkykkrrrr.", "rrrrrrrrrrrrrrr", 
          ".kkk..kkk..kkk.", ".kyk..kyk..kyk.", ".kkk..kkk..kkk.", "kkkkkkkkkkkkkkk", "n.n.n.n.n.n.n.n"}},
    /* 127: a treasure chest, 15x15, 3 sweeps, 48 left after the first */
    {15, {"...nnnnnnnnn...", "..nnnnnnnnnnn..", "..nn.......nn..", "..nn...y...nn..", "..nn.yyyyr.nn..", 
          "..nnyyqyyyynn..", "yyyyyyyyyyyyyyy", ".nnynnkkknnynn.", ".nnynnk.knnynn.", ".nnynnkkknnynn.", 
          ".nnynnnnnnnynn.", ".nnynnnnnnnynn.", ".nnynnnnnnnynn.", ".yyyyyyyyyyyyy.", ".nn.........nn."}},
    /* 128: a snail on a leaf, 15x15, 3 sweeps, 52 left after the first */
    {15, {"...............", "..k.k..........", "..k.k...nnnnn..", "..kk...nyyyyyn.", ".ooo..nyynnnyyn", 
          ".oooo.nynyyynyn", "..ooo.nynynynyn", "..ooo.nyynnyyyn", "..oooooyyyyyyn.", "..ooooooonnnno.", 
          "...oooooooooooo", ".llllllllllllll", "llllllllllllll.", "..llllll.......", "..............."}},
    /* 129: an ice-cream sundae, 15x15, 3 sweeps, 56 left after the first */
    {15, {"......rrk......", "......rr.......", ".....ooooo.....", "....ooooooo....", "..wwooooooonn..", 
          ".wwwooooooonnn.", ".wwwwooooonnnn.", ".wwwwwwwnnnnnn.", "sssssssssssssss", ".sssssssssssss.", 
          "..sssssssssss..", "....sssssss....", "......sss......", "......sss......", "....sssssss...."}},
    /* 130: a lion's face, 15x15, 3 sweeps, 58 left after the first */
    {15, {"...............", ".....nnnnn.....", "...nnnnnnnnn...", "..nnnyyyyynnn..", "..nnyyyyyyynn..", 
          ".nnykkyyykkynn.", ".nnyyyyyyyyynn.", ".nnyyykkkyyynn.", ".nnyyyykyyyynn.", ".nnyyykykyyynn.", 
          "..nnyyyyyyynn..", "..nnnyyyyynnn..", "...nnnnnnnnn...", ".....nnnnn.....", "..............."}},
    /* 131: a teapot, 15x15, 3 sweeps, 68 left after the first */
    {15, {".......n.......", "......nnn......", "....bbbbbbb....", "..bbbbbbbbbbb..", "b.bbbbbbbbbbbbb", 
          "bbbbbbbbbbbb..b", ".bbbbbbbbbbb..b", "..bbbbbbbbbbbb.", "..bbbbbbbbbbb..", "...bbbbbbbbb...", 
          "....bbbbbbb....", "...............", "..nnnnnnnnnnn..", "...............", "..............."}},
    /* 132: a snowman, 15x15, 3 sweeps, 84 left after the first */
    {15, {"......kkk......", "......kkk......", ".....kkkkk.....", ".....wwwww.....", "....ww.w.ww....", 
          "....wwwywww....", ".....wwwww.....", "....rrrrrrr....", ".n..wwwwwww..n.", ".nnwwwwkwwwwnn.", 
          "..wwwwwwwwwww..", "..wwwwwkwwwww..", "..wwwwwwwwwww..", "...wwwwkwwww...", "....wwwwwww...."}},
    /* 133: an umbrella in the rain, 15x15, 3 sweeps, 84 left after the first */
    {15, {".s...s...s...s.", ".s...s...s...s.", ".......k.......", ".....rrrrr.....", "...rrwrrrwrr...", 
          "..rrwrrrrrwrr..", ".rrwrrrrrrrwrr.", "rrwrrrrrrrrrwrr", ".......k.......", ".s.....k.....s.", 
          ".s.....k.....s.", ".......k.......", "....k..k.......", "....kkkk.......", "..sssssssssss.."}},
    /* 134: a vintage car, 15x15, 3 sweeps, 84 left after the first */
    {15, {"...............", ".kkkkkkkk......", ".r..r...r......", ".r..r...r......", ".r..r...r......", 
          ".rrrrrrrrrrrrry", ".rrrrrrrrrrrrrw", ".rrrrrrrrrrrrry", "kkkkkrrrrkkkkk.", "kkkkkkkkkkkkkkk", 
          ".kkk.......kkk.", "kk.kk.....kk.kk", "k.y.k.....k.y.k", "kk.kk.....kk.kk", ".kkk.......kkk."}},
    /* 135: a sunflower, 15x15, 3 sweeps, 96 left after the first */
    {15, {"....yyyyyyy....", "...yyyyyyyyy...", "..yyynnnnnyyy..", "..yynnnnnnnyy..", ".yyynnnnnnnyyy.", 
          "..yynnnnnnnyy..", "..yyynnnnnyyy..", "...yyyyyyyyy...", "....yyyyyyy....", ".......l.......", 
          ".ll....l....ll.", ".llll..l..llll.", "..lllllllllll..", ".......l.......", "..nnnnnnnnnnn.."}},
    /* 136: a guitar, 15x15, 3 sweeps, 120 left after the first */
    {15, {"......nnn......", ".....knnnk.....", "......nnn......", ".......n.......", ".......n.......", 
          ".....yynyy.....", "....yyynyyy....", "....yyynyyy....", ".....yynyy.....", "...yyy...yyy...", 
          "..yyyy...yyyy..", "..yyyyyyyyyyy..", "..yyyykkkyyyy..", "...yyyyyyyyy...", ".....yyyyy....."}},
    /* 137: a cactus in a pot, 15x15, 3 sweeps, 127 left after the first */
    {15, {"......ooo......", "......lll......", "......lll.ll...", ".ll...lll.ll...", ".ll...lll.ll...", 
          ".ll...llllll...", ".ll...lll......", ".llllllll......", "......lll......", "......lll......", 
          "..nnnnnnnnnnn..", "..rrrrrrrrrrr..", "...rrrrrrrrr...", "...rrrrrrrrr...", "....rrrrrrr...."}},
    /* 138: a deer, 15x15, 3 sweeps, 140 left after the first */
    {15, {"..n.n.n.n......", "..nnn.nnn......", "...n...n.......", ".nnnnnnnnn.....", "...nknkn.......", 
          "...nnnnn.......", "....nkn........", "....nnn........", "....nnnnnnnnnw.", "....nnnnnnnnnww", 
          "....nnnnnnnnnn.", "....n.n...n.n..", "....n.n...n.n..", "....n.n...n.n..", "....k.k...k.k.."}},
    /* 139: a peacock, 15x15, 4 sweeps, 44 left after the first */
    {15, {"....q.q.q.q....", "..qqyqqyqqyqq..", ".qyqqqqqqqqqyq.", ".qqqyqqyqqyqqq.", "qyqqqqqyqqqqqyq", 
          "qqqyqqqbqqqyqqq", "qqqqqqbbbqqqqqq", ".qyqqqbbbqqqyq.", "..qqqbbbbbqqq..", "...qqbbbbbqq...", 
          ".....bbbbb.....", "......bbb......", "......y.y......", ".....yy.yy.....", "..............."}},
    /* 140: an alarm clock, 15x15, 4 sweeps, 119 left after the first */
    {15, {".kk.........kk.", "kkk..rrrrr..kkk", "kk.rrrrrrrrr.kk", "..rrr..k..rrr..", ".rrr...k...rrr.", 
          ".rr....k....rr.", ".rr....k....rr.", ".rrk...kkk.krr.", ".rr.........rr.", ".rr.........rr.", 
          ".rrr.......rrr.", "..rrr..k..rrr..", "...rrrrrrrrr...", "..k..rrrrr..k..", ".kk.........kk."}},
    /* 141: a windmill, 15x15, 4 sweeps, 120 left after the first */
    {15, {"www.........www", ".www.......www.", "..www.....www..", "...www.r.www...", "....wwrrrww....", 
          ".....wwkww.....", "....wwnnnww....", "...wwnnnnnww...", "..ww.nn.nn.ww..", ".ww..nnnnn..ww.", 
          "ww..nnnnnnn..ww", "....nnnnnnn....", "....nnn.nnn....", "...nnnn.nnnn...", "lllllllllllllll"}},
    /* 142: a giraffe, 15x15, 4 sweeps, 142 left after the first */
    {15, {".n.n...........", ".yyy...........", "yykyy..........", "yyyyyy.........", "y..yyy.........", 
          "...yny.........", "...yyyy........", "....ynyy.......", "....yyyyyyyyy..", "....ynyynyyyyy.", 
          "....yyyynyyynyn", "....yy.....yy..", "....yy.....yy..", "....yy.....yy..", "....nn.....nn.."}},
    /* 143: a squirrel, 15x15, 4 sweeps, 149 left after the first */
    {15, {".........nnnn..", "........nnnnnn.", "..n....nnnnnnnn", ".nnn...nnn..nnn", ".nnnn..nnn...nn", 
          "nknnn..nnn.....", "nnnnn..nnnn....", ".nnnnn.nnnn....", "..nnnnnnnnn....", ".ynnnnnnnnn....", 
          "yyynnnnnnnn....", ".y.nnnnnnnn....", "...nnnnnnn.....", "...nnnnnnn.....", "..nnn.nnnn....."}},
    /* 144: a horse, 15x15, 4 sweeps, 158 left after the first */
    {15, {"...............", "...nk..........", "..nnnk.........", ".nknnkk........", "nnnnnnkk.......", 
          "nn..nnnkk......", ".....nnnnnnnnk.", ".....nnnnnnnnkk", ".....nnnnnnnn.k", ".....nnnnnnnn.k", 
          ".....n.n..n.n..", ".....n.n..n.n..", ".....n.n..n.n..", ".....n.n..n.n..", ".....k.k..k.k.."}},
    /* 145: a dinosaur, 15x15, 4 sweeps, 167 left after the first */
    {15, {".......llllll..", "......llklllll.", "......lllllllll", "......lllwwww..", "......llllll...", 
          ".....lllll.....", "l...llllll.ll..", "ll.lllllllll...", ".llllllllll....", "..lllllllll....", 
          "...lllllll.....", ".....ll.ll.....", ".....l..ll.....", ".....ll.lll....", "....lll........"}},
    /* 146: a dolphin, 15x15, 5 sweeps, 40 left after the first */
    {15, {"...............", "...............", "......bb.......", ".....bbb.......", "...bbbbbbbbb...", 
          "b.bbbbbbbbbbbb.", "bbbbbbbbbbbbkb.", "b.bbbbbbbbbbbbb", "b..bbwwwwwwwbb.", "......bbwww....", 
          ".......bb......", "........b......", "...............", ".s...s...s...s.", "sssssssssssssss"}},
    /* 147: a rooster, 15x15, 6 sweeps, 124 left after the first */
    {15, {"...rr..........", "..rrrr.........", "..wwww.....q...", ".ywkwww...qqq..", "yywwwww..qqlqq.", 
          "..rwwww..qqqlq.", "..rnwwww.qlqqq.", "...nnnnnnnqqlq.", "...nnnnnnnnqqq.", "...nnnnnnnnnq..", 
          "....nnnnnnnn...", ".....nnnnnn....", "......y..y.....", "......y..y.....", ".....yy.yy....."}},
    /* 148: a seahorse, 15x15, 6 sweeps, 152 left after the first */
    {15, {".....yy........", "....yyyy.......", "..yyykyyy......", "yyyy..yyyy.....", "......yyyyy....", 
          ".....yyyyyyoo..", "....yyyyyyyoo..", "....yyyyyyyoo..", ".....yyyyyyo...", "......yyyyy....", 
          ".......yyyy....", ".....yy.yyy....", "....yy..yyy....", "....yy.yyy.....", ".....yyyy......"}},
    /* 149: a hummingbird, 15x15, 6 sweeps, 156 left after the first */
    {15, {"...............", "....qqq........", "...qqqq........", "kkkqkqq.....qq.", "...qrqq...qqqq.", 
          "....rqqqqqqqq..", "....qqqqqqqqq..", ".....qqqqqq....", "......qqqqqq...", ".r.r.....qqqq..", 
          "rrrrr......qq..", ".rrr...........", "..l............", "..l............", ".lll..........."}},
    /* 150: a flamingo, 15x15, 6 sweeps, 157 left after the first */
    {15, {"....ooo........", "...oooko.......", "...o..ok.......", "...o...k.......", "...oo..........", 
          "....oo.........", ".....oo........", ".....oooooo....", "....oooooooooo.", ".....oooooooo..", 
          ".......oooo....", "........o......", "........o.o....", "........oo.....", "........o......"}},
};
