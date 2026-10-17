/*
 * Pipes' levels: our own, made by tools/pipes_gen.c (trees grown by chance from the source, kept when the
 * deductions alone solve them, ranked by how hard those deductions are), six chapters of 25 from the easiest.
 * Each: "<n>[w]:" and a character a tile (pipes.h, PP_CODE); in the comment its grade, its passes of
 * deduction and its par (the fewest quarter turns).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
static const char *const pp_levels[PP_LEVELS] = {
    "4:RwxnLLLL4KK4BcDW", /* 1: 4x4, grade 0, 2 passes, par 17 */
    "4:RXPpEfcKPfWLEfea", /* 2: 4x4, grade 0, 2 passes, par 21 */
    "4:QmPnKK7L4ExrAec7", /* 3: 4x4, grade 0, 2 passes, par 21 */
    "4:OeYISefbUfZIEfeb", /* 4: 4x4, grade 0, 2 passes, par 20 */
    "4:PweoK5J6LHDpFkeb", /* 5: 4x4, grade 0, 3 passes, par 27 */
    "4:PpBpKVWL7VoL8cDc", /* 6: 4x4, grade 0, 3 passes, par 20 */
    "4:HRpJL5LKLQkdFjeY", /* 7: 4x4, grade 0, 3 passes, par 23 */
    "4:Qwfn4LOdIKLGCbFd", /* 8: 4x4, grade 0, 3 passes, par 18 */
    "4:QXApCoPtGEdLFefb", /* 9: 4x4, grade 0, 3 passes, par 23 */
    "5:RxunJK5KKLLQd6KLEfesDffW5", /* 10: 5x5, grade 0, 2 passes, par 30 */
    "5:RfxfnCoLQaPcLFmCnLQbBa4FX", /* 11: 5x5, grade 0, 2 passes, par 33 */
    "5:GQfwYLSo5IEcSmLQeaFqEZBfd", /* 12: 5x5, grade 0, 3 passes, par 29 */
    "5:RfweoLGLJLSaLCtUXK9s4BjY5", /* 13: 5x5, grade 0, 3 passes, par 31 */
    "5:QWOoGLQdEsKL9fcSaRpIEebEa", /* 14: 5x5, grade 0, 3 passes, par 30 */
    "5:QYPfWKJSvoLLLLLDit6LBfdBa", /* 15: 5x5, grade 0, 3 passes, par 30 */
    "5:QnJOmLLLKL6LKK6QrKTn6Fjd7", /* 16: 5x5, grade 0, 3 passes, par 34 */
    "5:RfY9mKPWArLFupLUfaFtCefW6", /* 17: 5x5, grade 0, 3 passes, par 27 */
    "5:OwwxoL4K4KDYK9cOoLPn5Ela5", /* 18: 5x5, grade 0, 3 passes, par 36 */
    "5:RfemGEZRrLPfqLLUoKLK776Ed", /* 19: 5x5, grade 0, 3 passes, par 28 */
    "5:PXOemUfbI4DwfimG6QesEfdAb", /* 20: 5x5, grade 0, 3 passes, par 35 */
    "5:9xxwXHKLLICbKEdPpKRo7Eia6", /* 21: 5x5, grade 0, 3 passes, par 35 */
    "5:IRwoJKKLKLUbKDsCmKRbBa6DY", /* 22: 5x5, grade 0, 3 passes, par 32 */
    "5:BfnQWHPbEoUkewsDnPd6BcCeX", /* 23: 5x5, grade 0, 3 passes, par 46 */
    "5:PpGOn5LKK7RsLVoK5L5KFelfc", /* 24: 5x5, grade 0, 3 passes, par 31 */
    "5:PmGQpKLKKKL6L5KUnLQt7Ekd6", /* 25: 5x5, grade 0, 3 passes, par 31 */
    "5:RmJQn6LLL6HKKLHKKKKKFjkkc", /* 26: 5x5, grade 0, 3 passes, par 31 */
    "5:BenBoRvcIKKCvkcDoTpIAa5Da", /* 27: 5x5, grade 0, 3 passes, par 41 */
    "5:Pewvn5GK75GSlfnLCoPbFebCZ", /* 28: 5x5, grade 0, 3 passes, par 37 */
    "5:PXRemFoVWKQdLPaK9sFnFfb9b", /* 29: 5x5, grade 0, 3 passes, par 38 */
    "5:Bffeo8wwvcAd4ToBwnLLBaFc4", /* 30: 5x5, grade 0, 4 passes, par 34 */
    "5:BoOxmJL7LLLLH4KUkaGLFefkc", /* 31: 5x5, grade 0, 4 passes, par 25 */
    "5:RX9oGUwoSc7KKLGPqFjq7EX8b", /* 32: 5x5, grade 0, 4 passes, par 35 */
    "5:QenHGVoFdKKFfpLLPnEs55Cfd", /* 33: 5x5, grade 0, 4 passes, par 36 */
    "6:GJOuZHKDs6BtEnLPebOkbUxpCpQrL5Bia6DY", /* 34: 6x6, grade 0, 4 passes, par 53 */
    "6:RYQZRmVekvbLLOWK9dKUvluX6L68ip8keeY7", /* 35: 6x6, grade 0, 4 passes, par 42 */
    "6:QfoAfpKHUuft4KK5JLBjluq4QZ9qFXEefifX", /* 36: 6x6, grade 0, 4 passes, par 43 */
    "6:OmOnPoKTaDrKLTZBrKKCoQbLFpKLQd9b77DW", /* 37: 6x6, grade 0, 4 passes, par 51 */
    "6:9foPfnBpKVWLGLCq9dTd9kwpFuem7K8cBkeb", /* 38: 6x6, grade 0, 4 passes, par 44 */
    "6:GRuffYVrCWQX6L9ulXGLOqAnLDbUvbFfecFY", /* 39: 6x6, grade 0, 4 passes, par 52 */
    "6:RfY8eoEwoRwdJ5KL7JDerUfbRfilepCfW8ec", /* 40: 6x6, grade 0, 4 passes, par 42 */
    "6:PfxxenLAaEXLCfpOecBwcFwY8jnOkXAfbCfX", /* 41: 6x6, grade 0, 4 passes, par 53 */
    "6:AexWPoAerPs7I9lt7HCffimKQxemLK6EYCjb", /* 42: 6x6, grade 0, 5 passes, par 48 */
    "6:AmQmQZ9rLLSZQbKLEpTeaCftKRnRpLEd56Db", /* 43: 6x6, grade 0, 5 passes, par 51 */
    "6:RmHJRmKKLKKKLFrTaLLQcFoL6LRpK5AkcFjY", /* 44: 6x6, grade 0, 5 passes, par 45 */
    "6:OWHBeoCnEfesRjxuxbK9cKEZCpIVfpAbCdBd", /* 45: 6x6, grade 0, 5 passes, par 45 */
    "6:QeeXIJSm9pCrKDxjerDYEwX6RwfrIJ6FXDia", /* 46: 6x6, grade 0, 5 passes, par 58 */
    "6:QfvfuXEn4IEXOknCmIL9jejrTfpQZLDXEiY6", /* 47: 6x6, grade 0, 6 passes, par 63 */
    "6:PpJRZI5LKFfsPdDxeaLAoUuXEvjdFoBaAfed", /* 48: 6x6, grade 0, 6 passes, par 52 */
    "6:IBfoBpDnQa8tQksJBtKRdCnKKCmQdL79ljfd", /* 49: 6x6, grade 0, 6 passes, par 45 */
    "6:GRWAoHLUW8sLSjwxirKPaEnK7UpQr78b64EZ", /* 50: 6x6, grade 0, 5 passes, par 49 */
    "6:HIRpQWDs7DrJQdAejtDfuefc8xtPeWBdElfZ", /* 51: 6x6, grade 0, 5 passes, par 50 */
    "6:IQYBpJKDoQdKEvcExdQiZAipSpPoQq4CaEc4", /* 52: 6x6, grade 0, 5 passes, par 46 */
    "6:8m9emJHToIVqL7FkrLDeewa5RfulpIEZCWCd", /* 53: 6x6, grade 0, 5 passes, par 42 */
    "6:RZIJ8nKObEoLVbHJFsUwljurKCYAaKFeZBed", /* 54: 6x6, grade 0, 6 passes, par 46 */
    "6:G8oOYICxbEeq9lfmGKJRxibKLLUwxbEd65EZ", /* 55: 6x6, grade 0, 7 passes, par 44 */
    "6:PfZ9foKPXApKLCoRaKCfrSecRuqTvn67Cc66", /* 56: 6x6, grade 1, 7 passes, par 49 */
    "6:RepOfm7PcDm6HLJILGKUqUqLKLTtKLCc64Fd", /* 57: 6x6, grade 1, 8 passes, par 39 */
    "7:BvYPefW8jpDwZH9oKAjpLGUjxerLKTXUYFsFiXKOW4AeflifW", /* 58: 7x7, grade 0, 5 passes, par 67 */
    "7:QmBoOeWLEnKTenLPkrCnL4LJVet7PdLL8jnKRs4HObFa6BkjY", /* 59: 7x7, grade 0, 5 passes, par 67 */
    "7:QwfmAfnLFpCeWKKGK8o9qUbSnDoLK9cLBjq7AxkvfsAejYFZ6", /* 60: 7x7, grade 0, 5 passes, par 66 */
    "7:RmAwnOX7UurEioI4LFxYLSwaQsRdLCo6LTX5PaQbKIAdBbAjd", /* 61: 7x7, grade 0, 5 passes, par 75 */
    "7:QZBfmHHKRvxdTaVa5UfaHTvWUfwbK4RiYTn7RsAfrKBc6Bfc6", /* 62: 7x7, grade 0, 5 passes, par 74 */
    "7:I9wfZPnDxlfmLLAcHIEt6RnTcQsG7KUxsDaOt44KQX5EeeilX", /* 63: 7x7, grade 0, 5 passes, par 74 */
    "7:BoAnIApHEpKSoKTWVjsKLKPa8s5LTaJQsQaLPs44SpEdDeed5", /* 64: 7x7, grade 0, 6 passes, par 67 */
    "7:OemQmGJL9s4Slc5GLJLJIOdTlkdLDud8pPaIVeetEpEjfXFfd", /* 65: 7x7, grade 0, 6 passes, par 57 */
    "7:G8fpH9oCexsTfbIOt7LOnK6TudL6DoKTulYHVqLFvnEb548b4", /* 66: 7x7, grade 0, 6 passes, par 71 */
    "7:OfewpQYKHQr5LJDaLEmSdBwaQbCnRlWSfpLFp8bISr8c8eid6", /* 67: 7x7, grade 0, 6 passes, par 59 */
    "7:JBp8nRpL8jfqL5VmRYUrIKLVeqLL5Tq9bCrIKUfxXKDdCWDZ6", /* 68: 7x7, grade 0, 6 passes, par 66 */
    "7:PZ8wfvnL9xq9cLL8sFfmLLJFep7KKFenUWLLRWUkWLFjfkZ9c", /* 69: 7x7, grade 0, 6 passes, par 62 */
    "7:PwfffepKUfufn76CWL9sIQxXLBkb4UXLBoJIFfiftKFeefejb", /* 70: 7x7, grade 0, 7 passes, par 64 */
    "7:8mQmRemQjdKK9aEfm7UwZ9pVfqDXBirJFeoOZLKQxcFfdCdCZ", /* 71: 7x7, grade 0, 7 passes, par 53 */
    "7:OYQnIRWTesLSlZ6IL4CwoIUrRfb7KLLEexWSrDuvbI568aFfa", /* 72: 7x7, grade 1, 6 passes, par 66 */
    "7:QfoQepH6GEb8lsBrAemHLILAxrUrLDebVaLKRZPjo7EifbBiW", /* 73: 7x7, grade 1, 9 passes, par 72 */
    "7:AfvxoIIQub5DltL4GBfxcLAiwXDoLBpTefb4QtUpOmBcEbEd4", /* 74: 7x7, grade 1, 10 passes, par 72 */
    "7:AemPefX9esFwfZ8mUZTvmJLSfsL4EjqBrCWBfjpUeW9ffdEeZ", /* 75: 7x7, grade 0, 7 passes, par 60 */
    "7:BoAupPoBq9aVdKGL8xsPdKVpL6TpKKDs8bKFrILRfb8bDbCeX", /* 76: 7x7, grade 0, 5 passes, par 66 */
    "7:QWIQfwmLGVkZ6KLLL8nAaSjivlxm7PxbAbLAdUwnI4Bfa4ClW", /* 77: 7x7, grade 0, 6 passes, par 61 */
    "7:OfX8ueYSefpLRo5OpTdK69tUlmLHPc49liqDfpPfYK8edFffc", /* 78: 7x7, grade 0, 6 passes, par 59 */
    "7:IHBvenIVjW5QaKEpQnTfdPsKTtPXLEdKCioUoQlZ8t54DeY8a", /* 79: 7x7, grade 0, 8 passes, par 74 */
    "7:J9xnRfWLPcUaQZCtI7AkoOilfwudCxZH7Tm8cRqOdKBfaEjY6", /* 80: 7x7, grade 1, 10 passes, par 60 */
    "8:IQfYAfnISjenQejqKHBljXGLKEpIJOdKEubKKDxdRinKKQjp7OqLLSp7AbCaDaDY", /* 81: 8x8, grade 0, 9 passes, par 83 */
    "8:GRoHIRfYTaSbEjupUnLIHAsLK5TqExt55ObFmK4JGKAfkjfsLTYBefxcEjefW8iY", /* 82: 8x8, grade 0, 9 passes, par 91 */
    "8:JBuW9xWGTZEpRdBqUpILKHRtK7VaEq6KKIFoRcJLVjwcFwlrKOlxwinL668bFX44", /* 83: 8x8, grade 1, 7 passes, par 77 */
    "8:QueZPYRYLVxWLOlm45Dvjd9sIAxivfY4EeqQkvfoRfcVXLOaEo9dBsUXBieZAdEW", /* 84: 8x8, grade 1, 8 passes, par 89 */
    "8:JQuZ9vnGLKTZBqLKLLLPnKLKDbSbDtEaPurJHSuo6LUrVqL6Od6LL6Fn69ebFeW4", /* 85: 8x8, grade 1, 8 passes, par 89 */
    "8:9ufWQoOp9jwoLK4LOfdSrSfd5IH4KKIHQtUo6LUs7CaExdK5G9evc9sGEefiefja", /* 86: 8x8, grade 1, 9 passes, par 83 */
    "8:H9pPYIPXVmEqGKTX6VYDtVcGObPfaLGLSukoJLVbLKHKTlqI4SbLDZCq9b8lfeeb", /* 87: 8x8, grade 1, 9 passes, par 89 */
    "8:QZ9wwfvnEvfsLRsLQaOt4K6LLQaTY59bK7BifoIGL9nHQlltLISlqAwsFbCZ59c6", /* 88: 8x8, grade 1, 9 passes, par 100 */
    "8:9pPYAmPWPbFpQdFmKQfdFfmLVcGHJJDqKAtUrUYLKILLLKIKFtLLKKTb8lidEkkY", /* 89: 8x8, grade 1, 10 passes, par 84 */
    "8:8fepRffWRvfkjexoLCwmRucLSX7467AqCwoJIQubQcEqVaDoLHIKLGJLEaCdDaCa", /* 90: 8x8, grade 1, 10 passes, par 73 */
    "8:IIIGHHIJLTjtSjsLTinVtRlsVXLKLK9qK9tKLSYLL9dKLDYLL9fsSfZKEXAaFZBb", /* 91: 8x8, grade 1, 10 passes, par 92 */
    "8:BnOXAnOYAsSoRrSXJKKVsLLHLSs46SqKUcToQtDrEnLKKKQcOqKLLLTn75745767", /* 92: 8x8, grade 1, 11 passes, par 90 */
    "8:8pPfenRWILKGILLJKUiqVlrKFjn65QkbQviW9lxp7LOZ8pL5OjiW9ijnDffWBffd", /* 93: 8x8, grade 1, 11 passes, par 83 */
    "8:PXApQWAnLIHKKJJLTdEqTaEqDxfqSevbRiYSsBloDWQdEmAcRuimPjun6EY579c7", /* 94: 8x8, grade 1, 11 passes, par 94 */
    "8:8eefnQZGJQXOjjWLKKPioHHLCqEpCilbIFesReeoEvwktQXLPdKArVYLEZCYClW5", /* 95: 8x8, grade 1, 12 passes, par 85 */
    "8:PwpGJPunKKCdCcLL6Dexxfb7JPeaCfmILSemRfsLEaQaDpFcQXTZAtApEfiWAlfc", /* 96: 8x8, grade 1, 12 passes, par 78 */
    "8:G8fm8oJGVffcHEitVpOfcPfbKCtIIVfYCmCsVcOWGKBkrHKJUaBwkqLKEXAbBkkc", /* 97: 8x8, grade 1, 15 passes, par 76 */
    "8:RmPufoRWKKLEWDkZKKVnGRwW5CcLDs7J8fflxkxbQefxsIFYUZOdUsOnFY5BaFb6", /* 98: 8x8, grade 1, 16 passes, par 82 */
    "8:9oOpHPepArLTbDmLPaLEpH6LK9bJFjwcUpJFvwko6EqAdVnLAfjffs4LBfefejX4", /* 99: 8x8, grade 3, 9 passes, par 103 */
    "8:PfepRffnLIG77GILUjdJGFjtLAftUeYLLPfsVfmKKKQdCpLLKKDffbLKCleYAeld", /* 100: 8x8, grade 1, 14 passes, par 72 */
    "9:QepIOxfoHLJCq7KJFaLDekwkbRZ6HAvtRxiZOrPqSdCeoLLK5En8fa6Es8m7PpG9ekuaQtFs8ffiea5Bb", /* 101: 9x9, grade 0, 8 passes, par 97 */
    "9:8efnPWBvYHIBsExxbJUrPaBaUea6TievxkvpBbBesDoK6AvfXK9aKGAiwxjn8kqHOb6HLAfqEjeZDleY4", /* 102: 9x9, grade 0, 7 passes, par 121 */
    "9:8fufYOfmI9fifeqJFaBwuYPcEwp9rKIFeecL9cDrQYRnLGRfrDmKCsVc9aObLI6KOfWLQkloDiffliW8a", /* 103: 9x9, grade 0, 8 passes, par 100 */
    "9:RxY8ffmPX7Eeen9rUWJAemVetDpLAut69joKUwbFepQisKDvYHViYLDpSftFfZ5BqSYCffpJ9dEffX9lc", /* 104: 9x9, grade 0, 8 passes, par 113 */
    "9:OeefnPW9nDfpILDpIKRecSsRbVaDnBqEiejXJ4JCnOuepKAiujaKRbTxfiXBtLHLCpHOfrCq6BaFkX79d", /* 105: 9x9, grade 0, 8 passes, par 104 */
    "9:PefoIQefYK9ekqTwvW6HIQsLLDYJDlaTtEeoL9eoKDfZKDeoTtRxp69nLLUa75G8kktTxeYKAefa6Fefa", /* 106: 9x9, grade 0, 8 passes, par 114 */
    "9:J8oI8m9wpKGLCulfsLKDqArBer6KQr9r8viXEaUwt8bAoJILK4OwvsUc6DuaL5LCueWUpSpL8kffc5765", /* 107: 9x9, grade 0, 10 passes, par 107 */
    "9:8evvfveYHHI7L8lfXLLKAifeuYKFjxepPkecPwbGVivWG57OqL9kXL8p5VkxfxaJTmTmEn5GCcDd48kfc", /* 108: 9x9, grade 0, 12 passes, par 109 */
    "9:GOpJQXQepK6LEimKJ4TucBwiiko7FeoFuxp5QwoUfbKTXLLCd9esKJ7Cp9pRdUq8eqPt5QsL8fb5Efa74", /* 109: 9x9, grade 1, 9 passes, par 98 */
    "9:Op9vpIG9m7KAaLDlmKOtRpKBmVaLCaLUecTZKRwlkfoDpLLFvwoTeqKLG5L75OtL6SfaQXKLCXDefdBb4", /* 110: 9x9, grade 1, 11 passes, par 94 */
    "9:OXReenQnGLHCYJK7TaTtRWTcRip4LFmVXL9rQaQlqPbG6CetQdEpLIIOtKBetEqSsKSffiwc776EfeXCW", /* 111: 9x9, grade 1, 11 passes, par 96 */
    "9:QoGBfxffZ5LKAflp9pBrLIBpCvcQaEtIEurHUnBjqRb6K7DuvsFufaJJKKSxkfnKTsLKFfYLFc76FYAfa", /* 112: 9x9, grade 1, 12 passes, par 110 */
    "9:GGRpQfoQWDidCdJLKGQmAeoTlisLFZHTjpHKLPwcLQkcKK6Ewq48mKTxvtCfZDa7LK6PnPup9bEfaFa76", /* 113: 9x9, grade 1, 15 passes, par 105 */
    "9:AvpGOXOYGPqVaFukfq54EmOixX6QexisQbPnKG6IK4G7LFcPbSwlfdHBjvbDnGJSexkpRsVd4Bb8a5FlX", /* 114: 9x9, grade 1, 19 passes, par 107 */
    "7w:9vifoFdecQYFXQQpEwZBdK7OaJIJa9jeqKUHQeulsLFlpEZKK", /* 115: 7x7 wrapped, grade 3, 12 passes, par 61 */
    "7w:QXPXGILKPjmKLLTaOrTicLI7SdOX7K9sArJ9jnSxrKeWEc46T", /* 116: 7x7 wrapped, grade 3, 13 passes, par 75 */
    "7w:fwbKKI9W4JVrTeenCrDdRpKJKOoT7Cq6LKLH9kfdK4CfvpQkX", /* 117: 7x7 wrapped, grade 3, 13 passes, par 64 */
    "9:QefZGPZPoUwpILUZK77KFkaVwiY9t8xpL69pJEvaSaIRdDxkZKOkiZOdPYCkeeZKISvpHOnGCld4DjcCb", /* 118: 9x9, grade 3, 16 passes, par 101 */
    "7w:RaJLUtGr9sLK6CCWDsFYIenRkp9lRtUZ49pKVjeveq7L8mKI6", /* 119: 7x7 wrapped, grade 3, 14 passes, par 60 */
    "7w:UaKOuYL7Alb7QbPZ8nHUoc8nFqKDpOq9qKBtLKPllfLKLDeZG", /* 120: 7x7 wrapped, grade 3, 15 passes, par 60 */
    "7w:o56OcJEcPYKPa8HVeaLRYtFfnUaAbOukleeBsDeefn9rPevXL", /* 121: 7x7 wrapped, grade 3, 15 passes, par 72 */
    "7w:enG8en8XKL9fkvPlcPoQaFvmKDjnPdCcPwckxeZKKAXDfX6Ff", /* 122: 7x7 wrapped, grade 3, 16 passes, par 78 */
    "7w:9sFrPcIZDflsOlY9nQliwZRjkeYE8b9vffWAfoDpQoZILQdLC", /* 123: 7x7 wrapped, grade 3, 19 passes, par 81 */
    "7w:c6BrAsCnQmDmLBkdLAir8ApFwetIPcPbQirCfkW6AbGQen8um", /* 124: 7x7 wrapped, grade 3, 22 passes, par 71 */
    "7w:WIIQWIRGDqVfcKSwdUoILKSmKDsKK7CsOlsLQftSp5c4Ad67B", /* 125: 7x7 wrapped, grade 3, 17 passes, par 55 */
    "10:BffxmQxefWPffs45TeenVYICX9dJAsKQbQoOmEoLKKOd67CnKKUbTwYAuqDrLGKDWAdLIKCrVwefvqVbRktTYArVjpDX4FW8a49a", /* 126: 10x10, grade 1, 11 passes, par 135 */
    "10:IBmOfxeoPYEuicPrRaCnBsHAa7KAwbILCufeqQjnFqH6OWVjXLOaKGL8rPva6QijqPdLSZ9cAvjqJKVZQvp7GUrL4I64Cfja7Cfb", /* 127: 10x10, grade 1, 12 passes, par 133 */
    "10:QuZHHHG9voLKQtLLUpKK4FrFqSbVd5H8sJ47GVXJFnLTYAqKPc8sKVWArKVXHVdVffsErGLTfrGJSetLDbRsKKVmFbBfcCdEdEfZ", /* 128: 10x10, grade 1, 14 passes, par 135 */
    "10:GBnAfnRYPZUYEfwbVxdJLAfesHL5JLKJ9pVlkusLKDnK79eqDaEullexwjeZBioOeaVuvZQfs4PftKSXTmEYKBqLEm769elW659b", /* 129: 10x10, grade 1, 14 passes, par 134 */
    "10:AoOmIJPnQYBqLSdDqKUZHCr5HH6UbICujucDuixbPs9cIIEWVmLEZJTtIAbKFwfitVkexcRqRZKLApVpLFbJLKICcKEffb64Feea", /* 130: 10x10, grade 1, 16 passes, par 136 */
    "10:AfpJHHGRfY9ftDqVdTeYGBjfrSejXGUoPpKLPpQq4DbUdErFa4HQvkwwjxoGL56OqVn45LVXAcLKEXAqLGRwcEvoGLDlcCZAcCla", /* 131: 10x10, grade 1, 18 passes, par 150 */
    "10:OXIRWAm9fmLBsK9mLH9qFnKDwlaLBq8tLJVWRd9qJCqDiwiffcLQjvesJReZKLI4AqUkfXVlsRpUifeXKH77CrRfwY5CfffjjZDW", /* 132: 10x10, grade 1, 21 passes, par 126 */
    "10:PW8epReWBnKJQftSepJLCkdGKLGFlcGAetLKSeYHLAwqLLSwYKKBtDcEcUXKLHEuX9xdJLKTejnPierLTaI9sSXIDtDfa8liXDfa", /* 133: 10x10, grade 3, 14 passes, par 124 */
    "10:OfeXHJAfemLAfwtVxeYKEpH4LL7HOaPkbHLLICloFfuijilufdOxkvX9wjvm64PjpRkn74IBrOtSnUWIUpL6KL4LPq7Cc8cCYDc5", /* 134: 10x10, grade 3, 19 passes, par 136 */
    "11:8nJBwfmQeepPcVXSY6TXOcUYDukxoEnTZUxxbHKDeqEYL7FWUkY9sBm7HGRluxYTwbQlsKQsLHLVXLQlbKKFbKFZLKAurKRoCfXLKAbLSsKOxX64Aeb65EdDZ", /* 135: 11x11, grade 1, 11 passes, par 178 */
    "11:An9fmGRnJIHBkpBjkdEsUcG8rGRYAoVqIKQjjkW8kcTqKKAffwxfwcKTbPXGL4AdRtLOjfljeeZLLSsOxYAvxubL7TqDWAd5UZ7PtUnOuwYDvX465Ea4EfWEZ", /* 136: 11x11, grade 1, 13 passes, par 177 */
    "11:ReWOefweeenVen7Bo6PffaFoVnILPrQmGQc4LSbKEaDdFxYEtOlpHIJPqPxcLOkbUc656FwaVo8imPnRoLRtLIRd5DcTaL5FsFoRfecHCZJEerDfffjffd8ed", /* 137: 11x11, grade 1, 14 passes, par 166 */
    "11:IBemRfpPZOoDufkr8iaQaLQbRerJ8nVX76QlnFrOjlxmRdRcHSr8n5KLBsQibDuc9tDoL5OvfkfoKJ67BdLReZK6LRYQWLCfoCoUkmSeiuXUoLEYDieY7AaCb", /* 138: 11x11, grade 1, 15 passes, par 162 */
    "11:RfwffmPW8mG6BcRfdUfwldRefjpPcQc8nLGBuibPjnQc5CnUfY4JLTWBurTeefsVlYAsK6HBesFoGPaFvbPulW7LCZQcRcCwefbPuifqPeaQWH4CYBaEffjfb", /* 139: 11x11, grade 1, 17 passes, par 161 */
    "11:IHIBeefxm8nUaLBnOpLEYKEoEpVr6L9xdBtJEtDXKQcHAtVvbAoKLRaBirTextLSkY8uaLBbUktPmPloK8frQtL6Uo6LGRt5LSWL49bTqSYLVZ5Bffb6FYDkW", /* 140: 11x11, grade 1, 17 passes, par 170 */
    "11:H8pH9mGH9foFnTr9irUZOdObLTp9ltRinCfdLL8wtLJKRfxr6QqVja4CW7LIL7KIAmIQvqTaITjfdL4LSjYLLOfmEvcSwYEsKRdRcBtTvYVqEnEeY77CX5DW7", /* 141: 11x11, grade 1, 21 passes, par 171 */
    "11:JRXIGPYApRYLSfrElmQbUZLSWSoJKVZKJFrPq5SjlejcI4KTZUeuefpDo6UvjmSvX7IEm74ObKLRnUfkpQqG4Cd5CupEsCqRewpPdEZTXUlpLLFXAfkXEY564", /* 142: 11x11, grade 3, 21 passes, par 166 */
    "9w:m756Oed8eLHBpCweZJLDfjwkoAqKG9ejXVfcUloQwfrOn5OlbLQirLGKRfb79t7q4EfwZBiekumRiXIBe", /* 143: 9x9 wrapped, grade 3, 16 passes, par 103 */
    "9w:n57TYLLUfUoRbBlljW77KGBeveYPYVc8oUenTWTxfltAqEftEXPrAa8fleX4KHIBvvuWObDretLL8sIJF", /* 144: 9x9 wrapped, grade 3, 19 passes, par 118 */
    "9w:fvfioFaJBBqJOkffaIfbErPfpBjfoRaKRbHQZTjxqEfdDwqAcVfWAec6RuqJ9efxmK5CaBo8dDaPfpPbA", /* 145: 9x9 wrapped, grade 3, 19 passes, par 108 */
    "9w:Tm8fd5SoLK48pRXLFas9nSlZKJBb8kkpBdKRAoPwt8nLKpVsKSfdLFL6LLVfwjXlfrKUoSX9J8a6LKSYH", /* 146: 9x9 wrapped, grade 3, 20 passes, par 102 */
    "9w:er9pUffpOBbBr59m458fpK9oLPXemClwikaBOlwmVevWJiZ5VkpEZCwoAbGTvwfc4PpStKLAfn6Cr55De", /* 147: 9x9 wrapped, grade 3, 22 passes, par 114 */
    "9w:EXPaTtJ4KZOjesKKITp4RWK7LKTSejutRdDaCm9qSq9pGwbPb7VxdCtQd9xbDYP5KAn5OwX5RdBqJLEnJ", /* 148: 9x9 wrapped, grade 3, 24 passes, par 111 */
    "9w:tFoAmTZDeDekYDlmPo8efxxpL6KefeqK6KJEJAxd6AbEpK9kweW9etLGRbGG8pKiijesTvllGOfY6LFvX", /* 149: 9x9 wrapped, grade 3, 26 passes, par 100 */
    "9w:8woK9dRYJBdKLGJSYL8ftLVbEesJBklrRfvbEuewrVYEpRdAqUqPnKVfp45Sc76UZEeZVxfm4BfoAq69a", /* 150: 9x9 wrapped, grade 3, 30 passes, par 102 */
};
