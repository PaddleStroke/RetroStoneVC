/*
 * Loop's levels: our own, made by tools/loop_gen.c (a region grown by chance, its border the loop,
 * numbers taken away while the solver finishes it at its grade), six chapters of 25 from the easiest.
 * Each: "<n>:", a character a cell ('.' no number), ':' and the loop's sides (slk_decode); in the
 * comment the reasoning it needs (its grade), the solver's passes, its numbers and the loop's length.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
static const char *const slk_levels[SLK_NLEVELS] = {
    "5:.2.20.0.2..0.2..1.1..3...:mEgFEKRRFG", /* 1: 5x5, sides and dots, 9 passes, 10 numbers, 18 long */
    "5:....003.....03.0....0...3:GhSh5KMSMk", /* 2: 5x5, sides and dots, 9 passes, 8 numbers, 22 long */
    "5:..22.03112.3..2....10.2..:GtQjYKkiMU", /* 3: 5x5, sides and dots, 11 passes, 12 numbers, 22 long */
    "5:.322122..2112..002.....21:NxFSYXRoYU", /* 4: 5x5, sides and dots, 12 passes, 15 numbers, 24 long */
    "6:1..2103..2.212....00.2..00..13.1.2..:Ebrko0MMCxFGSK", /* 5: 6x6, sides and dots, 10 passes, 18 numbers, 32 long */
    "5:.0000.1.1...1132.1.3.211.:AwfO-AAhUh", /* 6: 5x5, sides and dots, 14 passes, 15 numbers, 22 long */
    "5:.022....1...2..011..1....:YKF58oUowi", /* 7: 5x5, sides and dots, 14 passes, 9 numbers, 24 long */
    "5:..10.22..0.01.2.1..33.211:CQL2HGGRiF", /* 8: 5x5, sides and dots, 15 passes, 14 numbers, 22 long */
    "5:0...0..21..2200212.1.....:EBJw-MKGFh", /* 9: 5x5, sides and dots, 15 passes, 12 numbers, 22 long */
    "5:123.0..1.2....2.21.1.00..:kFVkgMRtrw", /* 10: 5x5, sides and dots, 16 passes, 12 numbers, 24 long */
    "6:.2222..12.2.22......3..0.02....0..0.:zsWEFIAVDt5CGA", /* 11: 6x6, sides and dots, 14 passes, 15 numbers, 30 long */
    "5:.....21.111121011..1.2..0:+Og5CiFFRD", /* 12: 5x5, sides and dots, 18 passes, 14 numbers, 24 long */
    "5:..22231...2.21.0...2..02.:6Q1igutSow", /* 13: 5x5, sides and dots, 18 passes, 12 numbers, 26 long */
    "6:..2...1.03.010.....0.20..212.......0:HIIAJGIJikIhEM", /* 14: 6x6, sides and dots, 15 passes, 14 numbers, 22 long */
    "5:....30....2.12.21.2.2121.:QgIR+ww2th", /* 15: 5x5, sides and dots, 20 passes, 12 numbers, 24 long */
    "6:.0..1......3013..2....110220.10..212:IkEWCE4YojXIRk", /* 16: 6x6, sides and dots, 17 passes, 18 numbers, 28 long */
    "6:.2..3.222.....210.......30.30.......:ecBNJJOiMUQSCJ", /* 17: 6x6, sides and dots, 17 passes, 12 numbers, 30 long */
    "5:2....112.31....1.1...20..:fGEICh1FFD", /* 18: 5x5, sides and dots, 22 passes, 10 numbers, 22 long */
    "6:00.12..23211...3.11..11.1.....010..3:wECIUKgQ5qVofw", /* 19: 6x6, sides and dots, 19 passes, 19 numbers, 30 long */
    "6:2333..3..2.03..2.02.20.0..2....2.3..:KFJFANOeiooQBJ", /* 20: 6x6, sides and dots, 20 passes, 17 numbers, 28 long */
    "6:.11.......203102.1.0...021.0...2....:fYZZEFGhSIKRBF", /* 21: 6x6, sides and dots, 20 passes, 15 numbers, 30 long */
    "6:.12....01.3..3...0...1.......00.31.1:HYVSQUcJC5BFGS", /* 22: 6x6, sides and dots, 20 passes, 13 numbers, 30 long */
    "6:.3...........20.00.23..121..22..322.:DCAK0bkFGMoTQ2", /* 23: 6x6, sides and dots, 22 passes, 15 numbers, 30 long */
    "6:.1211.32.1.23.2.20...31.311.00......:-lRBJBCBd1xTBD", /* 24: 6x6, sides and dots, 24 passes, 19 numbers, 34 long */
    "6:2231..3....32.1...02...0.2.2.1.223..:G1taYLXKqKiBpc", /* 25: 6x6, sides and dots, 27 passes, 17 numbers, 38 long */
    "7:.0......12223.0..0......3.1...01.1....2.01...3...:gZBjCCIIwDK1EiLoQiI", /* 26: 7x7, sides and dots, 9 passes, 17 numbers, 34 long */
    "7:..2.1..1.0.2.02.0.21.2.1..001...3..023.....0.....:HwAIDGNEAkQIhogIMAA", /* 27: 7x7, sides and dots, 16 passes, 20 numbers, 26 long */
    "7:.3..1...0..0.3.1.0.03.12...2..0......0112...0.321:6LC8IKwgh4YQBiEMQCD", /* 28: 7x7, sides and dots, 17 passes, 21 numbers, 36 long */
    "7:01...1.1..02..0..01......0....0.201......2.....3.:ItIgECIABht4kjIkgCG", /* 29: 7x7, sides and dots, 18 passes, 16 numbers, 32 long */
    "7:..3...22.1.2..2.3.2..012.3......1.000322.0...12..:lVVTKJC049a0OiHFYQE", /* 30: 7x7, sides and dots, 21 passes, 22 numbers, 48 long */
    "7:.0..23.00.2.221.3......11.13.3.3...01..11...0..2.:YRRedVlAAjOk8EofgCK", /* 31: 7x7, sides and dots, 24 passes, 22 numbers, 44 long */
    "7:.....0..322...2.213102..1..122.1.10.221.103....1.:D0Y52NLAyUwKRgUErsC", /* 32: 7x7, sides and dots, 26 passes, 25 numbers, 44 long */
    "7:..0....2......11..3.01.1...11.121..1.3211121..100:BAYBAa+MeMwA5kTgFEB", /* 33: 7x7, sides and dots, 32 passes, 24 numbers, 36 long */
    "6:1.2.1...002.201...........00........:8jQNCAAEDFxAAA", /* 34: 6x6, corners, 7 passes, 11 numbers, 20 long */
    "6:2.13..1............3.310.....1.32210:PIIKZaGRSE5BJF", /* 35: 6x6, corners, 14 passes, 14 numbers, 30 long */
    "6:.....1.....3..3.2..31.02..231...3.1.:AwkqOM8AgyTIUi", /* 36: 6x6, corners, 15 passes, 13 numbers, 28 long */
    "7:2.112......0.3.120.......120...02..02.2....1.....:fEHkkBJIIEyhEhiIUwA", /* 37: 7x7, corners, 12 passes, 18 numbers, 34 long */
    "6:2..3..1....3.3..1.3..122....31.....0:3oqkBQAZD9dGMA", /* 38: 6x6, corners, 16 passes, 13 numbers, 32 long */
    "6:...0..0.......233...2.032.3..32.....:AEQpkm-AYw74qg", /* 39: 6x6, corners, 18 passes, 12 numbers, 32 long */
    "7:1...22.12212..0.2.3....2.2.2..3...2.12...2...20.0:Kp51rFioI4tggJIu0xA", /* 40: 7x7, corners, 16 passes, 22 numbers, 46 long */
    "6:...21132....1.2.1.3..11.......0.0.10:MlWmBYAU2HcFKA", /* 41: 6x6, corners, 21 passes, 15 numbers, 30 long */
    "6:.0.3.31...2..11..1.3.1......3..0..3.:oUzVSQY4JGxFGU", /* 42: 6x6, corners, 21 passes, 13 numbers, 32 long */
    "7:.....3..301.0....0.023...3..1.3......32..2....1.1:k0KY6RiMAxmgEGHu0BG", /* 43: 7x7, corners, 18 passes, 17 numbers, 42 long */
    "7:1...12.3.1010...32.......13.1.13.......122...00..:8GWJ0IEgBQUg+2NugCA", /* 44: 7x7, corners, 20 passes, 20 numbers, 40 long */
    "7:.0.11.0.1...1...30....1....0...02.1..101....1...1:AyQgCFwyfAwKnsxJHGB", /* 45: 7x7, corners, 21 passes, 18 numbers, 40 long */
    "6:0...0..1.02....13.......12..112.2...:AgTSaE-AAXdczg", /* 46: 6x6, corners, 26 passes, 13 numbers, 32 long */
    "7:....23.13...3.1.2...101..1.2.0.2.2.012..02.3.2.2.:oFRzAWsWcjr3CiIYIKL", /* 47: 7x7, corners, 23 passes, 23 numbers, 46 long */
    "7:.3.3.1...1.1.02.0...12...1.23.1.....2..00..2.0..0:WUMAcsmCCoDJhEo0FMA", /* 48: 7x7, corners, 24 passes, 20 numbers, 36 long */
    "7:.0.0....1..3.30.....2....3....2..220.2.3221.1.1..:AoiiOMqicCA-EiBS0KN", /* 49: 7x7, corners, 27 passes, 19 numbers, 40 long */
    "7:...22.......02.1..0....1...0.3.......010....10...:n1QEkEFCGk6hLtSHDUA", /* 50: 7x7, corners, 34 passes, 14 numbers, 42 long */
    "8:.0...0..0.01....2...3.21..31...0.0.0..3....2..1.3.2.......0232..:AAkKRRUdxsCEAAY+qJByDNBG", /* 51: 8x8, corners, 14 passes, 23 numbers, 46 long */
    "9:...10.....12200.0.......1...3.3.......1.3221.3.0..2.....1..1....1000..0..1..1..0.:EAAIQJaYCzYIA4HMADQBKD5ENESQEE", /* 52: 9x9, corners, 16 passes, 29 numbers, 48 long */
    "8:........0....0.......2..3212.3......3...1..0...0..0.1...3.2.1.13:AAAEpIDFAJa-AAAwYJdIEh7g", /* 53: 8x8, corners, 23 passes, 19 numbers, 40 long */
    "8:.21.1233.31...2....3.3.32.1..2..2....2..0.32.....0...1....0.1.0.:-O0agGgIEABABng5OLahHwAA", /* 54: 8x8, corners, 26 passes, 25 numbers, 48 long */
    "8:.....2.31.20.3.3.31..1.3.2.100..1...2...1...1.0...3........0....:YTYoCukMCQAAolcHMQcRBMAA", /* 55: 8x8, corners, 27 passes, 22 numbers, 42 long */
    "8:.3.2.1..312...003..221......1...1.3112001.1...1..0....23.23.3.2.:aERAxACBgT9KuoDeoJtIEBsP", /* 56: 8x8, corners, 30 passes, 30 numbers, 52 long */
    "8:........0..0....2.....3...232.2231.2..1.......2.131...0.....112.:AAgUFkYm7KofAAA2LfiNZEUQ", /* 57: 8x8, corners, 31 passes, 21 numbers, 50 long */
    "9:322.11.1.....12.10.32.2...011...3...2..01...1.3.2.2..3.3.2.13..0....0..00.1......:NBBwQNgZ2KWIEAAXzz8JhQgQlKDDAA", /* 58: 9x9, corners, 28 passes, 34 numbers, 58 long */
    "9:0..0.0......1..1....21..2..210.3.1.01.00..1.0.1...2232.23..11....12..23.312.1...1:AAA8bewAW31SJ9EAAAQQRQQEZIosnG", /* 59: 9x9, corners, 30 passes, 37 numbers, 58 long */
    "8:....3.2.211..11..3..1.323.....2011.1.....0.12.1...0....0.0..10..:rQ9oLdGYAABA9JgkLHQCGwAA", /* 60: 8x8, corners, 34 passes, 26 numbers, 46 long */
    "9:..31.1112...2...........1...10.2....2.3..302.2..0.......212..1.0..32.0.0.....0...:8nAYAargUnGCIAAEICCiQdLHsoAGAA", /* 61: 9x9, corners, 31 passes, 28 numbers, 52 long */
    "9:21.....2...2..1...2.323..003..1.......0.220....0...0..10....0..1..22.0..2.23.0...:-H-UoFhACQgCAYBBYCUDCRIEBhQH0B", /* 62: 9x9, corners, 33 passes, 30 numbers, 54 long */
    "8:.22233.....210.11.2...3....11..1...21.3.........10.0.000........:velSsWmeBAAAxHhWXENaAAAA", /* 63: 8x8, corners, 38 passes, 24 numbers, 50 long */
    "9:3.12.2.12..1.31.0..11....0.....3....2..313..20.01.31.1...0....1....131.1.....1.3.:vfATgChQ1gCEgBQxINGg9oXCKAFIBY", /* 64: 9x9, corners, 36 passes, 33 numbers, 58 long */
    "9:..121..1.1....01...0.11.3223.13.2..2.1.2..32.13......2...1..1...12321....0....0.0:5XgENX5ZP6wmIAALYDFsKcgzwCNGAA", /* 65: 9x9, corners, 38 passes, 35 numbers, 68 long */
    "9:2.22....02..12.1..201..21.....0....0..011.1..211...10..2..3.....2....0...22.0....:aJQUIWhBFjACD4AuTbFVCSIkF8ADkA", /* 66: 9x9, corners, 40 passes, 31 numbers, 58 long */
    "9:.....1.1.011.31.3...0.3..1.0...3......0....110..12..2.13.21122..1....13..13..1.2.:+xjQCCQAe+xBMi-CCoDhgIECwCOkTg", /* 67: 9x9, corners, 47 passes, 34 numbers, 62 long */
    "7:.12.2.10.1.12....1....0..3..0..1.12..30.1.....1.3:cADnMUCExTChUgLSEhI", /* 68: 7x7, a side tried, 23 passes, 20 numbers, 36 long */
    "7:.221...2...3.02.....2.012........332.30...1......:GgUiJqqEMogOVFY+GoA", /* 69: 7x7, a side tried, 34 passes, 17 numbers, 40 long */
    "7:.23..2.2.1..12...22031.32...1..12...1.2..3...23..:0W4sqWbQxwVQTymgYjC", /* 70: 7x7, a side tried, 43 passes, 22 numbers, 48 long */
    "7:..3..2..2.220..2.3...0311.13...11..0...3.0.1.....:klAUpYFQIwqmaRoI8wA", /* 71: 7x7, a side tried, 56 passes, 20 numbers, 38 long */
    "7:.23...1.0....0.10.2..1.232.0..2......3..0.00..0..:7oBZBAIEA0UEhwDPMAA", /* 72: 7x7, a side tried, 99 passes, 19 numbers, 32 long */
    "7:..3.0...12.0....2..101.....13.02....20.1.3.22.1..:EAAA0ioC5xADMwWcCSE", /* 73: 7x7, a side tried, 156 passes, 20 numbers, 32 long */
    "7:0..0.0...32.....23.2021.21.1.1...1.2...1...32.1..:AoEQVuECaAAP-EkCRcB", /* 74: 7x7, a side tried, 204 passes, 21 numbers, 36 long */
    "7:..21.1.23....1..11.32...2.2.1.3....0.100.0.......:9BIqBVDAAc4hhbvCAAA", /* 75: 7x7, a side tried, 789 passes, 19 numbers, 36 long */
    "10:.12..22....3..0.....0.1...1100...202......3.0111100..........210...20.1...0..2.02....3..1...21322...:4gR4GoCRQCGD4gCFw0gEEEwAkDBBtICEI6l6C", /* 76: 10x10, corners, 24 passes, 40 numbers, 66 long */
    "10:1..0.00...3......0...2.02..0..3....1.0....20..220.1...3.....1.3.21.3..10.21...........1.20.11.2.123.:AQAICRQUQJUChhYQ2-BAGAlBrQpIjUKSRxiAE", /* 77: 10x10, corners, 28 passes, 38 numbers, 64 long */
    "10:.0.1...121.0.3.3.0.20..2..10...33.......3...02..01..11...0.1..3..2.011...0..1..2......3..2......3.2.:AHKAiSQJMCYAJBGABwCkwRYR3YAkMyEB5AegO", /* 78: 10x10, corners, 31 passes, 37 numbers, 64 long */
    "10:..2.23....1..3...21..0210.1310.1........1...1311.0.12....00..2.........1..1.20...3.31.0....23.0.....:bDKwIEG4BCZRRoCIQB0WqIBiGULoJYKSDXgHA", /* 79: 10x10, corners, 34 passes, 38 numbers, 72 long */
    "10:.13.3.1233...2..1.2111..21..2.1.12..31......21...032..2...3....2222...0..3202..1.00...13......0...0.:0bMpbYJ0YSJEUhQACAwxfiB60QGZOhWwUAGAA", /* 80: 10x10, corners, 37 passes, 44 numbers, 76 long */
    "10:2..32..32.1....1..1..10......2.....13..3..32113.3.....1.3..03...21.........31.........0.............:3NiCOqJQSUAREACAAAkZDQhsP7XbSMFADAAAA", /* 81: 10x10, corners, 37 passes, 29 numbers, 66 long */
    "10:.0.0......0..0..23.0.1.11.20......1...2..3...3.1.3.3.10...322.0...0...1.0.31...31..3..32....1..22.3.:AAgQWglUqCDEEhsI37BAAMsIGjengHAj5phYE", /* 82: 10x10, corners, 43 passes, 39 numbers, 74 long */
    "10:322112..1....1....003.3.02.1...110..........0.3.......1..100..30..3.......101..1..1.2.2.1.....23...0:-z5MGlRYYGkCRhZOxLEQQBNCRQgYFccWQji4A", /* 83: 10x10, corners, 52 passes, 38 numbers, 82 long */
    "8:.12.3.2.....3..331.....22.3...0.0...2......03..200...1.3..0..0..:cD5sVIKFUiMAklaB0nYjcYFA", /* 84: 8x8, a side tried, 35 passes, 23 numbers, 54 long */
    "10:.3.1....1....1010.0..33.1.1.02.2...233......3..2.23.0..2...2.....2..2..02..02.2...1...0.000...0.....:+vBICEqUONDIVBIIAAIARguRxHFNMSmwMYAAA", /* 85: 10x10, a side tried, 40 passes, 38 numbers, 68 long */
    "8:.0..221.0..2.1...1..31.....3...2.0..11.1..3.2.0...32.1...3....22:wgAVEiADEYpfQBJ0Cngkm4UQ", /* 86: 8x8, a side tried, 50 passes, 25 numbers, 46 long */
    "8:..3.2.0...23...1..12.3.23.......2..2..1......2..010.3....0.0.113:cwgzpEAVSADgkAGiNja08QHw", /* 87: 8x8, a side tried, 57 passes, 24 numbers, 50 long */
    "8:.....12....3.3.3..........0..3.1.0.12...0....101.1.3.31....1.222:AjJqwACHoQA0AFdQEogkg8Fu", /* 88: 8x8, a side tried, 79 passes, 23 numbers, 44 long */
    "10:1.....1......3100..3.0......121.20.1.2.3210..1.2......32.11...121...0.1.3.0.0.0.2..2..0.0.2..1.3...0:+jDiQE5gC1gBDwAxgPIQg4IQiVQNWEBKARAhA", /* 89: 10x10, a side tried, 75 passes, 42 numbers, 66 long */
    "10:.......1.1.....3132.......1..2..0.33.132001...........2....3..11.212..000..2..1...02..00......1..2.3:ABIDKQMIzacKIDBIE+CMAtAR4CEkcDUAhQQEO", /* 90: 10x10, a side tried, 83 passes, 36 numbers, 64 long */
    "8:...3112....31.32.0...01......1.300...01....1..210..23.....3.21.3:4jMyYiJxcwN-IEKIEUIkIghg", /* 91: 8x8, a side tried, 123 passes, 27 numbers, 50 long */
    "10:...2.0..0.21.2.0.....3.......0..33.1.01.1......2.1..12.121......3..32.1.1...02.11.0.......2..111..0.:OQA4AKAdhHQBogYAwfIBiADgOQgLhl-yQBiAB", /* 92: 10x10, a side tried, 119 passes, 37 numbers, 66 long */
    "10:.1.......0.3...10....2..13102330..2...2...212.2..2.22.1....2....13.12.01.2...1.2.2..1..13.3....11..2:AgAAiFUlAwssbTgL0-DAMAm50q106IKhIc8AI", /* 93: 10x10, a side tried, 139 passes, 42 numbers, 80 long */
    "8:0.3...10.1.3..1..2..0.23.2.3...22....2.2..13..1.0...3....02..02.:0AS6s7eOaQFichHHcGBUpkFz", /* 94: 8x8, a side tried, 182 passes, 26 numbers, 62 long */
    "10:....0.312.........1.0....3.....23.01.1011.1.2..0.......2.0.1......3.1.23.21.11..0.2.3..1.2.1...212..:AHQAmkhSQBJRaJBEA7CkAUgQGpU0NWLd44caO", /* 95: 10x10, a side tried, 185 passes, 38 numbers, 76 long */
    "10:...0......0.31.0000...121......1..3...0..1..1.1.0.2.1.........123.2..112.....211..00.0..1.3...1.3.1.:AABIAYAGEGKVkIEB5vDAYAKARgCIVc3IWCogJ", /* 96: 10x10, a side tried, 219 passes, 38 numbers, 60 long */
    "8:3.3..1.1...3.......21.133..01.2.32..0112.......2...23.....0..00.:9xkqFU2goBBAHaKtW0NERwAA", /* 97: 8x8, a side tried, 303 passes, 25 numbers, 52 long */
    "10:.....0..22.013.022..0....3..01.1.1.......0....2.0.....233..10.0..0..1...1.1.3..2...1.1...21.33322...:AMiAFkAfACQBQ5IUj6CQxkoRCIgEcCBh+CZfO", /* 98: 10x10, a side tried, 305 passes, 39 numbers, 70 long */
    "10:3.2.0......3.3...000....1.....3..2..0..02.3.2.11....2.2.1.1.10...1220.2...3.......0..2.200.1.33..3..:HwCsAZQHQPICcEHpy1kAoAJARQBIjEIiACi8C", /* 99: 10x10, a side tried, 570 passes, 39 numbers, 66 long */
    "8:.0..0.0...1..0..0...22.30.2.2..2.1.1....3..2321112.1.2...121....:AAA3MySdyS0-AAAkFeEaDNPg", /* 100: 8x8, a side tried, 678 passes, 28 numbers, 52 long */
    "12:.1.1....21.01..1....3...1..0.121....1.0..2210...1.0..2.00......02.......1.0...0.00...0..1...2...20...0........0.3..1.2.0..0..2..3.2022.12.32....:-HwDwFQAgBwAwAwCAFRIhHxV+dBIiYEIIWQEggARACOEgQGSEGBT", /* 101: 12x12, corners, 31 passes, 53 numbers, 88 long */
    "12:.2..1.....0.22.1..002...11.0....1.3.211..0.0112.2..1......2.21.132.12.0.10..2..122....2...1....01.2..1.01..0..11..1..0..11.2.1..0.0.3.3...0..0.0:9DEEGQAIHU3IQEMLQM8AwAKAFAHEWQEgLAECnoChJqpUAIIQEgHA", /* 102: 12x12, corners, 39 passes, 63 numbers, 86 long */
    "12:.1.211......2....3.3....3.0..2....003....1.22......101.32...2120.01...3........10.20.2.22.1......1.2.2.2203.1.2..2.2.2...1.1.......022323.3.23..:PAgCBAREHCHbUQBChQAUoCVB2KRAiPIfIYBSjAQ9Q8t0dqbTJHtP", /* 103: 12x12, corners, 42 passes, 59 numbers, 112 long */
    "12:3.3.....3..0....1.3..11.1..1.0.2.....22...3.1232.31.3..3.0.....2223.....1.3.1.12....2..3.10.......3.....0...3.231.31........0.1.....3.1.........:rHVHG-FvQTCDMDMANCtANBBADA9IECgA8I2Nh4AlEiJYKIagBgCA", /* 104: 12x12, corners, 44 passes, 49 numbers, 102 long */
    "12:.21.111.33....10.......11.3...0.33.2.2.0..0...2......3..3.211.21.3..2..1.....21..1.2.3010...20.3...1....3...11....3..0....1.3...1...0..1.321.0..:+rAUGUFI0UmGkWKGimbHQDIDgAC8FAiw-g8W2YpdpHGXiAJAOBwA", /* 105: 12x12, corners, 51 passes, 56 numbers, 116 long */
    "12:.3..32.3..0..0..232.10.1....11......01.2.21..23....2..1..0.20.20.....1.20..3.20..220.0.2....1..1.0...2...01....3...0...3..0.....0..3.0.3..3..11.:SDFCpkeIsSwgUcoc4AwgogYh4+2FyHI4G0OVQCQ4EAChACLIIIkh", /* 106: 12x12, corners, 55 passes, 56 numbers, 104 long */
    "12:..3........2.02..12..1.2..0.0.3.211....0.1.2.3......13.1.101.......0.0.1..0.22....11..0...323..2....10...122...0....0........011...2......1.13..:8-0HInAvgPgAwAgWQFACAIAIAuEAxQCECAKIQggQAh3DQHQOgaA5", /* 107: 12x12, corners, 60 passes, 51 numbers, 88 long */
    "12:...0.3...23..00.1..2.1.....1.11......01..2122.1..0....2.121....01........0.0...2211....0.12.2..1.....22...0....1...02.02...31..2..2...0.2.2..2.3:g0ATgTYAoLwfgfgHADgEYtIkw2gdBRDwFlLEQAwgABQC0EEIyNot", /* 108: 12x12, corners, 74 passes, 54 numbers, 100 long */
    "12:311.12.1.0...2..2...1..1...23..3.3...1.....1.0.....0.....01131.1321....2.1.13...221.01...2....110.1....22....0....1011.......0...0..............:5AIDWqZWAAxQXKUgsYgdAAAAAALBmIkqXAoAwo4ovpeAaCAAAAAA", /* 109: 12x12, a side tried, 42 passes, 51 numbers, 86 long */
    "11:.21.1.11..1.1.3.0..3.11.....11112.2..0.2....3.32..221.2..1122.2..2....1..3....000.2...21......31.2.0.1.12.0.....0..2.0.01:+vE2B3ZUIzICQABQCLgChACQZEQgClNTmfgvgpgyIxwA", /* 110: 11x11, a side tried, 49 passes, 54 numbers, 90 long */
    "11:1.32.11..3..1.....22.03.01.3.....2.00.1112.32....0.0..1..0.323.0...11.2...012..22.222...00.......0.......0..3.0.00...0...:0vBW6I8CgiCBwDAgElCgAAcQZEiAB4BgyjxT4T4lAYAA", /* 111: 11x11, a side tried, 65 passes, 51 numbers, 80 long */
    "11:2...1...1..3.01..01..1.322.1.3.2..3.2.1.1..2..3...3.02..0...11.23...0.21....10....323....0.13..2..1..0.21.1......0.3..1.3:+-AwmTyKnpTAUYBsAiD8AhCgBgsyaIMiQ4QEADQEgggx", /* 112: 11x11, a side tried, 74 passes, 50 numbers, 86 long */
    "12:.2.221....0..1.210.1..2...1..1..01..2..01..22.2.10..32....0....0....3232.0....1...1.11.2213..1.....1.0.1.1..2.0.0.....0.12.....01....2.......00.:2BFgNRBjwsQsAUQCOBAACEAADAaCqELENgWUwwahJieMwYgRBgCA", /* 113: 12x12, a side tried, 79 passes, 56 numbers, 88 long */
    "11:.....0..1.3.0.00...1.....0..0.31..0....23.1.0.1.2.22.......0.1..11.12..3..10..13.1..0..30.1.0..0..2.0.....1002.....1.1...:AeAIgF0CZZGoEIIBEIAEBeAiASAogTQpEs8hMRAhAiAR", /* 114: 11x11, a side tried, 92 passes, 46 numbers, 68 long */
    "11:.0..322.1.3..1...2.11.0..1.11...2....00.0.12..3.....2...1.3....2.12....0.02..102..0..0.12..2.2.12..2...23..2.0..0.0.1...0:wf4J-gASwYgLECEVKgoCAEQgAUEQIgE0QENIBQ9L+HAG", /* 115: 11x11, a side tried, 127 passes, 50 numbers, 80 long */
    "12:0.12...1..1.0.2.2.1..0...1..2..0.0.1.1.22.1.....0.1223.3..2.2..2...202.....0....01....2...3.02..0.2.1..1.........0.1...1..00...3.0...0.........1:w-sAYAwAkbqKlATBEJIQAUAiAAQAJDyEkDIMbn5hypijpGQLgZAA", /* 116: 12x12, a side tried, 126 passes, 51 numbers, 96 long */
    "12:.3.1.2..12....11013..0.2.13........1.....01.2..111.2.1......1...0..1.3.0..2.0.01......1...0...1.0.022.232..0..0......3.........20..0...13..0....:9uCQM1EQEbIPAMDMoGACAKABQAHzDGWxJjVVgGINEQgAvBuD8AYA", /* 117: 12x12, a side tried, 168 passes, 51 numbers, 100 long */
    "11:..0...00.0.0...0....0..13..0..00.........1.101212..1212..12..2...1......1.3.1.0.23...1...3.110..0....0......013...221232.:AAAABABAz5CYEMTHQAoRK3AAAAMAUAElgmIqggHgBgvs", /* 118: 11x11, a side tried, 188 passes, 49 numbers, 66 long */
    "12:0...0..000....10.0.....01....1...10...03.12....03.1..2..000120....1........22........0..1..2.2221..1..3..3.22.22...01.12..1.2.0.32.2.112322.....:AAAAEHLGJAxikbMrAFep+UQCvpAAAAwkIagygMy1IDrFqbIYA-Y9", /* 119: 12x12, a side tried, 187 passes, 60 numbers, 108 long */
    "12:..2.2...2.1.2.1.2.3.3..313...2...3.3....32..3...131..12..31.1...231.1....1.2220....10.2.....3.02..31.1..11....31.21...02..3.12.0.3.2.0..2.10....:34BlStLsTt6O2AoEUiUhkIcogYZJ1MxGNuEhUlgQARbStCFJRUwU", /* 120: 12x12, a side tried, 250 passes, 62 numbers, 122 long */
    "12:...0....0..0.....31.0.1.....230...23....2..1..1......12..1..0000.1..2102.....0..1....1.21.....3.02..2.01.1.11.1.3...0...21....0..001...221..1..2:AAwAgwgsQFAiQCEYAeSAYANAz-AAgCADFqEoKQmQK5EzBUFoAwKg", /* 121: 12x12, a side tried, 283 passes, 55 numbers, 82 long */
    "11:1...2.21.3.112.21.0....33.....3.2.22...221.31..2.3......2....1.00..01.02......0.....1.21.....110.......0.....0.....01.2..:KXaEFXsbKVmkAAOADABsAYe5Cg+EYiyVsgAhAFARAJAU", /* 122: 11x11, a side tried, 298 passes, 44 numbers, 84 long */
    "12:.0..0.11.0....0..1...1.....0..22......010....1.0101.2.2..2.13.2.2...1.2.......3.1.3131.13..12...3....11..21.2.2....1.0.3....1110....212.....111.:AAAHAFAAocBYGYBdR4-l-hIA3-AAASAYAwA+o9FxjkJEwEQUAwMg", /* 123: 12x12, a side tried, 334 passes, 58 numbers, 98 long */
    "11:....00.11.2..0......3....321112.....1...12.101..22.2.1..31.1.2..11..322..........2....33....3.2..2......00.222........2..:AYgFCiCwEcD6F6EYESBQBwAoA1YtktwuCsMiQ4wKAUAo", /* 124: 11x11, a side tried, 660 passes, 44 numbers, 82 long */
    "12:0.....3.......00.21.......01.0.3...0...2..1..1..12...1.122.2.1..2.1....2..3...1.0.0.23.31.0.....1.....0.0..22.00.11....2.........022.01..110.33.:ABgAQCCSMG+gVIgAKAAyjwMVQoADAFARwSmAOgs-B-AGAMwC5RY8", /* 125: 12x12, a side tried, 781 passes, 54 numbers, 92 long */
    "13:33..23..2....2......32.3....23..0....0.22220.0.23...3......222..2..3310...3..213..0...10.0....3..20..0..........222.0.022...3....0.0...3...........0...000.....0..0....0.:dPEt8o5hhDrClKWcBy0iEAmEQAAAAAcGRgKQEEAxEZc6KAIcQk2gcDYAAAAAA", /* 126: 13x13, a side tried, 61 passes, 60 numbers, 108 long */
    "14:..01..3.....1.32.1....000..1..3..2..0..00.3....1...0.113.3..1..0..1...13..31.....1...12..2...3.22.00..132.13..3..0.00..3..............21.13......0.1..1............1.2.3.....00.0..1..........0..0.0:w+PFQaA1BOAqETGJhgIISAiBQEA3AgDAAAAQDsEgeCMAQEgVm24vF8eAuAARAEBAkAAAAA", /* 127: 14x14, a side tried, 74 passes, 66 numbers, 112 long */
    "15:..21.12..20..0.1.00.1........1..........100..1..00.3.0...2...00....21..1..1...1.22.2..00..1..0..2.12....01.0..21.3.1.....0.10..0...000......3......0...3.22223......0.221.1......00.1....1.0........1.....0........011.0..0......:9HQYNAVGIRAzFGAQdAUDAEAFAiDAeARAABAcAAAAHIQQtEogBReABEVAhiQQDEEA5AwHB8AA8AAJAAAA", /* 128: 15x15, a side tried, 74 passes, 77 numbers, 112 long */
    "15:..3..1..11..1..0.23....1..0..3.3....2.31.10.....13.3..0..1.103.....2.11..0..3..1.3...1..0...33....2.3..........3..2..232.0.211.1.3.0....0.1....2.......1....1.0..0...0.....0..0.0....1.20..0........0..3................00.......:8-nCgqHk7ESFQGAKJgqqYICgBECgFAsAgFAQAAAAEAIOAZggihJlbK8mc0J4ihMBIHARAADAEBAMAAAA", /* 129: 15x15, a side tried, 86 passes, 69 numbers, 128 long */
    "14:...0.13.1.3..3........2......0132.01.3.1.20...2....30..1....310.2...1..10.2.....0.3.1.3.2.3..1..13....1.....2.01.2...13.01....00.21..2.2......0.2..2...2.20....100...02..0.01....00......0....0...00:ApCIBOC4IBjAETqhQnpQOMQBqKARQACAAAAA7Ho5IUHohQUEE4eiWABEDBaWwMFGwAAAAA", /* 130: 14x14, a side tried, 99 passes, 73 numbers, 114 long */
    "13:...........200.000.332..........1....20.0.1.3..1..2.0.01...0.3.1.1.0...3...0.1.......123...22.011..122222.3122.......12.....1..111..3.0..0.1.0120..1.3..00.1..11.....1.1.:AUACBICCAIYwEBJkOXukCFCjyAAB++DwDwXA5AnDEmwCOnZBMqsbRpDghCGGI", /* 131: 13x13, a side tried, 104 passes, 69 numbers, 120 long */
    "15:13...21.........1.2...0102...0.3...10..1...32..2.211.13..1..21......03..01...3.....1..223.2....00.1.....3.321....3.2.32...100.00..222.12.3......00......2......00......333.3021..3100.....3...3..0.....0..............0.0.231..1.:+LwnC9gSBr3JoP1MYmhzCc5A8HCvqIKRBsIAkAAJCcAQIcAcZoIgMegMTg5BxEACIAYgR4nIg6BAtAgN", /* 132: 15x15, a side tried, 104 passes, 84 numbers, 166 long */
    "15:....1.2.1..331..00...2........0..2.21..2........3....233...0.00.211..1..2.0...1.1.3.....1.012..3...013..1.13...101....31.....2.2..0..220.13......13....01.1.2221.......0.2.2..2.3..000....13.32..10...0..0.2.1.1..0.0.0.1....3.0.:gfBMEQ6BNKIqAeMcDDFUUFGBcQgFBEwIAQJAmAwHggDggAzAI+DEgAQCkhFHgA+g4vMyjA9OA2DAKAIE", /* 133: 15x15, a side tried, 126 passes, 88 numbers, 144 long */
    "14:0....2....3.0..33.212..21......1.3.....2..2..1...11.12...1210.1.....0.2......2220...2.11..01....0.3...2...2.0.00..3.....0.....11....2..3.0..2.2.12..3..2.0..12........3.2...0100.3.2...3.3.1...2...3:YTIlQJIXFcIDDDTO0xA1AGJwKxEZv4pgf9-o1wzFxPBBIBWoSAgQ4gA7BIWDFtAOQBI4Dg", /* 134: 14x14, a side tried, 134 passes, 72 numbers, 156 long */
    "13:....0..3.2.3.....0.....3.....0.0023..3...0......02..3.1...2.2..2....21.1....2.11.22....0.1.....22...10...0.322.1.1.02.0..1.1.2.0.10..20.0...0...1...3..3...1.012.2...1..2:A6AoBYDwUoVvRPUhNgFEdACASAMEg+D4EgEAIBIyY2B8CBEwSoICxICjkNCHI", /* 135: 13x13, a side tried, 140 passes, 63 numbers, 112 long */
    "15:3.2.3..3.310.0.32..1..13........3.3....1310.0..........1....1..0..2..3......0..0.221...0..1..0..2.2.23...1......0....0..3...3.23.3..0...21...2....0.......1..2.3.....31....3.1......2.1..2.0.......100.023............2..........:fKoLApUg6DAPA4BgyAsFyVIWBDPI3BRBANAgB4DAhegRH0DCBBQAEEEAhWRAIc5BEBQAEoMA5AQgAECA", /* 136: 15x15, a side tried, 156 passes, 68 numbers, 134 long */
    "14:....11.3...22.2.20..01..23.3..20........03...1.013.310......0.2..21..32.0.02..3.3.3.12..1.....1..0.2.1....2........11.....0...3..2..0100.....1..1.........0.000..0.0.........0..............00......:5VTIIAqEKaAYQogUZ+gZDNCMBAAAAAAAAAAL+9AfHAcxTBeUofiAABCiGoGAAAAAAAAAAA", /* 137: 14x14, a side tried, 179 passes, 64 numbers, 106 long */
    "13:..3.....1.3.........3..1.2.0.1322...1.3.0...13..02......2.3.....0.01..2.3.0..2...2..3.0...2.13.3...0.1.10.2.2.221..2...2.1....1...1..2.222.31....3.3.1.1...101.0..1.3..0.:8v4nCPCOIXEmZoCpCwMzDCjGSkWEAUQAHAhQgDICZEwEhMINggVLUu1y6SA4B", /* 138: 13x13, a side tried, 185 passes, 65 numbers, 128 long */
    "15:..0.3.21...31.....3........0.0.....22320232.1....21...23.1.1...2...11...2.....21221.1....30.........11.....2.....1.1...3.001..2...1221.0...22...0...01..1.0.101.0..0.0..3......0........0..3..1.0.1.3...2.....3...0.11.2.2.2.2112:Q-ABGoyCuGQXABsEPm8t4BDGAIhAhAMBQVHG-A6-wBBaEAZcAYGcohGgkQCCIDImQjJCACxgEPoOQCHg", /* 139: 15x15, a side tried, 204 passes, 84 numbers, 156 long */
    "13:..0.0..........1...0.......0...1.333......2.22.....0.1.0...0..20.0....3..0......0..2.01.1.00..3.1...13...002.2.101......3.2.0..1.....2...0111.3...31.0..........33..22..1:AAQAApAqCNEaAoIwwgHBPGOQc+o8g+BAAGAoegFCBEYRAiACIASgAEQgIEAHE", /* 140: 13x13, a side tried, 231 passes, 57 numbers, 92 long */
    "14:32..23....23.22...0.0......11.0....1.32...2..1.0.0....3.32........10.32.2.200.32...31.2.11....1.12.1...23.1.21.333..1..1.2.2..1..0.........1...012..3..2..0...132..0.3.......2..2..1.0...0..2231...0:dvDyiAKAE3BdEjEECGkUklKBgEIBwiAXAgBnx8goB8MAOmAsiSZYMX7PpB0WguBYMQEAQB", /* 141: 14x14, a side tried, 243 passes, 78 numbers, 142 long */
    "15:......32............02.2.22122......3.2.1.32......0...1.32....1.0.1..0.10....2..0.2.1.....02..212.2..0.01..2....1..2..0..1......223..010.20132.....1.1.0...12.22...32....222.11.211.0..12...1..2.....233....0.2.3..0..02...12232.:A-G4VAPB4AAmywAaCgJBsUAEDE-D7f8ygCSoAAorABLAMDE+AoPgAqggkOIcFSgikBIKADEgEWJu5Dc+", /* 142: 15x15, a side tried, 295 passes, 90 numbers, 160 long */
    "13:.2..2..1...1231..01..13.....1...001...2..0.0.10..3.23.1.....002..2..01..1.1..1............23.1.12.2211.3..2.2..2.2....1.....131.310.10...0..0...0.0.0..0.0......0..0.....:e-DRMAfAtYkg4D2EImgFIAFJAAAAAAIGYAmEIEA7pgkREu05h8vxzGAAAAAAA", /* 143: 13x13, a side tried, 342 passes, 65 numbers, 108 long */
    "14:.............0............3..0...00.0...0.0.1......3221..1.123..21...30.........2....1.3..20..1..1...0.0..0..10100.......1.1....0.0..2...0.1......310..0......3.231..0.1......1.........022...3.311.:AAAAEAoAowFJolooQlAgAASAEA0AobgIBc9AAAAYAgEArkrjAq8pEQmA0EQgABEmgIoEyj", /* 144: 14x14, a side tried, 342 passes, 61 numbers, 102 long */
    "13:....1223...31..0..1..02..2..1.2..3...1..0....1200.....00....1.1.2....213....11..0........2...0..10..3......0.3..0.111..02.1....01......2.3.3..2..3....3...2...1...12...13:glAUgNEXCIQIeAdBwAjDGAyKSBsSgJC6GIcI1CII4QQHABOEyABCBIS-k6D1M", /* 145: 13x13, a side tried, 435 passes, 59 numbers, 114 long */
    "15:.12..221..1.1.3.0..1.0....0....2..0...0.0.0020..211....01......2.2.21.1....0..1...1012.221...3..3.....13..0..1.223.1...00..122...12.1......1...3..22.0..0.2..2..3.......0..0.3..........0..01.........................000.0......:H-HHgBA0QA4hifgUZChUAECjaIkAWCAEAAAAAAAAJBYAAJAgIGIA5TAO4LF4pAuSIXB0OAwAAAAAAAAA", /* 146: 15x15, a side tried, 466 passes, 76 numbers, 116 long */
    "14:2.11..1.2..2.32...0001.32.3..0.....1.2..1...32.11....2..1.13..2....3..10.2..21.31.......01.0......1.0.2.1..0...........1............2.10....20.3..1..1.0.....1...1......0.1...2.0.00..0.1.3.2.1.0...:+7HgKATcqwABSKgMgAAUAAAgaQ1BaAANA6ACMMgeBgo0LxuJtCJAICAhDIcAZIwMAIBAnA", /* 147: 14x14, a side tried, 538 passes, 68 numbers, 118 long */
    "15:.222.22.12.3...1...1....3...002.10..0..1.......1.....1..0...3.....1222..10...1...3..2....03.3.312132.223.3.....1........1.212.1..13...1..2...11.....1.0.10..212.2.3.01...2.....2.1..0...0....1..2100.0....01...2...........13.1..:b-AAFkQYwABSQsNV9KiZDgjiaQYRqNOyAgZAgDACtBRbOEgAECwBNEKIeFVbEgGTUNBZpIJBAEBAEBAD", /* 148: 15x15, a side tried, 868 passes, 84 numbers, 150 long */
    "14:3.....3....22.22..0..121...231.0..3.22..103.....1..2.0...0.0.3.23..0...1....2..220...10.0..22.3...2.1.........0...113.3.10.....2.1.12..2.00..1....0..3....3.1.1...3......2..2...3.....0122112..0....:-KfU7xfBPESQwAhTIgB0AAFUowJDjEAWAjABfFBQBBQAChdQEDBs4gA3B42B4QIqA0IgyA", /* 149: 14x14, a side tried, 1593 passes, 74 numbers, 134 long */
    "13:.3........1..0.22.3.000.011.1...1....0....2.2..0.12.00.1.....3.....02100....2.....1.01..23...0.3.2..3............121...00...0022.....0...0.223......3.0....3.......0.1...:+-9B4DQPIdOA8IwwMBlACCAKAIgIBAIAIQgCAyIEECQAiANouQhCGJY6gZDAA", /* 150: 13x13, a side tried, 2003 passes, 58 numbers, 102 long */
};
