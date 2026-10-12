/*
 * Pipes' levels: our own, made by tools/pipes_gen.c (trees grown by chance from the source, kept when the
 * deductions alone solve them, ranked by how hard those deductions are), six chapters of 25 from the easiest.
 * Each: "<n>[w]:" and a character a tile (pipes.h, PP_CODE); in the comment its grade, its passes of
 * deduction and its par (the fewest quarter turns).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
static const char *const pp_levels[PP_LEVELS] = {
    "4:GH9oLUWKTlwrEY47", /* 1: 4x4, grade 0, 3 passes, par 25 */
    "4:8eenPYPcFxjmBiZ5", /* 2: 4x4, grade 0, 3 passes, par 23 */
    "4:JQfXLDxZSuim769a", /* 3: 4x4, grade 0, 3 passes, par 19 */
    "4:JJQZKKTnKUt6EbDX", /* 4: 4x4, grade 0, 3 passes, par 27 */
    "4:JHHILLVqEjs59fjX", /* 5: 4x4, grade 0, 3 passes, par 32 */
    "4:IAvZVWVXVYVYCflY", /* 6: 4x4, grade 0, 3 passes, par 23 */
    "4:BvfZOrOX7CtHAekc", /* 7: 4x4, grade 0, 4 passes, par 23 */
    "4:AfenI9vaEoUY9ijZ", /* 8: 4x4, grade 0, 4 passes, par 29 */
    "4:An9nILQdUjjoDfY5", /* 9: 4x4, grade 0, 4 passes, par 36 */
    "5:QfuxnTZK75UYSvn68aK5BfejY", /* 10: 5x5, grade 0, 3 passes, par 37 */
    "5:JRfeWUdRZIVukvbLKHDo5FjW6", /* 11: 5x5, grade 0, 3 passes, par 37 */
    "5:IReZGUkxutFYLL6Ovs7H77Eed", /* 12: 5x5, grade 0, 3 passes, par 31 */
    "5:AenRWHHLTYTqSioLVaPs56Ac6", /* 13: 5x5, grade 0, 3 passes, par 41 */
    "5:RZQXIFvcGLHUoTdKLDipCkeW6", /* 14: 5x5, grade 0, 3 passes, par 36 */
    "5:RoQfYL5UfoTuq9dL47ApCefea", /* 15: 5x5, grade 0, 4 passes, par 30 */
    "5:HIBxZKFudGUmUwr7Er4LBfkY4", /* 16: 5x5, grade 0, 4 passes, par 32 */
    "5:RwXRY6TpVnBdVaKJPrOsEb777", /* 17: 5x5, grade 0, 4 passes, par 39 */
    "5:RpGPZ6DqKJRoVcKLUrAs57Feb", /* 18: 5x5, grade 0, 4 passes, par 36 */
    "5:PpOfp6VdI4PjulnLBsBt69iX4", /* 19: 5x5, grade 0, 4 passes, par 36 */
    "5:AffxWJAerITxwkrKLUWK664Ab", /* 20: 5x5, grade 0, 4 passes, par 36 */
    "5:AeweZ8pKOWQlrVnKGVt55EbDZ", /* 21: 5x5, grade 0, 4 passes, par 33 */
    "5:OfoRZKRrLH74Uks9oSZK9ka8c", /* 22: 5x5, grade 0, 4 passes, par 33 */
    "5:BenJHRfrScEZVipQebAs78ffa", /* 23: 5x5, grade 0, 4 passes, par 32 */
    "5:GAwfoDmSX4GUiwZUrHVm66Ed6", /* 24: 5x5, grade 0, 4 passes, par 36 */
    "5:9fmIIRwbUc6UnTmOrDq457BlX", /* 25: 5x5, grade 0, 5 passes, par 43 */
    "5:AmG8pGDlwqUxxs77L5UYBbAkW", /* 26: 5x5, grade 0, 4 passes, par 33 */
    "5:PXRwYExs5H9bVvtAwq5KAd79a", /* 27: 5x5, grade 0, 4 passes, par 47 */
    "5:H9ueYEpUWHPklwcCfYSpAefc7", /* 28: 5x5, grade 0, 4 passes, par 36 */
    "5:9ewZGAfqPqOxit57TXCo9jYAb", /* 29: 5x5, grade 0, 4 passes, par 39 */
    "5:AfoHG8vlibISvfmTbTYK69iX6", /* 30: 5x5, grade 0, 5 passes, par 42 */
    "5:9nGHIOcVlbVxjfnLLQxs74565", /* 31: 5x5, grade 0, 5 passes, par 43 */
    "5:H9weXCutRWAsLTmHKVq5Cb5DZ", /* 32: 5x5, grade 0, 5 passes, par 38 */
    "5:BmGGHIUcTdVlwknKRsGK47Fa5", /* 33: 5x5, grade 0, 6 passes, par 31 */
    "6:RoIBxW7Uc9rHJK8erLKUvfibUbUeWJ4Akeed", /* 34: 6x6, grade 0, 5 passes, par 48 */
    "6:9pBxZIHDwtRcDp5VimHKQsG5UjbLVY78ejiZ", /* 35: 6x6, grade 0, 5 passes, par 56 */
    "6:Ao8nJJBlfjqLOfpRtLFYUqFdRebLPY69ejkX", /* 36: 6x6, grade 0, 5 passes, par 51 */
    "6:PXJRWISxbKPd4TpKKGOdUrUt5PdUdLAa8lY6", /* 37: 6x6, grade 0, 5 passes, par 49 */
    "6:H9fmIGSXGVkbEwbSfXOlekfpUWQXPq4Bjfb6", /* 38: 6x6, grade 0, 5 passes, par 54 */
    "6:RvwWGIKKExktL5ITZL5Ojkn5RbOfkp4BieW6", /* 39: 6x6, grade 0, 5 passes, par 50 */
    "6:J9emHJL9nCqLDujwcKPb8kvc7PvmVZBb7FkY", /* 40: 6x6, grade 0, 6 passes, par 58 */
    "6:BeoHPn8ujr7LOtPtRc574ViYIAmTZGFflkfa", /* 41: 6x6, grade 0, 6 passes, par 49 */
    "6:BfexXGH9fsBsCnGUZKQlkkvq4QYPrK8kfa44", /* 42: 6x6, grade 0, 6 passes, par 47 */
    "6:8efxYIGHAtQcVjpSdHKPkkwdKVoBtJ654Bld", /* 43: 6x6, grade 0, 6 passes, par 49 */
    "6:RZBmOWVxWSlWK6OifYEurJHIH5TjjtFfkfX4", /* 44: 6x6, grade 0, 6 passes, par 46 */
    "6:9oAoHIJUxiicDdVWAnHITvvbVkqKUW48d4EW", /* 45: 6x6, grade 0, 6 passes, par 55 */
    "6:QfefvX7BnJKIBeklsKPpQwiq7TaSWK9kZ78b", /* 46: 6x6, grade 0, 7 passes, par 46 */
    "6:RXIPfYKHTcQZUsL9rI5VjwkqAqPb8t8cCX9b", /* 47: 6x6, grade 0, 7 passes, par 67 */
    "6:GIGAvnLLFudKEt8qIKPkultLLAsBsKDWFZ47", /* 48: 6x6, grade 0, 7 passes, par 41 */
    "6:ReYIQZSZJUjYUYCtPnFuxtLKQbKUt77Ba6EZ", /* 49: 6x6, grade 0, 8 passes, par 48 */
    "6:BfenPW9oGUkY9lisGJApAsLLQkfsKK58elkb", /* 50: 6x6, grade 0, 8 passes, par 51 */
    "6:OeXOuZLHAcLGTlxwjd58cUuXBn8dVoAleea7", /* 51: 6x6, grade 0, 5 passes, par 46 */
    "6:9foJRYHBlkrJKRZRbLKFvknLSvs8rK447Aid", /* 52: 6x6, grade 0, 6 passes, par 50 */
    "6:HGJ8nJEjt9rLBeioUbPeeljoK8ffxqFYBfc4", /* 53: 6x6, grade 0, 6 passes, par 48 */
    "6:9pBoRpBieiaLJ8oQYLLBjlxcEnJOln9ijcBc", /* 54: 6x6, grade 0, 7 passes, par 49 */
    "6:J9pGGHKArLTtTpUqL54EcUtGAfurKKAfd6Ca", /* 55: 6x6, grade 0, 8 passes, par 47 */
    "6:PuYPxW6KG7UYJUlWKIFlvwbKHOaLQsClYCb5", /* 56: 6x6, grade 1, 7 passes, par 49 */
    "6:AewWPYPoToSW4Sd7VYBixvinOurTX7676DfW", /* 57: 6x6, grade 1, 9 passes, par 51 */
    "7:PwfxZOp4KBqPr7I48lrEoKBvfdILSwrJBs767LCvdGBeififb", /* 58: 7x7, grade 0, 7 passes, par 60 */
    "7:HGHG8mIKCkjwdLDoQejZLOlkvfYLKGRkvfc5LUXVuWAikZ7EW", /* 59: 7x7, grade 0, 7 passes, par 66 */
    "7:AxfpOvn8dPlt54Bm5ArGHPjuwlkr6OdSYHLRlYUXL4FYAlfkY", /* 60: 7x7, grade 0, 7 passes, par 76 */
    "7:RWAfnHJSefZKLLFfwvijqBusSpRsPs5LKK5LUp65VZ66FY9lW", /* 61: 7x7, grade 0, 7 passes, par 66 */
    "7:9xeXGOZHKHAqKHKScBqTrDlewjb5OevlnGJEpTmVlc9b66EfX", /* 62: 7x7, grade 0, 8 passes, par 77 */
    "7:PYJAnOZFpL9lqIBqFxfic8ivlufmIJTXEWKVjjweo5CW9kXDW", /* 63: 7x7, grade 0, 8 passes, par 68 */
    "7:9wXQZIIIUfsRjcSbOtTeW5QbSjeXQbOiufX69tQrPY9ea7CkZ", /* 64: 7x7, grade 0, 9 passes, par 81 */
    "7:9xXGHBpIUYEsRtDsOfbL4PiivoUY5GQrFqGPkqKQjt4Ad5FW6", /* 65: 7x7, grade 0, 13 passes, par 75 */
    "7:RwxoQYG47K4DetAvkvxubG7HKLEWVeltTuXL9vsKSX6Ac75CW", /* 66: 7x7, grade 1, 7 passes, par 67 */
    "7:AwoGQfmG5LLSo4Feikt4HJOfetGLDdOvtUaHPbLEjmCaAkYBb", /* 67: 7x7, grade 1, 9 passes, par 62 */
    "7:IRufoHGK4VX4KLKJKRWErTkilwuc4AwebTnIG7GRr5EjfkcEX", /* 68: 7x7, grade 1, 9 passes, par 69 */
    "7:OxpQfeZ76KVfZGPvjtPea45AqUZHAvfsTfbIK8tLOmFiYEjc7", /* 69: 7x7, grade 1, 10 passes, par 66 */
    "7:JPexufZLDo5VeXUwrH4JJK6FluidKBwxjup7RaVZ7K9cAiZ8b", /* 70: 7x7, grade 1, 10 passes, par 61 */
    "7:9eoBffoIGLG9mLCrKEfrLGSjuultDqQbEWKRrUfeYL67CfZBb", /* 71: 7x7, grade 1, 11 passes, par 61 */
    "7:OpJGIHI5UrTikqG6ErHBrLRZSjWLFlujeoLJPkfmLKFcBeb76", /* 72: 7x7, grade 1, 12 passes, par 69 */
    "7:AmJQeeWBsLSfwX9jsDmDZOxlxifZLFZTufnSpAqTmL7CY7656", /* 73: 7x7, grade 1, 13 passes, par 63 */
    "7:BnHAvoIQijxtSb4JRs65HPibVwec7OvrExZPb5VXCo5BelXAa", /* 74: 7x7, grade 1, 13 passes, par 85 */
    "7:AoHGJQYJTbSrSWCs9q7LGQjxlfiq6JLGRfrHLSb7HKEljeYEc", /* 75: 7x7, grade 1, 14 passes, par 69 */
    "7:9p8m9wZHDnExdJFfkviZKRuXSwfb7CutSZG9xd5KJK8jWAilb", /* 76: 7x7, grade 0, 7 passes, par 63 */
    "7:IPpIOfYEdTklWG8ekxevcBxutGTXBc4SaUnPemUmKL6Ald567", /* 77: 7x7, grade 0, 10 passes, par 63 */
    "7:9wZGJIIHSucLLLK5KOjjrVulkepKLSuX8s4LKTmRlp7774FZ5", /* 78: 7x7, grade 1, 9 passes, par 57 */
    "7:I9m8wpJEvsBr6KQcFvdHL79fsOaLPeukkud4Oq9vio9aFZEY7", /* 79: 7x7, grade 1, 11 passes, par 69 */
    "7:8foIJGJBekrSldIHIEtPXFjiulsIQeerGFc5AwijeWAflfeeZ", /* 80: 7x7, grade 1, 15 passes, par 64 */
    "8:GBufoOeZLJEZFkvZVloBvmUX59tAtLLG8mFfq4LKOkvwkutKL8bVmKUsEY9b5666", /* 81: 8x8, grade 0, 9 passes, par 83 */
    "8:9oJQeWQXHTbUZRkmCtAsOrRaBsGTs64HISlaVeucSjYGSnUoKGJSc6K77FkieXDZ", /* 82: 8x8, grade 0, 11 passes, par 85 */
    "8:AwxYPeXGAcUuc8nKIAcDnPlqDuwer6PcRa78lvimDvYQxknKJK8aLJKLCa9fbFc5", /* 83: 8x8, grade 0, 12 passes, par 94 */
    "8:H8eefxfXTX9xvaRnFxZKSfd7GTZ4SoPZKTxxtFlYL4L4VxwZEnUX65KG8jc8eflc", /* 84: 8x8, grade 1, 6 passes, par 83 */
    "8:8eoJJJ9mAvsSlcBrBcUtGBvaHI6LCvlnUlukxlW76AtJVwunBvcFbK779d9fekeW", /* 85: 8x8, grade 1, 9 passes, par 89 */
    "8:8pHIPoPYGKLEbLKJLErBfrUdTviYBtSXKSwwxlcI446LTeftBeedTWH7BfefiejY", /* 86: 8x8, grade 1, 9 passes, par 86 */
    "8:QfZGBvWHTWGUWLOrVxrTYCs576TsRedJQeqSixmK7H6LILVbArJKEbSp9llkX9b7", /* 87: 8x8, grade 1, 10 passes, par 92 */
    "8:8efwpAwZ9feqLIUm9vvs6LK7I77SvkrHSo8aVpDcKUxesTpJ6LK9r7Ta8d69lWCX", /* 88: 8x8, grade 1, 10 passes, par 90 */
    "8:9nJ9wo9nBikuqCXLIJAr7BwaSjvlWJCn48kexlfbRefwjxeYKOvt9luW5547BeiX", /* 89: 8x8, grade 1, 11 passes, par 87 */
    "8:8xYBeefn9tAxYQvaJSvkucVYUsSpUYCWK566UpQmK9mHLLKL69qEsEq49fjelYEZ", /* 90: 8x8, grade 1, 11 passes, par 87 */
    "8:OoRuYJIH7Ts6JTkbH6DxcKQWDexjwcLIIRcOkulqTdPsOlX6TY5KSwvYFeX746EX", /* 91: 8x8, grade 1, 12 passes, par 105 */
    "8:BmHAoBvYJTdBqJSmKSW8rFt6EsIRiejZHCksOeWGLBulloJLLGKHRljtEjlaCXBa", /* 92: 8x8, grade 1, 12 passes, par 74 */
    "8:8ffuwfXJAeoK6Awb8nFqJGLHIUZTjaKLTkXTxfkrUwvsVep56LL7EpFpBcCeXEZ6", /* 93: 8x8, grade 1, 13 passes, par 96 */
    "8:HPYPW9enCkuiW8xdJHK9foTZUlbRnLLIKHAdSkkdFlxvjwXHAoKKRlvrBlc758a5", /* 94: 8x8, grade 1, 14 passes, par 100 */
    "8:AmGJHApIHFlqTWVqCnHLTfb6IKFqLBoGUkukkvjsL9sRnLG4UYL7LUknFW4BklY4", /* 95: 8x8, grade 1, 16 passes, par 91 */
    "8:IBxeYOZGUviXHDwd5FoHFxiYG9jkvlXHDoHJSxvaHEjktLDpL8wfcKRrDec8ed45", /* 96: 8x8, grade 1, 16 passes, par 103 */
    "8:I9upGHPnLG6VtLL6Fiut4SjXIAaUnLHHFvW6SilbIUuwkevWFcKDvpUW8fcBb6EZ", /* 97: 8x8, grade 1, 17 passes, par 93 */
    "8:J9oG8mAnKJVdHDuaUrLPcJTWLFklpKTZKPnPillo76UkxX9s8wjmVenKAlZ7DZ47", /* 98: 8x8, grade 1, 18 passes, par 93 */
    "8:GQfeZRWHTs9xYUwa5KITZK4GAsDlusQbAtQvtTlWJSdK6SuZEsAqQaLIBbAdDXEd", /* 99: 8x8, grade 1, 26 passes, par 92 */
    "8:BefvfWRYHIJEfxioEkrAetG5PmFvoKUY4VeaTrVnIVnBsTa4Dr4BdVeY8aAfekeW", /* 100: 8x8, grade 3, 13 passes, par 86 */
    "9:IBn9eevuZVnFxfvqDZ5UX7AcUfYPivxufaRZ7IKLExoLJHCtSpKUitSxq4LKTo565LRc4LTZAec48edEY", /* 101: 9x9, grade 1, 9 passes, par 107 */
    "9:8oApRXHJGIDnCjvaTsLAimIUfd4SefitVeepUX9pTjwn47Rejln7VXIVYBviYUoSleWL8eaKEYBeaBffd", /* 102: 9x9, grade 1, 11 passes, par 105 */
    "9:9fnOZHHIG9nSrJFqVtIL6KVWTs7CjeirOtDnIIAutKCW6Vc8qVdHAoKH9cUwifcUb9nLVfeYEeflaDefX", /* 103: 9x9, grade 1, 11 passes, par 117 */
    "9:IBefwWJPpLIAfs8jr6LEuYDxwcGK8jefs4OsDwuxwrJK6Ob76LDsTo68fulZSr5BexcJQcTY9eiWDa8kW", /* 104: 9x9, grade 1, 11 passes, par 112 */
    "9:JAefp9vfXK8pHFukYJVvdEwkvWL5Cefko4BsPuexwlwvd5Sm5KPcTZOr79dEWSXKSXGBvwlp74AjedCY6", /* 105: 9x9, grade 1, 13 passes, par 114 */
    "9:IAvXGGBxYKAqIUlYTXCxkqUffkYG5HCrIJ9oDwlutKSWLRlp7UijutUXLGVfW4K49tUio9wq9fa48dAd7", /* 106: 9x9, grade 1, 14 passes, par 105 */
    "9:I9xuevWQmTW7KJ4Qt4L8oSaOtDoUwtKQsTXL44UkqKSZLJQb9qKVp7Tln9rL5CZSY7QtVexpCZ9a4DZ75", /* 107: 9x9, grade 1, 14 passes, par 101 */
    "9:OffxZ9wfW7PXFnHVeYOb8nVkkWJDxfdTvvxdOq9fq57LH4UuwsIIFaBbK6StKRX8etBqDkrJAfaAlZBlb", /* 108: 9x9, grade 1, 15 passes, par 113 */
    "9:8n9ufpOeWAlflp4SwXJGApSZ6LJEsJKUfYVdJFjirIJSoVvuxtKKL6K574UjlqIVZ9wjn9lb68edAiefW", /* 109: 9x9, grade 1, 17 passes, par 100 */
    "9:RepG9emQW6RjqApUiXBd9s8qSfYRxXVpKUpJ5VxdUlcSrAtLBtHQaLAbVWTbVXK8fbRinLOdBfflW57EZ", /* 110: 9x9, grade 1, 18 passes, par 105 */
    "9:BnHIIHOWIBqCtTaEud9tRbTZOkZHUjnVfkxmCsGVixXL5QaVcOsRqG6RqIKL4EbOaKErDxeW69kWDXCeW", /* 111: 9x9, grade 1, 21 passes, par 107 */
    "9:AfxfeWApHHGLJGHGKKUjsLSjidLLAltEwewdSWQlxcH5IVoEXVZSxr6LPuler46AqKFpQjvZ9b6BdFXCY", /* 112: 9x9, grade 3, 12 passes, par 113 */
    "7w:qAtKKJ8lfs7Ta8GQjwkuoCqQqAc6H65UwXIK9ea5IKsBunQdE", /* 113: 7x7 wrapped, grade 3, 13 passes, par 67 */
    "7w:jbPiZLTBpUZ9s4JKUYITnKSjvis5aUXUXDuBrRjnAcHK4J4GH", /* 114: 7x7 wrapped, grade 3, 13 passes, par 83 */
    "7w:GAfvW9tFmIFeoLJLSZOldEllwqRYBewbCimnBrRfYC49kjeZJ", /* 115: 7x7 wrapped, grade 3, 14 passes, par 58 */
    "9:BfnBwZAuZAfqHUpOiYIJDks4VYIUkwpTuqQc5BbTs4VjnBffqUpTm5An8rLK6EWGUo4KSvfnEaFfc66Aa", /* 116: 9x9, grade 3, 16 passes, par 119 */
    "7w:d8cJGIPwWGLCiiivcVY9fejwsJQfuxcSs586CmLFfpJAtEfX5", /* 117: 7x7 wrapped, grade 3, 15 passes, par 62 */
    "9:JAfpI8evXFmITbGQqJBilloVq6K9xuxbK4IKOsLTxiwlaL66KCmCeo5IBdPkpI5HDepKGVipFeflidEW6", /* 118: 9x9, grade 3, 20 passes, par 95 */
    "7w:RrAfmPXa79mTbAnPZSjYAVaGTvfYTfjtTXGqAwrEYC5QbDefX", /* 119: 7x7 wrapped, grade 3, 16 passes, par 89 */
    "7w:IVWJGCsCjwaCmK8fjuwq6wxer55Ac49tRWBffuktIABn6BkrI", /* 120: 7x7 wrapped, grade 3, 17 passes, par 64 */
    "7w:lm8sAb9GLQbQYIdStPlwi8dTr8iWIObCxeZFlvZTvYIBdG7SZ", /* 121: 7x7 wrapped, grade 3, 19 passes, par 65 */
    "9:BoHBvXPZHISdIKOjecKVelqLAuZEqAurTeinG5BtVbGI4VeetTfdTY7I8qSfedIRjwcSeuxb7AbBb8aEX", /* 122: 9x9, grade 3, 25 passes, par 106 */
    "7w:fXG7Axe9xlXJVWYFoHSlf9xliqQWOjwnLKG6Ps7FlsAaDm9fd", /* 123: 7x7 wrapped, grade 3, 21 passes, par 66 */
    "7w:9wxluwaJ77OsDYUeY7LPZiuxflsO8bLOY66wfllZH859euZCo", /* 124: 7x7 wrapped, grade 3, 22 passes, par 62 */
    "7w:RvXFnIK6Fwvis6Aut6ISXAaTvb7HfxqUwWDudL4KBu7BdJDWL", /* 125: 7x7 wrapped, grade 3, 29 passes, par 70 */
    "10:AffueYIQZIHAmLHHLLGLEflktTcKLLIJPXLUZVcLVqKGLExies4TlkjvkxW4ArI8wdQifpOtKI59tH9rLTkcGJKEet5EeXFjkeW5", /* 126: 10x10, grade 1, 13 passes, par 143 */
    "10:JG9ffuxuXGKLJAet6EfdDrFwXLOYOZJTZUZLTejYSloFnSqBxY6JUulq4RkXGFsLJUZKQWTws4UlxkqI5KFoTYFnVdBbBcDYAaCY", /* 127: 10x10, grade 1, 14 passes, par 135 */
    "10:AeoAoIPZRZHGLAisVZLGSsFmBrSfcK4SerGKLRfc9cRqSdTjeWH8dTjerBepDemUYBsBwaBxsL9xkvjZ8cFdHLOiuo8feeldCW45", /* 128: 10x10, grade 1, 16 passes, par 120 */
    "10:HGAveXAwYHKVoKAxXSebL5FlxkXLPYFoIGFvXKLHBqUjesPbKK8rLQmStPkqJTr6TcVlWKDaLBcPrBftQesQvbCvp75Ba5CeX5CZ", /* 129: 10x10, grade 1, 19 passes, par 155 */
    "10:IAoRWPeYQWKHKLHSefiZCsFkrVeXQWRkfwsKBxdH7HBdDqQkfbHFnQukkwxWTWUrTmAbUnEwcKLLJQt4AsG7KEr7VZ8jlYCWDXEX", /* 130: 10x10, grade 1, 21 passes, par 147 */
    "10:QmJIOXRfZJ7Ekkr9qJIL8ufpFoTdFsI4AjerVfucSXBp9rTpVoUxetJSt5KK6K8jltVmK79qRvuaL7FYPb7LL8tQwn48ecEYCa56", /* 131: 10x10, grade 1, 29 passes, par 131 */
    "10:QmAeuZOZQZ6SnGUeaRdGI7FlqHAqAqUeeektJSecUZBeftEtOY5AmJGEwaVW9oTkkulxlXQkq8xr9kfWLBiYKTvewXFfYAd67AiX", /* 132: 10x10, grade 3, 19 passes, par 122 */
    "10:RXPZHHGOwYVXKICls5TWDmLVvZDucG9itK4JBsGKBuaFurOqTsRjuW6UaCaL5GSvxlwem4HEdLLQjZUWDpPrLTnOipBjb56664Bb", /* 133: 10x10, grade 3, 21 passes, par 138 */
    "10:GIIAevXReWEtFxWTfaQYAkxsQaJJUW9waSjWScVW9qPlufjfkpAbKRkup8ubBxdVW4UZLGBdOsG8tI7L8fsLSvkjxqAfc75EYAc6", /* 134: 10x10, grade 3, 26 passes, par 126 */
    "11:9fpPWRefeeY9wijwkYIIIG8qOftHHSikaQa7JTjltHHG6AfikxYLVib9mBfxloSqGHIUeesAtLDrLVtPfr8bEYEc67UZUWQeZRp9wsISvlxeq69d7Cc78d9kX", /* 135: 11x11, grade 1, 17 passes, par 170 */
    "11:AeufZIOeXRWI9tGJVdBpKHKQjaVlfYEqKLLHITY9fxbLSikkt9xvlfd7QffrPr7PuXIEZAiqFwrTWFoBvvsQs47HPkftL5KVufsK8xt5ArL6RaFW748fc7BjZ", /* 136: 11x11, grade 1, 18 passes, par 169 */
    "11:OffumAxoGQY68ftCW7TijZIIPr8xwlWRYUitDeqFxwlXLPbBoSWKDZGLKPZTlmDfwt75CeqBjwo5LH8eutQeq4J6EwwcLCXSvlZAsKBkoG4TwXAa4Afka8dDZ", /* 137: 11x11, grade 1, 19 passes, par 155 */
    "11:GPWJGPYAufZLUZLSaIHSXJKLGVcAsUiWLLKKCuWLLPYKSllfsAsSkebKOxmDxsLJIIKK5Dfq6VlitKLJAmSxjWJL57KJTcTxeb4IHEqTo5EvfZCleldCfXFeY", /* 138: 11x11, grade 1, 23 passes, par 144 */
    "11:IAoIOZGHQenKRqUioSaLI6FcFrJ7UYVdJIQnTcRkeqRdK4FtIKIGFipK9fitUltRZKTevuiqJDjZL4AdCWScHOZ7QfeueimTieY79xtIITrOeWAfcCic7DifZ", /* 139: 11x11, grade 1, 30 passes, par 141 */
    "11:OZRZRpQuXHHEeq9dEbKGVdQWUvuYJKVaGTvt4EmTbSed5448frTWLPWBxwevrK8rSoAc6I6UjvtLLH8esHVY7Vq6CoJDlqIQrUXGUjvvjr7KUZEd8aEY49aEZ", /* 140: 11x11, grade 3, 19 passes, par 174 */
    "11:Affm9oBpI8oIOoK9lpUsILK4LLHGDsFjcTwrUbKHSvmG56UivdVb57LJ8cIEulxvucFfft8q8rLVYH8oSxqI6KTYTvkcKSqG6EY549eaLUlXGG9eeffbDffib", /* 141: 11x11, grade 3, 23 passes, par 171 */
    "11:BxwXOZAvwmIILTYDmG64TbK7Dveia9fkmDpITZHRX9uq9sFqIFiwWL59jwkdOXVYLIOvlxwjxsOaK6L9tUn6VjftBbBbK6OqGBsHHIQs9sSrBdFika68a7CeZ", /* 142: 11x11, grade 3, 28 passes, par 179 */
    "9w:LHDqKHDfXtSXUjaPwfKTvrIRtCW7L6FtKSeYXK9ujrEweI6PjpKH6HrIUo5Tc8ila74GSffvJ8oHLDuX5", /* 143: 9x9 wrapped, grade 3, 20 passes, par 128 */
    "9w:uZG9mH7H9LHLJLLQdJcCtCqVcQj8frRcTxkmJ8tDvsVWKVvjwq768cb6OrUwvXAQxq66KFnJc448fko5F", /* 144: 9x9 wrapped, grade 3, 21 passes, par 118 */
    "9w:sIFjnKGG8LL8oKUcLHjlZSbKISjXBpExdKUwBwjpUwjbKQjXCqCfnLFXOvjnH7KIBdCWDkYLUWIRYOfY4", /* 145: 9x9 wrapped, grade 3, 21 passes, par 118 */
    "9w:4BfxeewYIupGKQW4IEaTtKLRecBudVikaRZAkZVfvwsIB9edAt6SkZfunRjo4JBusVt9jft9s54FeXAdA", /* 146: 9x9 wrapped, grade 3, 24 passes, par 101 */
    "9w:r9nLIIJKO48sTbCrTtGArUeZCr7CekkevurJBuveutKFtJ7LBqKCY6jmFY5TwfvHLBmRs78sL7Aq4FYH5", /* 147: 9x9 wrapped, grade 3, 26 passes, par 110 */
    "9w:ZBqJAkZ6PBvbCxeWBqflxxcOZHUPY7VejnK6lxwioAaKB8qLAjvxlYY6KOftTffxm5LQtUvf55I74L4SZ", /* 148: 9x9 wrapped, grade 3, 30 passes, par 107 */
    "9w:8vZKL9qHKwsPc4IUaD5LUYBsSxWvlivujr5P79urL8rI5evsKCYFbRRt5TvmRn5d5PdLUb7BeW4IK4OWQ", /* 149: 9x9 wrapped, grade 3, 32 passes, par 124 */
    "9w:wwkeeXAsDcVW9mHGL9pLIPaLDr9SlkkxlxqGbBuujpLKDfubSpK74RBdI67TeXKAwsQmTZI79cSb7CWKH", /* 150: 9x9 wrapped, grade 3, 38 passes, par 113 */
};
