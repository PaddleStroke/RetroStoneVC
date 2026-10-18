/* Hex Stacks' levels: made by tools/hexstack_gen.c (our own boards and sequences, each won by the rules'
 * player in `par` stacks, and by most players choosing worse; do not edit). Each: the goal, its colour, the colours
 * dealt, the par, the target, the board, the stacks already there, the tray's sequence (hexstack.h). */
const hx_level_def hx_levels[HX_LEVELS] = {
    {HX_TILES, 0, 3, 1, 10, /* 1: 1.00 */
     "..ooo..|.oSSo..|.oSoSo.|.oSSo..|..ooo..", "BBBBBB",
     "BEJKbCgKbEIfCEcKbICGbKaAgAgGbAkCkGaIgKaBeKeFiKKBGAGcEKbKKJCGkCKgICfGbKEkBkCBIgAKeKBJKIGjCkJg"},
    {HX_TILES, 1, 3, 2, 10, /* 2: 1.00 */
     "..ooo..|.oooo..|..SoS..|.SooS..", "EbEbJfJf",
     "CCgFCGIJFFjCfGBBJBFJfJAGAJgBjKGFIGkBkKgEcGkJAgFBjKcBfEBJKeIbKeKGCgJcFcEBjEEjBfKfGcFiFFKg"},
    {HX_TILES, 1, 3, 4, 10, /* 3: 1.00 */
     "..SoS..|.oSSo..|..SoS..", "FkFkGGBfBf",
     "GbKKcAkAgEFkFkFbJbKgFbJeCJbFcCFbJaKCjKaAkIcBICfGKCKCFiFbCkJKgAiAkABJaJgCGCGIEJbGKJgKJcGcAJKcGc"},
    {HX_TILES, 1, 3, 5, 10, /* 4: 1.00 */
     "..oSo..|.oooo..|..SSS..", "FiKEK",
     "GCCAiJIgEkBAFjFEcEJAFKcAgIfGkIgAIfJKAkFaBkCfCeFKgAkCBgFKGGFFCgGjJfJIfEFCkFbKfGkGCkIfFFjBBeI"},
    {HX_TILES, 1, 3, 2, 10, /* 5: 1.00 */
     "..ooo..|.SooS..|..ooo..|.oSSo..|..ooo..|.SooS..|..ooo..", "GkGkKfKfKfKf",
     "JfGGkBBIgCKeCgFGaEIeCGiGEKIKBgBCjCfFFkGbKGcKEbBkCfFFcGFiJeCFbCjFcJgGJBFEFjCkECCGbKGKKBB"},
    {HX_TILES, 0, 3, 4, 20, /* 6: 1.00 */
     "..SoS..|.oooo..|..ooo..|.oooo..", "BgBg",
     "EbEcGAAJfKaJCiGcBGjIeBCJGcFCjIeEBjEjGaEBJgKgFIGEIaKBAJeEAGbIgKKaGkBkCiFaBkAgCkCgJKeFFkBgKKCBf"},
    {HX_TILES, 1, 3, 6, 20, /* 7: 1.00 */
     "..SoS..|.SooS..|..ooo..", "GjGjEE",
     "AiEaECkFjFGIcKaFbKcCkKaEbGAGcFaGCGIGjBjFCkBIgJKeGCfBkKFcKbKgBgEAfCkBgEiJFJJFJcKbGEEKcIfKECJJf"},
    {HX_TILES, 2, 3, 4, 20, /* 8: 1.00 */
     "..ooo..|.oooo..|.oSSSo.|.oooo..|..SoS..", "AEkAJfJf",
     "CjBgFkJGiFjKGjKKBeABfAeBBFcCkAkEIJEkEkCCAGjGbCgKAGbIJaIfCiGIfBkAgBfJJaKcGjGFbGBkAfEjCkFbGGkICjJAk"},
    {HX_TILES, 2, 3, 7, 10, /* 9: 1.00 */
     "..ooo..|.oooo..|..SoS..|.oooo..", "CkCk",
     "CiCAJfFFCBkIfCGjGkFCCBjJIgIcGiIgJgBkGFaJeGcAkAEaBGiIACGCiGbCGIGAkKfFjCGcBKGaFbBkJcBEjJFaKCEi"},
    {HX_TILES, 0, 3, 6, 10, /* 10: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "GBgEkBJcCgEkFcGkKbKgEJJJcKeCBgGBGkAKbGcBkAkIBKFEaEkCGCkJfEcKGiEGAJFEGCkJcGbGjBkBiFGkCeKcKgBiGFi"},
    {HX_TILES, 1, 3, 7, 10, /* 11: 1.00 */
     "..SSS..|.oooo..|.ooooo.|.SooS..|..ooo..", "EGEBB",
     "BjFKFbIaIGkGKfEiIeIcKcCiGbCfFcAkCBIfCiKFcBkJcABeCiJgIcGKcGIgJFKEFkCFkBkIbBjEAEFFjGJJgKcFiBfBAEAk"},
    {HX_TILES, 0, 3, 6, 20, /* 12: 1.00 */
     "..ooo..|.SooS..|.ooooo.|.SooS..|..ooo..", "JeJeFF",
     "CAGjIgJFkJaKbAfBJJgGbKFjIcKBgJbEKcKfCgCAEFbEbJaFAeAkGJfKBkCGcKFKfEAeBkJcJEiIgCiJbAfKcAEjGaGjEIJgJ"},
    {HX_TILES, 1, 3, 9, 20, /* 13: 1.00 */
     "..ooo..|.oooo..|..oSo..", "Ia",
     "EFkAkFbKbFjECjKcCfJgBkKGkFaBgCkJCgEjJKgKCGiIKgFcCCiKaCjCfAjFbJKIFbCFcCkICKcGiEiEjIbEbCfCfCIaKgBGkKbCkG"},
    {HX_TILES, 2, 3, 7, 10, /* 14: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oooo..|..ooo..|.oooo..|..ooo..", "",
     "KgFaICgCkIJcJeJGbKgBBJcKCkAeEJfGCkIKKfEIgGbGkBkCkCJfFbGFbFJcAeJGKfGbKAiAgJKCjFaFkAeFiAiGcJfFkGIAAg"},
    {HX_TILES, 1, 3, 8, 20, /* 15: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "JCiFGcJgBJaJgKBjEkKaACCkJaAeJJGcGiCjCgJeKCkEkGbIgGcFCIbKeCCJKeKfCKaGJaBfIGaFiEbBeKCJJbKBKgAkIfIGj"},
    {HX_TILES, 2, 3, 7, 40, /* 16: 1.00 */
     "..SoS..|.oooo..|..ooo..|.SSSS..", "BfBfJeKfKfJe",
     "GICgCgAjFbKJGkEkIgKfKAGjEkGbAkAJaKFbFkJaJbJcAFcFkGGcBKcBeBJbECjKAiJKIFIbAJgCCgCCgIKgBKKcFiFiGiFc"},
    {HX_TILES, 2, 3, 11, 20, /* 17: 1.00 */
     "..ooo..|.oooo..|..ooo..", "",
     "GaFABgBAIFkFiKCfKJeBkKbIIKJIfIbAGjGaCKgABJFbFcGkIeIgJcECKIcKJFjBiCBjGbGbACkAjKeKBGcGAkFBjKbG"},
    {HX_TILES, 1, 3, 10, 30, /* 18: 1.00 */
     "..ooo..|.oooo..|..ooo..", "",
     "IAiAkEJcKgJeGbGjCIfGAFaIABiKBeJeBjCkBGiFaBjAiCfFJbJbAFBGEJBCCKcKGcIKAgFiGcKaEbKCkCgGbBEiKCJC"},
    {HX_TILES, 1, 3, 7, 40, /* 19: 1.00 */
     "..ooo..|.oooo..|.oSoSo.|.SSSS..|..ooo..", "FjFjIgKgKgIg",
     "GkBkBAjJKcGbFcAGCjIGFaGcBeGjFIaJbCjGJaAFcFkKEkEcCiFGbCkCKJKcIGKFJKgJfCiCkCgFcAkCjBfCJGEBAGaGjJg"},
    {HX_TILES, 0, 3, 9, 20, /* 20: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oooo..|..oSo..|.oooo..|..ooo..", "Fk",
     "AgAKFFcBKaJbBgCkGJaFCeCJCfJcGGJgKEkJfCgCjIaJCgKeIcIfBJKCBgFEaAfBFEFbBjGGaGJIFcAeCfFjBiEKGKA"},
    {HX_TILES, 0, 3, 10, 30, /* 21: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "BCfKcKcEJGFcJKaBGECCiEABgIEiBBGkKaAjIBACkGJfFbBEbGaFjFAEFICiKcFjJfFKGJAGCkFaFkIIeGjIbGkBg"},
    {HX_TILES, 2, 3, 10, 20, /* 22: 1.00 */
     "..SoS..|.oooo..|..SoS..|.SooS..|..ooo..|.oooo..|..ooo..", "BeBeCCAkAk",
     "JCgEjGjABBFCKbECCkKeJfIcJbGKFCFjIJCkCCfGJEcBiEBgBCIgBGkFbBKFIfEcBfAGBFcCkKCeKBIgKcCjCgCKg"},
    {HX_TILES, 2, 3, 10, 30, /* 23: 1.00 */
     "..ooo..|.oooo..|..SoS..|.oSSo..|..ooo..|.oooo..|..ooo..", "JgJgGG",
     "FGKAjGBfGCgFbKKcGcCjFaEaBICKBfAkAkGbKaKfKeJFcJgKAEbBgEbIcFIFcIcBEGFcBEBAjIfKbFIAeGFkFcAKcCkKB"},
    {HX_TILES, 2, 3, 11, 40, /* 24: 1.00 */
     "..ooo..|.oSSo..|..ooo..|.SooS..|..ooo..|.oSSo..|..ooo..", "JJEcEcFF",
     "IcEjCEGaJAIgGaGcBgFkJgFkKbEkBAJbKeEJKgBFCgGCBfJcAgKgJcFjGEJJbCCKKAgBCgEjGjBIIfEKCKfKeGcAgAFjC"},
    {HX_TILES, 2, 3, 10, 40, /* 25: 1.00 */
     "..oSo..|.oooo..|..ooo..|.oooo..|..ooo..|.oSSo..|..SoS..", "GjFbFbGG",
     "FiEFcKcEkJGjFcBBjGAfJfBkGkCfKFJEbKEkCKbAkKbKGaBjCgGjCkAjGkJGCeFAeGbFAgACGaEkAKEFJaJgKgFjJcICCCF"},
    {HX_TILES, 2, 4, 7, 20, /* 26: 1.00 */
     ".ooSoo.|ooSSoo.|.ooooo.|oooooo.|.SoooS.", "NCCJJ",
     "KBfIKcoBOkeOioBeCkAFKOibEoKJfONejONCBjJoaFaJEoMNjGbBfEKJmJcGMjCofKBFJbAgNJoKfGnINkKJFjImFcAeoGEMBEkCm"},
    {HX_TILES, 2, 4, 8, 20, /* 27: 1.00 */
     "..ooo..|.oooo..|..oSo..|.oSSo..|..ooo..|.SooS..|..ooo..", "NNfjNfjGG",
     "ENbnJbFGmcOkMIEFnEiCGKfJanACfbEnAokNjJoJgGkeBkaJfnAfNKAeCOgoAkNaKCkNGBKbMgJfGNgICjBfcCkoEBiOjfGKmgJGGbnOIbnBfbAJ"},
    {HX_TILES, 2, 4, 10, 20, /* 28: 1.00 */
     "..oSo..|.oSSo..|.ooooo.|.oooo..|..ooo..", "NkMM",
     "KnfOFmaCBEmAKaOkFjmGakICjIekGAjEAioOIbeMCNAgKoMJFknIKCgbBnFkNEKaKgcEEaBocNNJFmCnMiEnCjEKfKgJaAenObmJJaKmGGJo"},
    {HX_COLOUR, 2, 4, 10, 10, /* 29: 1.00 */
     "..ooo..|.oooo..|.ooSoo.|.oooo..|.ooooo.|.oSSo..|..ooo..", "FFnFn",
     "OOjCoJmbCiJMeEkoMEneIcJOanEnkJNkcAgGGFbNfjKafOKbAmBnEObkNGJgEmEcKcENKJnGENeIgAgaBknJbkBKfGbMKmCiFcKaFaKKGcNbjGaImk"},
    {HX_COLOUR, 0, 4, 14, 10, /* 30: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oSSo..", "GG",
     "KgGjOMecBjeNiJBNkBMaJfAjEafKaoIgOckFcGjKneGcBgaOgMBFbAJmBgkCABmCfoEEkNFbgNeFGjcJNbgGbIaFjJgEGEBgiOenBjNaAnKgKcOiMeOjaKo"},
    {HX_TILES, 3, 4, 10, 20, /* 31: 1.00 */
     ".ooooo.|oooooo.|.ooooo.|SooooS.|.ooSoo.", "EEBf",
     "CjnJNACiAkaImAEKcgGcIgkMGjMgjNcGoFjAkGokAoOaGEEcAJnFMjcMAjEkMKfCFCiIMbNgOOfNbMKMgbEcBioGkgBmCncGjEMgmKBkOjAnKcnCoj"},
    {HX_TILES, 3, 4, 11, 20, /* 32: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oooo..|..ooo..|.oooo..|..SoS..", "JgcJgc",
     "NfiOboNEnEjFOCneNAfaOiKcIMJemIBigEGkGcJcoBOAjIgKciGBjNAOiAoeKbNbAGBCmGaObCnkJmGkbMBGBeNgGAMkOeEcNGoAkeJcGkBeCnj"},
    {HX_COLOUR, 3, 4, 14, 10, /* 33: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.SooS..|..ooo..", "GG",
     "AfAEBnMifGmjJMFNaNeEGioGBocNBNAoGogJcjEEokGjbCNcFicBfJmKEnFJcKGMicGmgAEGMOcGnGkoNBkBojEJMgJCjCKnBNIEbnCnEc"},
    {HX_TILES, 3, 4, 12, 30, /* 34: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "IaIgBioKcEobKJcoEKIaKfoONaMGFjOEOiBMkGkEcoMeAoNMGkMfjNkfBnbCfEBEJmMGKofEoaNNjoAoGiGKFobOgNaKfKJJCkoFKGkNiBnFcg"},
    {HX_COLOUR, 2, 4, 16, 10, /* 35: 1.00 */
     "..ooo..|.oooo..|..SoS..|.oooo..", "MiMi",
     "JmCeJnBmIcIJGaBIMgNBjBfjCmOjMckNkGoBjEocEGBoFmfOfoIAgGEkINcCkcOEboIcCGcJcFceBOOBNKOEOKoKFjbAoeOGMgKgOGO"},
    {HX_TILES, 0, 4, 13, 20, /* 36: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oooo..|..ooo..|.oooo..|..ooo..", "",
     "IMkgIoIbKcKMOimNNMgKgGKfcBAfkCfJeJFoAAoObNAiFjCkmOjOjfOgBeEKIOKfGBeoCKbfFkgKEmJgNaMFEcgFjJmJceNCmgInKGjOkEnkKN"},
    {HX_TILES, 3, 4, 12, 30, /* 37: 1.00 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..", "JnJn",
     "KBnGMaOgkFcEBnJFJoGiKfNiFoCkbGoGjgGAMjKgFOfoACJJcGkaCjAkbBGoNioEJnCIenBnNENcCfFJIIboOcOBiCfbNfNgaGkoKMgBIOB"},
    {HX_TILES, 1, 4, 12, 40, /* 38: 1.00 */
     "..ooo..|.SooS..|..oSo..|.oooo..|..ooo..|.oooo..|..ooo..", "IIIf",
     "CKOMcFoBKFmFnFmFkbJoBfABeNiMGjBgnKBjJcgCBfACOEGjIgFBOeNInGagKKaOCofKFceCnOJgMBfJCoMNjOaIciGcmOGimJocJagOcCj"},
    {HX_TILES, 2, 4, 14, 40, /* 39: 1.00 */
     "..ooo..|.oooo..|.ooSoo.|.oooo..|..oSo..", "AK",
     "GMcFkCniJGOFCgbGoNjFjJgMNaMBIcIoNFiFnBFkFcAgGNOMgoOiKcjNbEjcKKFocGoAkBNCmCMiGjBeoIfOgnObFbGIIFcEBEkNfnGI"},
    {HX_TILES, 2, 4, 13, 50, /* 40: 1.00 */
     "..ooo..|.SooS..|.ooSoo.|.oooo..|..ooo..", "OOGkc",
     "KcNbEkmMGbNjKekJOcoNBiAfNBACofBIJbiInKJfiFJNjeGJIgnNjMOFFnFbkAgnJCkKNEbfIKciCFbNbkOOCnBOJfcKnONbnNkBeKKgEMi"},
    {HX_TILES, 2, 4, 13, 40, /* 41: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.SooS..|..ooo..", "CC",
     "GbAfCfCiJakObgFEOfjBgMFGaBKbKcCKEjMFcJoiFokMCCCkbGkfNjEoBJokOaOaGbCgCJbkJfOENGNiaOkMAAnAfGnCACAgcEcAkNKFiEm"},
    {HX_TILES, 2, 4, 16, 50, /* 42: 1.00 */
     "..ooo..|.oooo..|..ooo..|.SooS..", "IcIc",
     "BeKNfEnKJgCnfImFbFngIecJJcOjoKnFaOcKnNBgcKCImBoBGFGciOgaKKnFNjCjeFmaNkcBfbGCgOGNJCgKoGjAnCkCCkoAigGbEkNEoJgMibEKnNj"},
    {HX_TILES, 1, 4, 17, 50, /* 43: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oSSo..", "AfAf",
     "JCgcGEibNkJgGNjcMEbGjnMFkBBkCjKocGJfbCCACoiMFoBmFcgNibJmkGnkNebCnjFagJACNAifCkJfOCiAiJmJcKGoNgNNbCKFoGkKgmKnNaCFnMc"},
    {HX_COLOUR, 1, 4, 16, 10, /* 44: 0.88 */
     "..ooo..|.SooS..|.ooooo.|.oooo..|..ooo..", "NN",
     "BBjOObGOIgIOceIcInJcIcKFGkBemAjMNeCoNbEniBJKcJgJbBfmGoIBJGFmfOOaMaINEbgBJgkMeJGkCfOEmBBojNNFjCOiOGbABec"},
    {HX_TILES, 3, 4, 13, 60, /* 45: 1.00 */
     ".ooooo.|SooooS.|.SSoSS.|oooooo.|.ooooo.", "JJFcCkCkFc",
     "GAkKCkGcJIJONgFnkEcmJnGIOAMNcGkOkBCeBBgOcIcjNkgOOgCiIaBFnGkaJMejJeIEbBBgiJaKMejIKIKfAgaIgaCoBKaeFkKMkNgGmgCo"},
    {HX_TILES, 0, 4, 16, 40, /* 46: 1.00 */
     ".oSoSo.|ooSSoo.|.oSoSo.|oooooo.|.ooooo.", "CCEkmEkmNjNj",
     "NcgMeNfAMcMfCkoFbMBANgKgKCFoJKmNBoOgFkBjbFkBoFnBkbBBnaKbKcCFkfMfcJncOakFjIoGNJojKCnOjKoBKgBEOjfONkFjFoFAmgGOJgjO"},
    {HX_TILES, 2, 4, 16, 50, /* 47: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oooo..|..ooo..|.oooo..|..oSo..", "J",
     "OaImkGJMckKcEFKbiBkAoMgJcGcAjgNCBfNJnkFBCokGbNMGjBFjNBFnAMjgOeKckBIBNaGbBEoEnECoJmAJeGjFkmKaGNfJCGciCGOj"},
    {HX_TILES, 3, 4, 16, 40, /* 48: 0.88 */
     "..ooo..|.oooo..|..ooo..|.SooS..", "CfCf",
     "EFaNaNOkaNKfNboCGIoJfiMEkEcOjIGcKCeMOejGIaMfjEkCANcfAKGICoKFanMCNGJGaeMbKGbGokAnObBoJMGjMebFjnOgJmCkBmCjFmj"},
    {HX_TILES, 3, 4, 16, 60, /* 49: 1.00 */
     "..ooo..|.oooo..|.SoooS.|.oooo..|.oSoSo.|.oooo..|..ooo..", "GbGbOO",
     "IcnCeFkoKaNiGboBmOKgKEbBoOkMOagEBMCMOKFOieEjOibGbFNgEoaMfOigCnfKoFcKBfIcMMGnMCKoIANBKfAkJakEoFFifOiIboCgNOIak"},
    {HX_TILES, 3, 4, 16, 60, /* 50: 1.00 */
     ".ooSoo.|oooooo.|.ooooo.|ooSSoo.|.oSoSo.", "BGbGbAjcAjc",
     "NCOkGogJoeIoAKNkOkJaMIMCOjININgKmiKgcAgINgGnFKmcEaNBOMAMjJbGOjoGoMOfKBKBnJmKIOAeOaKoOkOOgKgInEcKfcJo"},
    {HX_TILES, 0, 4, 11, 30, /* 51: 1.00 */
     ".ooooo.|.#oo#..|.ooooo.|.o##o..|.ooooo.|.oooo..|.o#o#o.", "",
     "ImFCBeOfCEOcgCnNjECiKngFCkMOiKgBnKfNGbONjgMeBBkKaGjnKgoBFBfKOEnAnJocFcNIOgKoKnOCoMgoBNejEcFkKaKfCjEbkKFkBGkB"},
    {HX_TILES, 3, 4, 14, 40, /* 52: 1.00 */
     "..ooo..|.oooo..|.#SoS#.|.oooo..|..ooo..", "CmCm",
     "CiaAnNNaFjJgoGKfmCocMbFIBJfcBGGkBCobMkoKnkAjNBjCgoKbgOfEoFMfObfGJoiIfOkCoKnJcBiBogJbkFanIaKaABnGiGGkOgkCkGkCoGKboMjOfEcGjoOkb"},
    {HX_TILES, 2, 4, 13, 40, /* 53: 1.00 */
     ".ooooo.|oooooo.|.o#o#o.|#o##o#.|.ooooo.", "",
     "JbGcCoBgNfoOfkFoMgBCfjBoNgiAkoKmfEnONFKaKoIoCnGAnNOABnGnIgOeEoNKobEkeJaFcmOcnJKneAkeOkKnOjGoMfKEIENKbGnkACkcKaEcFcEojMgk"},
    {HX_TILES, 2, 4, 13, 40, /* 54: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.SoSoS.|.o##o..|..ooo..", "NeoCNeo",
     "GiAfEcmKcoNcnOjcMceMaNaNgAkNkgNbgAicMkmNGkFMMcJmImgJFncBoIoGJmEbIJoBgiMENOAfcKoFmgNBOkeMBAOinGcFiJKBfNGFkfCAAkfAjoKF"},
    {HX_TILES, 0, 4, 11, 30, /* 55: 0.88 */
     "..ooo..|.oooo..|..###..|.S##S..|..ooo..|.oooo..|..SoS..", "CeCeAA",
     "GcJgnBncCjfNNjoNbImjOjOboMCkaCemNfmEbJogKeCAGnbMbFJEcnEGckNKgJFBgJMgaOJgMBjBkJaoGABocCEGKfBekBFIgjOcOjKgMcMiCKgkFcGcOg"},
    {HX_TILES, 3, 4, 13, 50, /* 56: 1.00 */
     "..#o#..|.oooo..|.ooooo.|.oooo..|.#ooo#.|.oooo..|..ooo..", "",
     "CgEIejCnOckAKnNcfMcGcMFkJcoNjgNbFoOOboNkeMBnMckOfbNAkBgNiKgIoMJoGmOanFiOBjGFGCkKgGJJmKBCjbBkGnGanKECAeObmBgCgNjOfMe"},
    {HX_TILES, 1, 4, 10, 40, /* 57: 0.88 */
     ".o#S#o.|oo##oo.|.oSoSo.|##oo##.|.ooooo.", "MgoGbGb",
     "IbNaEjnEcgCoKnBfkEjBoIfEmKocAgcMgNkKcEaNfbNeCCgEcJFGcmCfkGkAoeEjgKbAjMKckGoJekOkcOCiInaMaMNiCfNBicImAjEbjObIAKgnAInFCimMCgEaJa"},
    {HX_TILES, 0, 4, 17, 30, /* 58: 1.00 */
     "..ooo..|.oooo..|.o#o#o.|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "CekAGbOfJmMamCfcFjmFiGkKnNOfJcAAniBmCjGoAeFBnMiCifFkoCCNkJfIGcgMBnBgaKofNfaMNkbAgEkfIfOgoONjKECmcBjBkOgCjcJGiaIoEKoGnkKoKgiJn"},
    {HX_COLOUR, 0, 4, 18, 20, /* 59: 1.00 */
     ".oSoSo.|.#oo#..|.o#o#o.|.oooo..|.ooooo.|.oooo..|.oo#oo.", "OgoOgo",
     "JEaeBIfBoGjEkCIgKagGafGcCimCgFnCIBInKaGcgBnkGiCjnJfbMcoOcJgCjEckAjgAgnNiaAFbOkKbFmNMbfBOgcMgKaiGjGcGGkBfJOCfmNjmOgOKnaGaGjJmcM"},
    {HX_TILES, 1, 4, 16, 40, /* 60: 1.00 */
     ".#ooo#.|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.o###o.", "",
     "KnJcKbJBGJajObnKGOCfcMIfCmbKckEcfNbIfAMjJGAgbNgFafJobBniOcJciEmJnEjbCEkCAoJcJbeCkeCoIanCicMCnaFAjINaJnkIEckEmcKgJcOGaOcObJ"},
    {HX_COLOUR, 2, 4, 22, 20, /* 61: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.S##S..|..SoS..", "JJJgJg",
     "MinKIgmOajJfcCNfMcEEcFNCFjcFBnkJnMeJoMkeKfKcgGnOkbFMkgKOCJbNkoAEOObFaKcFoICjoJNGcBmGiOIenKoCAeBoiKbnFjEGIojKGOiGm"},
    {HX_TILES, 3, 4, 19, 40, /* 62: 1.00 */
     "..#o#..|.o##o..|..o#o..|.#oo#..|..ooo..|.oooo..|..ooo..", "",
     "KaGCFjJcKcKgNfOCkICFCfMecGmJNKbObgNkgAoEkBgKCJCgbGJgEaeMaEnkFbeBfOCmFKgbAeOjcFiJBmiJojEKoiNeiEboMgJgbCoJJbJeMkOGiEjoCk"},
    {HX_TILES, 0, 4, 16, 50, /* 63: 1.00 */
     ".oSoSo.|.#oo#..|.ooooo.|.oooo..|.o#o#o.|.oooo..|.oo#oo.", "EmEm",
     "KoBgEoFkcIEofFoOgaKKecBogOkgCOemACFjgGKoiOKciBGKmOceEbiNcMakCgKIgImNbkCgInCkaFCimAIgoNBGoaFkBnOecIGkEbgFkfCgcGkFobMkJBiKAfKcGa"},
    {HX_TILES, 0, 4, 16, 60, /* 64: 1.00 */
     "..o#o..|.#SS#..|.#ooo#.|.oSSo..|.ooooo.|.oooo..|..ooo..", "BfBfKcKc",
     "KfiAIMGmFJoJCkmEcgOiFojNkEcnAoJbBkoGBFMbBoJfIeEaeGnjCgGmNjBJmJnFoNibKMOFbFFObjCofANBimJfcCkoAGnCEBeJgCKJnfGmgABkOkm"},
    {HX_TILES, 3, 4, 13, 50, /* 65: 0.88 */
     ".o#o#o.|#SSSS#.|.ooooo.|oo##oo.|.#ooo#.", "OgIoeIoeOg",
     "JgoCjgOcJNeIofJnOkFcoMeFbNgjAfcAEmfOJniKmfGbiNiAgGKFicOFoFEofKcKcgJaOaCFcOaeIcMfGokMiCkNJFbAkgCfKocENgGncGoFCiaJnNMfbEiKAjCfJ"},
    {HX_TILES, 2, 4, 16, 70, /* 66: 1.00 */
     "..o#o..|.oooo..|.oSoSo.|.oSSo..|.oo#oo.|.#oo#..|..ooo..", "BBKoeKoe",
     "OeMbBfNkOfAnCeMkBnfOFnNEkJgKgKcABgMjNecFGaiIAnkJfnOjbBnCFFOaOFKFcCGnCMjnKnEAoCioCinJbkMaEcoJniFmJaGjcJOMjoGNgnKCoaBgoOam"},
    {HX_COLOUR, 3, 4, 17, 20, /* 67: 0.88 */
     ".SoooS.|.oooo..|.ooooo.|.#oo#..|.o#o#o.|.oooo..|.o#o#o.", "EaeEae",
     "BjgCnIciJgnInjFcOfbAnNboINeAgGcKcGmaIokNjBeFanMKnCJmEcOBfAobBmiGcCnCokCMbNIfkOgiMfoKgOaGcNOiBFogKgnKgKgcJfBINifKGcmJgbGoKCnfBfGkgEco"},
    {HX_TILES, 1, 4, 20, 50, /* 68: 1.00 */
     ".ooooo.|##oo##.|.#ooo#.|oooooo.|.ooooo.", "",
     "FEkMFiKfOcgFncFaNbIfjENFAfCoCmGigEmfBoCoAfGjKOcgBfJEMkgAkBeCiKogJCCfOFkOFNjCCgGjnFEFInGJcONeEnkBkAnGkaFcAmJNkJ"},
    {HX_COLOUR, 3, 4, 22, 30, /* 69: 1.00 */
     ".oSoSo.|.oooo..|.ooooo.|.#oo#..|.o#o#o.|.oooo..|.##o##.", "CjcCjc",
     "GjfGcNjMgbObgOCfCekKNjKoCgiFbnOcnIfoAkOjoOibJoCMafGoBjfAiKeJojFiJBoAjEajCgnOAiAMcMiACKfkKEaoJMebIebBjnCeInkNaKcFcoCjoKcCjFkfOiEinCOfN"},
    {HX_TILES, 3, 4, 19, 70, /* 70: 1.00 */
     ".ooooo.|oSooSo.|.ooooo.|oo##oo.|.ooooo.", "BekBek",
     "MciFknImgCnMkcOagJEbeOgNKMjnKmfJafBjCINkMeMaAkoMjEObJoeGcnBAgkImMkKaKnjIfaCfJcmKBNeaJcFBnFjKgcBJcGieOFNfNBKagMjbKfcNgIofGFjaKbnC"},
    {HX_TILES, 1, 4, 22, 50, /* 71: 1.00 */
     ".ooooo.|oooooo.|.ooooo.|SooooS.|.o#o#o.", "JJ",
     "KbAmAeNMBoFAnBBKnGcGkMkEkaOakJofIfcOfnCmgAIbkFaOakCAEakBnFbfFjnFCoCfBfFjJmCFaOkcIgFFFbnEIKoGbGJbJgkCoNbKboGOfkNbEjAMfOic"},
    {HX_TILES, 0, 4, 21, 70, /* 72: 1.00 */
     ".ooooo.|.oooo..|.#ooo#.|.#oo#..|.#ooo#.|.#oo#..|.ooooo.", "",
     "IbmBnFFKbmBkoOeMCIaGckJnFCmBmcKmfGGbkEnjIfkOjGckBkIgOafNcoAkJogAogJKafFBkFbGGjJaMaEBFbjGocCoFiCmFCngOcINJBeGMfFJmcCCkCF"},
    {HX_TILES, 0, 4, 19, 50, /* 73: 0.75 */
     "..#o#..|.oooo..|.ooooo.|.#oo#..|..###..", "",
     "FEGAjBgoFjOjJaeCmGjBgoGaeIcEIJIcfKgnFbiMjBmcCNfEcNgjGcoObBkaNjCjNJcOciNeFoGnbCkaEafEkOgCeInFofCeoNfcEoeAkBGbGocJmFboCOceKnJJGmfNGc"},
    {HX_TILES, 1, 4, 22, 70, /* 74: 0.88 */
     "..ooo..|.oooo..|..#o#..|.oooo..|..ooo..|.oooo..|..ooo..", "",
     "NjNcAJbgFOfmCKcMcfAgFkIcgKnAfJJeoGmeFnJeCfoCJCmgAmGiOBjIcgNGocIcMfMMJaFnbBJncOKeFkCjNGoNgKocOIKFGFnkOgOaoCfMgoNiGjmAK"},
    {HX_TILES, 0, 4, 19, 70, /* 75: 0.88 */
     ".ooooo.|.oooo..|.ooooo.|.####..|.oSoSo.|.oooo..|.o#o#o.", "NcfNcf",
     "KcJgMkBimOGjOEmjGkoKJCBinJMKGAkJgNkfBfcFOfAoiKKeaBeAgkEMgMjnNGJFoiMGOceGciGbmKEoJgoCiOfcMBgNfbKfbFMbCoMkBeCFjJObgAeGb"},
    {HX_COLOUR, 3, 4, 19, 20, /* 76: 1.00 */
     ".ooooo.|oooooo.|.1ooo1.|SoSSoS.|.ooooo.", "OcoKbKbOco",
     "GFcnCfIekCBnjNfKecKFaMaBkoAjcJmCNOkCfbMgoJmNAGngOgBmCobACmeCgmJfcFiEckMNkgNkJeIJoCfnBfCieCEjIGAgCCiGaoCfkEjgBjJnNjCfFmcOcnBCfkE"},
    {HX_TILES, 2, 4, 17, 50, /* 77: 1.00 */
     "..ooo..|.oooo..|..ooo..|.o##o..|..3o3..|.oooo..|..ooo..", "",
     "CjeEbCCgFnOeBMgFbnKGNkoBNkgMGboFoONOBoIgcCfJbKmiKnbBInkGioJcgKIMfCIgFafNebOgbBkbCoNfJejAmOjnOKcfIMAAoeFmfKgKMekBjCkAfNgmMO"},
    {HX_TILES, 0, 4, 18, 50, /* 78: 1.00 */
     ".2ooo2.|oooooo.|.2ooo2.|oooooo.|.ooooo.", "",
     "AnJEmAokKIOciOKgaCeJmcAneNBeCjgKfoIcOiKeoECfkICogGCmKcJoInBmaCgiMcfAnFkIgcBmGobKbGjKFnBocEaoJBJbJnEbEOcBknAfCOeBjBfjOiCibNbGckI"},
    {HX_TILES, 2, 4, 19, 50, /* 79: 1.00 */
     ".ooooo.|oooooo.|.5oSo5.|oooooo.|.1ooo1.", "Ob",
     "BkCfOiCiOJfnBFjcGoKmMbfIfiGmgEJoGFkGicMgOOMbImfJNkaKgFkeCOBNBjOfOObAimNciJgjFAONjgNkFIgbFokNjOckFnFkKbgAJCkFbkMKoCkKoFnb"},
    {HX_TILES, 2, 4, 18, 50, /* 80: 1.00 */
     ".1ooo1.|.oooo..|.ooooo.|.#oo#..|.ooooo.|.oooo..|.ooooo.", "",
     "JokEFmkCngGmgNIcnNeBeOfjNIaiFkGaOCKKnOgJaoNjEmNbEaGJaCmAJbFcgNfBgnKOGamMeGmAkNcjJcOaNcFMbAibNbeOiBgFjBjCKoaIABknIcFcBjKGkb"},
    {HX_TILES, 1, 4, 18, 70, /* 81: 1.00 */
     ".2ooo2.|oooooo.|.So2oS.|oooooo.|.o#o#o.", "OicOic",
     "KmgGmbJfKEoOcfFjFoBIcOcjEkbFOjMCjOeOBmNkGinCgiBnFbCgcIenBOknFkBkgAjMkGoBkOeBnjAmaACnGoKobGoaAinJKmCkoMOcFkIJGjfOgbNeIbAGaNGbMan"},
    {HX_TILES, 1, 5, 17, 50, /* 82: 1.00 */
     ".ooooo.|.5oo5..|.SoooS.|.oooo..|.o3o3o.|.oooo..|.ooooo.", "GsGs",
     "OagBrcCsCnsGkQKfSamRnSRiGcfEoCkAisFqkFsCsjJsBjrEqgGKcGmRgrJbGinOkFcmOcgJaMskNsMgrGJsgKAiEcnRNroRjfGmCjqKebSkrOcRKesMCqGsKoGscMajNgAjNcBIak"},
    {HX_TILES, 3, 4, 23, 50, /* 83: 1.00 */
     ".o4o4o.|#oooo#.|.o#o#o.|oooooo.|.ooooo.", "",
     "KOanBgFIcCnJoJobJcjFkOMcKgCnBkOCngCGbOgaFaBnAnkOjJOcfMNBekFknCkgBinCoeCBkFkNgCnBfbCjMFFofBjMbjKEoOMGkENceGanKcMbJnIFNjO"},
    {HX_TILES, 2, 4, 21, 70, /* 84: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.o5o5o.|.o11o..|..o#o..", "",
     "BkJOicKFFcNfaOkOjfMGckOfFcJgFjJmIgAecAoEFOcjIcBMejCgoCOjmKeOCIeAigBkmKnBkNObkAgMBojCOBkoAokCiIeGmgFncJamFGnIfkEGbiKmBAoOkoI"},
    {HX_TILES, 0, 4, 22, 60, /* 85: 1.00 */
     ".ooooo.|.oooo..|.4o3o4.|.oooo..|.SSoSS.|.o##o..|.ooooo.", "IFFI",
     "CfmNGjCjEFbiBMFAgGaKobCfEjBoaBnGmFoaNIboIgkABeaIfcCjBngNafCOeFOgmOiIocBBoGkOceNjKKgNkGogAoMfCJGcFobOGCoBknMkFbfIneMBBEbeO"},
    {HX_TILES, 1, 4, 25, 50, /* 86: 1.00 */
     "..ooo..|.1##1..|.ooooo.|.oooo..|.ooSoo.|.oooo..|..SoS..", "OKcnKcn",
     "JocJJJMCfKgcCiFAGcnKmAGmjFCMBIneInKeEjgNFkBnjImBJgAfAknGcCkoFcIEbfEjmMcGjNcEbgGcEnjGojBeoEoNeNgKcCnOkbMkmKgjFoIEbgEibKfJocGkGj"},
    {HX_TILES, 3, 4, 22, 70, /* 87: 1.00 */
     ".o#5#o.|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.o1o1o.", "",
     "MFFjoCCKfJekKKoIfKgNcfNeBjFiCkoBiOiAnbEofFibAkNkObjInKgJbOFmCifFaEbEkbCnAioMKGiOagOgcIgiCnbEnkBimMkoEcAeGkeEmFFGBnOkoEiGjbNgjJeAjcGig"},
    {HX_COLOUR, 3, 4, 28, 30, /* 88: 1.00 */
     ".ooooo.|.oooo..|.ooooo.|.#oo#..|.oSoSo.|.o33o..|.S#o#S.", "FcFcNN",
     "BgOaCGiFmgCocBngKKboCGcOgMaoBncEcfBkaGobGiCjCjMaOBONjbNecFkfOOaeEkCkGKEjgIgOgJcKmbCgoGACgkFEnEaJfjIogKfcJckEjACgJmNKgiBBgmNNanOjo"},
    {HX_TILES, 1, 4, 22, 90, /* 89: 1.00 */
     "..ooo..|.oooo..|.3ooo3.|.oooo..|.o2o2o.|.SooS..|..S#S..", "BBGobGob",
     "MOcjNboOibOiJEBgGmbCoaKKnGOibEcIojMNbBmFcoNkoNKmiGFmCeaCOcCkoOFcMjmAoaGcGnIfAmkCjMeBjKKbgFbFkIKcNKgkKnCfoEboNGnKAkeGnEbFONc"},
    {HX_TILES, 3, 4, 18, 60, /* 90: 0.75 */
     "..ooo..|.oooo..|..4S4..|.1##1..|..#o#..|.oooo..|..ooo..", "Ok",
     "NgEiNiaEmcOgoGAjfFoeJeiGjeEkImOekBoaCKfEniMkBNfJEbGnCocCoIfFoeCojKbCgbObGNIeGknIOaOgNcKmbIfCIoCmeIcEOckKcFnEafNFocCgGnJncIgIoaGbGngGo"},
    {HX_TILES, 1, 4, 25, 90, /* 91: 1.00 */
     "..SoS..|.oooo..|..o#o..|.oooo..|..SoS..|.o33o..|..ooo..", "JmiJmiGG",
     "CfmNgFkFJGFoOBfaMgoOeGmCngCnBnkEaNiNeFcFFjFnJnjNJOjoJNjEcIeJGCoCenCenNInkObEkKmfGaJJaKokGcCjObFoGIcEkgIbIckKmaAekGoObOCfI"},
    {HX_TILES, 3, 5, 22, 80, /* 92: 1.00 */
     "..ooo..|.oooo..|.oSoSo.|.oooo..|.oo5oo.|.oooo..|..1o1..", "MM",
     "JcAeaJneNSArOgcCejNgGmKorBfRkRgrFobMeAgmBgEjnGjnArnKKGcqJsQoJgGbmRogKqeFcnNkFEcGmRgJKAsGjqNkBrjMfReoKNesOiKbKOqCMcKeaBBJgMGoKcgAe"},
    {HX_TILES, 3, 4, 25, 90, /* 93: 1.00 */
     ".ooooo.|.oooo..|.SSoSS.|.oooo..|.2ooo2.|.oooo..|.ooooo.", "BMcMcB",
     "FAmJbOfBoJfAFGNkbKfOeKcNiKEjAKoKfFiKKoKaJaGmkFFOcoEkNeGBjGJcgGaIbkAGbIfnCFiNMJOgBfJbFCnMjfKGciKNbMgnBoaAgOGokJn"},
    {HX_COLOUR, 4, 5, 22, 20, /* 94: 0.75 */
     ".4o4o4.|oooooo.|.oo#oo.|#oooo#.|.ooooo.", "",
     "QoiAqbNcIgKGkQgSbmQiRkEbeSkrEbkCsAsfSbNFcAsgERgrNcoIsIojSnjFsAqeBoScFoCgCqSboQagOkrQoMqKbKoCIqbIgAkfRoGmAjAgCqoJmRkOckFnKCejQGsAokMbkJgGrk"},
    {HX_TILES, 0, 5, 26, 70, /* 95: 1.00 */
     ".ooooo.|.oooo..|.ooooo.|.oooo..|.o3o3o.|.oooo..|.ooooo.", "",
     "ResIfCJokNGsbBQfkSjbOKrQiCAekEQnMcsSasBjSjCoNimSkMgOkSgFResQoFoSJSceNrSnQisEbmBCscCoNscKnsREiOaBoQOgsSobBBscSgJBgaGcSiKeBqcJcC"},
    {HX_TILES, 4, 5, 23, 80, /* 96: 0.88 */
     ".o#o#o.|oooooo.|.oSoSo.|oooooo.|.31o13.", "EE",
     "JocOGrMaRbJIbeEqaSnGkRcSmFqgOcBsKrkOGmBmgJgkJaObCKKbRbrGkbIcScCjIgFqBkFneFbkCkOeMCgFsGseSnsJoRCjeNAgSgOKcCnaJQcKaSaeJoGbSbkFsgNjf"},
    {HX_TILES, 2, 4, 25, 70, /* 97: 0.75 */
     "..ooo..|.#oo#..|.o#S#o.|.oooo..|.o4o4o.|.oooo..|..5o5..", "Gjb",
     "MgCoJMcGmfKcEkoNboOcMkBjKFmBoFBKaEkgCgJoKmCgMJbGoIbICnaKaNecCkeKOkGkgFImgCioCfBnCiAjFAeGcOGkBkgAiFbnGoJbgOfoCmcAMkMfOjNBoBm"},
    {HX_COLOUR, 4, 5, 28, 30, /* 98: 0.75 */
     ".ooooo.|o2oo2o.|.oS3So.|oooooo.|.ooooo.", "QgsQgs",
     "QgoKrnRnrBmjECJecFbkRosSisSgnNbMMiAFKQjsFboCfJoIGoQoSmBgkJrQIOcqNMfArArRRfmOigEJbRRcqMjRcNKnjBSagRiRfkQCfSQmcRkIBjfRgKaKbsSgc"},
    {HX_COLOUR, 2, 5, 28, 30, /* 99: 0.62 */
     "..ooo..|.oooo..|.ooooo.|.o##o..|.o5o5o.|.oooo..|..ooo..", "",
     "JGqgSKqOgkKrQOrJqCgbNNjJoeGAogJOReFscNqeKgQJmiNjmQkqBsQfBioKCfImkCGsCeIocOgsSSiEosQaOcjKemFaGinSAqnJNafOsJbIoAoNamMcSBkCgOrgOS"},
    {HX_TILES, 1, 5, 28, 80, /* 100: 0.88 */
     ".ooooo.|.oooo..|.o#o#o.|.oooo..|.SoooS.|.oooo..|.o2S2o.", "EcEcF",
     "IajKKMjOaeMsgGscEkAsmGRgaNbmOfAgsSgMiEKqmQmJnSJgcQoAfBgbOgBQnSaJbKCgjRoScQbGnJckBNagSBmEAoGojMrKbRMfIsRcgEciFBsCkOFMsJgbBmrMb"},
    {HX_TILES, 1, 5, 19, 60, /* 101: 1.00 */
     ".ooooo.|.oooo..|.SoooS.|.oSSo..|.4ooo4.|.oooo..|.ooooo.", "JcJcOigOig",
     "EkqJsfQoGmRGRiScrFkbKnRjmBnNaCIrcOseKeJFJmIgIsQaNgcIfsRocCKoEnkEoRIsmIoSjmEMcCSMObmQcEcFRogNjEjEbQioCjCkCqnGjGKnNesQfqSjRogFqGm"},
    {HX_TILES, 1, 5, 20, 50, /* 102: 1.00 */
     "..ooo..|.SooS..|.o#o#o.|.oooo..|.#ooo#.|.4oo4..|.oSoSo.|.oooo..|..ooo..", "AoAoSS",
     "NcSaOaRkNFaNSOrbEcRerOknNgKaGbGnRnFsKKQaArBjOkKcAjKqCqoGnIceQmeGrcJgsOifGjsQjbSgIgkMbAESjNScnJcjMgBeGjgBrNbGKbIoFncEsEkCqSKcrFq"},
    {HX_TILES, 1, 5, 17, 80, /* 103: 1.00 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.o22o..|..SoS..", "KsKsFF",
     "CfmCoKcKfNrGcRniFjoBmcBrKrkJInFskCfRfbAfkQnJJfiRkSeOkrRnRjsQneJcjNcEOekISCrgArOcMroBRcSigJaJaQiRcnAoKeiKrOeFbiFoCfQfIsSiGKNMbgGqoCgA"},
    {HX_TILES, 4, 5, 21, 70, /* 104: 1.00 */
     ".ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.3oo3..|.ooooo.", "",
     "JfSkgKJqgFjJfArAIrJeSjoBrKgnBnOiKobCeoKgRkRcoGBeoQbMcjNsBKsArNbQjmGngFamSfsJceJfJnqIcSgnJnRnBinGnkKorSciIeGksCOKmMiBsaBksKrMgcSfbAnfNfNqgMjoKc"},
    {HX_TILES, 0, 5, 22, 60, /* 105: 1.00 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|.1ooo1.|.oooo..|.1SoS1.|.o##o..|..ooo..", "BoiBoiKK",
     "KFkqGECrKnEJbBnJmGiqNkGkoQoaGsFrGieMsOgkBrAjMaRkeSbKrkKgCkqQJcFsFaOkeOakRNSOsIsNbiNQbmFnbSkqRjeKcoSgSaCmIasRbNrAqbCmJgrFjSnAnjOgjSmqBf"},
    {HX_TILES, 2, 5, 22, 80, /* 106: 1.00 */
     ".ooooo.|.#oo#..|.ooooo.|.oSSo..|.So#oS.|.oooo..|.4ooo4.", "QajQajGkfGkf",
     "SiFiQjgCosRoqMeGqIbjBmkFoOseMaEoCjnMbmGEofKKAjoQeJcgBfoIAnsJmCqKeABsJsaEqoAsOiMsCnaScgRnOgGoIogCejKIbJsfMEcoFojFmBGkRQCmrNebSemJCgoSocJ"},
    {HX_TILES, 4, 5, 23, 80, /* 107: 1.00 */
     ".ooooo.|.oSSo..|.ooooo.|.1oo1..|.ooSoo.|.oooo..|.oo4oo.", "SSOaf",
     "SnkGrfBneOskRSBfJbkReNsfGsNbeRnGcrGnBfEQgKniJaeEnFmJfImBkQfsSgQcSECrNjNecGcRbOaoBjSbrNkbQmAfjFBqRafFReiNaSmRnSSnMfQfbRkErFjBgrJesQbqGk"},
    {HX_TILES, 0, 5, 23, 80, /* 108: 1.00 */
     ".......|.oooo..|.o1o1o.|#oooo#.|SoooooS|oooooo.|.SoooS.|.oooo..", "AgAgBkqBkq",
     "InaMqGisJbrEjCRmfMrkKJckCseJcGiEnCscBfkFQaJfaOgrEoSOASaKaNjfOeCgsMNsnGmBFAjoIrKakBECNrmCkqBoJmKfFmrMbnNGOgRgcFbmSnkORgQfCocCCoS"},
    {HX_TILES, 3, 5, 23, 90, /* 109: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.5oo5..|.ooooo.|.oooo..|..ooo..", "",
     "NajRocNfkGsObCkgCkaRnOksGsiMjNsoRbeBQbOjGofAeFnKGobJbsBCqCmgFiKsaCqQakOfGceNScNgKMgRjEQnSerMgkMkSNkQoaBskJbfBoCMJBkSbKFiNROqkMekNfn"},
    {HX_COLOUR, 4, 5, 31, 30, /* 110: 1.00 */
     "..ooo..|.S66S..|.4ooo4.|.oooo..|.ooooo.|.oooo..|.ooooo.|.S##S..|..ooo..", "FojFojNriNri",
     "MKbqRfsGjrEcoOqnEkcKJanCfGCinSgGsSFRfMqcICmSfsSogCOKFkqGiArMsoJeOgmIgRkqBqnRfkErCsgCgKqjKgsQnRnkSSBkFoBASimSkJoAnOqkBjNfBreQnsJgCgI"},
    {HX_TILES, 1, 5, 25, 80, /* 111: 1.00 */
     "..4o4..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.#oo#..|..#3#..", "",
     "QojJeNsOjfEKMcOjBnOsiEbEbeJoGnkGNiSbrCejJKfsGMGcnBsNgjBqOCgSQEjAFjgQcsGjMkIsCsgBqmBqoEkgIrNeJqKoEaoKcsGkmMbjScjEOKmeSaoAkfKnkNibFRfsC"},
    {HX_TILES, 2, 5, 16, 60, /* 112: 0.62 */
     "..SoS..|.SooS..|.#oSo#.|.o##o..|.o161o.|.oooo..|..ooo..", "CigCigQaqQaqSos",
     "SmFEnEoOjCrfRmOgjSoKbQbsMebBoFsScFnQcQQgnRmcEcjJbBCfGjoGaGmCkCqOsGcCnCRkgIcBgaFncGOCiNgkFJcRisSknIbQkfCskSKaBqJmGkBfGcrBqNcfMbSfSb"},
    {HX_TILES, 4, 5, 25, 110, /* 113: 1.00 */
     ".5ooo5.|.SSSS..|.1SoS1.|.oooo..|.ooooo.|.oooo..|.ooooo.", "McsKqiKqiMcsFcrFcr",
     "IIoCobEAnIqiBqjMjIBoKBkORGkgOkmSgGkOcGbqObGogRnsQnEcrMieOeRfrFcsCsBQbAkKObjGAkrFsaMgQCfKenOfsCsMNgKaOboJgaKmkJsKnBOjGsMbjRFOkbEme"},
    {HX_TILES, 2, 5, 28, 80, /* 114: 1.00 */
     ".......|.oooo..|.ooooo.|oooooo.|2o4S4o2|oooooo.|.o#o#o.|.oooo..", "Cos",
     "ObBfnMjeCknRiQgiRkoAmrArmSfsGrfEmICKRnrOgrOfNbqBsBosCeSfsMcsOimFImMaRbSfaRkOrNCmBQSkqKsKgQaCnrEmkEaSgEmOcNgBgGcJsfNckMCeSocOeOkSEnqCmrBkr"},
    {HX_TILES, 3, 5, 28, 90, /* 115: 1.00 */
     "..ooo..|.oooo..|.ooooo.|.SooS..|.o2o2o.|.#oo#..|.ooooo.|.#oo#..|..ooo..", "QkQk",
     "NkFSaEBgoKnSjRnCjFnESoqISfkCkFjEsSOqSmsIRfOEjGjFGkJnSnFnkGBfBoSesNimJcCsObEsgNrKMgSfNEboGbkQcMcoSnkQgJorQeCqmNrEniSbKsCkfBsjRm"},
    {HX_TILES, 3, 5, 28, 110, /* 116: 1.00 */
     ".ooooo.|.o11o..|.o2o2o.|.oSSo..|.ooooo.|.oSSo..|.ooooo.", "MMNseNse",
     "SNsOfIBgoNgJokBrGrjCsFsmQeJbnNcQSeaBqCsJqOieIeGmrBnrIoBsmSbEjfAocEBOsQbfObrIgCRmrGcCofGnsFGqjBjcFFMEsAAesOcRJcqGnQcMbjMjFrIsNbRosRi"},
    {HX_TILES, 0, 5, 29, 100, /* 117: 1.00 */
     "..o2o..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.So#oS.|.o##o..|..ooo..", "EkgEkg",
     "JmJMMgnKeBkoSbSkQiGBfsRnsIFkgMKBkBOafEsMsCKsBnKboOfkGCmEoGrCiOgSeCknRkSCkgKfCorCmEiGrnOMarSfRnAjmBekCAjrIrgEsFksIJBIQNgO"},
    {HX_TILES, 2, 5, 32, 90, /* 118: 1.00 */
     ".2ooo2.|.oooo..|.ooooo.|.oooo..|.ooooo.|.#oo#..|.ooooo.", "",
     "MIRosJmNCkEsjMrfEsSfQfrFNbsKfoENCiGcsORnIeAiSJfaGqmOSgGsOfRknRSbSecGosJgOKcNirJqQbrCjRkcJbJfmNqoKBReOcjBosMfNgcJeQmsJcrNesMBORo"},
    {HX_TILES, 0, 5, 32, 90, /* 119: 1.00 */
     ".......|.5oo5..|.ooooo.|oooooo.|o#ooo#o|oooooo.|.ooooo.|.oooo..", "",
     "KaFSgNecAEAJICmBqjSKoCFskKfKogGaiOgOkBBeaOgcEnqNrnBmsJagMfJnfSkSgSniSOCsMbGCskAsoJgFciIfRobSaKgJqjJBqQSncNgOaSfnEocQofEkcBsAIQck"},
    {HX_COLOUR, 2, 5, 32, 30, /* 120: 0.75 */
     ".ooooo.|.oooo..|.ooooo.|.4oo4..|.ooooo.|.oooo..|.o#o#o.", "",
     "OgkIbsOqQoOKKIfBfNskBirCkqBerQoGkqQeCqMceKQinIeRJbrSaFbqOfsRbsOiCqEjfFiBeNkOjbRfiKofNGkNaSobKcBsFkaCsnAOsSjBrgKmcArKFinEcGBkbEiFmaIocBsCsj"},
    {HX_TILES, 3, 5, 30, 100, /* 121: 0.88 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|.o2o2o.|.oooo..|..ooo..", "RinRin",
     "OcOfcQgrNaSimFqIBnaBqFrMrGsJmbOjrGicFCoeRogRCCeaSoRnNMEqeNcgAJbRcKaGCqnRgNeoBoFGFBnkGcGcKnrBsEcKQoGjqOfkNkfFcJNrJAoQKoQiOgmJrk"},
    {HX_TILES, 1, 5, 23, 90, /* 122: 0.62 */
     "..SoS..|.SooS..|.SoooS.|.oooo..|.o6o6o.|.oooo..|..ooo..", "KbKbScmScmBnBn",
     "RorGrCiBmNkfNgRbKcAkbGjgGcfSerFFjcMsNeGakOifNqBEoNjBqjQoBQfFskBmsJskFsMcgCsmMgmCnFbfBrFMaKoScGbjQbKfSfcKgiEaIoKsfNgSbQocKBQgkBsmKreOgKenNKn"},
    {HX_TILES, 3, 5, 30, 90, /* 123: 0.75 */
     "..ooo..|.oooo..|.6ooo6.|.5oo5..|.#ooo#.|.oooo..|..ooo..", "",
     "NagQcgQGFbSagNknAnSkmMbFnOjJOgjSbfManBNiGobNbSKMbsJfmFbSeEObsIEbqGIcsKmrMSjOrcBjqBsKKNaiJfRckJeISgNrIRokAkAsfGJOiFIFoIfjMkEjs"},
    {HX_TILES, 0, 5, 31, 90, /* 124: 0.62 */
     "..ooo..|.oooo..|.o4o4o.|.oooo..|.o#o#o.|.oooo..|.ooooo.|.oooo..|..#o#..", "",
     "IRnaKcqOqaQKaONqkNjaJmjAoJArmBNirERIfjKsBsnFcFmGjsCMgaGoERifKrnQgnSbmSJmOgqGsOroMjmQoSFjoBfmFGsfJnaQnfOGoKafJeaArfNgMMaECqJojCqOsoJObn"},
    {HX_TILES, 1, 5, 32, 100, /* 125: 0.62 */
     ".oSoSo.|.oooo..|.36#63.|.#oo#..|.SoooS.|.SooS..|.ooooo.", "SofSofRmsRmsKgKg",
     "NsoGmcIqOkJONfKOqgCicEcoNREIgOMenRCiqJqMBABrGArgCnRbMAgACnjJmcSjFOcfQNjqGoSeEkSfoEFiqCfGaKcKcKnIbqEoNrcSFnFmKGcnFBSo"},
    {HX_TILES, 0, 5, 22, 90, /* 126: 1.00 */
     "..ooo..|.#oo#..|.7ooo7.|.oooo..|.ooooo.|.2oo2..|.ooooo.|.oooo..|..SoS..", "CgCg",
     "FsRAgrJfsBfnAoBGAsCrBobInjKnsGFckJgsAsoNjQogIncBnQjCncMqoAnNbkOceJcMjNfkBsgOskSfRmKqGkQbOgoCscEoObSfcNgjFSmbJmaEMqgSeiInkGraCosEbCsmKoBnkIaSgOrEsb"},
    {HX_TILES, 1, 6, 22, 80, /* 127: 1.00 */
     ".......|.oSSo..|.SoooS.|oooooo.|ooooooo|oooooo.|.oo1oo.|.o55o..", "IbIbOjmOjm",
     "GbmGBvFoIqkIsJoKvaWjvVgKsuEKgAwJfcKvbWkfJaoMkuOsCJwKbvVjEkWqkAsuFjwOOkEiMRkWcvIWanRcBkcUirBsnRkgNsIcgGvSwCvsSvRoCevGKcmWfScKqwJoVfuAqEuJqJw"},
    {HX_TILES, 0, 5, 26, 90, /* 128: 1.00 */
     ".......|.oooo..|.oSoSo.|3oooo3.|o#ooo#o|oooooo.|.oSoSo.|.5225..", "KoKoFF",
     "JgcFosCqiFAnMCkbBkNrKceKmESfNiKoKrgNaKaMkSnrGcgEsgGnSnfSkqNrFojGifCkGiSneCoFjoQiRGsjObeGKcgKcSkcOaKSojCkbSbrBkrFcKsGaoMcoNfnOcsGcKgMBqjBnCjgKmf"},
    {HX_TILES, 3, 5, 23, 120, /* 129: 1.00 */
     ".......|.oooo..|.ooooo.|SooooS.|o2SoS2o|SooooS.|.ooooo.|.oooo..", "GjfGjfJnJnJJ",
     "BncOrcBfCirSogFnrEigOcrJceSgjKcoGnRcsNrGmGaKCAojBfaBrGsgGobFoNrCCofKnsIGnrCsKeGsbKRicMfGSJqkMRojSNcfKrbAimNkaFnABkbCsQfBeAsNiAnGjQbAkaBRna"},
    {HX_TILES, 0, 5, 25, 110, /* 130: 1.00 */
     "..ooo..|.oooo..|.o2o2o.|.oooo..|.3SoS3.|.4oo4..|.ooooo.|.oSSo..|..ooo..", "BkBkFqfFqf",
     "QcgCnNKbgMbnKriGciRorAnJcAoScjJScKrMckFngMcAiNerGcnAogJsCorOqfNcsBsBgmSbKoaACIeMcgMjrOjaBmSbgRoaGkbOgbSanJgsFbIfRmOgmNsgEqoJceCjRokFcmMNbNsAneNCkFja"},
    {HX_TILES, 4, 5, 27, 110, /* 131: 1.00 */
     ".......|.oooo..|.oSoSo.|oooooo.|ooooooo|oo44oo.|.#SoS#.|.o55o..", "IrmIrmRgnRgn",
     "BIrnOcIfkEFCrGokFniJfrGjcRmaCnFSgsIsmIqcFnRjnGrcJmaAeCrkGcQkrKcEcfJeRfkSfQkEaCocOKfOsgEOecCgOeNgmQaEjbGrFsNsbSbCAjnCoNcjGnkBgaArAOkBgjRoGbjNr"},
    {HX_TILES, 4, 5, 25, 90, /* 132: 0.88 */
     "..ooo..|.oooo..|.#o#o#.|.SooS..|.4ooo4.|.o22o..|.ooooo.|.#SS#..|..ooo..", "QeQeFngFng",
     "SSkCkfKoIsJsNrIoRmkRbSjoBkIcoBnfEaCmcFcORBImbJoGreMkeBqFjGosRgCofSkSoGrkQbQfCfjMJmrNfKBfcKIoQbSaOAoNsOekJfCRbgNrjOCqkKfiQfbRMaGRke"},
    {HX_TILES, 4, 6, 25, 70, /* 133: 0.88 */
     "..ooo..|.oooo..|.ooooo.|.3113..|.ooooo.|.oooo..|.ooooo.|.oooo..|..5o5..", "",
     "EqCvjRkwGwFoiMFuVCjmVkQwrSivONasAivEovJqnSwGjuKnrCqwGugJmsNicEroJcWjFJeUoObEAuoJmCvfVoWrNqMrfQFcEGicCscSnwQcIsgRcqIgsBkGvoRnKocIgrAoIrwOsgEvjAnGq"},
    {HX_COLOUR, 1, 5, 35, 30, /* 134: 0.88 */
     ".......|.1oo1..|.ooooo.|#SooS#.|oo6o6oo|SooooS.|.#7o7#.|.o##o..", "SoSoNcjNcj",
     "BicSOkmQkmCjgJqoCmaOcJoAKcoJaeKgkBmfBecOrObnNsmGkNkCqFNgsQnjAjJgnBecKoBrfAgkAnBesGsFaSgsGcgNGmrRKfsCoCjOrbSfmQCnrOgnOqJaQKcQcfSjoCqRcfSfNkBjsEQf"},
    {HX_TILES, 4, 5, 26, 70, /* 135: 0.75 */
     ".......|.SooS..|.ooooo.|o#oo#o.|oSoooSo|So##oS.|.3ooo3.|.oooo..", "QQKmKmOjOj",
     "OagOrnJSbfOkONkCfmCRoMsSkRemGojQnGkbFmjKEbJeEIeiQEmKRiKqkCnScoAqEaRMjrIbGaIrCoGnkFjBkJqnSnjRjJgNcmFmqFkfJEjRjKskGbeKgKajRoGogQemQjSiqE"},
    {HX_COLOUR, 0, 5, 27, 30, /* 136: 0.62 */
     "..ooo..|.o##o..|.3ooo3.|.oooo..|.oSoSo.|.#55#..|.SoooS.|.oooo..|..SoS..", "CgCgOkeOkeQinQin",
     "MsAigBgBscBibOGOqfGjFifGjRbeKmRecGjrObrKSkBJrkGnkBIqNrOcmAobBeJgCnrMsmKckOafBjgBqCseMrKriQgMsaMreAnJoBncOamIrCoCCmOkOceMjGkqJaJskGkcBrAsKbiBAgn"},
    {HX_TILES, 5, 6, 28, 80, /* 137: 0.88 */
     "..ooo..|.o33o..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.5oo5..|..oSo..", "Jwj",
     "InvIrnWgGUmErGAksWgkEvcMkGkRRkgVgoMvFnViAfwUiuEnrAnCwGksKrBvsCfbGcgUbNkmFoIqCgsSibWgbEciFbnKoGvNWqIvaWnBfVfnEwWgAsGaiMqJvcRfqRvgMuUbsNIGkbOfwN"},
    {HX_TILES, 4, 6, 26, 100, /* 138: 0.88 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.2oo2..|..ooo..", "",
     "MaSjgBsvFksQcGwJwKmCsjUksGrAvjKsNfbOwViCknVOcgVgkCnKmJvnBrjEnvNwnOjKwrSBovVFieUiuGjWcnOwWojMgsFSuCgwWcAirGbJwGaiVsmICknMigOvNGmBkvQcgCqfGisCwMwEcj"},
    {HX_TILES, 2, 6, 25, 80, /* 139: 0.75 */
     ".SoooS.|.2SS2..|.ooooo.|.oooo..|.ooooo.|.oooo..|.oSoSo.", "EkEkGkwGkwRkRk",
     "NreWkcFuFvsMwVkEkCfsGNbUBifJQkOjuJviSusNkoCeMkqSAfwBqEoeSeBwnJmFnUgiAGqOwGjnKucEBkrVneAfmFujCfcJbsJmVkGubGjNsbMJcwEvEoqFnNkbCfbQigCrFmkKbFsaUqeSw"},
    {HX_TILES, 2, 5, 34, 130, /* 140: 1.00 */
     ".ooooo.|.oooo..|.o5o5o.|.oooo..|.So7oS.|.oooo..|.oSoSo.", "MsjMsjFofFof",
     "JaQbGrfFbRckSIMKgGnCnGOfaBjKrbEqaOIekRnIsGrOJsgBkJrAjEbKJqcSmgIgAkqOkeJnEckScMsCfOqfKoQcNErBemCjbRoObKcFqMJgmMgGakKbQoQkaSosKoSgmSbk"},
    {HX_TILES, 0, 5, 32, 100, /* 141: 0.88 */
     ".......|.4oo4..|.ooooo.|oo##oo.|ooooooo|#oooo#.|.ooooo.|.o11o..", "",
     "MrcKbRieBrFakFagCqOfBkSkJenEjeJBgSebIfjCsjCsjAerGbgAmBsoSbmKsEkJcmRfSkgQobScgNboBmsAqCkIGjfScjCjKamAJSkOKgiOqfMFFinOKAjAqReBkGjEjmCfiGmJfKei"},
    {HX_TILES, 0, 6, 24, 70, /* 142: 0.62 */
     ".ooooo.|.oooo..|.oo6oo.|.SooS..|.ooooo.|.oSSo..|.S6o6S.", "NgNgGcGcRgRg",
     "UqRkSekWnCfVcmWqJcMrFoeRfoFbJcmQaeIbeBfrIUbuBfaOfjFrGoOqGnWcJfiFnScEqnIvsAWsuNvcWbmUWscObqNcBgaBvCrjAAwGFsbCfrFkFowJFwMUncGocFmOgoRkQwjOwAsuKsu"},
    {HX_TILES, 4, 6, 32, 90, /* 143: 0.88 */
     ".......|.6oo6..|.ooooo.|o####o.|5ooooo5|ooSSoo.|.o1o1o.|.oooo..", "RwrRwr",
     "FFJebVIvSiRkoBfmAwNgUaKeBqBsMeAvrWsSWbSbGocVfvKwbVagEsNcjSewGjgQBgMgoOIAGjQcgFqjMqCiKcJwnCmsCkGcoJeJgrGjcAuaAiEQoCJuGswEsgFvGumOjSfSa"},
    {HX_TILES, 0, 5, 34, 120, /* 144: 0.88 */
     ".......|.oSSo..|.ooooo.|oooooo.|o#ooo#o|o7777o.|.6o#o6.|.oooo..", "JqJq",
     "KsjAjoNasJcgMiCscCkoKskAnQeqNsoSnkQbkNfQbqObgMbFcsIsiQoaMsFinFOroFbkIqmRfqKmfOcjNbJgCosNfoORokQjaRbQjnQmgAgcEoGGsGkgCrOqEaNiIOCiOrFicMeAmjKrNsbFArgE"},
    {HX_TILES, 0, 5, 32, 120, /* 145: 0.75 */
     ".ooooo.|.oooo..|.o#o#o.|.oooo..|.oSoSo.|.#oo#..|.1ooo1.", "NqmNqm",
     "BkCqoKscKCjFcsBfBrbOAgFbjOsNbOCGokKsEsaGiJcjBksKfQcBfGcoKoqGiSgjNrQKRnkEasRorSBkMEraGsQAkSafGRSafIcSkoBoeGkBniKnFoqIsgQbsMbOqcGbQekGkcQo"},
    {HX_TILES, 3, 6, 32, 100, /* 146: 0.75 */
     "..ooo..|.oooo..|.oSoSo.|.oooo..|.ooooo.|.oooo..|.4ooo4.|.oooo..|..ooo..", "JvJv",
     "QbNCnNjsSoAjeWnKagNweNaAORCiVrCjBkNuGnWaAvFuSkoQkfUcEoRkIfqWqgRJuWoUBqoGwqGvmFiWsmUjCfkRbkIEbjKwrIbAJGCnsWbSvkKgSaeUoQvcCGvEvAmMbq"},
    {HX_TILES, 0, 5, 34, 130, /* 147: 0.75 */
     "..ooo..|.#oo#..|.o#o#o.|.oooo..|.S4o4S.|.oooo..|.oSoSo.|.5SS5..|..ooo..", "BrgBrgQnQnBqBq",
     "SjQSfNieRbmFOsaGsKbeOaIsnEckMiSaGaiCqOkeOjbBrQkJcjJRcAnJqFbSbFkrSbOioMfJgqSoMjgErIOrGkrQgcNBkCsGngQmjQgFncSeRcCnMbOqoNiOgSbmFjoMiObgNskQkInk"},
    {HX_TILES, 0, 5, 34, 120, /* 148: 0.62 */
     ".......|.oooo..|.##o##.|oooooo.|o#ooo#o|ooSSoo.|.6SoS6.|.oSSo..", "EnkEnkSnqSnqJeJe",
     "KQesBgmIgkOjsIqaRnRcfQjEkImJmEoRamCjEKeOeCNKaMQngJfINbOqaGsnFirEjsOeiGcgAGnJfGoSnbGKnJgsJbGniSMsIFsjAoGaAnGmQirOjKJmsRjKoiOcGsJqjIgc"},
    {HX_TILES, 4, 6, 37, 120, /* 149: 0.62 */
     ".......|.oooo..|.ooooo.|1#oo#1.|ooooooo|oooooo.|.SoooS.|.oooo..", "SfSf",
     "GcfCrbEvRnQvCEmiGsuCmgMgvFKcjKoWBoRbrKBnvIcvNraIsGVcJeoEmKCuVgnIgcUsmUswJrRvbFqvOesJgqFOgBJugQwKUAJrjNACJbiMjNrWfbMSoBIcOkJcWjKus"},
    {HX_TILES, 1, 6, 33, 130, /* 150: 0.50 */
     "..SoS..|.oooo..|.2SoS2.|.oooo..|.ooooo.|.oooo..|.#ooo#.|.oSSo..|..#o#..", "FFJJFbFb",
     "VqEimJwGsGcsOjRkWciAguBkbFrfMNgbQvBkuFsFjUgOisFkRbvQngJaCsiGmKmvIrvNrWnCiuOkFjRkwQmUkuSnaWsCjgJwNbgKsSnWcNgJwUjUcJwmQniEaObFcoBrwFwmNgoAucKruKsKqgQfv"},
};
