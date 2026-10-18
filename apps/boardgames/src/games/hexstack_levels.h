/* Hex Stacks' levels: made by tools/hexstack_gen.c (our own boards and sequences, each won by the rules'
 * player in `par` stacks, and by most players choosing worse; do not edit). Each: the goal, its colour, the colours
 * dealt, the par, the target, the board, the stacks already there, the tray's sequence (hexstack.h). */
const hx_level_def hx_levels[HX_LEVELS] = {
    {HX_TILES, 0, 3, 1, 10, /* 1: 1.00 */
     "..ooo..|.oSSo..|.oSoSo.|.oSSo..|..ooo..", "BBBBBB",
     "BEJKbCgKbEIfCEcKbICGbKaAgAgGbAkCkGaIgKaBeKeFiKKBGAGcEKbKKJCGkCKgICfGbKEkBkCBIgAKeKBJKIGjCkJgIbJKcEkBkGiFcAKgAfAkBIKFaJeCJgKeE"},
    {HX_TILES, 3, 4, 19, 50, /* 2: 1.00 */
     "..ooo..|.oooo..|..ooo..|.oSSo..", "KojKoj",
     "BogCFGaCFAjNJbCCJecMfFoIeNGaNkFjIOcKgkGaNeJEKeKEkIJGObCkFKCokOFbeFboBojBoFnMfFNkCjJFKoGGIbBeFoaCAkCokBIgiFnKmNgNNGcJgaJofJBfFOkBFoFoOeAJKC"},
    {HX_TILES, 1, 4, 17, 60, /* 3: 1.00 */
     "..SoS..|.oooo..|.So.oS.|.oooo..|..SoS..", "FFAoAoKbKb",
     "EkcFFACMGJfICCngNjIfAfMjaNbNKocCIAnJFncFKoIBNkOkgCJfCOfoNKJbOjeGoGEIfMFbNfImIcEKokCCEcOcCifJOCgcBGcFoMBEkCkKaJmOjJKeGIcGEJFBCejFjBfGcnO"},
    {HX_TILES, 0, 5, 13, 50, /* 4: 0.94 */
     "..SoS..|.oSSo..|..ooo..|.SooS..", "ScnScnFcFcRgqRgq",
     "BsoKnJsCrMasOgmGsFrSiSoCArEOQcRcJQbkJIqIrECMGNKIfKsGSMBMfAgKrSbSJCBiaFsCCfAfQcGiSKfBfoNNrKCKSjFIsQjMIcJmFrGqJGmFGCgsIAqSIBjGnGrNOAfOj"},
    {HX_TILES, 2, 4, 23, 70, /* 5: 0.88 */
     "..ooo..|.oSSo..|..oSo..", "BjBjC",
     "KAJOcKoGCoBBJJIoNafBBAGEcCokMCFGoCkGmkMiEmkJIFKJCCBmOgkCfBfICjNcOOkIaCFnjCoGjOkFNafMNOKfOBKmFbEGAfMjAIbEKEcBIJfGNkBGCocJnjMIgkM"},
    {HX_TILES, 1, 5, 19, 50, /* 6: 0.88 */
     "...o...|..oo...|..ooo..|.oooo..|..SoS..|..oo...|...o...", "BfBf",
     "OReCAQkGkKRakNqJrQGaCsBnCeCIEsEEbRoaFJBOIROrNjKEjoBgQaSbOaBqBsFEMSkbGMsSogRfjQgARoJscOesSGoOiOABrMGQBNBjKObFBQkIrgARgjJOjNNEFmQkoKgOaN"},
    {HX_TILES, 4, 5, 17, 60, /* 7: 0.69 */
     "..ooo..|.oSSo..|..SSS..|.oooo..", "KsKsBSB",
     "FFJCqGSMjgJoCfCkCBEjNbMbAQoKArbQnsFkoCRCNBsKRKgnGmeNSbjCGEneJgESiAeAmBnIeKbSReRMNqQkAKgGBeiEceNfMOkMiSRbOGEKJQcKrFKeEaFqkMcOOkSCgKFqeEkbF"},
    {HX_TILES, 0, 5, 13, 50, /* 8: 0.75 */
     "..ooo..|.oSSo..|.oo.oo.|.oSSo..|..ooo..", "AjsAjsEcEc",
     "GbFkIgNRoSgOgnKmGSGIbRJejJrKMAOKrImNEGbeBsRSJRnqOEInBArEsScrCAsJecNOfkCqRkrQIsmFcEsRGbGbiNfSnCgAoBCsEJfMMSkbNaKmJgScNIOcOgaNecAmIsEEsEsBKcqABIg"},
    {HX_TILES, 0, 4, 26, 80, /* 9: 0.75 */
     "..oSo..|.oSSo..|..ooo..", "EkmBmaBma",
     "NKJoCkIEiGmFkEcMFOgjOfEGaNANMFkGkMknGbjIgGAEcIJEncFFAmBgcAnEkBOcoKNjNEjnFcAJGiFEJcnKJGnKmGanGBOenFJeAGOjJOGnGaoKnJfOAncBfJGkEaBmEkKCoMC"},
    {HX_TILES, 0, 5, 17, 60, /* 10: 0.62 */
     "..oSo..|.oooo..|.oo.oo.|.oooo..|..SoS..", "GmjRR",
     "GcNSKBgOjFnrIosBebKSSOJqkOeGCKcOfOCeOkOKQFnRGjInSibOcQAFFngSaGIMGRbOgJoCEqJSiAgEoAKSfRjGqmQkcRRiGOgcObJEjSjeAjNcmQiFINKcOGOkNrgMKqRgoQBB"},
    {HX_TILES, 1, 4, 20, 80, /* 11: 0.62 */
     "..oSo..|.oSSo..|..SoS..", "GmOcoOcoMM",
     "EcKBMGOjbJfEbGFoMbnCoJoNfNGcCiGoiJcEbeNjOfJKnKMiNOgcAoJbEoJeKCIGoIKGkMfNgBjFAjCfBfbKaIoBBjcOfbBmJGCOgEIAjGnNbgEBenOcIKokOIcoGoFECjOBiIMGICKf"},
    {HX_TILES, 1, 5, 19, 60, /* 12: 0.75 */
     "..ooo..|.oooo..|..ooo..|.oooo..", "",
     "FnCJrOFSkGEoCorJGsNbsOsJOSkSfqFmBGGqQKMfsJQCiQJbCfSjKnKmSQQRfNqKKNQoKnNJBCOFrMOaBfGAqeBrnMcMaSiBgRinSCoMBkErIGcRGqbIrkRbgESojJfkAKoGJcEC"},
    {HX_TILES, 1, 4, 28, 70, /* 13: 0.81 */
     "..ooo..|.oooo..|..ooo..", "",
     "NECnMOaEJKKOCFBkCoGoGFNfMjIgMBFCoKbCigJeOEGbOeFogMcNNfAgOeFbBAgCBekCCAKGcoJOfBoBIcNciGICCNKbAOkJeNGBnCGnNckACocFEcCkMkMCfAkoCnNiaNc"},
    {HX_TILES, 0, 5, 19, 50, /* 14: 0.62 */
     "..oSo..|.oooo..|.oo.oo.|.oooo..|..ooo..", "G",
     "OIocENbOKbGsNJBgjRGnEaKImCOgJreQFGnBJgCmbMSfBCGGArEbBrIBGIBQcSjIJsfEoQncSRSRbnFoIcGGsFsBqRKqRAsgSRcQiMsFQjGIaNCMOkaMioGaKOfaNrNACNjQn"},
    {HX_TILES, 1, 4, 25, 90, /* 15: 0.50 */
     "..ooo..|.oSSo..|..ooo..", "KK",
     "IJeNkGCeFjmJoFmkCCeKcFoCmcNjBfOICJbGoGjJJJnjFKMjgCJfNkGFoCJmFKaNNgcFnMbGiONEKOfoNkeKcGoNKObEoCJOBioGjNkOfbMNIAiBjFbkNEAkGNNfnGCMiAmfGGFkBo"},
    {HX_TILES, 3, 5, 23, 70, /* 16: 0.62 */
     "...o...|..oo...|..ooo..|.SooS..|..ooo..|..oo...|...o...", "SfSf",
     "KbKfMkCABnKsQMOAsgNCKasOfsNicBniORgrJEnBqOBgKRCBrKrSIgBnCmiENQkrCiOiOiOqRoEJEKnEkCoNbFcJCnOrKQNcJcEjNsSanImGaOqGCCFIQeCkKmgFojQjKGbESCOSJ"},
    {HX_TILES, 2, 5, 28, 80, /* 17: 0.69 */
     "..ooo..|.oooo..|..ooo..|.oooo..", "",
     "NOCnRkJfFsBIGqKfGSSjRaGKfQcQgJFaFjFFGoKgcQjbAobCKMkoKNeBBrFRkNBNgOscFGAgNgKGIaSjASFBkrSKEnSSIcQQEEBNbOcRoIBOcBkAJqkCsgKgCjgGnfORNR"},
    {HX_TILES, 1, 5, 28, 90, /* 18: 0.69 */
     "...o...|..SS...|..SoS..|.oSSo..|..ooo..|..oo...|...o...", "CqjCqjCfCfGG",
     "RBeRjrNrKajIcGQcACNarBGCrmAmFcCMgiROSoJBqGKIbOOejFbIgArMcMekKrNbQaICSbCiKbrScjESiQaBGsEobJMSgbRgAFObFJEKAoEsNscJsEoSRcoNEKnjGoKqGakOFkKJnGsMFFa"},
    {HX_TILES, 0, 5, 20, 40, /* 19: 0.50 */
     "..ooo..|.oooo..|.oo.oo.|.SooS..|..SoS..", "RRKaKa",
     "NcEreRkoObMANfJJBekQEoaNiAOIOcCoiQieKcKNNSaFBJSOKrBfMaMrRoJBAoqQOfBgaObnFifKfbFNgGaBqFJfcNIFsCerGjORojKmAGNeGosAfIMcOkRaOAeGBJbgRBOeArKqOcrBrQCk"},
    {HX_TILES, 3, 5, 28, 70, /* 20: 0.62 */
     "...o...|..oo...|..ooo..|.SooS..|..ooo..|..SS...|...o...", "GGCC",
     "CAIsCNaGaqMgqCfKOsRkgOFEnEaOSFaJKISNOMSKONJqJcEsQBeEEROMbNOcQSCIaJqGoBrBFkBnjOgaSkKQKgKaQcArjFsSoJIaCfKrmBkSbAfiKKgFrcEniSCiJoqFoASbFJo"},
    {HX_TILES, 4, 5, 22, 50, /* 21: 0.50 */
     "..ooo..|.oooo..|.oo.oo.|.oooo..|..ooo..", "",
     "IcBeJNsKSSaeGCKeCEOrGoNrCsSEkqNIqeMqFSgFQRogMFGoRmMGSnSemFoGIOfJGmrJJeNCiSckKFkOFIqMCrOjRIsKAKEJnNQbQgqFFokRKqEqCnKofMOAgMkRoNEEFogO"},
    {HX_TILES, 3, 5, 30, 90, /* 22: 0.69 */
     "...o...|..oo...|..SSS..|.oooo..|..ooo..|..oo...|...S...", "IOsIGb",
     "CnMNcOOGnBIqmOfrEOSGmCRnGsbSfJmfRAGIbOfjJGFsOkAfbQKCnIcjRkMAkRSGcGOfKRjSEmCjoJnEEsRnBSGsGSBJoFAGjGiNfJgRIfMOGqGrEnEkGceSorEiMBiOgEkoAeqNQc"},
    {HX_TILES, 1, 5, 26, 80, /* 23: 0.50 */
     "..SSS..|.SooS..|..ooo..|.oooo..", "SIsSBB",
     "GKRkBBsNsCSmKbGkJsBmGRCGQkbINKbBgGRaIAoSQkIGsNsIsjJrGAmsIRQgBfrSicOkRJcCfAocNgCKQbRCkmQcFirSRNskSmCoCjCOqEmBFiCnMkSeReKgsQJsMCgKECOeSoaGGkO"},
    {HX_TILES, 4, 5, 28, 80, /* 24: 0.50 */
     "..ooo..|.oooo..|.oS.So.|.oSSo..|..ooo..", "SSQcQc",
     "AsCGrBMiMiFCeJOrfNsKbMjCgSbQBeJmjCFKKbOOfrSBjFJeFcJSbEiScGOigGsOaRaGoaCnsOqoKQkgNKcKbOimKOEGaCoSArFEInGnQbqOqAiCRCFGkFEiGaAjfBrOAKFMgABgiCK"},
    {HX_TILES, 3, 5, 30, 80, /* 25: 0.50 */
     "..SoS..|.oSSo..|..ooo..|.oooo..", "BBCeCe",
     "NaMcEKbAQEmFIBfSbAgGiSAniBAAAKBCIcSEoNsgNOGsjOgoSjIERoJQCFSnEmFCGaOrjFONbeKoKncOfICgOBIoJRCkSBSeKBrkRoAroEicKNIIgqAiINsFcOeKSjNfJB"},
    {HX_TILES, 2, 4, 26, 100, /* 26: 0.94 */
     "..ooo..|.oooo..|..oSo..|.oooo..", "Nf",
     "JaCoMfJgiGiNiFibBFkoCncKCAECFCNanFjBnkCmOgEobGkCfAmFoKfcJmCnbKOecCGGbMgFkeBkKoNAFjOekCjfEKBeKoJbfKOcfMNNKaiFoiFMfOBNeKIgbNcfNfGnIcCnkIJmcNcoMfOBjKamJcOaKMcO"},
    {HX_TILES, 2, 4, 30, 120, /* 27: 1.00 */
     "...o...|..oo...|..SoS..|.oooo..|..ooo..|..SS...|...S...", "BnBnOamOamJn",
     "KoEoFbFanBAFjCnONkcNaNCgCMNcCnjFceIOfbOfGBoFjBgoCJGjCoIOicJoiENkJcFokOejKoBIeACgEEIOgJcBOgbNbgCnjEafNbEmOgEkOKaCnJoAiMeJBkgAifOkMcFkgBJeEGCgJCmFOeNJK"},
    {HX_TILES, 0, 5, 15, 60, /* 28: 0.75 */
     "..SoS..|.oSSo..|.oo.oo.|.oSSo..|..ooo..", "NNGGKrKr",
     "AsJsBKBjOscIcsOqAKmRGRckGKsbNOEnScSmjCmqEBImFGrCRCmSbjRRMcJaGEiAkgSRcgMJKeEsIeaMKKsaEKcCIrJsCGMqMjNrFRBsOgGNfMsSjGoGQjsRaQFoNCORcsSQJgKeBgb"},
    {HX_TILES, 1, 5, 22, 80, /* 29: 0.75 */
     "...o...|..oo...|..ooo..|.oooo..|..oSo..|..oo...|...o...", "Ks",
     "RGcnCjaGsIfGbkOSFGsoCNsOjsGBRSoNGiSMFaCIFniCGmRnOfaFSBfEoRBsNsgJoqKroFObSJMeGjbEjAfQiBGMJOkIoCkMkImASokGJgCeMrBgrQcGsfJOckBECrKMEbNimFsMBjCkEsGjm"},
    {HX_TILES, 0, 5, 31, 120, /* 30: 0.88 */
     "..ooo..|.oooo..|.SoooS.|.SooS..|..SoS..", "ObObQfQfNN",
     "MbFIMrKRnQQQerCeoRisKqRoGOKSGkSNSfaRCnbOcCrCJoEoFSnAgGckKSmGigNcrQckIsMMfiOOfNGJfjOqGJONfKOSNGncOIISiOARRcEkmSfIKmJBoFaFEsARcFOAJRBf"},
    {HX_TILES, 0, 5, 28, 80, /* 31: 0.88 */
     "...o...|..SS...|..ooo..|.oooo..|..ooo..|..SS...|...o...", "CfaCfaOqOq",
     "RoCseAjBRisEcOeOcKenMgGSkKFSROACOONjfNeKJARgaCEmSgGsGOrOfGOKMCkmJcnEsSInKcQOrkCiqFjgBgbNgBFOfnBsMSejNQFnRcoRBsRFksCCmbRbRgEjoKgrAgGNeQRMMeSasGEj"},
    {HX_COLOUR, 2, 4, 40, 40, /* 32: 0.88 */
     "..SoS..|.oSSo..|..ooo..|.SooS..", "GoGoMgMgAioAio",
     "NkeFbGjJNGjBkOgcKgCnNImAgGnMkoCNfIMgBCkFmfFcBoEGMbEnJcOgICmGoKNjFjKngBiOkAgBkGjJgnNjOOaNMicAgJKOGGmMcJNKngAfKaiMbJfJoOkbFcGOMgoBAoGiFKNejAFCFjcOKcf"},
    {HX_COLOUR, 1, 5, 38, 40, /* 33: 0.81 */
     "..SoS..|.oSSo..|.ooooo.|.oooo..|..ooo..", "FFSaSa",
     "RcCBKcCOcSoNfOgbOIbQbGjgGcBiaNjaMAjnMqkACgaCGoFEsBmaRgKfkFogImiCkCGkIGkOQfSfMqKaSkGQoBSNkJOKOkFsKJOANIeoQmrOFkEOasNgaQOMfSOCesGbNkgEMrbGIrIIGEIm"},
    {HX_TILES, 1, 5, 38, 140, /* 34: 0.88 */
     "..oSo..|.oSSo..|..ooo..|.oooo..|..ooo..|.oooo..|..ooo..", "EoQbmQbm",
     "RKRmQkEmEqORKcsCqGksCQnAjeNbgRcrECqGjEofRgBgCBskQjOgKEMMcoFOFnSRJeKOGcJrJSofJcNArSOsFkbOsjCEjNKrINgBGsEbnCiFkABJfsSAOOfAqSjJJNqObOjJAQOeGrC"},
    {HX_TILES, 1, 5, 31, 110, /* 35: 0.75 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|..ooo..", "RR",
     "OMQoFjNciIEqKoGOscOibRbjKsMrnOagOECoKaOeaCrSFjeFrIgCIriJBosNbGsmOiCFjSjGAREsAosMcSaKnQgqGoCrcGABkSfOsOgRnOrMGbBGMiEGsFRibSmJoJgoGCAinIgScInCmQgrENOsKgNsQc"},
    {HX_TILES, 2, 5, 34, 90, /* 36: 0.81 */
     "...o...|..oo...|..oSo..|.oooo..|..ooo..|..oo...|...o...", "Bk",
     "QSeJmRQgFrSErERFkFSRIcRNcgAGkFqcCjJcNraOBjnSaNInjFjRbnOCFGKOrcCfAJSKoIgJsKbfFkQjJIbRfqScsNiNJEKejJbBjBJJaoRSofSfSmkScCeREQGbeSeCfOrnJbfEaCCOcSnM"},
    {HX_COLOUR, 3, 5, 40, 40, /* 37: 0.81 */
     "..SSS..|.oooo..|..ooo..|.oooo..|..ooo..|.oSSo..|..ooo..", "RoaSRoaOO",
     "CfNQknOsGGBGkcQJKFMkFrFGokCQGIcCBgkNGjGkNrnGkGroNBMsInNCBGReGkObKOCfGnAkBoaCNiEaAfnIaBosBRQkFkEaFaEcSIsgBARCfBgkAoAsGSKbQjEasFNJnSncKEcNE"},
    {HX_TILES, 2, 5, 29, 90, /* 38: 0.69 */
     "...o...|..oo...|..SoS..|.oSSo..|..ooo..|..oo...|...o...", "NNSS",
     "MKJnRoQKgrAGOjmFKbRcOQkgNFcnAeCogGscKfNOJKobKsEbBfaKSIcKFrnQceKbFNeAgFQMfOscOjIbmQkfGbKrOJnNcBnKOMQbiEoNfkQbJbAgEIoABEcCqIgQcmRciNsScGNcSCfGoCRNaRmgEj"},
    {HX_TILES, 1, 5, 17, 60, /* 39: 0.62 */
     "..oSo..|.SooS..|.oS.So.|.oooo..|..ooo..", "OBfkBfkSS",
     "JOqBgBoMbCBkIaqJJnCsRfkGrKosJogQaRfQiCjEFRCOfkInRcNSOMCrkEQRfbQCfNKsnMiCsgBORjAEJGcjArOCerGKmcKSNGsAgNfSFnEBsGcIGigRcKrFRIMJnNkCgKsiOeEaqGqeIobM"},
    {HX_TILES, 0, 5, 37, 120, /* 40: 0.62 */
     "..ooo..|.oooo..|..SoS..|.oSSo..|..ooo..|.oooo..|..oSo..", "CsCsRRJq",
     "GSeCAAfsKrCSRoeMsAsiBgFAKscAkgBQiNriEaMRjQenIaGaCkJIqObJCFkCANiEOisQjAFnFmBGCjJQkNfNcGcJGbNKaKsFrMjGkSGkgCsgEjEAArGnGAgnBrJRorNkNQoAsoImqISEkgBAj"},
    {HX_TILES, 3, 5, 35, 110, /* 41: 0.75 */
     "..ooo..|.oooo..|.SoooS.|.oooo..|..ooo..", "GjGj",
     "BirKfQaQCSeKqgEJfGqRoBsMIEJfNAiKcGsOjcRiOreQGBorFkeBqNbJGNBIoAnjAgcKAKbfRnOJKeaBmNeOjaOgQEboQANbQiEFbnGmBsfMJENFnNGmjJNcsRGEbrAiBBQoOSBkSKoKeCj"},
    {HX_TILES, 3, 5, 32, 100, /* 42: 0.62 */
     "...o...|..oo...|..oSo..|.oooo..|..ooo..|..oo...|...S...", "OjrS",
     "KfBncJOcGcEnFnCABgnORSaSmArSSOskRCnGiBKnFJSncJmfGoBBsGnbGcCIErjCsbJGkCsQgCgSIOjcRkSnAojCSGSbOISbSocCSgbMsiKmNanBiQbMRfsNFQnCQJcqCkNrGKrbOfcSoAAJrM"},
    {HX_COLOUR, 4, 5, 38, 30, /* 43: 0.69 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "SkcAKrjOfSSgbRoJEsjKbFnfJcKqGbObJKCnRgnBgNKKScQiQcGsCrcOCOCkBoFqGBfCnbSGbrIsfGqMgCjbKFFcsBiRkRcoQjGsGoJAKnNsFmQScsBkJoJBSeOckCSSJokEbmKmMOjAsGJNjaQgCIc"},
    {HX_COLOUR, 2, 5, 29, 30, /* 44: 0.50 */
     "..ooo..|.oooo..|..ooo..|.oSSo..", "NN",
     "QGFnsKCksOQbKcoEGnFsSgkFKKSnOgBgISmGKbKbOOkNcORkJCiEsIaJsOBBKOaeJEajIaOqkJnGQQgcJeoKeBSKgjNebOafSnQMQoJGkFoEJCNRoRorKCoInJroNcCfCJrGjGRCjKbB"},
    {HX_TILES, 4, 5, 31, 120, /* 45: 0.56 */
     "...o...|..oo...|..ooo..|.oSSo..|..ooo..|..oo...|...o...", "GaoGao",
     "GskGrOjFbfCeJSoCfqOrQFnFnBkOrcNebFCOsoRBiOCgObEFJNejCfcEkgSkqQcKSkrOFokOOObgJJGJJKsoFSbsRbESEmgGGqMFjBeoErFBOqNKcGaFcRekMcJReJOqkJcSJAiaFkEqcOkNCIosBrn"},
    {HX_TILES, 0, 5, 36, 100, /* 46: 0.62 */
     "..ooo..|.oooo..|..ooo..|.oooo..", "",
     "QnAIKsSBgQJsGMskGNMqnIEGqRamIOknSAgRcJSaqQkgRcNsNIbrBqKQJfRaGsSSkSkbIfKERFkaGKaFQeIrjGnqGofOsmQRnAeoKsbNcQQmQgNSknFasKKOcBfIniOJqoOAJIgCoGBAMkFO"},
    {HX_TILES, 0, 5, 38, 100, /* 47: 0.56 */
     "...S...|..oo...|..ooo..|.SooS..|..ooo..|..oo...|...o...", "KnII",
     "MCRbRCnsIgJNCOGNcKNfGnjAkJJgBfAEnEqaNeScfFMfMCsQQFQAMjFMQARIsCFKnQeBSoRcCioAOEjNCmaQjOBJoJJJnjEbSkGjcKmEsRGkQkSeNOCroCIgRoiAIfCIsI"},
    {HX_TILES, 2, 5, 39, 150, /* 48: 0.62 */
     "..ooo..|.oSSo..|.oo.oo.|.oooo..|..ooo..", "SjSj",
     "OfcCoGojSbkRfBorAfbCGOSbOkFBkInGKCmCfKFncFFKsjFKOeBjSCBNjOMjNkfRkrJGKQiMiRbCkbOcfFrQaKRObBerGAoCmjFKosEoCmrMkbIJEmjCsErGorEkSniNbFOcfFAFmgFsICiRKJrnCN"},
    {HX_TILES, 1, 5, 37, 120, /* 49: 0.56 */
     "..SoS..|.SooS..|..ooo..|.oSSo..", "ScScSkSkEE",
     "NICGqMsBkSEafIGnjOeNKmSCoMkFmeMgjFcSJFcrKgaIFsRAGjNJcJgEisGcNJeSONCNbnEoiJsBAkGMaGaNgCnGrSgSoEKcsKoMiGokJrfAsGkEcKcsJOjFBqBCkGRasMKmCnIgICOsGcCOrb"},
    {HX_COLOUR, 3, 5, 40, 30, /* 50: 0.56 */
     "..ooo..|.oooo..|.SoooS.|.oooo..|..ooo..", "SgnSgn",
     "SNjBgmIrOqoNebCeRGiRnKKBeGjInsBKqmFnNjcGRkJEosQkoSoKgjCkfBfSiECkNcIoRAArjRbCsCkmBrCmsMsSoQmsBgOOinCGOkrGQOkAosEsgNaiJmIBncBmSGKnkCsIaQAfCmSAoOcMMgbGnsBFiSaKgaIaOr"},
    {HX_TILES, 2, 5, 25, 80, /* 51: 0.88 */
     "..S#S..|.oooo..|..ooo..|.oSSo..|..ooo..|.oooo..|..ooo..", "KcKcCjgCjg",
     "OsKOcRbjAFqMqmGogMCneNEiKKFaFsgMCqbGoKfCjOckOFEcIqAsjKgKnGoqSjcCoFbsRobGqSjEqCmRaCmqOjAkBiAkSjCrOscQnNgIqeKgOeIbBseKoOiNgFaGcCkOeEKqIbASFsGrmCJaeNasKQgMcIgFGRbjEjQk"},
    {HX_TILES, 1, 5, 32, 80, /* 52: 0.88 */
     "..ooo..|.o##o..|.oo#oo.|.oooo..|.SoooS.|.oooo..|..ooo..", "GjGj",
     "KoBMroECrNsCjoSjfCgoACgOgaCBrGkCnfGcfONJsIqBrmFOekQbiJBerIoiOqMcGOSGScCCgsScJKEoGKQgoGmBocCFqSanInJqECNkgAfkJoASoqOgkFqBsnIrRmRkEcEnqSgaCQnsSmBGKakBeBNRmqGrSGo"},
    {HX_TILES, 1, 5, 29, 110, /* 53: 0.81 */
     "..ooo..|.#oo#..|..ooo..|.oooo..|..ooo..|.oooo..|..ooo..", "",
     "CgNRaQiGKcSkbGjBCsmJrMMrEQcJbiNfEknOjGnqOrMjbSkcCgcSCGIsmKBoBirFnOkIoFScCmbMCfCgQokSJEsGsSfIOMBsMskCsGsgAfOqQceMCfGFCKbrEAgoOcFmGqiMkbEosRnCiNEMsGkOgmJEjKBke"},
    {HX_TILES, 0, 5, 36, 120, /* 54: 0.88 */
     ".#ooo#.|.oSSo..|.o#o#o.|.oooo..|.oSoSo.|.oooo..|.ooooo.", "BieBieOcOc",
     "NkINjRnRnrQCsnCoAqCsaMSaJcSGGmJaMJciOiErAKsnOjMQObqAqmBioRaNcGmKnjSJORneMcSjEksCjCnJcOsGNbQEaeKqOgsGBenNCrjCkcSirOeRbmGOSgMsoBGjJfkJgMicMcNcScKAoiKCGcFinEroJnNM"},
    {HX_TILES, 1, 5, 35, 110, /* 55: 0.88 */
     ".SoooS.|oooooo.|.oSoSo.|o#oo#o.|.oo#oo.", "FnFnKK",
     "GcjGoBnSgbCOBncBNkKMqnEmASbgKgIFroRJmSAOsFcoOsKsjFnFoKaGagEkaGiEnFGjaQnRfoNcMRSMqGsGFkAROqcJBkOrMckEbSnCqiMboAeRbsBmEmaScKnsRKGEiBiICjnKSeCkRocKfBCgkKqkCkFqKsmRg"},
    {HX_TILES, 2, 5, 34, 90, /* 56: 0.81 */
     "..ooo..|.oooo..|.ooooo.|.#oo#..|.ooooo.|.#oo#..|..#o#..", "",
     "IbgJeiAqfEmJNsOcMKGMknBkBkFkQjJAsBsQfRgFGsBnfQgaKcAkmEArBGoErRfFnNCeCfcJSFkoEImjEcjMMcSmaOgnQcJaNJKgGiGnbENeSCNKIbkCNcrKKseRbrMGkSfJbgMaqNIqgKbeBKbKIRciKcIbk"},
    {HX_TILES, 4, 5, 34, 130, /* 57: 0.81 */
     ".o#o#o.|oooooo.|.oSoSo.|#SooS#.|.ooooo.", "SfoSfoKK",
     "CCjGaNKnsReRkKrOOkcSOEoCmcCskSEsOGjOqkQnSfcJbmInqCCjAnsCmaBscGKmAiSocNMibGArmOrBkSbeAfEbNFneNcjBgCrEqgSoKnbKaRisJKeCoeSeaBobGkMiRocCOrgKbErcJaCRKbBmKCsfEkReEBIrSCkCjnQb"},
    {HX_TILES, 0, 5, 31, 90, /* 58: 0.69 */
     ".ooooo.|.#SS#..|.ooooo.|.oooo..|.o#o#o.|.oooo..|.ooooo.", "EE",
     "KfKqkEbgMcKGnFbjBnRmRnjBFISkImSjsNjgNRnGGrRSCojSSaSIfEifIoAkrBRKaFqiOFkCCqCIBkOgOqFkFjSKgKbAnEqBJAgEoMkIsBrKFsQQSioSfBSnOgSOfGaNeFcSMsoFosJrnMsRjInJr"},
    {HX_TILES, 1, 5, 26, 90, /* 59: 0.50 */
     "..ooo..|.#oo#..|.ooSoo.|.SooS..|..#o#..", "QeGcGc",
     "JgoJFkoBEGrGroRKQcsKorGnbKqoKnFifIOkRCoAioBgCkGOiOcInCqoOQcOqjRSbQAqIgCrNAmQEjNcrOSNsaNKmNgqEsfKcsFcNsbBoeJrNkOjoJFBnrSaGJESSOfsGcOKqbRosRmEiOBrJgrBrgImsQbFkgIeB"},
    {HX_TILES, 0, 5, 45, 160, /* 60: 0.75 */
     ".oSoSo.|oooooo.|.ooooo.|oooooo.|.#o#o#.", "OgoOgo",
     "KErSMksGjScFjKIGmGnQkGjKgJamRjJcMcIciEjQeRFcGnQkEsABmFoGbiMfMqJFsRgbKfGkFbOagSkQOkSgFrIrfGsQoEakQeInJCjnJrKJOCIoAOgOfScFroOcSjaRcrMemRboCFmfSMcsNBeiIbkKGbRfkREj"},
    {HX_TILES, 3, 5, 26, 80, /* 61: 0.56 */
     "..ooo..|.oooo..|..ooo..|.oooo..|..S#S..|.o##o..|..#o#..", "BB",
     "NroNscFkrIMjeGnbAOaeCjGbjGQAebAgNfkAgOjrCiKreGagIQSgsSgKfnJnrKmjKeNbqKamFkSfBsCkKSoOSkqNgBnsKJgGcoGcFoQOfbNGSQiARkoJImfNgjABinOaCkfBkEkEkJqAFIoIqoEksNiGmJcoAGogJmInRnrOaoCifFoj"},
    {HX_TILES, 0, 5, 24, 80, /* 62: 0.44 */
     ".#ooo#.|.o##o..|.oS#So.|.o##o..|.ooooo.|.oooo..|.ooooo.", "II",
     "GcgSASRJcsIgOfJsnSFsjNqIbEbIanSbNcIsoJfbOcBgMrcNsjGnAoKgaQkFnSgSbJeGmJBerBsnBGoGcgJcBrQSBnkJaGBniGaKbjSgCeSkrGoeSbKanKcrMRaJScQkrEjgJoIQncKSkEsBkgRfMcEcKBQbiRQRmFFrAk"},
    {HX_TILES, 2, 5, 37, 110, /* 63: 0.69 */
     "..ooo..|.#oo#..|..#o#..|.oooo..|..ooo..|.oooo..|..ooo..", "",
     "OgJJeKqjCFCjGbeFagNrOQfGCGsfNgqAkKnbKRJaKCieSgbKsiOOgGorFAmkGQaoEmFmbOQiGsKoSejKscMfIrKfOjsOeKJfbKcNgCfCiGkQjOBRSMqmRnRmrJCnkCeAnkSKbSCqgGiFrGaSaBIGBnRgcJJfoJo"},
    {HX_TILES, 0, 5, 35, 120, /* 64: 0.69 */
     "..SoS..|.o##o..|.o#o#o.|.oSSo..|.ooooo.|.oooo..|..#o#..", "NqbNqbBoBo",
     "QcmCjoEoOejFmgKbJbNgCogGoMrfScKsmMeoCBfoCgsGrmFBfQeKmOCMgkKSOMarRQfMErkSNeGSBoFnAKnMQbgEcMQnaCsBqnRAsOasKnQkEknQcCgOkGkMeqKbkIcrGSbgNIbOaKIbGRgCniIGIOgJfKmgSgJNc"},
    {HX_TILES, 1, 5, 25, 80, /* 65: 0.44 */
     "..ooo..|.#oo#..|..ooo..|.#SS#..|..o#o..|.oooo..|..ooo..", "JJ",
     "IFjCrgMcGkKIgERgiGOBgaOkCkCkoIMcNQKoJsfORaCgRbFbkCAsQcGjFKgiBJOIsKokJqCkqBFsJcmFobBiImRfsMkeRRogGsiQOjrJIskJcAmGQoIocOsoBgcFcoFoKnSiCeoOeJemGcNOBgGQAgcFqRNsfOqbA"},
    {HX_TILES, 2, 5, 41, 110, /* 66: 0.69 */
     ".SoooS.|o#oo#o.|.ooooo.|#oooo#.|.SoooS.", "NNRmRm",
     "KACknEJOrKfAGosKenFanEOfJJBnSfCjcBmkMQeaOjOSCjgIeCsCkMSksQaARngFbJnKqbBksJSbiMfnEJobAAJgImcKJKbrFSaCQgiOCqFncNEcSOqgIoSibRFinGknNfiEjrRkGArgCjaKOsAkOSkBKGM"},
    {HX_TILES, 3, 5, 44, 160, /* 67: 0.69 */
     ".oo#oo.|.oSSo..|.ooooo.|.oooo..|.oo#oo.|.#oo#..|.ooooo.", "FjFj",
     "OkSoBoCfaEoMfOfsBqkGFJJFOkfOGaAnjEKqBogEcIQGFjGsgFrnGoBCRkcEbEnOaAgJcSgoFrbSgGCrGKmcSasKmeBifNBiCkNbGamBGnCeCnIobQjQbrAqIngBgQjNgjJgsBrSnjQgrSFmIgIsoMbnNsKJeANSeGcoAgo"},
    {HX_TILES, 0, 5, 43, 150, /* 68: 0.69 */
     "..ooo..|.oooo..|.ooooo.|.SooS..|.S#o#S.|.oooo..|..#o#..", "RbRbFF",
     "BfkMKsjNbjKKNqGcoGbEbMIcKrfQSgmGkgFocJnMgiMjcAmbOjAsEanJejICKoAfFjObNsCkREkfBIfQGciBNcoOsKrBfbGaSgEiRcFRoMcSOeoJRnCrFnBsjKoRokJmCgFbjGkKcnQOJIraBgAjaOQgRnSKKaACN"},
    {HX_TILES, 4, 5, 34, 90, /* 69: 0.50 */
     ".#SoS#.|oo##oo.|.ooooo.|oooooo.|.ooooo.", "FmrFmr",
     "OgnSicCfFrKEoMkCkJscMEmAAogQkFskCRmQnCrKfqIfnRcABmGjsJnsBsImSQSciFrkOgnFoCoCgMqfQcMQnIsQbCsBnJqeJseRIfJOIrSigKfjCskGcSAFsaQIOkREjnSAQmjAgbKrJcAScSfNGSObmRfMgGiMe"},
    {HX_TILES, 3, 5, 43, 130, /* 70: 0.62 */
     "..#o#..|.SooS..|.#ooo#.|.oooo..|.oo#oo.|.oooo..|..ooo..", "BoBo",
     "GNrMrNsAnSfCkNkAJgKNjFoOsMJGMkfKeGciRicECrjBiaAGkJqRjqOgiFAmQbRFrBnNkIcOrbFOAiNcoGNgSfCMioCqbIGbfEbGFjaCiBfGnsBqCMsIaRobJGarSkgFcoQANqQkRfbBsNOAnBmNMrMIrkFos"},
    {HX_TILES, 2, 5, 40, 150, /* 71: 0.50 */
     "..ooo..|.#oo#..|..SoS..|.oSSo..|..ooo..|.o##o..|..ooo..", "SjSjBgBg",
     "KMrmAgbErQGrSgJqgJqCnMKQJRRkAgJrKEqoJcjJOrfBKckCsFmfEnFiRjSkoGGKeNAgRnOebBoSfsAiQkRmgNkoKcCGBJoORmkObnEmOgqGkbSCkGasSgsOfnQnaRSGoOMAgAorBsOkAosGkgRfAsSmkEkgJbfBmiQF"},
    {HX_TILES, 4, 5, 43, 160, /* 72: 0.50 */
     "..ooo..|.oooo..|.ooSoo.|.oooo..|..#o#..", "Aj",
     "CgGsaSCsbFGjsGJrJSkMqQBsgECEbkMqKrJNsnOrMjGcmIaFbgSicOicReGrgEkIakOCkoINcOrkNigOfCngSoFIsKOcMjOiRkeCqkSkBiRieKrQoSNGQkCkNOfcGaNAIrfSgiFIMKgiEAfiFmsGrNiAQABkoBQc"},
    {HX_TILES, 0, 5, 40, 110, /* 73: 0.56 */
     ".o#o#o.|o#SS#o.|.ooooo.|oooooo.|.o#o#o.", "FojFoj",
     "OkIRQciErcKecRcfKQmEAgBkcSeREnaAiMgRJcIIFIfSgObCrBfoQgSoRoJCMQbFcqRgiKeSkoMrFcjEqgJrKnSKsgGCsGiCIqBgBiNgBGoCocSeKCCjFMOBjFMkCJSmSkoOAfJeCnaQoBoOsfSgnKrS"},
    {HX_TILES, 1, 5, 43, 130, /* 74: 0.44 */
     "..###..|.o##o..|..S#S..|.oooo..|..ooo..|.oooo..|..ooo..", "JbnJbn",
     "JNeIIoROcoMJeMrBiGjCibKmFIQmSCrnGsFcGKAOeIaInMQfjRGaFoKEcfCBsCeJnqRcfFcQReQoNGBmFAjQcNrcFcAokBifGMsaCqMcNsgFsgOfbIgGOgaOBfoBmqSjnOkKgGJrbSkeSaCmRfFiGjNeGqSOj"},
    {HX_TILES, 0, 5, 43, 140, /* 75: 0.50 */
     ".ooooo.|.oooo..|.o#o#o.|.#oo#..|.ooooo.|.SooS..|.ooSoo.", "OOO",
     "GcSkbGEIoMkcScJbQgOsiQnrEcJnqOfRgcGorGmRReKMfrFjEjJqIoOafEjIcmAgBejGFrQmKGiROsAGbqNsoQkfFSaRgmMgQfaQknAoCoOaRIEJJnrBsMRRfCSoCGmaGjEEkSgCKcJcGckEFsgGSoNsBgQoCimS"},
    {HX_TILES, 4, 5, 32, 150, /* 76: 1.00 */
     ".ooooo.|.oooo..|.S4o4S.|.oooo..|.2ooo2.|.oooo..|.ooSoo.", "JcJcEns",
     "NsOboCrcAmiSkOrSfBmCmbSboOaEskBOkaGoSoRciNCfoJgEsmQjGboARoOcsCnFkFsaSeOgsCfFnEmEsgFjQAQbIbIKeKbsOrjEObjNqGrIRBkNgbQIbfFkMqFsGFOfRMsJqJebOfBfqFmEKsOfJqGniFbOgkKeOfBgJRaoCn"},
    {HX_TILES, 0, 5, 28, 100, /* 77: 0.88 */
     ".4ooo4.|ooSSoo.|.oo1oo.|#SooS#.|.ooooo.", "SnSnGG",
     "EanAokRBeiFnbFoKRmgAkNCsAjAorObeBfoOqmErRkFEmNciEkbJocNeiBGnCqcNfRSgCroIgFifFobEafFqbBKrJrGSeqEOrfQfbOkgRksSmKoGsoBoRoOrErKqGcKrbCogFnFAgAkfRcQIKmKbESosCrBfmIfqKBgqNsGSjCrScmFknJrc"},
    {HX_TILES, 2, 5, 28, 100, /* 78: 0.69 */
     "..5o5..|.o33o..|..ooo..|.SooS..|..ooo..|.oooo..|..SoS..", "SSJJ",
     "GJocMcKqaMjQbGimFnEarCsGaoSmBCJIqaFksCfCsNqkKcOiKeSRAkMkSCGrNjSmrAsAmNfbROgOrjSjNjAneSoJgGrfObjNaGOgAjNeJsCiJafMgbRGbjFnrIgEsjFbrBmMKQiKasAmbNfoJEoSfOgbOcGqAoCiMkEcoMgEoOb"},
    {HX_TILES, 1, 5, 30, 120, /* 79: 0.69 */
     ".So5oS.|oooooo.|.ooooo.|5#oo#5.|.oSoSo.", "BsBsGbGb",
     "GnkCNkJRcRbFkmArBosNcBgBAgQfKsaFjRbrBOFsKqASkFJNeKcGkrNgRjErnEqRnNsRmBsRcKmJeQjKfnAsSkfNeBjcAnrGneCgaCScSgGckIsiOqFnEbNjeMqkKqCraSmGseAoCIEcKcOAmInMjQcFskMrJIoaFsMiCekCoa"},
    {HX_TILES, 4, 5, 43, 160, /* 80: 0.81 */
     ".ooooo.|.o##o..|.ooooo.|.oSSo..|.#ooo#.|.oooo..|.3ooo3.", "NejNej",
     "RgsKmCorOcRagGsmKgIsaFnCkqRecScgQkGFboGsGboGmaKsSEiCorOsKFRBsQkSfmJeCQkaKgkNgIaEcRJocJCQgnObSaECknNRkbKbfAsaOgkCeKemORiFrKcMcSbICfBesSorSbKgSKMkcNceOfoOqeRjSfcQKeiNGJMaJsfCem"},
    {HX_TILES, 2, 5, 34, 120, /* 81: 0.56 */
     ".ooooo.|oooooo.|.#ooo#.|oo55oo.|.ooooo.", "",
     "NbGqGqbAfkGjKsSmKfcGarCsfOSSiqOrOqOfkKcjMeqOqKFRnOjQcmOCsfSOCgsIgQfoSioQogFbNaOqJoKfbCCgaMcBGiBsfJEnAosRaoCiFoOFasOcBFmcBBfBnFboGkRgmObnRfKrMaJSbEkCAQbeSFNjEOiGmeKKoIs"},
    {HX_TILES, 3, 5, 43, 170, /* 82: 0.75 */
     "..ooo..|.oooo..|.2ooo2.|.#SS#..|.#ooo#.|.SooS..|..ooo..", "GGAA",
     "OgKsKsBmSJskKoAieNsCoRenEGkNfNeqCkaBebRjNRfJFrGiMfQirIbmKgAsaKnBibSgQgAeIskMMNrgKnJNsEroJmsSkmOrBfKfaEokQkQGaSoNknMiIKKesKnbOaOeMjcOKqGGcsNKgrQjBqgSIriEaoEKakBiBrNJcqJqoJG"},
    {HX_TILES, 1, 6, 36, 100, /* 83: 0.69 */
     ".ooooo.|.#oo#..|.ooooo.|.1oo1..|.ooooo.|.oooo..|.o#o#o.", "",
     "WisUCowOsEWUgjJJwObKwaSoNfMjaIEJNwkSoBwCruQkCkNkSmFcrKWRRkvNwoCwrKOArREnRbSbKnsCjqOFaQfbEWSjuEwBIoNgAvNrBIvoRgjNfoIOriAoRgCWbvJrkUjeIsoASofIbiSaoCwIQvCrfUjbKwaJarNEC"},
    {HX_TILES, 0, 5, 37, 130, /* 84: 0.56 */
     "..ooo..|.oooo..|..o4o..|.oooo..|..ooo..|.oooo..|..#o#..", "",
     "CnSKKscFqKgKoBsRogSgoNqGJIrNgoSGaIqcGjQfkKbFjnKbBkSfSfNkIGkBJQcjGCNBsoJarGnkCknMbAmiNcGrGoAgArnInbSFcBoNgOqBmCqfQisFiFKsQeoQOqInGaKoOIeEbgBrCrkSJaGBsiObJqjKfKenFCiKnk"},
    {HX_TILES, 0, 6, 34, 100, /* 85: 0.50 */
     ".ooooo.|.oooo..|.ooooo.|.5oo5..|.ooSoo.|.oooo..|.o4o4o.", "C",
     "RjcVevUOquWgAuUnFWrGcFovBEbwGviMcwWmNvNkqGocKoEFNvNQBkOwRncWifCfkWoKuOkUoAjOSeqBuJcFNwBjwSkKeRoJokCEBMjbRgjNbMcMrOrcFjFwfWgsWkVgOivJWcoKuImfWmQeSisGCusKwnGrGuRafNRoMvMjcNWjo"},
    {HX_TILES, 2, 5, 35, 100, /* 86: 0.56 */
     "..o3o..|.oooo..|.o2o2o.|.o##o..|.oo#oo.|.oooo..|..ooo..", "",
     "GsjRgEkcNsjOCiFCmkIgINsKOcNfsQcGkKQiGcOjAKsCiCCsiMkQIgsSbfGaGcKKOkmBjKocScSGmcNjsMscRkNiqRimSkqFbsScjOgKskFscOQSgQjGFCQiFMbFGokRgbRisBnbBCkaSjBkOqfIeoKckCCofIeOqeNscEqIsiK"},
    {HX_TILES, 3, 5, 47, 160, /* 87: 0.75 */
     ".ooSoo.|.oooo..|.oo#oo.|.oooo..|.#4o4#.|.SooS..|.ooooo.", "FMcMc",
     "BCqOcnMRfsJgMIGFqCGkFjnIorNCgOfGmBmRjgKgqCIanKaEsmCjQofMQJFscBrGkfQBrgKrNcFFriIaGrCeCkaJnqEaCrJqfFcKbBfEasJcFneKosKnNrIqaKObFsJcKrfFRaQOiErSjBGcIRoMcNIBfsGNcqNgRj"},
    {HX_TILES, 0, 5, 29, 100, /* 88: 0.38 */
     "..ooo..|.SooS..|..ooo..|.S##S..|..1#1..|.oooo..|..ooo..", "CgCgCoCo",
     "QnOsgQfqFoiKrCSnMfkQCobOcgMAsBqKmgRGkCNCfnNACNcnRbFjgMSbrGifCmErjEKnsNiJqOfRbMfSiGqCeqJsQgCnsKcgGBgiMiQckOkNbSmNfBoQjEoqNAsFSjmQQkJQokNsfAofSgRFnaNcMsoNcKfEjgKCjGcoFrSeKg"},
    {HX_TILES, 3, 6, 36, 120, /* 89: 0.50 */
     ".5ooo5.|.oooo..|.ooooo.|.oooo..|.SoooS.|.S33S..|.oo#oo.", "AsAsKnvKnv",
     "MuKNcnNwNbSouVaGnkBqoKmMJuMrKrNjsGsgSeMcuJrCngSknRgwWsvWjeFrfGrnJQwfBKeoJBeUcjCuOjQwkFrjCeKGuAeoSowObNiSfMMumVEakQVbfFwMgGanCkuBfwUocRfMfFkGKciJqfOBvUceQkWqNsAwRboNsjAfWsBRjEjInEnVc"},
    {HX_TILES, 4, 5, 49, 190, /* 90: 0.62 */
     ".ooooo.|ooSSoo.|.ooooo.|oo##oo.|.S5o5S.", "QQJbqJbq",
     "AsKBsBCOfFBmgIaArKsJbCoFoiSgRaOioQFbrIOjMgkQMjaFBkgIOebArNSgcKokBenImsKrSOfsScNcCmbQjKnRoFrJeNFcMgQNfoSfiEkmKAsnAojEMsfNBAeKakGoJgsKGoJnfSeSMbnKrICkbRgMbKfFmRoQcKrR"},
    {HX_TILES, 3, 5, 46, 150, /* 91: 0.50 */
     ".SoooS.|.S##S..|.o#o#o.|.oooo..|.ooooo.|.oooo..|.1ooo1.", "CgoCgoBrBr",
     "JCnjJcNbAmFnfRmKmSfbRmArCnOscSoMqMFbNsAfKoKqCqFroGmCgJCeFBsiAnCmsFBiKfFafCEGmkEbiOfEcoKfFcnFSmsCABeOjCfCiRkKScGcrSaMNsnFkQkcORoKaQnaGicCgRgAMfSgGSBfBoKbgKcRnbGmQSgb"},
    {HX_TILES, 2, 6, 48, 190, /* 92: 0.50 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..o1o..", "",
     "NwRbgWOjnWgSjWfOfnQmEqgUkwEwnQIsgMScWQwgWAMgMaWbAkVkKcGNkCjCjrVgWAoVsVsNenWesCfwVKgnGvWbJvWeCkOKnjBVOwkSwOIcOgsOFkVBqSMavKRFJSkcKvOvbWrCRmJCocGruRQGKrnNEmkQ"},
    {HX_TILES, 4, 5, 47, 160, /* 93: 0.50 */
     ".ooooo.|.#oo#..|.ooooo.|.#55#..|.ooooo.|.oooo..|.ooooo.", "",
     "JEkJqjSoOfrMSCjBnBgoGbBrEcjRbqOkGbAkeFnEkmJEKfQkOsbFsjNcgMfRibRcOsNfbGOfIGoSSjKBSaNRkqJCScnIogCiKGsKsSJfsIgFcCqOaMBiJanKfQgmRkgRkCsBifNOsjCANkrCfbGSfKIgFEbsQFsRb"},
    {HX_TILES, 1, 6, 39, 130, /* 94: 0.44 */
     ".ooooo.|oo11oo.|.ooooo.|oooooo.|.o#o#o.", "",
     "OskJbfGcoRbsUgoSwWrvCeBBNuBncCqWkQRfwCrWmBBukSgSVbMBgcOOjRevKoBmEwQkKnNOrUOBmrRWARQOwqVckCkmSimGaCIqeKONrWsVoWEsOGUckEsBRJniSgbWVcNgnVjFbEogVmcOBqfKMBsgCeGovWnj"},
    {HX_TILES, 1, 6, 40, 120, /* 95: 0.44 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|.#oSo#.|.1oo1..|..ooo..", "FviFviU",
     "NIFAqoQcqVjrWmFcgIuQcjFqKuqFAewSiJQiGbGvKSwmIMQgWKnEqOOaNgVFsvOsWfmQoFSboIoJfmRIAiWcgGQnVosVKsWjoGGueOvCmgCqmJsgGkGmRjKrBmGnAkWsRbqFoIKCfVfCunIRgQWokCwAkGcuAsnRCm"},
    {HX_TILES, 2, 5, 47, 190, /* 96: 0.44 */
     "..o#o..|.oooo..|..o5o..|.#SS#..|..ooo..|.o22o..|..ooo..", "JmJm",
     "GkFsSJnaREsKfsFkSoiKbSnFkSeEkJqBQkEosReOKqaGNsBkCjKFRcJaQnNrfSgsQGkGsfJrCksNGjGGRosCgrBBifSnMgBskSnbKNqKerQjBrKEFoKqkNgFjcRkgKONfSaBrbGmIbNaFaBrBnNesQjgNgmSgSocINjOc"},
    {HX_TILES, 4, 5, 50, 190, /* 97: 0.44 */
     ".o4o4o.|.oooo..|.oo#oo.|.#SS#..|.ooooo.|.oooo..|.2ooo2.", "FciFci",
     "OriOSoKfJoKrBenFmiIbOkCJfGjAgrCoIGkBsnGNagBgRmKsRocEsJGjGCjrKqmAkOrROjQfSiJsIKskCgCECqmBKnJmObOkRbeJcGnObFIaRjnScsOfMBFsFMgoIgrEcqCmcRgjBfmCAFcSjcEocJgFNGsRkSjcBnGof"},
    {HX_TILES, 5, 6, 44, 180, /* 98: 0.38 */
     "..ooo..|.oooo..|.4ooo4.|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "FuAknSjvEkFkeGqwSmiABjwWWSmkOewRoKmfMVfvOkQVrbNugGswEbIGnNujBroUnrKasGiqJouWbUoBWnVnRgaCwWkvGbMKmGsUfkOanVIwrEiUbqKroJKcwScmJcrMNjNkQOVrjKbsQfaUjvASJWcsIeuRfwSBAUfGMuFnRisBofNbEi"},
    {HX_TILES, 2, 6, 41, 120, /* 99: 0.38 */
     ".ooooo.|.oooo..|.o5o5o.|.oooo..|.#ooo#.|.oooo..|.ooooo.", "",
     "CRoSeUiBeNqwUgkCUfRnqSjQWmGEsnBkgKBgGmfSkRfoWjwOqKuCgFAmsQvmIWIAAEWfrKAmcCngUsOseRfiUscRbqIouUesAjFoBRwGcWBqJfSEUGcGoeIruKcgJoeVknEoKsBnBVogNCwmSWbRFbjSnQoWmGjqWbBjRgm"},
    {HX_TILES, 1, 6, 46, 170, /* 100: 0.38 */
     ".SoooS.|.oooo..|.ooooo.|.oooo..|.2ooo2.|.oooo..|.o#o#o.", "NesNes",
     "FqEnGciEvBNwcWCrGvVgnCKoKnKesEnOejSucKcgSbvQVoOCWbkJvJncBMckKRuNqwMswBWqGquAjaSneARnWcOqmIgwKqkCiFaJOcBgEjWgNceACubGcjRwrNNQFwRgBKwjEwNkvRokEmfQewFWawWsSgCgkUscBmeGujBjSerQSkVkg"},
    {HX_TILES, 0, 6, 28, 120, /* 101: 0.94 */
     ".......|.o66o..|.oSoSo.|3oooo3.|SoooooS|oooooo.|.SoooS.|.oooo..", "OgsOgsWkWkIbIb",
     "GkIwrKfrSveFvcQKFnSjMGcBJcmBkaVfRBwsKrmWKmBgSkcMUngWkCiMbmOfkWbQvVjNGrfBgCvkCwRvGEVCkCoVQbgSgVgSGqwKbIgIQnqFrwUcwVgmNjfWbwUNeIoKwgOaWsUcGkFjsBkQnOcnRfoGVmjAwFrjWcgMKvoWseNNsi"},
    {HX_TILES, 2, 6, 35, 130, /* 102: 0.88 */
     ".......|.3oo3..|.o2o2o.|oooooo.|ooooooo|oooooo.|.ooooo.|.oooo..", "",
     "EcrEIfkFqgNroOrNbAwUjmFqoWgmBWsWkwJfCiwSfAnUjeJFmgQfjGbkEWeiBsgBjcVriKasEwFuNcoVeVgcAfmKNJFaRoFnsWfSuRguNsoNknRaVsGKreEksSnBnScEvjCAFkNjoCBeRaCeFukEvQifWeIfOBnVcjKSmWfGOsOIgQuA"},
    {HX_TILES, 4, 6, 34, 130, /* 103: 0.75 */
     "..ooo..|.oooo..|.SoooS.|.oSSo..|.ooooo.|.oooo..|..4o4..", "WnWnJaJa",
     "IEcgRvbBucNgbOEoaVGBVbOjrCwMgFnqUgKGkCiKWovSnMjVbgOjwSvkGkmKgSavWVfVnuCgiAwKusOjnBgrCkKeaBuVgwFjsWnSJeOqKsiWocRfcKfRfqUQSaSowWawFvnRjnCswFcQMwMfNcCRvGseCrwWjgAjQwkCiNrfMkUcJcWOOjSwb"},
    {HX_TILES, 1, 6, 43, 160, /* 104: 0.75 */
     "..SoS..|.4oo4..|.o5S5o.|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..", "FcwFcwEkb",
     "QkQkaRnUaqBoScoWfWnuSwrFqfOwEckVnrJwKesOGouBgAerGoEcGqeOcNJVrbJwFFwaJcnAkSmKqvOAwfIaIoQgwCwiGcRRgGUUQgcOwSvoUcwKEsvJbfSuiObMuoVkFoOOsuBwFmUkwSckNivCfbOjBqBufKIwnGuFwAvSNqVgGcBgWeoC"},
    {HX_TILES, 3, 6, 34, 120, /* 105: 0.56 */
     ".......|.oooo..|.SoooS.|65oo56.|ooSoSoo|oooooo.|.ooooo.|.oSSo..", "ReReFaFaIaIa",
     "QuJbSvBvcSckKbAvjNsRifKrAvnCwrNqvQgOarQKcCgFcwRkoUbOaBofMbBnsFvWgFobQecRbkFcnKWNqRbBkcBSjnNaIcnOekKVsmSkcQnIcRKfQbCwSmeBsCFjsQnAvcBqAkIGvJgRmbOanEqAqcIuoRoCnCnuBvfGkmCnkSEWCNavScNwU"},
    {HX_TILES, 5, 6, 43, 180, /* 106: 0.56 */
     "..3o3..|.1SS1..|.ooSoo.|.oooo..|.ooooo.|.oooo..|..ooo..", "RwrRwrAem",
     "VokFwKcuSkrGnwIcQkSwWCsSkVbOciKorGOjSgkSkBjgKsGrIrIvnOaiKGjoWmSogRcjGiWjRkeRfGnEbGkRFnSgGIRbkRbCgIJcCNQaMjSQAvBwSwSvQWorVbAgFwfMgoSnNgoKnIeIoMusMbSaFcWjGcBkoKeoOJeOvoQuAj"},
    {HX_TILES, 5, 6, 46, 190, /* 107: 0.69 */
     ".......|.4##4..|.ooooo.|oSSSSo.|oSoooSo|oooooo.|.ooooo.|.oooo..", "GkWWGkSjSj",
     "KeGkuJbfVoGubRwnGjrFjnJurJemRoFFsfGjsUBgGUjcGjuMFNkCUbuSuOIbqRSnAvbAQofJorRnbMkaAgJfGwmWfNwRcgNKEoiRnvCiMkqGwFKgsAnvWcQvbVgCkOckOEofVSgUQneGwEnVSuoNjMWIgUafNKocJfrSeUbRfiEorGnNsw"},
    {HX_TILES, 2, 6, 46, 180, /* 108: 0.69 */
     "..SoS..|.o66o..|.ooooo.|.oSSo..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..", "MwMwGaGa",
     "JfbGWakSgrOBjgAkIroWVnfCvrSgiNrwKbfSJbmCenCueArcJIgcWbMkSbWjNuCQfrVSbVMjwReVfGSEqIeArkWeKgFwQoUmcGImKrNfvKvgIgkRcOrkQBoaBwjKgGjrVVqwVcBoeCoAnOeMvjIqoIeWEwRjqSgArfGrCgnUgnCokFwjBWrOq"},
    {HX_TILES, 0, 6, 37, 150, /* 109: 0.44 */
     ".ooooo.|.SooS..|.ooooo.|.oooo..|.3ooo3.|.oooo..|.ooooo.", "FjbFjb",
     "MNqOuoIfOvgRosWnCoCruAjVWEoCoWboAfjEnFkfKaGigBSvnJrIvFqGsmNgSwMkvFjsAUBFOjFOqWBsvArfBibIqNJmvKcvCsgBsvOrFAvsRcsRjnOrAiVisBirAkrOcvKbQvOuWnrAebRevQwsWfaVeOvcBoEkIgjOgQUgAqFqoWknGoUeC"},
    {HX_TILES, 3, 6, 34, 130, /* 110: 0.38 */
     "..ooo..|.oooo..|.ooooo.|.#11#..|.ooooo.|.SooS..|.ooooo.|.o66o..|..ooo..", "SfsSfs",
     "CBnKuiAnrWjBOujSSOcwBeVbrGweFasBKCeKseAvcWoJaKbwQvMRcAwWkcKqvVrOejQNrSMceNgcEnQbmEnSoqQjcJmAseUIKqRvUeQakSMvVOcfKoGsJnNvNuGOsIgCjFjJnFinVNqUcvWquQbvJvNkGsvWqfGWoaCowIaKGieGbn"},
    {HX_TILES, 0, 6, 37, 170, /* 111: 0.38 */
     "..oSo..|.3oo3..|.ooooo.|.oooo..|.ooooo.|.1oo1..|..oSo..", "OsE",
     "WawFrWiaFnBkcKgaCfsIcFvWfKEveKoWnwWRoBjbSavSoGWeGnJcQcwQmgUosEqvSasSwWrfQbVAgAmkGwjCifCMcjFnvMsvSnCsiVojVBfOcOOjNfbSkeKvmSeWovFueWqfEwKfoKwrGqCNcBuJMbWeOscUiWfRbjArNgGkgKNbMjCfJoCkmOaOj"},
    {HX_TILES, 2, 6, 44, 190, /* 112: 0.50 */
     "..#o#..|.oooo..|.oSoSo.|.oooo..|.6oSo6.|.oooo..|.ooooo.|.oooo..|..o5o..", "KcKcOa",
     "NucRkoIoSmCjNkSoScsGSkvNfrBosQwRQuoUbWjGnkJbNNQeIsiMOwnRwqMcvAsCsfNfvUnBoWmvVUrGOQbnOkfKSicCnOjbOEquMsRVjmCruMbSvSgvQnOkbVgaCwsFiKbvMukJaGcWbrBiWaoKavJrmCmSjMsAVJfQeWrRnkFsKusIasEiqVsb"},
    {HX_TILES, 4, 6, 40, 120, /* 113: 0.50 */
     "..ooo..|.oooo..|.ooooo.|.oooo..|.ooooo.|.o11o..|..ooo..", "",
     "UcNvGImuBunWKoOsgEkaBFanWOSwMBnqGCWeWfoSnbJvUmjQeOiBsuIcwIswQavKRMriJRofSjuCSmkGwrSgKnaMRfjScRcCoqWcsQjsGbGSGbGigUNqKcWgjKcuKWeaAmEwbVeWrgBoiUsbOviSEuQiNrjBmUaJokMgWoSCwsRgOaO"},
    {HX_TILES, 4, 6, 52, 200, /* 114: 0.69 */
     "..ooo..|.3oo3..|.o#o#o.|.4oo4..|.ooooo.|.oooo..|.ooooo.|.oooo..|..ooo..", "",
     "RiVkoCqcBGFkSfMbSWGucCjKCnvCnsMibOiWVkWqCjwWncJnOOkCuGwiCosOkSOeCMcqNWBsFnfNvbKmEjFwGjSkCruRckWWWoWgwJCgoEcWbsJoAncEmNbvOqcKquRfBgoQfoGuJvjGcKakRwCqoFGoGnFamMiQoEvBJanRceWaNsf"},
    {HX_TILES, 3, 6, 40, 160, /* 115: 0.44 */
     "..5o5..|.oooo..|.ooooo.|.oSSo..|.o3o3o.|.SSSS..|..ooo..", "KcwKcwSOgjOgjS",
     "UqNeOecJvRjKvFcoCeoKMwWgQjoCmkMcIuKqVmfJvWnUiuFFoAVqBfmGokRwCKngEbWqIrOvEMckGnIokCMfkKruSucSNarIbwGnfSgvWqcNeaMruFrgFkfKukKwRorJaGUjwWgbWbnMeUeQWkOvEnvKeOwrCqJrjFaMcjGwGJugKcfOMBgOibOku"},
    {HX_TILES, 1, 6, 46, 160, /* 116: 0.50 */
     "..ooo..|.o##o..|.oo#oo.|.o33o..|.ooooo.|.oooo..|.ooooo.|.oooo..|..1o1..", "",
     "KCqGwfMjvGsoKoRgbCnWokOrOwFoWKveAOcoInsOgsWokKgSbiERobFInjImsWEWqiSkONeoVkUaKuCeJqUqgFwWaCqmInOKOanUkWkeCuOCjAvIBrBowGoJcGVJqbRwbBjmCqRnBreKumNqCKnwWkUaqUjAnOcwFsiKasMuSnwUjgQvkCg"},
    {HX_TILES, 5, 6, 43, 150, /* 117: 0.38 */
     ".......|.oooo..|.ooooo.|ooSSoo.|4ooooo4|oooooo.|.o#o#o.|.SooS..", "VcrVcrRkRk",
     "RBsAocVrNIwOsgUbCsUraWeKwrUgJcvKnMwKnFFuVkSaqKgNeCnNueCMeSKqAgrKRWSmWKwSaCFrWJfKucVeoQeKskRfvGwcNGNugBjSwSvcKKnjSoRvkGwgWfwFsnSivBjvSuoCkSfKsfGnvAkuCkmIGmEwOKOgGjqJwoQUnwK"},
    {HX_TILES, 4, 6, 50, 180, /* 118: 0.50 */
     "..#o#..|.oooo..|.ooooo.|.o##o..|.ooooo.|.oooo..|.ooSoo.|.oooo..|..2o2..", "Cv",
     "BrVknCrMjFrWecWOkIMqQmKvkObQfUfcSoNruOiCsgKrEagGcwBsOuOsvVNwgNioUVrcEQocNeGuBmKgCnAnWkFKVgcNicUgsCwaCwmIwmBnCNsKqWGrwMcQoKNkcSmkEcOjmFsWgrFsMGvmBinFsNaBnBsOeiOwgJBFkKfjVeGroWekIwb"},
    {HX_TILES, 4, 6, 52, 180, /* 119: 0.56 */
     ".ooooo.|.5oo5..|.o3o3o.|.oooo..|.ooooo.|.oooo..|.ooooo.", "",
     "FjrJfoFiWfCgKbVbmQjGigOcJuJOqjSwmOAgvKrbSnOiAseOBwoWSokFuCnwBvaSjVSusRuMfmMsBwcRnRgMwEbCwqSvgRkABwMaVnfGniVObsIvsQcKeJcrKbGjcWfcSfVqiGoKbvCqOGkIeAsCvcRJMWmOrCKAwRmwNcmFBfwSojNEo"},
    {HX_TILES, 4, 6, 40, 150, /* 120: 0.31 */
     "..ooo..|.S44S..|.ooooo.|.oSSo..|.o#o#o.|.oooo..|.#SoS#.|.oooo..|..ooo..", "CiCiQbfQbfIskIsk",
     "QCuKoKJbnVfGcgGcjGviBmJnBgAkwBrOOMgsNgvQwbSWcFwObeFaRjeGrAkKNjGscKvWgjCnCkeSmjJaNeiVgsAsiNJqwWRuKcVsoWgiUjvKgcRbUkOjuBkcBgMeWkWgqMsgJqfGnWojIsjCuVsMbjVNSfoIsEsKmAqQcrJrvWVoWfKokIGkQk"},
    {HX_TILES, 3, 6, 46, 190, /* 121: 0.38 */
     ".ooooo.|.SooS..|.ooooo.|.oooo..|.ooooo.|.oooo..|.oo4oo.", "CfCf",
     "NaqRgJvArWiFVcfOsRfoRFnrAwSgIuoOeSjWiWcOufIUrGokOgkWcwCrnGkoIVnkWjRvcEbgKbJcfFWsNrGrJaNguBoJajEkcGrwWivWqEOuObQwROrRgCnSvaRewIseCejSveMOcAeqJwsSknRaCvfBfvCogMcIrcEwVgcNRSgJcCjnWnJrAvVr"},
    {HX_TILES, 2, 6, 55, 210, /* 122: 0.44 */
     "..S6S..|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.o2o2o.|.SooS..|..SoS..", "IrIrAgAgRavRav",
     "GvoQwQObuRfOgnRfEwgECjvGqcWivNKrWmkBjJeFcUJwVivSoMsbWeSnvAfbUokGiCCVbWofJboSwcEobFcGaCveSCBoqJrbOeWWkSemOunRweWsOfnAUmjMbuKqRfmGCoRgSbRbrOwbEiFskFuUjsObgGakJcvKJnJJSGjKsKGaCjReQj"},
    {HX_TILES, 1, 6, 52, 220, /* 123: 0.31 */
     "..3o3..|.oooo..|.ooooo.|.oSSo..|.o2o2o.|.oooo..|..ooo..", "QwQw",
     "OSgcIuJKebOswQjrSRfcGscRnOinGwfRvrAwaVcRaBsfNkIruVcSEriRUcoJqMNKfGucEcWFrbSuaKgEaNCnfNRfqEWbkReAUcsJoReGjuNgiIekWgaOrmUoWbNkOvRUgvSRgGIcuBgSjuRucFkRAqFmvKfGwnKsCFneUcVWbKwnUboMc"},
    {HX_TILES, 3, 6, 52, 200, /* 124: 0.31 */
     ".......|.oooo..|.1SoS1.|oooooo.|ooooooo|oo##oo.|.ooooo.|.1oo1..", "WiWi",
     "FjKwSGuMukGmFkvJaqOEreEnbBoKGnGqgVfWjBMugUsUanNseOuoOgsJSofEJFuQSmcFCfvSMFIgkNwrWfFNrmVeNcsBvoJuAvSfuSfGckVjoOQwWnCEwEjbRoNwoSeAnVnErfGjKeJwaOinEUgbCnCeaCsWbiJFsnIwOfjVeiNjbSinEa"},
    {HX_TILES, 4, 6, 52, 210, /* 125: 0.31 */
     ".......|.oooo..|.ooooo.|5oooo5.|o#ooo#o|ooSSoo.|.ooooo.|.SooS..", "WgWgOgnOgn",
     "JmCsoBfjSfRnwNvFoSgjCFnCjnOrcKoSwJqBgFroJscUgKAnUoEvmRuRQofFoBwrSojBrfSEqbQmgVbRifMwBoeIGAojUNFwgIsRwsNaObSfcOvFwNckOfVjVbSbSgjFusEOafGQJUswUiOjeBekOrAngVRokJoBsoCjgVAjeCoSvcOjNjqB"},
    {HX_TILES, 4, 6, 44, 180, /* 126: 0.81 */
     ".......|.6oo6..|.oSoSo.|oooooo.|ooooooo|SSooSS.|.ooooo.|.oooo..", "VjVjCuQksQksCu",
     "GngBjnOqcKswVaCgVeCgWcgBsuSoFMOfkUfcMgVgmVsWioIrRgcUovOFKqOrGvoWgwSWBiSkRkvFmrJrgNurCAGjSWAovObKwnImqEvoGowGkoBgMqwWkWkBrwJFsSmkFuKekAiaRgWmFEvkOfqOvkOfNbFbsVrSmcBmrGVoeQvRbJwJsnVsOskKq"},
    {HX_TILES, 4, 6, 43, 170, /* 127: 0.81 */
     ".......|.oooo..|.SoooS.|oooooo.|oo5o5oo|oooooo.|.oSoSo.|.oooo..", "VnvVnvVmVm",
     "GsvMwNWjVJEcKsSkGkWjOgwIsnGoOjWgSaOwBEmgGcSmsSaBiMcsFqBgBiEkOufWfsFwNkeOqAoJoWAogGwoRcoRjKGvcVoScQnuBiSvoMebAveJGGbSAnfFwWncFcuBibRoWjeVaCkVkAvCkBjQcgAowFqoGuBKfjFoaCkBeqROrcRwfGmu"},
    {HX_TILES, 0, 6, 34, 160, /* 128: 0.56 */
     ".ooooo.|.oooo..|.4ooo4.|.oooo..|.3ooo3.|.oSSo..|.o1o1o.", "AA",
     "SmrGafJwnRaFjnRksGrSOgGrkOwOiWnFvGjwUcwWcVraUbnJcjNsgWcgCroVGcFcoOErEjwNisSkvAwRagKfrJwIckUnVsMjUjcOicKbvWMAGrWcmIwfEqGwgRmNagFuIgRjfNVcSgBwBfkNsRkGjuFnaWgqSjKrGbvCqcBWjbJfNiAsNkKQbAjmMfbQi"},
    {HX_TILES, 2, 6, 35, 160, /* 129: 0.38 */
     "..ooo..|.SooS..|.SoooS.|.o##o..|.ooooo.|.SooS..|.ooooo.|.1331..|..ooo..", "WmsWmsCqCqOjeOje",
     "AvbIUJscSjrNjcGnfJbBsuFocOrQfCkfOwfCvGEowEnKsJuOgaASbsKecEbGvrFFvCUbnMemKaIfJwcOIcCurMkfCoWiFVofKrOgEieMwrFIfbVcUgqOBjgSoNrKcBgqVNcUrNrvFjFcrUaWfrGqwBeqIsSWrnSocBsoBqvFukMjeOjGjfRcqKvIvbRmb"},
    {HX_TILES, 3, 6, 52, 220, /* 130: 0.56 */
     ".ooooo.|.SooS..|.ooooo.|.oooo..|.ooooo.|.oSSo..|.6ooo6.", "BgcBgcWnWn",
     "CwUgOjbIfvOOjoOSiaOkoEcBkWbOjAvrFBeOvoEuAgOquFwKvbOObrOcsVsMeIeOrWobFcrSbmOweGbRisJgCsCgwQkEcQmjUaOemUNcIesBkQeGqoIgvQuFvVAnWnGjFVcsVEcnGaRujCfnAfcSiQbeJsSkgASmjOkEwoVocRfFcVWkKcgRksFjc"},
    {HX_TILES, 5, 6, 35, 170, /* 131: 0.31 */
     "..ooo..|.SooS..|.o5o5o.|.S##S..|.4ooo4.|.oooo..|.oo3oo.|.o##o..|..oSo..", "ReReGwGwNin",
     "GbEIgjRoCsNenWiAwgRgIgwKOfOeCgcWGkgWCorSbwUgrNjgMwKfmWrvVkgVfcAsoKgjOSkcGweVigRwNkVkqNgEwsNwVWfvMksCfrNbwKcEmSjOfqVmNvmKOgAgjNboWgbVCuCobGcuNwaWiuRkmRocRfjQujUkWsgSnCrAeRjsUJveUbAjaUoCksVcIoWVg"},
    {HX_TILES, 3, 6, 43, 170, /* 132: 0.31 */
     ".2#o#2.|.2oo2..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.", "",
     "BnGbuNcOboJmQNimGsvJcJsNfWqkQkgGwkJavVbNjsVgSveRwWevKnSmvEcGwaSeBvQcWnfNeOiFcrVmaCWOwGvjJgFwsGbOiuIvJuJvBwUfmReEsJmNaQerEwBquOrcNswVGujFkwOiKgEoqFqKmrIwGNwIaEnKnBJwSVbnWcGogWaWbOioRcfNj"},
    {HX_TILES, 4, 6, 46, 170, /* 133: 0.44 */
     "..ooo..|.oSSo..|.ooooo.|.oooo..|.ooooo.|.#oo#..|.oo5oo.|.####..|..ooo..", "NsNs",
     "OfmCRkKofVfcOcBkCrGNVcFmvKgoSfcJnuRvVmRCrOwSucFbCWmOWNviIgqAguSOwUnjAsOfMriOkcKJWboEwjOafMCvKmGkOqANfMuCiWqCvgFosUqGcrNwVrvUneUkEsWfKagGoeMjQuqOcgGubSvkSokVsBmuJcfWamKrVnUfJamFkfOFi"},
    {HX_TILES, 3, 6, 58, 220, /* 134: 0.56 */
     ".......|.5oo5..|.ooooo.|oSooSo.|ooooooo|oo11oo.|.o1o1o.|.oSSo..", "WjgWjgFjwFjw",
     "NfSeQoUiRgjUUsJguUWbRgANUbFvMMcvOaEjcVniKbBGRkqMgKoVmgEmcEqbSJwFjwQeqAuFmJmvCGweMuJvoOfIBebKgcMsaJnNVeVKoMkcINFrFbrAnwVOfkGbJsfRuOaiGkMwKoVcRoOrWosKoQwjEqfGjGMsQgsWbBruEWqg"},
    {HX_TILES, 4, 6, 55, 220, /* 135: 0.50 */
     ".oo6oo.|.oooo..|.ooooo.|.oooo..|.ooooo.|.oooo..|.ooooo.", "",
     "KsvEiSaqSjIniKSvQiMgsCrAvqCncGanVoKugFsNcrNrcKsiQmMgRjFrSgjVoiKsnNqmGqJnsKvkRfqGrKosJOwGkCoJucCmqIosSjKcrEjwBumUcgMVngUfNqUoRkuGkVaAjcUqWgwNuSnJcGrBvoScgVcrOwiOjBgkUfkJbrSRjWSgMwoQkQjbKnNvbMwCejRo"},
    {HX_TILES, 2, 6, 53, 220, /* 136: 0.56 */
     "..ooo..|.oooo..|.oSoSo.|.oooo..|.SoooS.|.#oo#..|.oo3oo.|.oooo..|..o5o..", "GGSjSj",
     "EskSuAsRniVsUoVfcFoRkFoIbIwKuBruCgCieKvkCwoFcmJGwoEnRgbWnwMsbOkFowOsjQkGwiSoNsbBvRwgGwnIckKrKrAqjArcKeQeKFiKsgKoRcWneNGFjIqEbmBokORceGrAigBvMsoGkwAiqFoEqiKsUrCsCGjNgjNiWocIoSwSkMwBFaIscIbr"},
    {HX_TILES, 2, 6, 49, 180, /* 137: 0.38 */
     ".......|.o##o..|.#ooo#.|#5oo5#.|oo1o1oo|oooooo.|.ooooo.|.oooo..", "",
     "UewKrRisJwrSvsVrBuKFbkVaqVKsFrSoCwGjuBnsVFucRobAfnRGvcWWQgrBfOjWgoCqjIgWofMUgoENawSReOsQcwMgoNioWkrSfjJvoCNwoCvRkOqcBqUqGrWecNrJbRcIgnGrNbsBvfIqoEavRBrbAkqIfRkQuiCjCuSksGjQbjNeJbeCkeVmqWjsSnc"},
    {HX_TILES, 1, 6, 46, 180, /* 138: 0.25 */
     ".oo3oo.|.5SS5..|.ooooo.|.oSSo..|.o#o#o.|.o##o..|.S#5#S.", "WsWsCCGbGb",
     "CrwRFkBksKfNioBvIoGokNgoGjgWqCoaKcgUkcIasWcCiRwSnNcGsIOcMcSCqwCqUkOivOawRjBjoWcoCnGunUJfaVrFEvBFUKanSbsKqVEuqUasSWoFwQowSuaMrwAIFqUgIvUkCAjCvfBSmqRkWrbESGRbKgmEwfOraAfQbBrvOwg"},
    {HX_TILES, 5, 6, 55, 220, /* 139: 0.50 */
     ".......|.o##o..|.oo#oo.|S6oo6S.|ooSoSoo|oooooo.|.oSoSo.|.oooo..", "FkFkRRVjVj",
     "WbNBiUcOwkNwfOajGoAfCqOvNkaKgoInvQvrSuBfSkmFnkKvNcgOjbQerRnRgWnsRogFieSucJbvJsUqjAqbQCUmsOuSVbAeiImGvGbBrOaoVkwAujQiOSjmCJrvJwKcUbgQKofCjeAnRCvrMVcfRuiFKfaVgbFmFBubNjAiWQbVrKfoWcUgGbFnf"},
    {HX_TILES, 0, 6, 55, 210, /* 140: 0.38 */
     "..ooo..|.o11o..|.ooooo.|.o22o..|.ooooo.|.oooo..|.o3o3o.|.oooo..|..ooo..", "",
     "AqeCArkWeiIbQouGoOkGiSmNcRcqMqkFrBkOkOjvGjmUknJbjUnrMkwNsWgmRksFcGmsKnAewKbCmrNAukQJcnEvrJoWoUgsQwBkqWiMkIbUsuObUfmUinEsoCgmCgAgKsWbnGuMjsRfWeNfWbWrkFBvGusCjFmMcBsVqeFKnNIofFmkRbiJnaVokAwWeFof"},
    {HX_TILES, 2, 6, 61, 250, /* 141: 0.50 */
     ".......|.oooo..|.5ooo5.|oooooo.|ooooooo|#oooo#.|.ooooo.|.SSSS..", "JrmIuIuJrm",
     "WrKcnOcwBqoGGOkwJcAJbmAsGiaAisUboVfjJsfGmsVBCroGjuWfwSkqCwOgUibEbnNGwqFiFiwGwiEqRKqNfVGvImCkCkQnsJfbNSwfBfoBfbMqVowEkCfNfGvFmGoRoSmFsfFnaFwKbRbkIeNWgcFkvRcnRjnWfwIKfkUgQamJweOVeGwNveVebCfw"},
    {HX_TILES, 2, 6, 59, 270, /* 142: 0.38 */
     "..ooo..|.oooo..|.o5o5o.|.oooo..|.ooooo.|.oooo..|.SoooS.|.oooo..|..SoS..", "BvkBvkAvfAvf",
     "VfbJcfSmbKmrJnsJobCsRSfaRwoUnvUgoAfwFsmWKouUebOasIbJwiIcKIaeGsBoKaBuRgSkgBgUorFmEOgwJnvWBvkCokIbFcsVqSfJrGuRoqJWokVRcnJvVIQmsCqCrNgsOqBjwQgObBnUgIrRmcGbvAkVkaQWnQBqiSaFoKowJogOevArKbnJfMvo"},
    {HX_TILES, 5, 6, 56, 230, /* 143: 0.38 */
     ".ooooo.|.oooo..|.1ooo1.|.oooo..|.ooooo.|.o77o..|.#4o4#.", "",
     "UguUgCgoWkVosNgoWfoQibUaNcvNwSuqVfFwQceSvKGrbIcOwIcNQjgGSkIwkWmbOjSmWmfWfuGkcSvScsJNcGrCfIcjAicOgrJvCiKasBwMsFjqFbEbFswNCJrmRoIamWOawMgqSoGmsSKoOMgmGObRFbBsmOMruSOveJroNbeWsCioGScAwk"},
    {HX_TILES, 3, 6, 61, 240, /* 144: 0.44 */
     ".......|.3oo3..|.ooooo.|#oooo#.|#ooooo#|oo11oo.|.ooooo.|.7oo7..", "",
     "WkbIsFvQRnsBCjIsfVswCgBeWirMsfKgErCkEBgmRkOFckAjvKJgqIrKcsFoaSWnsAiEcwIoAigSwsMbuWMjaSkMinMrmIsREaFsRfIMaBFjeOksSngQkfOiJsUfwKmNusKsFbIoqCmiAgoWjvVruGoCBuBOwCjbEcwEaiSemImiOceBJrjMgjNicJ"},
    {HX_TILES, 5, 6, 52, 180, /* 145: 0.25 */
     ".ooooo.|.oooo..|.o6o6o.|.oooo..|.ooooo.|.oooo..|.ooooo.", "",
     "AmgSbKeBwcWmVnsOvgVbfCGuFwKFMQbuWeaMgUgcQwFOcwIaBuBsJqMwkVkQkuCfvNgrEsiWsgCrIgbIunUOsiSnAwfIqCgoUgoEImFkBgjSmkIfWrJnQbVbkFawSfiUkGjEbKckNveCOrbKsaAwkKwAkrSwCjrBsNseGorJeSmAsbGnrJbwUgJQkrBnRko"},
    {HX_TILES, 5, 6, 55, 250, /* 146: 0.25 */
     "..ooo..|.#oo#..|.ooooo.|.SooS..|.oSoSo.|.o33o..|.ooooo.|.oooo..|..ooo..", "GmkGmkQjcQjc",
     "UCkvCoUbQeOeaKniQNajKwgBmkOsmSuKGCkgUroGisUoiWqVjwQarEoGMbVkGrUeaKvbWAvMujOrGoSusCfwKroNbCnvGWgrEocRoQcIgRfIIfOweJgFieNjMrfQmRfkAvRjKgcGuEcWjqIbwCeaWrWnqSumVqkNrBrCgFbRoAoEuqMESjWcoCrSc"},
    {HX_TILES, 5, 6, 61, 200, /* 147: 0.38 */
     ".......|.oooo..|.o#o#o.|#oooo#.|ooooooo|oooooo.|.3ooo3.|.oooo..", "",
     "FqRuBgAsaBsFswBvCgsCsSgWrKJKnGOswKvKwoVarNaCrFjBovOcBQnKrvGvoBeCecMugSaWiKJnAnuOuKcmUjCnAvaSjfWmjOurWIrFrcGnjAouSeMuRgwNjbGakSweQjmIreRniKoOsQcNJwjQAScjBvViuVswSfjBkEusJvVCjKeFmFRkRvf"},
    {HX_TILES, 4, 6, 58, 250, /* 148: 0.25 */
     ".5oSo5.|.oooo..|.ooooo.|.oooo..|.ooooo.|.SooS..|.SoooS.", "NuKcwKcwEE",
     "JgjJJsgFcvRvkEoMRMkgJuoVkKskRwcJumIwjKqnCkBngVfNanNfwGmMkbROiBmsBvoEujEcBwWmJuOcWaAfiAwiMGsvCgmFjwCgiIqfRRnvMgwSAAfWkaCeGcIvnFmuWRiFUjqOkWeFvKcrAjqWEuUsbUajBkeMgJscCfEqJSavNVSCmIeBrW"},
    {HX_TILES, 3, 6, 64, 270, /* 149: 0.38 */
     "..ooo..|.oooo..|.ooooo.|.o77o..|.SoooS.|.6336..|.ooooo.|.oooo..|..ooo..", "NvjNvj",
     "VNcGsnEwGskJUkoKeoSnIoWfKagSEwBroOJwkBGoNfNuKcQcoNCjFcKosNkqRbkGsvEiSgcRBsCqInOgnCmCeWVoAWkGqfFwNEovMrcOKvkEaRkVgjRbKnaOweUnsJwgUaJsQgvOjEVrWgvIwFrnENeGiJsfBmGbqCfrJmUsVbkKuKQ"},
    {HX_TILES, 4, 6, 62, 270, /* 150: 0.31 */
     "..#o#..|.4oo4..|.ooooo.|.oooo..|.4SoS4.|.oSSo..|.ooooo.|.S##S..|..5o5..", "RafRafQceQceEjEj",
     "MOesUogWOImEkVarGuGjsOsOvaGrgUofBSCvnWfwKCqjQfNRcoCjnJUskKsVkoFcfCRoOksCniMueJukBGmCgwVbEjuKsKOKugNkoUgEbJRcrQkoFcAuSjWoeNgbCIrCBwfWbqIgbRovNwkUnsUiMekKnvSfwMFjSorWjVmiUmBoeNjfKCouVgOra"},
};
