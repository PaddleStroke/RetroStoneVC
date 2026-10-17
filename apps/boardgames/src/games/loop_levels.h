/*
 * Loop's levels: our own, made by tools/loop_gen.c (a loop from the game's maker, its numbers taken
 * away in symmetric sets while the solver finishes it at its grade; chosen for their look), six chapters
 * of 25 from the easiest, each ending on a showpiece with quarter-turn symmetry.
 * Each: "<n>:", a character a cell ('.' no number), ':' and the loop's sides (slk_decode); in the
 * comment the reasoning it needs (its grade), the solver's passes, its numbers and the loop's length.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
static const char *const slk_levels[SLK_NLEVELS] = {
    "5:.1023.33.1..2..1.21.0012.:QV5Awwuhoo", /* 1: 5x5, sides and dots, 6 passes, 15 numbers, 22 long */
    "5:....2102.3322232.2031....:Ma0eRUobhY", /* 2: 5x5, sides and dots, 8 passes, 15 numbers, 26 long */
    "5:1..0.2..0.21200.0..1.1..3:CQgg-GGFJh", /* 3: 5x5, sides and dots, 8 passes, 13 numbers, 20 long */
    "5:.22...3.22.223.31.1...20.:HdaWEJn8JG", /* 4: 5x5, sides and dots, 10 passes, 13 numbers, 28 long */
    "5:..1.1011.2233123.2122.2..:Igs0FYYuhG", /* 5: 5x5, sides and dots, 11 passes, 17 numbers, 22 long */
    "5:.12103...130.232...11231.:nQuTMJKhGK", /* 6: 5x5, sides and dots, 11 passes, 16 numbers, 24 long */
    "5:...3.0130.01.12.2212.2...:IKxWWYkoSd", /* 7: 5x5, sides and dots, 11 passes, 14 numbers, 24 long */
    "5:.3.2.2..121121300..2.1.2.:NxFSYXRoYU", /* 8: 5x5, sides and dots, 12 passes, 15 numbers, 24 long */
    "5:.23.0211.2.....3.1211.02.:kFVkgMRtrw", /* 9: 5x5, sides and dots, 13 passes, 14 numbers, 24 long */
    "6:2310..2210.02..0.01.1..01.3222..2113:CBCCYk-GKMoQrg", /* 10: 6x6, sides and dots, 10 passes, 24 numbers, 28 long */
    "5:00...0.1120.1.1233.1...12:IIQU+Yoouh", /* 11: 5x5, sides and dots, 14 passes, 15 numbers, 22 long */
    "5:00..3.10.2231222.32.2..12:QgIR+ww2th", /* 12: 5x5, sides and dots, 15 passes, 17 numbers, 24 long */
    "6:..112.222.1..2211..0110..0.101.012..:gfmhAA4gDuCJSk", /* 13: 6x6, sides and dots, 12 passes, 22 numbers, 28 long */
    "5:.2.2..3123112212120..3.0.:FsmEEPnJFG", /* 14: 5x5, sides and dots, 16 passes, 17 numbers, 24 long */
    "6:...13.003.....20220120.....102.22...:cUUgEnfkwQiIyQ", /* 15: 6x6, sides and dots, 13 passes, 18 numbers, 30 long */
    "6:.2..00..2.2.012.1323.223.1.3..12..2.:HL0qlrcJog0WWS", /* 16: 6x6, sides and dots, 14 passes, 20 numbers, 38 long */
    "6:..320.01...0..3..00..0..2...03.222..:EIEUitTMoghkwa", /* 17: 6x6, sides and dots, 14 passes, 16 numbers, 28 long */
    "6:.31..0..20..20.20..11.33..21..3..32.:CAFoWwJGMkI+yN", /* 18: 6x6, sides and dots, 16 passes, 18 numbers, 30 long */
    "6:.2.23..3.1...232....113...0.2..10.0.:vSYUQgBxP+e64B", /* 19: 6x6, sides and dots, 17 passes, 16 numbers, 38 long */
    "5:.1.2..1.2.11.01.2.0..2.0.:fGEICh1FFD", /* 20: 5x5, sides and dots, 21 passes, 12 numbers, 22 long */
    "6:00002.0.1.311..13..23..220.2.2.12331:gQWUtIXghq2aoc", /* 21: 6x6, sides and dots, 18 passes, 24 numbers, 34 long */
    "6:.221....01322.11221332.20113....011.:8TBECIgEjL3Pew", /* 22: 6x6, sides and dots, 21 passes, 24 numbers, 34 long */
    "6:..3.22112.3232..1111..1100.20201.1..:8cfPIkcEBHEKSS", /* 23: 6x6, sides and dots, 23 passes, 24 numbers, 34 long */
    "6:3..1..1..1..1.30.01.31.0..2..2..2..3:FCEEWu-PSUIxsg", /* 24: 6x6, sides and dots, 24 passes, 16 numbers, 34 long */
    "6:...11.13001.13113..21222.11112.00...:8CSDPAgEFSPOYw", /* 25: 6x6, sides and dots, 11 passes, 24 numbers, 30 long, showpiece */
    "7:....200120.21.2.0.1..310.110..2.2.1.13.123001....:MkAIwCwkhQhIiECPcCD", /* 26: 7x7, sides and dots, 11 passes, 28 numbers, 32 long */
    "7:.113223.3323.3.0..1.3..3.1..2.0..2.3.0122.100131.:oFWlsawBggrfB2ykzAD", /* 27: 7x7, sides and dots, 13 passes, 30 numbers, 46 long */
    "7:02320.....31.0.323110..122..232222.1.11.....12323:EUoYnPfQfzgEME0ABFL", /* 28: 7x7, sides and dots, 15 passes, 31 numbers, 44 long */
    "7:..001.300.133..11..1..11.01..0..10..322.010.131..:AhA6JyCKwDM8QGUmWiI", /* 29: 7x7, sides and dots, 18 passes, 28 numbers, 38 long */
    "7:01.232.1....122.01011110.23323221.132....0.131.00:UAvAkWABIwDnBG+DPwA", /* 30: 7x7, sides and dots, 20 passes, 34 numbers, 38 long */
    "7:.321..003...0.1.1111.211.212.0121.2.2...113..310.:GEIAei+NeoADKoZgGEB", /* 31: 7x7, sides and dots, 20 passes, 30 numbers, 38 long */
    "7:1...1..222..123...003111.113001...302..222..2...0:+YkcIWu6MIkVNiEoEpA", /* 32: 7x7, sides and dots, 25 passes, 28 numbers, 44 long */
    "7:.1..2..112.2..3...2111123311000...1..1.201..1..2.:8597CCMQQTIwBiLoIiL", /* 33: 7x7, sides and dots, 29 passes, 27 numbers, 44 long */
    "6:21.1..2010.2....2231....2.1000..1.00:-AlZBACBD6qgBD", /* 34: 6x6, corners, 7 passes, 20 numbers, 28 long */
    "6:000123...2...0.11..22.0...2...233222:gYghGlagRiaZQX", /* 35: 6x6, corners, 13 passes, 20 numbers, 30 long */
    "6:3.22....3.3...3.3100.1...0.1....32.1:PXU0A0MRQRBJSK", /* 36: 6x6, corners, 13 passes, 16 numbers, 30 long */
    "6:.3323..1.....2.112002.1.....1..0122.:aVzMsswuiQEKJo", /* 37: 6x6, corners, 14 passes, 18 numbers, 34 long */
    "6:.1...1..22.21223.32.01133.21..1...3.:UAgritQ84xN0RY", /* 38: 6x6, corners, 17 passes, 20 numbers, 34 long */
    "7:01..23.112..03....0...20.00...3....00..103.00..22:kgmU4SiAhxGnHFIXQCF", /* 39: 7x7, corners, 14 passes, 24 numbers, 40 long */
    "6:.20.0.22..13...3....2...01..01.1.11.:BiJUGE8DK7XISi", /* 40: 6x6, corners, 19 passes, 16 numbers, 32 long */
    "7:.11233.3....13..001...11.12...222..22....0.20000.:viGOArYCCEnghIiVFMA", /* 41: 7x7, corners, 17 passes, 26 numbers, 38 long */
    "6:.2..1.3..2..301.2231.211..3..0.2..0.:LhV5FFGd8HURBF", /* 42: 6x6, corners, 21 passes, 18 numbers, 36 long */
    "7:3..1122.3.....01.3.13...1...00.2.12.....2.0000..3:-KqkJRMABHkjQRISgBK", /* 43: 7x7, corners, 20 passes, 23 numbers, 38 long */
    "6:02..2..2...3.2.21..32.3.3...1..1..31:8SoSLPQElrgXIY", /* 44: 6x6, corners, 24 passes, 16 numbers, 34 long */
    "7:..0..130.0.1...0..2..1.220.2..1..3...0.3.131..2..:wBCC9pox+BJUwgY4SEE", /* 45: 7x7, corners, 21 passes, 21 numbers, 40 long */
    "7:.21.3.032122...1....21.2.1.32....3...110102.3.00.:TCkioVUGOUjNNF4eDkA", /* 46: 7x7, corners, 24 passes, 26 numbers, 44 long */
    "7:1013........222.2221.1.2.3.3.1322.300........1322:4z65LXuQhh4YBBKRgCF", /* 47: 7x7, corners, 25 passes, 26 numbers, 46 long */
    "7:.0.0.1...1.01.1.13.1...2.1...2.11.1.23.0...3.1.1.:AB4QxuDG8DMwSzcgIKI", /* 48: 7x7, corners, 27 passes, 22 numbers, 40 long */
    "7:0..22.2..2.2.33232..1.1...3.2..11131.2.1..1.12..0:kwW3UVwIkwGhbEnjuYD", /* 49: 7x7, corners, 32 passes, 26 numbers, 48 long */
    "7:10233.2..2...22.1.2212..2..0322.2.21...3..0.32111:orI+ytrkZg7hhgRQ4SB", /* 50: 7x7, corners, 23 passes, 29 numbers, 48 long, showpiece */
    "8:.0.01.31...0..2.321......1.3202210010.1......233.0..2...22.13.0.:gBUAWhIQoSwEgCMjJTBM45oG", /* 51: 8x8, corners, 16 passes, 32 numbers, 44 long */
    "8:.0.22..01.2.3..1.310.1...1..0.2..1.1..0...2.210.0..2.1.01..21.0.:4QT8CDyDMohBIhCHMIhAGkQB", /* 52: 8x8, corners, 19 passes, 30 numbers, 44 long */
    "9:...3...0.00.2.1.1...12.0..312..00..13.......11..10..330..3.12...0.2.2.00.0...0...:YANAkRDYQMENQABoAkA5EYAS4QCKgB", /* 53: 9x9, corners, 18 passes, 34 numbers, 44 long */
    "8:.0..20..0..2..00.3...11.31.1........0.22.21...2.20..1..0..23..0.:IAhCNfqRhgwFYAF2IsISIhIH", /* 54: 8x8, corners, 22 passes, 26 numbers, 46 long */
    "8:.0.....0..3.13..1....3112.13...33...13.1322....0..22.3..0.....1.:QUiIgneLrQCPw4L5IjioKogI", /* 55: 8x8, corners, 23 passes, 26 numbers, 54 long */
    "8:310.00....1...10.23.11...322.22..21.231...23.20.21...3....12.132:BAARQI4IrA5TDYAP7f5ljhNa", /* 56: 8x8, corners, 24 passes, 34 numbers, 56 long */
    "8:.332.3..1...0..1..2...0.22..0......3..30.0...1..0..2...0..1.320.:aTiDPqUVcxCEuVgQMQfBUEBG", /* 57: 8x8, corners, 26 passes, 24 numbers, 54 long */
    "9:.23.22.2..2.0......122.1011......0..00.3.3.33..1......0002.102......3.2..0.32.10.:btAmCDoCmIG0IBDtv4jggI2jIQI8gC", /* 58: 9x9, corners, 24 passes, 36 numbers, 64 long */
    "8:3.22.311..32....2..1.23.2.2311....2223.2.22.3..2....10..311.00.0:7EbNKBCgQRQANxzB-j-9fPYA", /* 59: 8x8, corners, 29 passes, 34 numbers, 64 long */
    "9:.000..3...0...2.110.1.3.3....1.2.2.2.01.2.13.3.3.2.0....1.2.0.120.1...0...1..223.:ABEIFKYoQSABh1TADog3IOo-iZYAKa", /* 60: 9x9, corners, 26 passes, 37 numbers, 58 long */
    "9:..00.132.0.0..31....1.12130001.2....1.......2....1.10120121.0....21..1.1.311.21..:ABSoCXUdMIoHwQ+ADchIAFvmhBoRah", /* 61: 9x9, corners, 28 passes, 40 numbers, 62 long */
    "9:..32.112.12.0...3....1.11...1.0.2..12211.10221..1.3.1...11.1....2...1.00.122.00..:6nwG+MRyG3cCQgBOoggghqEj0RAJQB", /* 62: 9x9, corners, 30 passes, 40 numbers, 64 long */
    "9:3..2.......31.1113312.132....31.122.....1.....011.12....012.1220002.31.......3..0:7Lgl-8EnrQC8oAHNkDGCIIRA6QIMgE", /* 63: 9x9, corners, 31 passes, 39 numbers, 66 long */
    "8:..0...0.1..21.3..222.1212...132..123...2322.102..1.31..1.0...2..:QAaSmlaxLjFYwA6LM0apiIBU", /* 64: 8x8, corners, 36 passes, 32 numbers, 54 long */
    "9:..2..21.23.10..0.1.23.32..1.1.12..1.1.3...2.1.1..22.1.2..12.10.1.3..31.01.20..0..:sfCX4EUGBBhE0QA0YAijRZ7UP2gWYA", /* 65: 9x9, corners, 33 passes, 40 numbers, 66 long */
    "9:0001.2.2.0...22023..0.2....1322.2.2.22.1.3.32.1.0.2022....1.2..31201...2.3.2.2223:AB34WzAu0n6gFW7ADCCMup7Egof44m", /* 66: 9x9, corners, 39 passes, 46 numbers, 78 long */
    "7:.02.......1.1200...0....3....0...3312.0.......12.:oISBBKij-jHtICJ6kEI", /* 67: 7x7, a side tried, 26 passes, 17 numbers, 42 long */
    "7:211...2.1..1.1.2...1...311...0...3.0.0..3.0...023:-BsBAJQgAGYgsyKwgCM", /* 68: 7x7, a side tried, 40 passes, 21 numbers, 34 long */
    "7:.1..2.2132.0.20..1.0...210...2.1..11.1.1230.0..2.:wcaA4p3ghAlgIiUIQCD", /* 69: 7x7, a side tried, 49 passes, 25 numbers, 36 long */
    "7:.22.1.3.1.31...1....00..0..01....1...10.2.1.3.33.:-fyCgEYUUFIUkQiEC5H", /* 70: 7x7, a side tried, 60 passes, 21 numbers, 42 long */
    "7:..0..22.0.3.1.0....0...3.1...2....3.1.1.0.11..1..:gRQhgtgHQAKuEio0RgB", /* 71: 7x7, a side tried, 80 passes, 18 numbers, 34 long */
    "7:0.0113..0....2.2.2.1..1...3..1.0.1.1....1..0112.1:gRFwCohcABK2bHegEBG", /* 72: 7x7, a side tried, 93 passes, 22 numbers, 38 long */
    "7:211.21...0.....0...13.23.33.11...1.....3...13.100:fAAm6IKYIESIB2HGkwA", /* 73: 7x7, a side tried, 162 passes, 22 numbers, 36 long */
    "7:.2..3..12....3.1.23.2.3...0.0.11.1.0....22..0..3.:vny1jHgAAFLRgKIogBG", /* 74: 7x7, a side tried, 241 passes, 20 numbers, 40 long */
    "9:..1.12.2.1..0.31...13.1.1.021..2..0.3.22.11.0.1..1..102.3.3.10...20.0..0.2.10.0..:-H8kAGJoPqhCG4ABYI0EdgBEI-wAkA", /* 75: 9x9, corners, 37 passes, 40 numbers, 62 long, showpiece */
    "10:0.32.1.0.01.2.21...03.322.00..2..312..002....2.1.02.1.1....011..132..1..13.222.11...11.0.22.1.1.11.2:cADUAgACAKiEOh3Aw-TCgB-gvwR40EsSMBwAI", /* 76: 10x10, corners, 23 passes, 54 numbers, 72 long */
    "10:0.01.1..221..1...2..3.......0..33.0.21.101..11.1.10.1.10..320.02.2.22..0.......2..1...1..200..0.01.2:wNU8JFgCgUYBGAJoEADZBrBTnJ0EhD+R+4ZAK", /* 77: 10x10, corners, 28 passes, 46 numbers, 78 long */
    "10:332..22.3.2...1.1..012.0.0..2211.1..0..2..2.1...2112...2.1..1..2..2.2321..1.3.113..0.3...0.3.00..000:NnsNYE4hcVGDSWIhgAclCEEIEYgFPLzVPlBDA", /* 78: 10x10, corners, 32 passes, 50 numbers, 80 long */
    "10:...2.1...0122.1...10..11122...2.2.2.02.3.3..21.2....0.10..0.1.10.0.2.0...20232..12...2.1213...3.1...:fAHIWMKirBACRgSCQkFCKAjub4rIQEQaO1qZF", /* 79: 10x10, corners, 32 passes, 48 numbers, 78 long */
    "10:.111..3..0.0.0.1..0..0...0231.11...1...1.3.3............3.3.3...1...01.3221...0..3..2.3.0.0..0..200.:-BQAMAjyHUD6AIEWBIEICCBkgwHk7nF4CaCwA", /* 80: 10x10, corners, 34 passes, 40 numbers, 70 long */
    "10:3.131.23....2.1..1003......0...12.21..1.12.1.3.33..21.1.0.12.0..21.01...1......1331..2.2....32.320.0:5jy0AJxN5LJIMrKJBJsQqCChMBwTAlBTK3C2A", /* 81: 10x10, corners, 36 passes, 48 numbers, 82 long */
    "10:31.0..1000..1.3.3.....2...1.1.22...3..00.322..1.1..2.3..110.20..0...00.1.0...3.....0.0.3..3211..1.22:BAUYIBioIOpAwMoD2-PAmH5F9BHRHEECIE6AI", /* 82: 10x10, corners, 40 passes, 46 numbers, 76 long */
    "10:...0.12.101.0..2....3200..2.0..3.1.2.31.2..12.2......3.10..3.10.0.0.2..3.0..2222....1..2.221.33.2...:ACcELSigYqI0oNAIy1CYgIDj6y8IjHAJ3h78O", /* 83: 10x10, corners, 45 passes, 46 numbers, 84 long */
    "8:3....2.2..102.112..322..12....2..3....20..101..022.000..3.0....0:-HSFoKlMCAQABUsdtUTpADYA", /* 84: 8x8, a side tried, 32 passes, 30 numbers, 50 long */
    "8:.1...22...20132.01....1.1..122.00.323..0.2....21.23123...21...3.:8DGp2YiVlIRSEk0IRC4RQtbb", /* 85: 8x8, a side tried, 34 passes, 32 numbers, 58 long */
    "10:.0.3..1.0....1..2.110.32..1..302.0.3..11....0..1.00.2..3....22..0.2.002..0..31.021.1..2....3.2..1.0.:IAZQ6UuI4GuQQQEBgOgBoFYRyhgAGIECENBnA", /* 86: 10x10, a side tried, 32 passes, 44 numbers, 64 long */
    "10:..1.2121..0.......02..010...0..12.3.1.1..2....0.0..0.1....2..1.3.0.12..3...021..10.......0..3211.1..:4HWCCopFGIIkAWEMBfggAnASckAMwmp-EQBCB", /* 87: 10x10, a side tried, 51 passes, 40 numbers, 70 long */
    "8:3.3.32..1..21..1..3.2.33.12311....22110.21.2.3..3..22..0..22.1.0:VqGXs6gP90xH-OglPCDIIGIE", /* 88: 8x8, a side tried, 58 passes, 34 numbers, 64 long */
    "8:..2..2..3.1.11.2.1.2.21.2..3.1.01.0.1..1.31.2.1.2.23.0.0..2..0..:2GU6ZgnYYgQDaPjiIBB6EP4C", /* 89: 8x8, a side tried, 62 passes, 30 numbers, 56 long */
    "10:.1.22.111.11....1.2..0.11.2.1.1..0.01..22.0.0....12....3.1.20..32.3..2.0.2.21.1..0.3....00.001.23.0.:-AvxrALAFyKCRjCYBYEEKUB1AUARMyfhBIBQB", /* 90: 10x10, a side tried, 64 passes, 48 numbers, 72 long */
    "8:.00.3..3.0.1.12...12...101.2.22..12.2.323...31...21.1.0.0..1.11.:QCAQMxyVbAA-wG2wl0QPlEkg", /* 91: 8x8, a side tried, 75 passes, 32 numbers, 52 long */
    "10:..132....10.22..11.10.20..10....11..01.01.2..11..23..12..2.33.02..22....12..22.33.21..12.33....013..:oFoQBikH9tRZCGmF6gj7wSkJBCIKoK7StKsCJ", /* 92: 10x10, a side tried, 74 passes, 50 numbers, 86 long */
    "10:00..01.232.0000....30..0..22.21..2.1222.3.1.233......211.1.2.2222.1..13.22..2..11....1010.212.21..12:AFACoUzQGSCUMoLIw9D8AuAJeW+VTzZfuBwMI", /* 93: 10x10, a side tried, 113 passes, 56 numbers, 86 long */
    "8:00...23300012.1.0.....13..31..1311..22..32.....3.2.10112122...22:wCFiUy+ihRofQHk4igwo1OUQ", /* 94: 8x8, a side tried, 126 passes, 36 numbers, 56 long */
    "8:01.1.00....1...1...2..13.2132.1322.3122.31..2...2...0....22.1.12:EAA4IqaWndw-MgBskWKN0IMg", /* 95: 8x8, a side tried, 157 passes, 32 numbers, 54 long */
    "10:.13....2.31..221.1...0.1..2.1..231...22..2.112.3.12.3.221.1..13...230..2.2..1.1...2.011..00.3....00.:nP1zEkugTIUCpkOAABkCrCBi2464ssNuJMAGA", /* 96: 10x10, a side tried, 194 passes, 48 numbers, 84 long */
    "8:.012.33..1..0.1..0.3.01.0..2.2....2.2..2.12.0.1..1.2..2..23.323.:QxCjMx67sWgVwjQQk4wIg1Tf", /* 97: 8x8, a side tried, 368 passes, 30 numbers, 60 long */
    "10:1.....2.223.120.01...2.3.0....3....222....0.1..221022..1.2....321....0....1.2.0...13.200.000.2.....0:U3K+4PVooWACRgCIAEwnCgQigmoijwJgCoAYA", /* 98: 10x10, a side tried, 415 passes, 44 numbers, 70 long */
    "10:..1...1.0.1.3.210..0..2.2.01..0.2.3.2.1.1.2....2....2....2.0.1.3.3.2.1..21.1.1..1..100.1.3.3.1...1..:4gMQAWiUALpF7cgGvPgE8ASAUjXxRUzCQIQhA", /* 99: 10x10, a side tried, 835 passes, 42 numbers, 76 long */
    "10:3.2.21.3.0.2.1.2.21.30..13...0...0..1.1.313.32.2.12.2.12.122.1.1..1...0...22..13.21.3.1.2.1.3.32.1.1:bSQUIhS9E-iEhIpUk21acLhDBSHwBi2J3SF3E", /* 100: 10x10, a side tried, 113 passes, 52 numbers, 92 long, showpiece */
    "12:2..3....00.02..0.3.0.0..13...1.0.0.0...031.1.1....21..2212..3..1.3...1....2...1.3..3..1322..21....0.2.223...2.2.2.1...00..0.1.0.0..03.32....0..0:LA0AzASASOVBk0654EbGrAKAFAdACCQBQEAlk1IHMTMEsgNQEgHA", /* 101: 12x12, corners, 26 passes, 62 numbers, 96 long */
    "12:..22.32..11..21.01..1.0.1.2.1.0..20.......1.1..2.3.21.1.3.10.0..3.0..2....3..2.2..3.01.1.3.30.1.0..0.2.......00..1.2.0.0.0.3..20.00..00..31.00..:Y-mAIEGqKUVKFUqagCACYBYAgAoBFAqxESmjjYI+sAGASAUASAwA", /* 102: 12x12, corners, 35 passes, 64 numbers, 90 long */
    "12:.22..12...2.31..0.3.222.2..0..1....2.13.3..011.222.3.10.0.1...1.....0..20..3.....0...0.2.23.0.120.001..0.21.0....2..3..0.000.1.0..00.0...33..10.:zPN3B7U8BAIwawoBQ5gEAEABgGVQESGQoHoPQTIIAhFCkAkAICwF", /* 103: 12x12, corners, 39 passes, 66 numbers, 94 long */
    "12:3...3.0.00..22213.131..0......2..10.2.0.22..2.1021...3...1.....0.0.1...03...1.1.2.....1...3...0033.1..02.3.0.20..2......2..012.11133..22.2.1...2:ZAUCwAyMhMQBBMTNhKSCikQRt-rAuMccISi8BBCBQoPchJ0zx8bg", /* 104: 12x12, corners, 43 passes, 64 numbers, 114 long */
    "12:.1.13...00.013.1..32..0.2..1..03.0....2..0...0.02013....201.31..2..1........3..0..22.001....22111.1...2..1....0.11..1..2.1..00..3.113.11...11.0.:QACBgCFCACp1RpQYYn4fB6BE-BwAsHYRITQGBRtRhCTkAOAogjAB", /* 105: 12x12, corners, 47 passes, 64 numbers, 98 long */
    "12:21.1.2.1.12....0.......11.1.0..2..202...1.3.3.311..3.11.3.2...221.00..0010..23.000...1.1.20.0..013.0.2.0...022..1..2.0.02.......1....23.3.3.0.00:--gxgCGVyVIM8AwAzACBxCUCbBBADlEUZRVHlCABBiAQEQQQUh2B", /* 106: 12x12, corners, 49 passes, 68 numbers, 98 long */
    "12:0.002......0..0..113.2....103....12.31.1..21.....2...23..31322.2..2002....2222..0.32112..21...1.....20..0.01.13....123....1.032..1..0......231.3:gGQCS6RwRoYJ1BvRcAYIkGgWgngLgaYOsai5aH4RyBBSAkFLwUQ0", /* 107: 12x12, corners, 52 passes, 66 numbers, 112 long */
    "12:.2232..2.1....112323...3.0......2.0311.1..321....12331122.02..1..1..21120122..3..2..01.00122221....011..1.3201.1......1.0...212100....3.2..1000.:p-mmwiEjQjJAaxEwAoQSEBEAsB7AjWFwsJVPTTmIfIeScjOAuAaB", /* 108: 12x12, corners, 64 passes, 78 numbers, 116 long */
    "12:.0..1..1..2..3.00.1...03..1.2.3..0.32...1.22....13........1.0..120123..20..131213..0.3........11....13.3...30.0..1.1.1..00...0.02.2..0..0..1..1.:4fFgihdloKSsUUCFOigWAwAEAMIgOAGKyopvUET0akrAOHAJAGAK", /* 109: 12x12, a side tried, 43 passes, 60 numbers, 104 long */
    "11:..0.1.3.10.0.13....1...1.2..31...3..1..2..00.....0.3...0210...1121...0.1.....13..0..2..1...00..1.3...3....01.0.23.1.1.2..:gCkAw4IMRAECZB-wzARA6PgH4E4BxCEOEICjEBBQLIHI", /* 110: 11x11, a side tried, 50 passes, 48 numbers, 80 long */
    "12:00....1..23....0101...120.01..3.1.1...02222....1.......2...10.0.1.1.110..111.0.3.0.13...1.......2....00100...1.3.2..20.100...3222....00..2....33:AWAiQDADoBIIAAHCJAWCoEwSwrA6A4CnFmLiXIjQGzAKAE3IwSI+", /* 111: 12x12, a side tried, 56 passes, 64 numbers, 94 long */
    "11:0012.3..222.1....2......13.3..10.10.3..0.01..2.32..3.0..2.......0..1.3..21.1..13.2..1.01.01..1.11......3....3.000..2.2222:waWATBFmKdAPFmDHA6B+g-QvkuwhIQznhnwokhAhwwQg", /* 112: 11x11, a side tried, 77 passes, 54 numbers, 102 long */
    "12:..331.000...3...1....0003.0.3..0.00...1011..0.0.331.1...0..1.1..1.101.2..2.112.1..3.1..1...3.022.1.1..2211...33.1..3.1.2301....2...2...222.323..:LAFAhATBhACpEAjWAyCxVaBKutdAkAEHgQw5gQ8t4RGiUxxYMaZ7", /* 113: 12x12, a side tried, 76 passes, 70 numbers, 108 long */
    "12:.2.1.22..10..3.0.1.3..002..02.22....100....2.2.....3221..2.3.3....0.0.1..1.1.0....2.1.3..1223.....2.1....123....01.00..120..1.0.0.0..11..00.0.0.:-EDBjKQPovbQDqcAElhwJAAAPABNIcEGJCQKcCgB+K9lGVBQEgIA", /* 114: 12x12, a side tried, 84 passes, 64 numbers, 102 long */
    "12:1.20.1..112.....1.1...1....12.1.1.2.3.101..1..2...100.....023...0.113.3213.310.1...012.....212...1..3..232.0.3.1.1.11....1...2.0.....001..0.00.0:CfEjZ6CwhIDtLUCbQZUEiBAAQAGhUIGq4ULxwAacI0Kp6wSAMAYA", /* 115: 12x12, a side tried, 106 passes, 66 numbers, 102 long */
    "11:..2.3..0..023110.211.0....112.3111.0..2......0.3.2.333.....2.1.....323.2.2.3......2..3.1221.132....2.201.121003..3..2.1..:XgVwwB+iWwU6L8IYBcQDSG5AEBBIRwp+BgP8HOnAHEbF", /* 116: 11x11, a side tried, 120 passes, 58 numbers, 102 long */
    "11:..0000..11.0..0.1.0.0.0...0...3.1......2.21.000...13...000.2.2.212...22...223.22.1......1.2...3...3.0.1.3.2..3.00..1013..:AegAQBIALgFYzz7VERCiBIAiAhAuAtQqAtoYBg4TglAM", /* 117: 11x11, a side tried, 137 passes, 54 numbers, 78 long */
    "12:.1....1.....1..12.211212.0.....222.1.0.111....3.1.....0.31.0.0.3.3...3....1...1.1.0.1.22.3.....0.2....000.0.3.301.....0.213100.00..0.....0....0.:-3goYP4HAUoMAPjAiAVAFAEACABYDDlALAXAh8I5A4CoDoAgCADA", /* 118: 12x12, a side tried, 169 passes, 60 numbers, 90 long */
    "11:.023.23.222........2..002.231..0..1...21.2..31.2..12..0.1...3...1.2..01..1.20..1.31...2..1..112.231..0........222.12.231.:IZaCvg32rwIkAIlCBJlcMPYrEmAhEaDFrJHI3WpaxBqI", /* 119: 11x11, a side tried, 188 passes, 55 numbers, 104 long */
    "11:.321..2.1120...2........3....0.1..02.21...02......2....32012.13002....2......01...13.13..0.0....0........2...0000.0..320.:+f7AZkBSNWFTSkFRCMBWAECgYi0QggclhiUUMJASgIAG", /* 120: 11x11, a side tried, 238 passes, 48 numbers, 86 long */
    "12:2..2.2......3.3.22211.00..1....22.0.32..22.1..0.322.3.2.3.0.2...1...310..020...2...0.1.1.1.1.220.1..0.13..31.1.11....1..32.21231.2.3......1.0..2:uBFBDGDABEREkEkNASgQB0JpTQyC6Dgvofh-hmFhGag0Yq9i7yaY", /* 121: 12x12, a side tried, 355 passes, 68 numbers, 118 long */
    "11:.0..31.131..0.2.1.2...0112......11...32.3123.1.0..22....2111.2220....31..1.1.3221.00...01......2100...3.1.1.0..322.13..0.:YEIAoQQTtBPunHyBAZ4E8DoMYM4L+lCIiAQIBEIEQBCC", /* 122: 11x11, a side tried, 382 passes, 60 numbers, 84 long */
    "11:00.0..2.10..0..2.1..1.0.1322.12.2...1.2...3..0...221..100.......210..010...3..0...2.0...0.01.1111.1.1..3.2..1..22.2..3.12:ADQIWBHAnAPQKgGMA68Z+9AFg0I7A6QgwoA2AhQggpBj", /* 123: 11x11, a side tried, 499 passes, 56 numbers, 86 long */
    "12:322..2.1.2.3.....3.1..2201.1..3.1..3..32..1.0.3..1..1.1...1..1211...3.1..1.1...1022..3...2.0..3..2.2.0..01..0..0.0..0.2200..0.0.....0.0.0.0..013:z-t8e90xAwolkqmQaQEQAAAAAgVAFLAIkDCHQSXIRFBzAFAMAYAw", /* 124: 12x12, a side tried, 1752 passes, 64 numbers, 98 long */
    "12:00.0..0....2..11...000.2.12...00022..12.22.1..01.031..2.3...0.3.1...2......2...2.3.3...2.1..110.12..2.11.00..11122...00.2.011...00..0....1..2.12:AwAQMQQIkCEEcuOvGAnDxDKC09AQBATBlEJyTihgxkBiDMioAQOj", /* 125: 12x12, a side tried, 206 passes, 68 numbers, 98 long, showpiece */
    "14:.0..311.11..12..01....1.0....3....1..2....22.20.013..0.3.1.2..0.311...31...3.....002.0..1.00.1..1..2..1.02.1..0.022.....1...01...222.1..0.0.0.1..201.32.23....1..0....2....0.0....21..00..00.011..1.:w-DBgmDAGmQYNB5QGQpmAQNQiKEhAMBwBAgQAEEgOS0BTVUUJjBoSgisSkrgw8DAuAkGAw", /* 126: 14x14, a side tried, 46 passes, 84 numbers, 128 long */
    "13:000001.22.0.3......22.1....0..21.3..32200.2...1.2....0..31..1...2.0.13..1..0..00.011.100.01..1..2..00.0.0...2..11..0....1.2...3.01031..1.23..1....1.22......3.1.11.232222:ADBKgIBeAGAoUwAACwCQ4hUDZeB8e6DUMgyY8jgPI+QDCIIQg0AKHJhhSDHdI", /* 127: 13x13, a side tried, 57 passes, 80 numbers, 118 long */
    "15:..322..00000.0...31..1....000.1..1...0.1..000...13231.3..0..32....32.21.0..23.01.2.2....0.2....22..0..00031.1...3...0.00200..1..00....0.1....0.1.10.00..3.10.12....00..1..1.13122...232..1.0...2..3.122....3..12...3.13223..332..:MAgGA2AQMBRFAYByHIcABCASAHY4CILUei2lRRSFUAASAIBA0awRFcgARAgMBELABEACLEzOsQZGjY7H", /* 128: 15x15, a side tried, 59 passes, 107 numbers, 138 long */
    "13:3.33.1..123..12.......0..1....10000.0..121..1..11..12....3..2..0..23.1..2..00.1..112.211..1.00..1..2.31..0..2..1....10..02..2..321..0.01111....0..0.......13..000..1.31.1:1vVgIAICgwjSGLDg9A0BKCYIAEAIBY8BbAgHASDockdECEIkoQFCaLA1AMDQB", /* 129: 13x13, a side tried, 75 passes, 76 numbers, 108 long */
    "14:13...13..13..2.1.0.......2.1...002.31.1.1.2111.0.......2.2..3..1.32.2..1.2..23....1.12.32.22.012.13.132.23.01.31.3....02..3.2..1.00.2..0..1.0.......0.1322.0.0.01.211...0.0.......1.1.0..00..13...12:KfTFBoMBD5hRnKYxuIUrdC2DECwwAsDwAA-ehVUqCFMA4jonwysFJeQGYAQgAhFgEAQEgg", /* 130: 14x14, a side tried, 78 passes, 90 numbers, 144 long */
    "13:.31..1.........1.1...00000.2.003100..0.30..1...0.0.0....1.20..000.2......1....32.211.131.00....1......0.231..31.0....1.0.0...0..21.0..010101.0.21311...2.1.........2..33.:eAEAICIEQIApAhFQCgpUEEAESFBg0qKCAJACBECgQA0CcxwzDnRHAbAgtf6+P", /* 131: 13x13, a side tried, 101 passes, 74 numbers, 106 long */
    "14:3.2.1023..1.....11.12..2.0...21..3..22.23.2.....1..1111..2000.1.1....2..1....1..00132.....01.3.02..22.0.02.....12113..1....0..3....0.3.0211..0013..2.....1.00.31..1..10...0.3..23.00.....3..2110.0.0:NKAUAkTg8KAsgwAMCgEjyISkJMUIQHgdAOAXe4KCnD7wgBG5AgHU6gJTSchkgiBKAgIAJA", /* 132: 14x14, a side tried, 98 passes, 90 numbers, 130 long */
    "15:..3..1.0..0.0..0.1.231000..1.1.12......0..323112.32...23.2.......2.3.3.1.2.1..2.11.......3..2.3310132.1...32..1...2..13...0.0013111.3..1.......20.0..1.0.0.3.1.2.......0.01...32.100231..0......00.2.0..131100.0.0..1.1..2.1..0..:EAAHA4AFqCJgSTZWIkFWC6AAIgVDcADqYIAAH4-AMAARAwA88--5HmE0zZxBiFYcBGUIeEwHECQQAEgA", /* 133: 15x15, a side tried, 101 passes, 102 numbers, 152 long */
    "13:.2.2....11..0.31222......0211.2.323121....3.010..1.03..1..0..3.2..0.1..3..0.1..2...1.2...3..0.1..2..1.1..0.1..2..0..01.1..122.0....322211.1.0001......10000.2..00....0.0.:vCGQsABqQCqQrdF7HBfAOgjA7BEAGAEfA9HpvkBiAWhjawWACRAEIwEBJAUAA", /* 134: 13x13, a side tried, 121 passes, 78 numbers, 118 long */
    "15:.2.32.3.12222......0...0....1221..0.3..32.32.232...2.....10.1..0.12.3120.0.1....0..21....0..231..3.3.33...22..0...2..12...23.1.2..210..3....02..2....0.3.2113.13.1..0.21.....3...000.02.31..1.0..0022....2...0......31111.0.00.0.:1OHJGEpmAICSAoCOEhRFJfa4QDrRIBREoCA0BwAAfT5BAusPH7QQDEIDT+zBgwFSZoCXFo5ApCglAoAA", /* 135: 15x15, a side tried, 142 passes, 102 numbers, 162 long */
    "14:.3.......23..3..113210..1.233.22.2.0.......00.1310.2222.......0.3223..0.32...0.1.0.1...2.21...02....21...01.3...0.2.0.0...21.0..2323.1.......1321.2323.13.......2.2.21.000.0..221221..0..00.......0.:dTLxoNo5wByKHX8AxhCIEs4woEDcQYAgAQFn1NKcLAEIygkhAQABhMHEIAOIkfC+BAnA4H", /* 136: 14x14, a side tried, 152 passes, 90 numbers, 142 long */
    "14:0....31...0.....0.3..00..0003.0.3.00.0.0.03.1.2..00...0..1.2.......0.022...12..2..0...210.13220..01..01222.100...0..2..13...001.1.......3.1..0...22..2.1.11.0.3.21.2.3.0231..11..2.2.....1...11....3:8AIDQEAZAEjghBMKIKAQBYUgrGcXyvAAf--EBQCAhAQBAZZYaCHNIUBhFIJHBGKCMHQ4Ag", /* 137: 14x14, a side tried, 204 passes, 88 numbers, 128 long */
    "13:1.2..2..2.1.2..011.2.3...1..0.0111...0.3.23..0.1..0..1...2.0..111..2.13.1.2..22.2.3.0.2.1.20..2.1.11.2..000..2.2...2..0..3.1..22.2.0...2332.3..0...0.2.211..0.0.1..2..0.0:W-ROEeIBRFAUShIkQgHAuAsAgogDAPoHoIiBAqhZJmd17zOC9A2DhPw-AMAiA", /* 138: 13x13, a side tried, 209 passes, 79 numbers, 130 long */
    "15:3.21..10.00...0.2..13.00...0.02.........10..03111..2311.1..0...3.32.1...100.01......22.......1.113.3.10..1.3.2.0.0.0.2.1..32.1.010.1.......11......13.231...2.12.3...1..0.1112..21001..00.........01.0...00.11..0.3...11.22..00.0:-A4EAiAoYCrDgsIQgmRxFAhiYgrS8IAgAAGCEofABBAKA4EAB0APMA5PgeINgsDEHM0J8EgDBYRACcQA", /* 139: 15x15, a side tried, 204 passes, 102 numbers, 140 long */
    "15:.002.1.0..000......1.0..0..0.00........0.00.00.31..23...0..001.12.....0.0......1.222.1..0..22.01.2..0.0.0.3.1..1.1..0.0.3.3.0..3.30.11..2..1.010.2......2.2.....32.023..2...10..13.11.10.1........11.0..0..0.0......111..1.1.111.:QAABAAAgaAUCwgBmIQABBIoQZeQ8NKzxRAAAAw--wAAKAgCAUFA6AIeBIOgjF04ACQFIYGJpEAoAAKAg", /* 140: 15x15, a side tried, 232 passes, 96 numbers, 120 long */
    "13:.00.2.1223...012.1..1.1.13..2..101..2.2..2213....3.....2..100..0.2..21.3..00010..3..2..1..21122..2.21..3.2..131..1.....3....1312..3.1..222..0..12.3.2..1.001...1310.0.00.:grwgaA3n8LkqAKAQ4eC9kyBpVgECgAAyHCjCcAEbMgODCNIyZhWisawx8GADA", /* 141: 13x13, a side tried, 270 passes, 83 numbers, 128 long */
    "14:.0000.01.2....0....0...23.030.0..00.3221...0.0..0.1..31..2.2.11.1.2.0.3.1..2....3.0...1....0.0..0102..1.2....0...0.2....3..0.1.0.2.0.21.0.0..01..1.3..0.1...3222.22..0.201.32...1....1....2.21.2223.:A8BwJAnA8ShywABUoEApBIHAIIZiADgAE3tAECAkAECAo84Mvn6BkUgIJECgQGinj8ZkM7", /* 142: 14x14, a side tried, 280 passes, 88 numbers, 130 long */
    "15:....0..3...0......01130.2.2.1..0......33.22.001.31..222..2.1.1.......2...2.32.10131.12..2..1....1......21.3.1.00.01.0.2.12......1....0..2..23.01223.01.2...2.......2.2.2..132..10.113.02.22......0..0.1.2.32210......0...0..1....:ACAoJQlgEJIMMAYMBnBaCI0FEMAhoyIFIQBQFAABAGAQ0A7LcnBxcfMjTwxBAFghz55ZuuaAzJA8FAgB", /* 143: 15x15, a side tried, 286 passes, 94 numbers, 150 long */
    "15:0.000.00..1...0..0.0..22..31.1..00..3..2....20....2...2.2.11.001..2212.2212...11..2.1.1..30.13..111.0.3...1..2.1.1.0..1...2.2.113..01.32..1.1.1..01...3021.0112..011.22.2.1...1....11....2..2..01..0.10..22..3.1..0...1..33.221.3:AIAgEALGaAgLA+ioHEEoSEEE4FAvJAUCywiAEGU9AYAAdAkuwsLE8Cgc4ALWA52gOMRADa9gqJO4sA+j", /* 144: 15x15, a side tried, 333 passes, 108 numbers, 156 long */
    "13:..000.00..0.30.0..0.0...22000.0...32...0.00.0.21..23...2..2.131...01...1.....10.012...223.10.....1...20...310.1..2...11..00.3.11.2...11...1.13233...3.2..1.21.1..32.321..:AABAAQCAJ0SUE4HgnwohgzhYd3lMA7DAMAwAsCQHcqIwCgLEjEiiYYAIPTCaI", /* 145: 13x13, a side tried, 359 passes, 80 numbers, 112 long */
    "14:22.1.122.0.2....0.02..2..0.1.21..2..2........112..32.32...2.2.101..2..1.2.11.0..013.....22.....021001.....31.....001..0.21.1.2..0..222.2.3...00.01..221........3..2..32.0.0..1..12.1....0.1.233.0.13:+jHURIAGlZwKHgKABTEGAgQAMCIZArBIAYhCkMYjiaEt2iBxI4YBEh5gMHo-AYGsmgIG0x", /* 146: 14x14, a side tried, 401 passes, 88 numbers, 138 long */
    "14:0000..3.3.122..1..1..1..01.....0.2.22.1..23.20..2....1...0.1.11..0.132.0.3...2.1.....1.1.322.2..0..2..1.022.3.0.....2.2...1.1.101.0..22.0.1...0....1..11.20..0.01.1.3.....00..0..1..1..000.0.0..1333:g6CUwwDBOUKAUQIMYaBtmNACQMQ0AIAAEAogPHE4lR3c4JEP-ghTcBlXHC4gALBAnAgEA8", /* 147: 14x14, a side tried, 1101 passes, 88 numbers, 136 long */
    "13:3.3.1...12....1..11112.1.3....2.1.2....2.1....2...3.1.2.20.0.0..3.2..3.1.1..2.1.32.....31.3.0..3.1.1..1.0..1.1.01.1.1.0...1....2.2....1.2.3....0.0.32113..3....01...2.0.0:7-LQWfM1lWgLgrvpfChALACVQiQkAE0AoAWdQaOOC4xgwAJEQgYhsF0pwqBYA", /* 148: 13x13, a side tried, 1166 passes, 70 numbers, 134 long */
    "15:...00.12....0.000..0....313...0111..1.....10......1.122221.0...100....2121..1100.13223...31...00..2.2..2..0.10..1..12.0..1..2.2..12...02...21211.3310..2321....201...2.121221.2......11.....1..2000...222....1..000.3....10.11...:AMAYFAIwnBGY4Q6AGGgNo8wICMsglBAQolEY6gAAAUAQaAEHCkBCTEojBwSABkHEzyxpBMNBTjApTwAA", /* 149: 15x15, a side tried, 1576 passes, 107 numbers, 138 long */
    "15:.23...22.3.2....2110..2..22.33..10.3..2.2.20122.....11.3.31..221.0.0.0...0.1...01.1.13.3....10..........010.013.1.211.001..........21....3.22.2.11...1.0...3.0.3.321..10.3.00.....00232.2.0..0.00..22.10..0..0000....2.1.10...00.:VLHFWgzF8MEQRIKEABEFgQgXd8UDGFSAgEACBYPA-d5BjfiBHwyCQ0MLBzRoCEsDFEUQeEBAnAwSA0IA", /* 150: 15x15, a side tried, 87 passes, 105 numbers, 158 long, showpiece */
};
