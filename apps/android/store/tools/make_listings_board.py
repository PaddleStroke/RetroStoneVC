#!/usr/bin/env python3
"""The Google Play store listings of RetroStone Board (paid) and RetroStone Board Free, in the app's 22 languages:
apps/android/store/<board|boardfree>/listings/<locale>/title.txt, short_description.txt (80 characters at most),
full_description.txt (4000 at most), and <board|boardfree>/listings-all-languages.txt: the same format as the Arcade's
(make_listings.py's write_listings()); and <board|boardfree>/release-notes-<APP_VERSION_NAME>.txt (the Arcade's
format: <locale> ... </locale>, 500 characters at most).

    python3 apps/android/store/tools/make_listings_board.py

The facts come from the app's code: the games (src/games: each game's .id), their menu tab (shell.c's SECTION_OF; the
others are strategy games), their names in each language (help/<lang>/<id>.txt, the first line: the menu's), the
tabs' names (help/<lang>/_menu.txt), the bots (src/games/<id>_bot.c), the games for one player (.players), the
designed levels (.levels, resolved through the games' #defines) and the free edition (app-boardfree.conf: FREE_GAMES,
FREE_LEVELS). The texts are here; English is the source, French is written with "tu"; the 20 other languages were
written by an AI (Claude) and need a native speaker's review, like the Arcade's (CHECKLIST-board.md).
No trademark: the games go by the app's own names, and a check refuses the known brand names (TRADEMARKS).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import glob
import os
import re
import sys
import unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
BG = os.path.join(ROOT, "apps", "boardgames")
sys.path.insert(0, HERE)
from make_listings import LOCALES, write_listings  # noqa: E402  (the Arcade's languages and file format)

# the menu's tabs, in shell.c's order (enum SEC_*)
SECTIONS = ["PUZZLE", "STRATEGY", "CHESS", "RACE", "CARDS", "GUESS"]

# brand names that must never appear (regular expressions on the casefolded text); CASED: on the text as written (the
# card game's brand, not Spanish and Italian "uno"); BY_LANG: in one language only (数独 is a brand in Japan, the
# plain word for the puzzle in Chinese)
TRADEMARKS = ["scrabble", "mastermind", "superhirn", "yahtzee", "yatzy", "kniffel", "sokoban", "othello", "オセロ",
              "connect four", "connect 4", "vier gewinnt", "puissance 4", "battleship", "rummikub", "abalone",
              "guess who", "qui est-ce", "wer ist es", "skyjo", r"\b2048\b", "kenken", "picross", "lights out",
              "catan", "freecell", "tripeaks", "tri-peaks", "circuit scramble", "shanghai", "monopoly", "cluedo",
              "boggle", "stratego", "mensch ärgere", "malefiz", "dobble", "triominos", "jenga", "tetris",
              r"\bmemory\b", "skip-bo", "phase 10", "halli galli", "nikoli", "sudoku.com", "microsoft"]
CASED = [r"\bUNO\b", r"\bUno\b"]
BY_LANG = {"ja": ["数独"]}
# a game's name in the listing when the menu's own is, or looks like, a brand: the game's generic name instead
# (the menus call the dice game Yatzy in these languages: reported to the owner)
NAMES = {("sv", "yacht"): "Yacht", ("da", "yacht"): "Yacht", ("nb", "yacht"): "Yacht", ("fi", "yacht"): "Yacht"}

# the store titles (30 characters at most): "RetroStone" (the brand, never translated), then what people search ("board
# games"), and the count as the Arcade's paid title has it. (paid, free); None, or a title too long: the English one.
TITLES = {
    "en": ("RetroStone: {n} Board Games", "RetroStone: Board Games Lite"),
    "fr": ("RetroStone: jeux de société", None),
    "de": ("RetroStone: {n} Brettspiele", "RetroStone: Brettspiele Lite"),
    "es": ("RetroStone: {n} juegos de mesa", None),
    "it": ("RetroStone: giochi da tavolo", None),
    "pt_BR": ("RetroStone: jogos de tabuleiro", None),
    "pt_PT": ("RetroStone: jogos de tabuleiro", None),
    "nl": ("RetroStone: {n} bordspellen", "RetroStone: bordspellen Lite"),
    "sv": ("RetroStone: {n} brädspel", "RetroStone: brädspel Lite"),
    "da": ("RetroStone: {n} brætspil", "RetroStone: brætspil Lite"),
    "nb": ("RetroStone: {n} brettspill", "RetroStone: brettspill Lite"),
    "fi": ("RetroStone: {n} lautapeliä", "RetroStone: lautapelit Lite"),
    "pl": ("RetroStone: gry planszowe", "RetroStone: gry planszowe Lite"),
    "cs": ("RetroStone: {n} deskových her", "RetroStone: deskové hry Lite"),
    "ru": ("RetroStone: настольные игры", None),
    "uk": ("RetroStone: настільні ігри", None),
    "el": ("RetroStone: {n} επιτραπέζια", "RetroStone: επιτραπέζια Lite"),
    "tr": ("RetroStone: {n} Masa Oyunu", "RetroStone: Masa Oyunları Lite"),
    "id": ("RetroStone: {n} Game Papan", "RetroStone: Game Papan Lite"),
    "ja": ("RetroStone: ボードゲーム{n}種", "RetroStone: ボードゲーム Lite"),
    "ko": ("RetroStone: 보드게임 {n}종", "RetroStone: 보드게임 Lite"),
    "zh_CN": ("RetroStone: {n}款棋牌游戏", "RetroStone: 棋牌游戏 Lite"),
}


def title_for(lang, app, n):
    paid, free = TITLES.get(lang, (None, None))
    t = (paid if app == "board" else free) or TITLES["en"][app != "board"]
    t = t.format(n=n)
    return t if len(t) <= 30 else TITLES["en"][app != "board"].format(n=n)


# {n} the games, {f} the free app's games outside the puzzles, {p} the puzzles, {m} the games only in the paid app,
# {b} the games with a bot, {mp} the games for two or more, {L} the games with designed levels, {lv} their levels
# (rounded down to the thousand), {fl} the free app's levels per game
T = {
    "en": dict(
        short_board="{n} board, card and puzzle games, with friends or against the device. Offline.",
        short_boardfree="Chess, backgammon, solitaire and {p} puzzles: free, offline, with no ads.",
        intro_board="RetroStone Board gathers {n} classic games around one green table: chess and draughts, "
                    "backgammon and go, mahjong, hearts, belote, bridge and tarot, sudoku, nonograms and solitaire. "
                    "Wooden boards, cards and dice drawn with care, gentle animations, and the rules of every game "
                    "inside the app.",
        intro_boardfree="RetroStone Board Free gives you {f} classic games and all {p} puzzles of RetroStone Board, around "
                   "one green table: chess, draughts, backgammon, hearts, solitaire, sudoku, nonograms and more. "
                   "Wooden boards, cards and dice drawn with care, and the rules of every game inside the app.",
        h_games="THE GAMES", h_play="PLAY TOGETHER, OR AGAINST THE DEVICE",
        friends="Friends on one device, passed from hand to hand: 2 to 6 players, depending on the game",
        bots_board="Or let the device take seats, at three levels (easy, medium, hard): in {b} of the {mp} games "
                   "for two or more",
        bots_boardfree="Or let the device take seats, at three levels (easy, medium, hard), in every game for two or more",
        f2f="Face to face: lay the phone or tablet flat between you, and the top player's pieces turn to face them",
        hidden="Card games keep every hand secret: a cover hides the cards while the device goes to the next player",
        tools="Undo, turn the board round, portrait or landscape, and each game's rules a tap away",
        h_levels="PUZZLES AND LEVELS",
        levels_board=["{L} puzzles and solitaire games with hand-made levels, 150 for most of them: over {lv} levels",
                      "Up to 3 stars per level, and a hint when you are stuck",
                      "Then random puzzles without end"],
        levels_boardfree=["Every puzzle with its first {fl} levels, up to 3 stars per level",
                     "A hint when you are stuck"],
        h_fair="NO ADS, NO TRACKING",
        fair_board=["One purchase for every game: no ads, no in-app purchases, no account",
                    "No data collected; it plays offline", "22 languages"],
        fair_boardfree=["No ads, no in-app purchases, no account", "No data collected; it plays offline", "22 languages"],
        more="RetroStone Board, the full version, adds {m} more games, every level of every puzzle and random "
             "puzzles: one purchase, no ads. You can see them all in the menu:",
        credit="Made by 8BCraft."),
    "fr": dict(
        short_board="{n} jeux de société, de cartes et casse-tête, entre amis ou contre l'appareil.",
        short_boardfree="Échecs, backgammon, réussite et {p} casse-tête : gratuit, hors ligne, sans pub.",
        intro_board="RetroStone Board réunit {n} jeux classiques autour d'un tapis vert : échecs et dames, "
                    "backgammon et go, mahjong, dame de pique, belote, bridge et tarot, sudoku, nonogrammes et "
                    "réussites. Des plateaux en bois, des cartes et des dés dessinés avec soin, des animations "
                    "douces, et les règles de chaque jeu dans l'appli.",
        intro_boardfree="RetroStone Board Free t'offre {f} jeux classiques et les {p} casse-tête de RetroStone Board, "
                   "autour d'un tapis vert : échecs, dames, backgammon, dame de pique, réussite, sudoku, nonogrammes "
                   "et bien d'autres. Des plateaux en bois, des cartes et des dés dessinés avec soin, et les règles "
                   "de chaque jeu dans l'appli.",
        h_games="LES JEUX", h_play="À PLUSIEURS, OU CONTRE L'APPAREIL",
        friends="Entre amis sur un seul appareil, qui passe de main en main : de 2 à 6 joueurs selon le jeu",
        bots_board="Ou laisse l'appareil prendre des places, à trois niveaux (facile, moyen, difficile) : dans {b} "
                   "des {mp} jeux à deux ou plus",
        bots_boardfree="Ou laisse l'appareil prendre des places, à trois niveaux (facile, moyen, difficile), dans tous les "
                  "jeux à deux ou plus",
        f2f="Face à face : pose le téléphone ou la tablette à plat entre vous, et les pièces du joueur d'en face se "
            "tournent vers lui",
        hidden="Les jeux de cartes gardent chaque main secrète : un écran cache les cartes pendant que l'appareil "
               "passe au joueur suivant",
        tools="Annuler, retourner le plateau, en portrait ou en paysage, et les règles de chaque jeu à portée de doigt",
        h_levels="CASSE-TÊTE ET NIVEAUX",
        levels_board=["{L} casse-tête et réussites avec des niveaux conçus un par un, 150 pour la plupart : plus de "
                      "{lv} niveaux",
                      "Jusqu'à 3 étoiles par niveau, et un indice quand tu bloques",
                      "Puis des grilles au hasard, sans fin"],
        levels_boardfree=["Chaque casse-tête avec ses {fl} premiers niveaux, jusqu'à 3 étoiles par niveau",
                     "Un indice quand tu bloques"],
        h_fair="SANS PUB, SANS PISTAGE",
        fair_board=["Un seul achat pour tous les jeux : pas de pub, pas d'achat intégré, pas de compte",
                    "Aucune donnée collectée ; fonctionne hors ligne", "22 langues"],
        fair_boardfree=["Pas de pub, pas d'achat intégré, pas de compte", "Aucune donnée collectée ; fonctionne hors ligne",
                   "22 langues"],
        more="RetroStone Board, la version complète, ajoute {m} jeux, tous les niveaux de chaque casse-tête et des "
             "grilles au hasard : un seul achat, sans pub. Tu les vois tous dans le menu :",
        credit="Créé par 8BCraft."),
    "de": dict(
        short_board="{n} Brett-, Karten- und Knobelspiele: mit Freunden oder gegen das Gerät.",
        short_boardfree="Schach, Backgammon, Solitär und {p} Knobelspiele: gratis, offline, werbefrei.",
        intro_board="RetroStone Board vereint {n} klassische Spiele an einem grünen Spieltisch: Schach und Dame, "
                    "Backgammon und Go, Mahjong, Hearts, Belote, Bridge und Tarot, Sudoku, Nonogramme und Solitär. "
                    "Holzbretter, liebevoll gezeichnete Karten und Würfel, sanfte Animationen und die Regeln jedes "
                    "Spiels direkt in der App.",
        intro_boardfree="RetroStone Board Free schenkt dir {f} klassische Spiele und alle {p} Knobelspiele von RetroStone "
                   "Board an einem grünen Spieltisch: Schach, Dame, Backgammon, Hearts, Solitär, Sudoku, Nonogramme "
                   "und mehr. Holzbretter, liebevoll gezeichnete Karten und Würfel und die Regeln jedes Spiels direkt "
                   "in der App.",
        h_games="DIE SPIELE", h_play="ZUSAMMEN ODER GEGEN DAS GERÄT",
        friends="Mit Freunden an einem Gerät, das von Hand zu Hand geht: 2 bis 6 Spieler, je nach Spiel",
        bots_board="Oder lass das Gerät Plätze übernehmen, in drei Stufen (leicht, mittel, schwer): in {b} der {mp} "
                   "Spiele für zwei oder mehr",
        bots_boardfree="Oder lass das Gerät Plätze übernehmen, in drei Stufen (leicht, mittel, schwer), in jedem Spiel für "
                  "zwei oder mehr",
        f2f="Gegenüber: Leg Handy oder Tablet flach zwischen euch, und die Figuren des Spielers oben drehen sich zu "
            "ihm",
        hidden="Kartenspiele halten jede Hand geheim: Eine Abdeckung verbirgt die Karten, während das Gerät "
               "weitergereicht wird",
        tools="Zug zurück, Brett drehen, Hoch- oder Querformat und die Regeln jedes Spiels nur einen Fingertipp "
              "entfernt",
        h_levels="KNOBELSPIELE UND LEVEL",
        levels_board=["{L} Knobelspiele und Patiencen mit handgebauten Leveln, meist 150 pro Spiel: über {lv} Level",
                      "Bis zu 3 Sterne pro Level und ein Hinweis, wenn du feststeckst",
                      "Danach Zufallsrätsel ohne Ende"],
        levels_boardfree=["Jedes Knobelspiel mit seinen ersten {fl} Leveln, bis zu 3 Sterne pro Level",
                     "Ein Hinweis, wenn du feststeckst"],
        h_fair="OHNE WERBUNG, OHNE TRACKING",
        fair_board=["Ein Kauf für alle Spiele: keine Werbung, keine In-App-Käufe, kein Konto",
                    "Keine Datenerhebung; spielbar offline", "22 Sprachen"],
        fair_boardfree=["Keine Werbung, keine In-App-Käufe, kein Konto", "Keine Datenerhebung; spielbar offline",
                   "22 Sprachen"],
        more="RetroStone Board, die Vollversion, bietet {m} weitere Spiele, alle Level jedes Knobelspiels und "
             "Zufallsrätsel: ein Kauf, keine Werbung. Du siehst sie alle im Menü:",
        credit="Von 8BCraft."),
    "es": dict(
        short_board="{n} juegos de mesa, de cartas y rompecabezas, con amigos o contra el móvil.",
        short_boardfree="Ajedrez, backgammon, solitario y {p} rompecabezas: gratis, sin conexión.",
        intro_board="RetroStone Board reúne {n} juegos clásicos alrededor de un tapete verde: ajedrez y damas, "
                    "backgammon y go, mahjong, corazones, belote, bridge y tarot, sudoku, nonogramas y solitarios. "
                    "Tableros de madera, cartas y dados dibujados con cariño, animaciones suaves y las reglas de cada "
                    "juego dentro de la app.",
        intro_boardfree="RetroStone Board Free te regala {f} juegos clásicos y los {p} rompecabezas de RetroStone Board "
                   "alrededor de un tapete verde: ajedrez, damas, backgammon, corazones, solitario, sudoku, "
                   "nonogramas y más. Tableros de madera, cartas y dados dibujados con cariño y las reglas de cada "
                   "juego dentro de la app.",
        h_games="LOS JUEGOS", h_play="EN COMPAÑÍA O CONTRA EL DISPOSITIVO",
        friends="Con amigos en un solo dispositivo que pasa de mano en mano: de 2 a 6 jugadores, según el juego",
        bots_board="O deja que el dispositivo ocupe asientos, en tres niveles (fácil, medio, difícil): en {b} de los "
                   "{mp} juegos para dos o más",
        bots_boardfree="O deja que el dispositivo ocupe asientos, en tres niveles (fácil, medio, difícil), en todos los "
                  "juegos para dos o más",
        f2f="Cara a cara: deja el móvil o la tableta plano entre los dos, y las piezas del jugador de enfrente se "
            "giran hacia él",
        hidden="Los juegos de cartas mantienen cada mano en secreto: una pantalla tapa las cartas mientras el "
               "dispositivo pasa al siguiente jugador",
        tools="Deshacer, girar el tablero, en vertical u horizontal, y las reglas de cada juego a un toque",
        h_levels="ROMPECABEZAS Y NIVELES",
        levels_board=["{L} rompecabezas y solitarios con niveles diseñados uno a uno, 150 en la mayoría: más de {lv} "
                      "niveles",
                      "Hasta 3 estrellas por nivel, y una pista cuando te atascas",
                      "Después, rompecabezas al azar sin fin"],
        levels_boardfree=["Cada rompecabezas con sus {fl} primeros niveles, hasta 3 estrellas por nivel",
                     "Una pista cuando te atascas"],
        h_fair="SIN ANUNCIOS NI RASTREO",
        fair_board=["Una sola compra para todos los juegos: sin anuncios, sin compras dentro de la app, sin cuenta",
                    "No recoge ningún dato; funciona sin conexión", "22 idiomas"],
        fair_boardfree=["Sin anuncios, sin compras dentro de la app, sin cuenta",
                   "No recoge ningún dato; funciona sin conexión", "22 idiomas"],
        more="RetroStone Board, la versión completa, añade {m} juegos más, todos los niveles de cada rompecabezas y "
             "rompecabezas al azar: una sola compra, sin anuncios. Los ves todos en el menú:",
        credit="Creado por 8BCraft."),
    "it": dict(
        short_board="{n} giochi da tavolo, di carte e rompicapi: con amici o contro il telefono.",
        short_boardfree="Scacchi, backgammon, solitario e {p} rompicapi, gratis e senza pubblicità.",
        intro_board="RetroStone Board riunisce {n} giochi classici attorno a un tappeto verde: scacchi e dama, "
                    "backgammon e go, mahjong, cuori, belote, bridge e tarot, sudoku, nonogrammi e solitari. "
                    "Tavolieri in legno, carte e dadi disegnati con cura, animazioni morbide e le regole di ogni "
                    "gioco nell'app.",
        intro_boardfree="RetroStone Board Free ti regala {f} giochi classici e tutti i {p} rompicapi di RetroStone Board "
                   "attorno a un tappeto verde: scacchi, dama, backgammon, cuori, solitario, sudoku, nonogrammi e "
                   "altro ancora. Tavolieri in legno, carte e dadi disegnati con cura e le regole di ogni gioco "
                   "nell'app.",
        h_games="I GIOCHI", h_play="IN COMPAGNIA O CONTRO IL DISPOSITIVO",
        friends="Con gli amici su un solo dispositivo, passato di mano in mano: da 2 a 6 giocatori, secondo il gioco",
        bots_board="Oppure lascia che il dispositivo prenda dei posti, a tre livelli (facile, medio, difficile): in "
                   "{b} dei {mp} giochi per due o più",
        bots_boardfree="Oppure lascia che il dispositivo prenda dei posti, a tre livelli (facile, medio, difficile), in "
                  "tutti i giochi per due o più",
        f2f="Faccia a faccia: appoggia il telefono o il tablet tra voi, e i pezzi del giocatore di fronte si girano "
            "verso di lui",
        hidden="I giochi di carte tengono segreta ogni mano: una schermata copre le carte mentre il dispositivo passa "
               "al giocatore successivo",
        tools="Annulla, gira il tavoliere, in verticale o in orizzontale, e le regole di ogni gioco a portata di tocco",
        h_levels="ROMPICAPI E LIVELLI",
        levels_board=["{L} rompicapi e solitari con livelli creati uno per uno, 150 per la maggior parte: oltre {lv} "
                      "livelli",
                      "Fino a 3 stelle per livello, e un aiuto quando ti blocchi",
                      "Poi rompicapi casuali senza fine"],
        levels_boardfree=["Ogni rompicapo con i suoi primi {fl} livelli, fino a 3 stelle per livello",
                     "Un aiuto quando ti blocchi"],
        h_fair="SENZA PUBBLICITÀ, SENZA TRACCIAMENTO",
        fair_board=["Un solo acquisto per tutti i giochi: niente pubblicità, niente acquisti in-app, nessun account",
                    "Nessun dato raccolto; funziona offline", "22 lingue"],
        fair_boardfree=["Niente pubblicità, niente acquisti in-app, nessun account", "Nessun dato raccolto; funziona "
                   "offline", "22 lingue"],
        more="RetroStone Board, la versione completa, aggiunge altri {m} giochi, tutti i livelli di ogni rompicapo e "
             "i rompicapi casuali: un solo acquisto, senza pubblicità. Li vedi tutti nel menu:",
        credit="Creato da 8BCraft."),
    "pt_BR": dict(
        short_board="{n} jogos de tabuleiro, cartas e quebra-cabeças, com amigos ou contra o celular.",
        short_boardfree="Xadrez, gamão, paciência e {p} quebra-cabeças: grátis, offline e sem anúncios.",
        intro_board="RetroStone Board reúne {n} jogos clássicos em volta de um feltro verde: xadrez e damas, gamão e "
                    "go, mahjong, copas, belote, bridge e tarô, sudoku, nonogramas e paciências. Tabuleiros de "
                    "madeira, cartas e dados desenhados com carinho, animações suaves e as regras de cada jogo "
                    "dentro do app.",
        intro_boardfree="RetroStone Board Free oferece {f} jogos clássicos e todos os {p} quebra-cabeças do RetroStone "
                   "Board, em volta de um feltro verde: xadrez, damas, gamão, copas, paciência, sudoku, nonogramas e "
                   "muito mais. Tabuleiros de madeira, cartas e dados desenhados com carinho e as regras de cada jogo "
                   "dentro do app.",
        h_games="OS JOGOS", h_play="COM AMIGOS OU CONTRA O APARELHO",
        friends="Com amigos em um só aparelho, passado de mão em mão: de 2 a 6 jogadores, conforme o jogo",
        bots_board="Ou deixe o aparelho ocupar lugares, em três níveis (fácil, médio, difícil): em {b} dos {mp} jogos "
                   "para dois ou mais",
        bots_boardfree="Ou deixe o aparelho ocupar lugares, em três níveis (fácil, médio, difícil), em todos os jogos para "
                  "dois ou mais",
        f2f="Frente a frente: deite o celular ou o tablet entre vocês, e as peças do jogador de cima se viram para "
            "ele",
        hidden="Os jogos de cartas mantêm cada mão em segredo: uma tela cobre as cartas enquanto o aparelho passa "
               "para o próximo jogador",
        tools="Desfazer, girar o tabuleiro, retrato ou paisagem, e as regras de cada jogo a um toque",
        h_levels="QUEBRA-CABEÇAS E NÍVEIS",
        levels_board=["{L} quebra-cabeças e paciências com níveis criados um a um, 150 na maioria: mais de {lv} "
                      "níveis",
                      "Até 3 estrelas por nível, e uma dica quando você travar",
                      "Depois, quebra-cabeças aleatórios sem fim"],
        levels_boardfree=["Cada quebra-cabeça com os seus {fl} primeiros níveis, até 3 estrelas por nível",
                     "Uma dica quando você travar"],
        h_fair="SEM ANÚNCIOS, SEM RASTREAMENTO",
        fair_board=["Uma só compra para todos os jogos: sem anúncios, sem compras no app, sem conta",
                    "Nenhum dado coletado; funciona offline", "22 idiomas"],
        fair_boardfree=["Sem anúncios, sem compras no app, sem conta", "Nenhum dado coletado; funciona offline",
                   "22 idiomas"],
        more="RetroStone Board, a versão completa, traz mais {m} jogos, todos os níveis de cada quebra-cabeça e "
             "quebra-cabeças aleatórios: uma só compra, sem anúncios. Você vê todos no menu:",
        credit="Criado por 8BCraft."),
    "pt_PT": dict(
        short_board="{n} jogos de tabuleiro, cartas e quebra-cabeças, com amigos ou o telemóvel.",
        short_boardfree="Xadrez, gamão, solitário e {p} quebra-cabeças: grátis, offline e sem anúncios.",
        intro_board="RetroStone Board reúne {n} jogos clássicos à volta de um pano verde: xadrez e damas, gamão e "
                    "go, mahjong, copas, belote, bridge e tarot, sudoku, nonogramas e solitários. Tabuleiros de "
                    "madeira, cartas e dados desenhados com cuidado, animações suaves e as regras de cada jogo "
                    "dentro da app.",
        intro_boardfree="RetroStone Board Free oferece-te {f} jogos clássicos e todos os {p} quebra-cabeças do RetroStone "
                   "Board, à volta de um pano verde: xadrez, damas, gamão, copas, solitário, sudoku, nonogramas e "
                   "muito mais. Tabuleiros de madeira, cartas e dados desenhados com cuidado e as regras de cada jogo "
                   "dentro da app.",
        h_games="OS JOGOS", h_play="COM AMIGOS OU CONTRA O DISPOSITIVO",
        friends="Com amigos num só dispositivo, passado de mão em mão: de 2 a 6 jogadores, conforme o jogo",
        bots_board="Ou deixa o dispositivo ocupar lugares, em três níveis (fácil, médio, difícil): em {b} dos {mp} "
                   "jogos para dois ou mais",
        bots_boardfree="Ou deixa o dispositivo ocupar lugares, em três níveis (fácil, médio, difícil), em todos os jogos "
                  "para dois ou mais",
        f2f="Frente a frente: pousa o telemóvel ou o tablet entre os dois, e as peças do jogador de cima viram-se "
            "para ele",
        hidden="Os jogos de cartas mantêm cada mão em segredo: um ecrã tapa as cartas enquanto o dispositivo passa ao "
               "jogador seguinte",
        tools="Anular, rodar o tabuleiro, na vertical ou na horizontal, e as regras de cada jogo à distância de um "
              "toque",
        h_levels="QUEBRA-CABEÇAS E NÍVEIS",
        levels_board=["{L} quebra-cabeças e solitários com níveis criados um a um, 150 na maioria: mais de {lv} "
                      "níveis",
                      "Até 3 estrelas por nível, e uma pista quando ficas preso",
                      "Depois, quebra-cabeças aleatórios sem fim"],
        levels_boardfree=["Cada quebra-cabeças com os seus {fl} primeiros níveis, até 3 estrelas por nível",
                     "Uma pista quando ficas preso"],
        h_fair="SEM ANÚNCIOS, SEM RASTREIO",
        fair_board=["Uma só compra para todos os jogos: sem anúncios, sem compras na app, sem conta",
                    "Nenhum dado recolhido; funciona offline", "22 idiomas"],
        fair_boardfree=["Sem anúncios, sem compras na app, sem conta", "Nenhum dado recolhido; funciona offline",
                   "22 idiomas"],
        more="RetroStone Board, a versão completa, traz mais {m} jogos, todos os níveis de cada quebra-cabeças e "
             "quebra-cabeças aleatórios: uma só compra, sem anúncios. Vês todos no menu:",
        credit="Criado por 8BCraft."),
    "nl": dict(
        short_board="{n} bord-, kaart- en puzzelspellen: met vrienden of tegen het toestel, offline.",
        short_boardfree="Schaken, backgammon, patience en {p} puzzels: gratis, offline, zonder reclame.",
        intro_board="RetroStone Board brengt {n} klassieke spellen samen rond één groene speeltafel: schaken en "
                    "dammen, backgammon en go, mahjong, hartenjagen, belote, bridge en tarot, sudoku, nonogrammen en "
                    "patience. Houten borden, met zorg getekende kaarten en dobbelstenen, zachte animaties en de "
                    "regels van elk spel in de app.",
        intro_boardfree="RetroStone Board Free geeft je {f} klassieke spellen en alle {p} puzzels van RetroStone Board "
                   "rond één groene speeltafel: schaken, dammen, backgammon, hartenjagen, patience, sudoku, "
                   "nonogrammen en meer. Houten borden, met zorg getekende kaarten en dobbelstenen en de regels van "
                   "elk spel in de app.",
        h_games="DE SPELLEN", h_play="SAMEN, OF TEGEN HET TOESTEL",
        friends="Met vrienden op één toestel dat van hand tot hand gaat: 2 tot 6 spelers, afhankelijk van het spel",
        bots_board="Of laat het toestel plaatsen innemen, op drie niveaus (makkelijk, gemiddeld, moeilijk): in {b} "
                   "van de {mp} spellen voor twee of meer",
        bots_boardfree="Of laat het toestel plaatsen innemen, op drie niveaus (makkelijk, gemiddeld, moeilijk), in elk "
                  "spel voor twee of meer",
        f2f="Tegenover elkaar: leg de telefoon of tablet plat tussen jullie in, en de stukken van de speler boven "
            "draaien naar hem toe",
        hidden="Kaartspellen houden elke hand geheim: een scherm bedekt de kaarten terwijl het toestel naar de "
               "volgende speler gaat",
        tools="Ongedaan maken, het bord draaien, staand of liggend, en de regels van elk spel met één tik",
        h_levels="PUZZELS EN LEVELS",
        levels_board=["{L} puzzels en patiences met zorgvuldig ontworpen levels, meestal 150 per spel: meer dan {lv} "
                      "levels",
                      "Tot 3 sterren per level, en een hint als je vastzit",
                      "Daarna willekeurige puzzels zonder einde"],
        levels_boardfree=["Elke puzzel met zijn eerste {fl} levels, tot 3 sterren per level", "Een hint als je vastzit"],
        h_fair="GEEN RECLAME, GEEN TRACKING",
        fair_board=["Eén aankoop voor alle spellen: geen reclame, geen in-app-aankopen, geen account",
                    "Er worden geen gegevens verzameld; werkt offline", "22 talen"],
        fair_boardfree=["Geen reclame, geen in-app-aankopen, geen account",
                   "Er worden geen gegevens verzameld; werkt offline", "22 talen"],
        more="RetroStone Board, de volledige versie, voegt {m} spellen toe, alle levels van elke puzzel en "
             "willekeurige puzzels: één aankoop, geen reclame. Je ziet ze allemaal in het menu:",
        credit="Gemaakt door 8BCraft."),
    "sv": dict(
        short_board="{n} bräd-, kort- och pusselspel: med vänner eller mot enheten, offline.",
        short_boardfree="Schack, backgammon, patiens och {p} pussel: gratis, offline, utan reklam.",
        intro_board="RetroStone Board samlar {n} klassiska spel kring ett grönt spelbord: schack och dam, backgammon "
                    "och go, mahjong, hjärter, belote, bridge och tarot, sudoku, nonogram och patienser. Träbräden, "
                    "omsorgsfullt ritade kort och tärningar, mjuka animationer och reglerna för varje spel i appen.",
        intro_boardfree="RetroStone Board Free ger dig {f} klassiska spel och alla {p} pussel från RetroStone Board kring "
                   "ett grönt spelbord: schack, dam, backgammon, hjärter, patiens, sudoku, nonogram och mer. "
                   "Träbräden, omsorgsfullt ritade kort och tärningar och reglerna för varje spel i appen.",
        h_games="SPELEN", h_play="TILLSAMMANS ELLER MOT ENHETEN",
        friends="Med vänner på en enda enhet som går från hand till hand: 2 till 6 spelare, beroende på spelet",
        bots_board="Eller låt enheten ta platser, på tre nivåer (lätt, medel, svår): i {b} av de {mp} spelen för två "
                   "eller fler",
        bots_boardfree="Eller låt enheten ta platser, på tre nivåer (lätt, medel, svår), i alla spel för två eller fler",
        f2f="Ansikte mot ansikte: lägg mobilen eller surfplattan platt mellan er, så vänds den övre spelarens pjäser "
            "mot hen",
        hidden="Kortspelen håller varje hand hemlig: en skärm döljer korten medan enheten går vidare till nästa "
               "spelare",
        tools="Ångra, vänd brädet, stående eller liggande, och varje spels regler ett tryck bort",
        h_levels="PUSSEL OCH NIVÅER",
        levels_board=["{L} pussel och patienser med noga utformade nivåer, 150 för de flesta: över {lv} nivåer",
                      "Upp till 3 stjärnor per nivå, och en ledtråd när du kör fast",
                      "Sedan slumpade pussel utan slut"],
        levels_boardfree=["Varje pussel med sina första {fl} nivåer, upp till 3 stjärnor per nivå",
                     "En ledtråd när du kör fast"],
        h_fair="INGEN REKLAM, INGEN SPÅRNING",
        fair_board=["Ett köp för alla spel: ingen reklam, inga köp i appen, inget konto",
                    "Inga data samlas in; fungerar offline", "22 språk"],
        fair_boardfree=["Ingen reklam, inga köp i appen, inget konto", "Inga data samlas in; fungerar offline", "22 språk"],
        more="RetroStone Board, den fullständiga versionen, har {m} spel till, alla nivåer i varje pussel och "
             "slumpade pussel: ett köp, ingen reklam. Du ser dem alla i menyn:",
        credit="Skapat av 8BCraft."),
    "da": dict(
        short_board="{n} bræt-, kort- og hjernevriderspil: med venner eller mod enheden, offline.",
        short_boardfree="Skak, backgammon, kabale og {p} hjernevridere: gratis, offline, uden reklamer.",
        intro_board="RetroStone Board samler {n} klassiske spil om ét grønt spillebord: skak og dam, backgammon og "
                    "go, mahjong, hjerterfri, belote, bridge og tarot, sudoku, nonogrammer og kabaler. Træbrætter, "
                    "omhyggeligt tegnede kort og terninger, bløde animationer og reglerne for hvert spil i appen.",
        intro_boardfree="RetroStone Board Free giver dig {f} klassiske spil og alle {p} hjernevridere fra RetroStone Board "
                   "om ét grønt spillebord: skak, dam, backgammon, hjerterfri, kabale, sudoku, nonogrammer og mere. "
                   "Træbrætter, omhyggeligt tegnede kort og terninger og reglerne for hvert spil i appen.",
        h_games="SPILLENE", h_play="SAMMEN ELLER MOD ENHEDEN",
        friends="Med venner på én enhed, der går fra hånd til hånd: 2 til 6 spillere, alt efter spillet",
        bots_board="Eller lad enheden tage pladser, på tre niveauer (let, mellem, svær): i {b} af de {mp} spil for to "
                   "eller flere",
        bots_boardfree="Eller lad enheden tage pladser, på tre niveauer (let, mellem, svær), i alle spil for to eller "
                  "flere",
        f2f="Ansigt til ansigt: læg telefonen eller tabletten fladt mellem jer, og den øverste spillers brikker "
            "vender sig mod ham",
        hidden="Kortspillene holder hver hånd hemmelig: en skærm dækker kortene, mens enheden går videre til næste "
               "spiller",
        tools="Fortryd, vend brættet, stående eller liggende, og hvert spils regler et tryk væk",
        h_levels="HJERNEVRIDERE OG NIVEAUER",
        levels_board=["{L} hjernevridere og kabaler med omhyggeligt designede niveauer, 150 for de fleste: over {lv} "
                      "niveauer",
                      "Op til 3 stjerner pr. niveau, og et hint, når du sidder fast",
                      "Derefter tilfældige opgaver uden ende"],
        levels_boardfree=["Hver hjernevrider med sine første {fl} niveauer, op til 3 stjerner pr. niveau",
                     "Et hint, når du sidder fast"],
        h_fair="INGEN REKLAMER, INGEN SPORING",
        fair_board=["Ét køb for alle spil: ingen reklamer, ingen køb i appen, ingen konto",
                    "Ingen data indsamles; virker offline", "22 sprog"],
        fair_boardfree=["Ingen reklamer, ingen køb i appen, ingen konto", "Ingen data indsamles; virker offline",
                   "22 sprog"],
        more="RetroStone Board, den fulde version, har {m} spil mere, alle niveauer i hver hjernevrider og "
             "tilfældige opgaver: ét køb, ingen reklamer. Du kan se dem alle i menuen:",
        credit="Lavet af 8BCraft."),
    "nb": dict(
        short_board="{n} brett-, kort- og tenkespill: med venner eller mot enheten, offline.",
        short_boardfree="Sjakk, backgammon, kabal og {p} nøtter: gratis, offline, uten reklame.",
        intro_board="RetroStone Board samler {n} klassiske spill rundt ett grønt spillebord: sjakk og dam, "
                    "backgammon og go, mahjong, hjerter, belote, bridge og tarot, sudoku, nonogrammer og kabaler. "
                    "Brett av tre, omhyggelig tegnede kort og terninger, myke animasjoner og reglene for hvert spill "
                    "i appen.",
        intro_boardfree="RetroStone Board Free gir deg {f} klassiske spill og alle {p} nøttene fra RetroStone Board rundt "
                   "ett grønt spillebord: sjakk, dam, backgammon, hjerter, kabal, sudoku, nonogrammer og mer. Brett "
                   "av tre, omhyggelig tegnede kort og terninger og reglene for hvert spill i appen.",
        h_games="SPILLENE", h_play="SAMMEN ELLER MOT ENHETEN",
        friends="Med venner på én enhet som går fra hånd til hånd: 2 til 6 spillere, avhengig av spillet",
        bots_board="Eller la enheten ta plasser, på tre nivåer (lett, middels, vanskelig): i {b} av de {mp} spillene "
                   "for to eller flere",
        bots_boardfree="Eller la enheten ta plasser, på tre nivåer (lett, middels, vanskelig), i alle spill for to eller "
                  "flere",
        f2f="Ansikt til ansikt: legg telefonen eller nettbrettet flatt mellom dere, og brikkene til spilleren øverst "
            "snur seg mot ham",
        hidden="Kortspillene holder hver hånd hemmelig: en skjerm dekker kortene mens enheten går videre til neste "
               "spiller",
        tools="Angre, snu brettet, stående eller liggende, og reglene for hvert spill ett trykk unna",
        h_levels="NØTTER OG NIVÅER",
        levels_board=["{L} nøtter og kabaler med nøye utformede nivåer, 150 for de fleste: over {lv} nivåer",
                      "Opptil 3 stjerner per nivå, og et hint når du står fast",
                      "Deretter tilfeldige oppgaver uten slutt"],
        levels_boardfree=["Hver nøtt med sine første {fl} nivåer, opptil 3 stjerner per nivå", "Et hint når du står fast"],
        h_fair="INGEN REKLAME, INGEN SPORING",
        fair_board=["Ett kjøp for alle spill: ingen reklame, ingen kjøp i appen, ingen konto",
                    "Ingen data samles inn; fungerer offline", "22 språk"],
        fair_boardfree=["Ingen reklame, ingen kjøp i appen, ingen konto", "Ingen data samles inn; fungerer offline",
                   "22 språk"],
        more="RetroStone Board, fullversjonen, har {m} spill til, alle nivåene i hver nøtt og tilfeldige oppgaver: "
             "ett kjøp, ingen reklame. Du ser dem alle i menyen:",
        credit="Laget av 8BCraft."),
    "fi": dict(
        short_board="{n} lauta-, kortti- ja pulmapeliä: ystävien kanssa tai laitetta vastaan.",
        short_boardfree="Shakki, backgammon, pasianssi ja {p} pulmapeliä: ilmainen, offline, ei mainoksia.",
        intro_board="RetroStone Board kokoaa {n} klassista peliä saman vihreän pelipöydän ääreen: shakki ja tammi, "
                    "backgammon ja go, mahjong, hertta, belote, bridge ja tarot, sudoku, kuvaristikot ja pasianssit. "
                    "Puiset pelilaudat, huolella piirretyt kortit ja nopat, pehmeät animaatiot ja jokaisen pelin "
                    "säännöt sovelluksessa.",
        intro_boardfree="RetroStone Board Free antaa sinulle {f} klassista peliä ja kaikki RetroStone Boardin {p} "
                   "pulmapeliä saman vihreän pelipöydän ääressä: shakki, tammi, backgammon, hertta, pasianssi, "
                   "sudoku, kuvaristikot ja paljon muuta. Puiset pelilaudat, huolella piirretyt kortit ja nopat ja "
                   "jokaisen pelin säännöt sovelluksessa.",
        h_games="PELIT", h_play="YHDESSÄ TAI LAITETTA VASTAAN",
        friends="Ystävien kanssa yhdellä laitteella, joka kulkee kädestä käteen: 2–6 pelaajaa pelistä riippuen",
        bots_board="Tai anna laitteen ottaa paikkoja kolmella tasolla (helppo, keskitaso, vaikea): {b} peliä {mp}:stä "
                   "kahden tai useamman pelaajan pelistä",
        bots_boardfree="Tai anna laitteen ottaa paikkoja kolmella tasolla (helppo, keskitaso, vaikea) jokaisessa kahden "
                  "tai useamman pelaajan pelissä",
        f2f="Kasvokkain: aseta puhelin tai tabletti tasaisesti väliinne, niin ylemmän pelaajan nappulat kääntyvät "
            "häneen päin",
        hidden="Korttipelit pitävät jokaisen käden salassa: suojakuva peittää kortit, kun laite siirtyy seuraavalle "
               "pelaajalle",
        tools="Kumoa, käännä lauta, pysty- tai vaakasuunta, ja jokaisen pelin säännöt yhden napautuksen päässä",
        h_levels="PULMAT JA TASOT",
        levels_board=["{L} pulmapeliä ja pasianssia huolella suunnitelluilla tasoilla, useimmissa 150: yli {lv} "
                      "tasoa",
                      "Jopa 3 tähteä tasoa kohden ja vihje, kun jäät jumiin",
                      "Sen jälkeen satunnaisia pulmia loputtomasti"],
        levels_boardfree=["Jokainen pulmapeli ensimmäisillä {fl} tasollaan, jopa 3 tähteä tasoa kohden",
                     "Vihje, kun jäät jumiin"],
        h_fair="EI MAINOKSIA, EI SEURANTAA",
        fair_board=["Yksi osto kaikkiin peleihin: ei mainoksia, ei sovelluksen sisäisiä ostoja, ei tiliä",
                    "Mitään tietoja ei kerätä; toimii offline-tilassa", "22 kieltä"],
        fair_boardfree=["Ei mainoksia, ei sovelluksen sisäisiä ostoja, ei tiliä",
                   "Mitään tietoja ei kerätä; toimii offline-tilassa", "22 kieltä"],
        more="RetroStone Board, täysi versio, tuo {m} peliä lisää, kaikkien pulmapelien kaikki tasot ja satunnaiset "
             "pulmat: yksi osto, ei mainoksia. Näet ne kaikki valikossa:",
        credit="Tekijä: 8BCraft."),
    "pl": dict(
        short_board="{n} gier planszowych, karcianych i łamigłówek: ze znajomymi lub z urządzeniem.",
        short_boardfree="Szachy, backgammon, pasjans i {p} łamigłówek: za darmo, offline, bez reklam.",
        intro_board="RetroStone Board łączy {n} klasycznych gier przy jednym zielonym stole: szachy i warcaby, "
                    "backgammon i go, mahjong, kierki, belote, brydż i tarot, sudoku, obrazki logiczne i pasjanse. "
                    "Drewniane plansze, starannie narysowane karty i kości, płynne animacje i zasady każdej gry w "
                    "aplikacji.",
        intro_boardfree="RetroStone Board Free daje ci {f} klasycznych gier i komplet {p} łamigłówek z RetroStone Board "
                   "przy jednym zielonym stole: szachy, warcaby, backgammon, kierki, pasjans, sudoku, obrazki "
                   "logiczne i więcej. Drewniane plansze, starannie narysowane karty i kości oraz zasady każdej gry w "
                   "aplikacji.",
        h_games="GRY", h_play="RAZEM ALBO PRZECIW URZĄDZENIU",
        friends="Ze znajomymi na jednym urządzeniu, podawanym z rąk do rąk: od 2 do 6 graczy, zależnie od gry",
        bots_board="Albo oddaj miejsca urządzeniu, na trzech poziomach (łatwy, średni, trudny): w {b} z {mp} gier dla "
                   "dwóch lub więcej osób",
        bots_boardfree="Albo oddaj miejsca urządzeniu, na trzech poziomach (łatwy, średni, trudny), w każdej grze dla "
                  "dwóch lub więcej osób",
        f2f="Twarzą w twarz: połóż telefon lub tablet płasko między wami, a pionki gracza u góry obrócą się do niego",
        hidden="Gry karciane chronią każdą rękę: zasłona zakrywa karty, gdy urządzenie przechodzi do następnego "
               "gracza",
        tools="Cofanie ruchu, obracanie planszy, pion lub poziom, a zasady każdej gry o jedno dotknięcie",
        h_levels="ŁAMIGŁÓWKI I POZIOMY",
        levels_board=["{L} łamigłówek i pasjansów z poziomami projektowanymi jeden po drugim, w większości po 150: "
                      "ponad {lv} poziomów",
                      "Do 3 gwiazdek za poziom i podpowiedź, gdy utkniesz",
                      "Potem losowe łamigłówki bez końca"],
        levels_boardfree=["Każda łamigłówka z pierwszymi {fl} poziomami, do 3 gwiazdek za poziom",
                     "Podpowiedź, gdy utkniesz"],
        h_fair="BEZ REKLAM, BEZ ŚLEDZENIA",
        fair_board=["Jeden zakup na wszystkie gry: bez reklam, bez zakupów w aplikacji, bez konta",
                    "Żadne dane nie są zbierane; działa offline", "22 języki"],
        fair_boardfree=["Bez reklam, bez zakupów w aplikacji, bez konta", "Żadne dane nie są zbierane; działa offline",
                   "22 języki"],
        more="RetroStone Board, pełna wersja, dodaje {m} kolejnych gier, wszystkie poziomy każdej łamigłówki i "
             "losowe łamigłówki: jeden zakup, bez reklam. Wszystkie widać w menu:",
        credit="Stworzone przez 8BCraft."),
    "cs": dict(
        short_board="{n} deskových a karetních her a hlavolamů: s přáteli nebo proti zařízení.",
        short_boardfree="Šachy, vrhcáby, solitér a {p} hlavolamů: zdarma, offline, bez reklam.",
        intro_board="RetroStone Board spojuje {n} klasických her u jednoho zeleného stolu: šachy a dámu, vrhcáby a "
                    "go, mahjong, srdce, belote, bridž a tarot, sudoku, malované křížovky a solitéry. Dřevěné desky, "
                    "pečlivě kreslené karty a kostky, plynulé animace a pravidla každé hry přímo v aplikaci.",
        intro_boardfree="RetroStone Board Free ti dává {f} klasických her a všech {p} hlavolamů z RetroStone Board u "
                   "jednoho zeleného stolu: šachy, dámu, vrhcáby, srdce, solitér, sudoku, malované křížovky a další. "
                   "Dřevěné desky, pečlivě kreslené karty a kostky a pravidla každé hry přímo v aplikaci.",
        h_games="HRY", h_play="SPOLU, NEBO PROTI ZAŘÍZENÍ",
        friends="S přáteli na jednom zařízení, které jde z ruky do ruky: 2 až 6 hráčů podle hry",
        bots_board="Nebo nech zařízení obsadit místa, ve třech úrovních (lehká, střední, těžká): v {b} z {mp} her pro "
                   "dva a více hráčů",
        bots_boardfree="Nebo nech zařízení obsadit místa, ve třech úrovních (lehká, střední, těžká), v každé hře pro dva a "
                  "více hráčů",
        f2f="Tváří v tvář: polož telefon nebo tablet naplocho mezi sebe a figurky horního hráče se otočí k němu",
        hidden="Karetní hry drží každou ruku v tajnosti: clona zakryje karty, zatímco zařízení putuje k dalšímu hráči",
        tools="Vrácení tahu, otočení desky, na výšku i na šířku a pravidla každé hry na jedno klepnutí",
        h_levels="HLAVOLAMY A ÚROVNĚ",
        levels_board=["{L} hlavolamů a solitérů s pečlivě navrženými úrovněmi, většinou 150: přes {lv} úrovní",
                      "Až 3 hvězdičky za úroveň a nápověda, když se zasekneš",
                      "Pak náhodné hlavolamy bez konce"],
        levels_boardfree=["Každý hlavolam s prvními {fl} úrovněmi, až 3 hvězdičky za úroveň", "Nápověda, když se "
                     "zasekneš"],
        h_fair="BEZ REKLAM, BEZ SLEDOVÁNÍ",
        fair_board=["Jeden nákup pro všechny hry: bez reklam, bez nákupů v aplikaci, bez účtu",
                    "Žádná data se nesbírají; funguje offline", "22 jazyků"],
        fair_boardfree=["Bez reklam, bez nákupů v aplikaci, bez účtu", "Žádná data se nesbírají; funguje offline",
                   "22 jazyků"],
        more="RetroStone Board, plná verze, přidává dalších {m} her, všechny úrovně každého hlavolamu a náhodné "
             "hlavolamy: jeden nákup, bez reklam. Všechny je uvidíš v menu:",
        credit="Vytvořil 8BCraft."),
    "ru": dict(
        short_board="Настольные игры, карты и головоломки, всего {n}: с друзьями или с устройством.",
        short_boardfree="Шахматы, нарды, пасьянс и головоломки: бесплатно, офлайн, без рекламы.",
        intro_board="RetroStone Board — это классические игры за одним зелёным столом, всего {n}: шахматы и шашки, "
                    "нарды и го, маджонг, червы, белот, бридж и таро, судоку, японские кроссворды и пасьянсы. "
                    "Деревянные доски, старательно нарисованные карты и кости, плавная анимация и правила каждой "
                    "игры прямо в приложении.",
        intro_boardfree="RetroStone Board Free дарит тебе классические игры ({f}) и все головоломки RetroStone Board "
                   "({p}) за одним зелёным столом: шахматы, шашки, нарды, червы, пасьянс, судоку, японские "
                   "кроссворды и не только. Деревянные доски, старательно нарисованные карты и кости и правила "
                   "каждой игры прямо в приложении.",
        h_games="ИГРЫ", h_play="ВМЕСТЕ ИЛИ ПРОТИВ УСТРОЙСТВА",
        friends="С друзьями на одном устройстве, которое переходит из рук в руки: от 2 до 6 игроков, в зависимости "
                "от игры",
        bots_board="Или отдай места устройству, на трёх уровнях (лёгкий, средний, сложный): так можно играть почти во "
                   "все игры для двоих и больше ({b} из {mp})",
        bots_boardfree="Или отдай места устройству, на трёх уровнях (лёгкий, средний, сложный), в любой игре для двоих и "
                  "больше",
        f2f="Лицом к лицу: положи телефон или планшет между вами, и фигуры верхнего игрока повернутся к нему",
        hidden="Карточные игры хранят каждую руку в тайне: заставка закрывает карты, пока устройство переходит к "
               "следующему игроку",
        tools="Отмена хода, поворот доски, вертикально или горизонтально, и правила каждой игры в одно касание",
        h_levels="ГОЛОВОЛОМКИ И УРОВНИ",
        levels_board=["Головоломки и пасьянсы с продуманными уровнями ({L}), в большинстве по 150: больше {lv} "
                      "уровней",
                      "До 3 звёзд за уровень и подсказка, если застрял",
                      "А потом — бесконечные случайные задачи"],
        levels_boardfree=["Каждая головоломка с первыми {fl} уровнями, до 3 звёзд за уровень", "Подсказка, если застрял"],
        h_fair="БЕЗ РЕКЛАМЫ И СЛЕЖКИ",
        fair_board=["Одна покупка — все игры: без рекламы, без покупок в приложении, без аккаунта",
                    "Никакие данные не собираются; работает без интернета", "22 языка"],
        fair_boardfree=["Без рекламы, без покупок в приложении, без аккаунта",
                   "Никакие данные не собираются; работает без интернета", "22 языка"],
        more="RetroStone Board, полная версия, добавляет ещё игры ({m}), все уровни каждой головоломки и случайные "
             "задачи: одна покупка, без рекламы. Все они видны в меню:",
        credit="Создано 8BCraft."),
    "uk": dict(
        short_board="Настільні ігри, карти й головоломки, усього {n}: з друзями або з пристроєм.",
        short_boardfree="Шахи, нарди, пасьянс і головоломки: безкоштовно, офлайн, без реклами.",
        intro_board="RetroStone Board — це класичні ігри за одним зеленим столом, усього {n}: шахи й шашки, нарди й "
                    "го, маджонг, чирви, белот, бридж і таро, судоку, японські кросворди й пасьянси. Дерев'яні дошки, "
                    "старанно намальовані карти й кості, плавна анімація та правила кожної гри просто в застосунку.",
        intro_boardfree="RetroStone Board Free дарує тобі класичні ігри ({f}) і всі головоломки RetroStone Board ({p}) за "
                   "одним зеленим столом: шахи, шашки, нарди, чирви, пасьянс, судоку, японські кросворди й не тільки. "
                   "Дерев'яні дошки, старанно намальовані карти й кості та правила кожної гри просто в застосунку.",
        h_games="ІГРИ", h_play="РАЗОМ АБО ПРОТИ ПРИСТРОЮ",
        friends="З друзями на одному пристрої, що переходить з рук у руки: від 2 до 6 гравців, залежно від гри",
        bots_board="Або віддай місця пристрою, на трьох рівнях (легкий, середній, складний): так можна грати майже в "
                   "усі ігри для двох і більше ({b} з {mp})",
        bots_boardfree="Або віддай місця пристрою, на трьох рівнях (легкий, середній, складний), у будь-якій грі для двох "
                  "і більше",
        f2f="Обличчям до обличчя: поклади телефон або планшет між вами, і фігури верхнього гравця повернуться до "
            "нього",
        hidden="Карткові ігри тримають кожну руку в таємниці: заставка закриває карти, поки пристрій переходить до "
               "наступного гравця",
        tools="Скасування ходу, поворот дошки, вертикально чи горизонтально, і правила кожної гри в один дотик",
        h_levels="ГОЛОВОЛОМКИ ТА РІВНІ",
        levels_board=["Головоломки й пасьянси з продуманими рівнями ({L}), здебільшого по 150: понад {lv} рівнів",
                      "До 3 зірок за рівень і підказка, якщо застряг",
                      "А потім — нескінченні випадкові задачі"],
        levels_boardfree=["Кожна головоломка з першими {fl} рівнями, до 3 зірок за рівень", "Підказка, якщо застряг"],
        h_fair="БЕЗ РЕКЛАМИ Й СТЕЖЕННЯ",
        fair_board=["Одна покупка — усі ігри: без реклами, без покупок у застосунку, без облікового запису",
                    "Жодні дані не збираються; працює без інтернету", "22 мови"],
        fair_boardfree=["Без реклами, без покупок у застосунку, без облікового запису",
                   "Жодні дані не збираються; працює без інтернету", "22 мови"],
        more="RetroStone Board, повна версія, додає ще ігри ({m}), усі рівні кожної головоломки та випадкові задачі: "
             "одна покупка, без реклами. Усі вони видні в меню:",
        credit="Створено 8BCraft."),
    "el": dict(
        short_board="{n} επιτραπέζια, παιχνίδια με τράπουλα και γρίφοι, με φίλους ή με τη συσκευή.",
        short_boardfree="Σκάκι, τάβλι, πασιέντζα και {p} γρίφοι: δωρεάν, χωρίς σύνδεση, χωρίς διαφημίσεις.",
        intro_board="Το RetroStone Board συγκεντρώνει {n} κλασικά παιχνίδια γύρω από ένα πράσινο τραπέζι: σκάκι και "
                    "ντάμα, τάβλι και γκο, μαζόνγκ, κούπες, μπελότ, μπριτζ και ταρό, σουντόκου, νονόγραμμα και "
                    "πασιέντζες. Ξύλινα ταμπλό, προσεγμένες κάρτες και ζάρια, απαλές κινήσεις και οι κανόνες κάθε "
                    "παιχνιδιού μέσα στην εφαρμογή.",
        intro_boardfree="Το RetroStone Board Free σού δίνει {f} κλασικά παιχνίδια και όλους τους {p} γρίφους του "
                   "RetroStone Board γύρω από ένα πράσινο τραπέζι: σκάκι, ντάμα, τάβλι, κούπες, πασιέντζα, "
                   "σουντόκου, νονόγραμμα και άλλα. Ξύλινα ταμπλό, προσεγμένες κάρτες και ζάρια και οι κανόνες κάθε "
                   "παιχνιδιού μέσα στην εφαρμογή.",
        h_games="ΤΑ ΠΑΙΧΝΙΔΙΑ", h_play="ΜΑΖΙ Ή ΚΟΝΤΡΑ ΣΤΗ ΣΥΣΚΕΥΗ",
        friends="Με φίλους σε μία συσκευή που περνά από χέρι σε χέρι: από 2 έως 6 παίκτες, ανάλογα με το παιχνίδι",
        bots_board="Ή άσε τη συσκευή να πάρει θέσεις, σε τρία επίπεδα (εύκολο, μέτριο, δύσκολο): σε {b} από τα {mp} "
                   "παιχνίδια για δύο ή περισσότερους",
        bots_boardfree="Ή άσε τη συσκευή να πάρει θέσεις, σε τρία επίπεδα (εύκολο, μέτριο, δύσκολο), σε κάθε παιχνίδι για "
                  "δύο ή περισσότερους",
        f2f="Πρόσωπο με πρόσωπο: άφησε το κινητό ή το τάμπλετ ανάμεσά σας, και τα πιόνια του παίκτη απέναντι "
            "γυρίζουν προς αυτόν",
        hidden="Τα παιχνίδια με κάρτες κρατούν κάθε χέρι μυστικό: μια οθόνη κρύβει τις κάρτες όσο η συσκευή πηγαίνει "
               "στον επόμενο παίκτη",
        tools="Αναίρεση, γύρισμα του ταμπλό, κάθετα ή οριζόντια, και οι κανόνες κάθε παιχνιδιού με ένα άγγιγμα",
        h_levels="ΓΡΙΦΟΙ ΚΑΙ ΕΠΙΠΕΔΑ",
        levels_board=["{L} γρίφοι και πασιέντζες με προσεγμένα επίπεδα, 150 στους περισσότερους: πάνω από {lv} "
                      "επίπεδα",
                      "Έως 3 αστέρια ανά επίπεδο, και μια βοήθεια όταν κολλήσεις",
                      "Μετά, τυχαίοι γρίφοι χωρίς τέλος"],
        levels_boardfree=["Κάθε γρίφος με τα πρώτα {fl} επίπεδά του, έως 3 αστέρια ανά επίπεδο",
                     "Μια βοήθεια όταν κολλήσεις"],
        h_fair="ΧΩΡΙΣ ΔΙΑΦΗΜΙΣΕΙΣ, ΧΩΡΙΣ ΠΑΡΑΚΟΛΟΥΘΗΣΗ",
        fair_board=["Μία αγορά για όλα τα παιχνίδια: χωρίς διαφημίσεις, χωρίς αγορές εντός εφαρμογής, χωρίς "
                    "λογαριασμό", "Δεν συλλέγονται δεδομένα· παίζει χωρίς σύνδεση", "22 γλώσσες"],
        fair_boardfree=["Χωρίς διαφημίσεις, χωρίς αγορές εντός εφαρμογής, χωρίς λογαριασμό",
                   "Δεν συλλέγονται δεδομένα· παίζει χωρίς σύνδεση", "22 γλώσσες"],
        more="Το RetroStone Board, η πλήρης έκδοση, προσθέτει άλλα {m} παιχνίδια, όλα τα επίπεδα κάθε γρίφου και "
             "τυχαίους γρίφους: μία αγορά, χωρίς διαφημίσεις. Τα βλέπεις όλα στο μενού:",
        credit="Δημιουργία: 8BCraft."),
    "tr": dict(
        short_board="{n} masa, kart ve bulmaca oyunu: arkadaşlarınla ya da cihaza karşı, çevrimdışı.",
        short_boardfree="Satranç, tavla, solitaire ve {p} bulmaca: ücretsiz, çevrimdışı, reklamsız.",
        intro_board="RetroStone Board, {n} klasik oyunu tek bir yeşil masada buluşturuyor: satranç ve dama, tavla ve "
                    "go, mahjong, kupa, belot, briç ve tarot, sudoku, nonogram ve solitaire. Ahşap tahtalar, özenle "
                    "çizilmiş kartlar ve zarlar, yumuşak animasyonlar ve her oyunun kuralları uygulamanın içinde.",
        intro_boardfree="RetroStone Board Free sana {f} klasik oyunu ve RetroStone Board'un {p} bulmacasının hepsini tek "
                   "bir yeşil masada sunuyor: satranç, dama, tavla, kupa, solitaire, sudoku, nonogram ve daha "
                   "fazlası. Ahşap tahtalar, özenle çizilmiş kartlar ve zarlar ve her oyunun kuralları uygulamanın "
                   "içinde.",
        h_games="OYUNLAR", h_play="BİRLİKTE YA DA CİHAZA KARŞI",
        friends="Elden ele dolaşan tek bir cihazda arkadaşlarınla: oyuna göre 2 ila 6 oyuncu",
        bots_board="Ya da yerleri cihaza bırak, üç seviyede (kolay, orta, zor): iki ve daha fazla kişilik {mp} "
                   "oyunun {b} tanesinde",
        bots_boardfree="Ya da yerleri cihaza bırak, üç seviyede (kolay, orta, zor), iki ve daha fazla kişilik her oyunda",
        f2f="Yüz yüze: telefonu ya da tableti aranıza düz bırak, üstteki oyuncunun taşları ona doğru döner",
        hidden="Kart oyunları her eli gizli tutar: cihaz sıradaki oyuncuya geçerken bir ekran kartları örter",
        tools="Geri alma, tahtayı çevirme, dikey ya da yatay, ve her oyunun kuralları bir dokunuş uzağında",
        h_levels="BULMACALAR VE SEVİYELER",
        levels_board=["Özenle tasarlanmış seviyeleri olan {L} bulmaca ve solitaire, çoğunda 150 seviye: {lv} "
                      "seviyeden fazla",
                      "Seviye başına 3 yıldıza kadar, takıldığında bir ipucu",
                      "Sonra bitmeyen rastgele bulmacalar"],
        levels_boardfree=["Her bulmaca ilk {fl} seviyesiyle, seviye başına 3 yıldıza kadar", "Takıldığında bir ipucu"],
        h_fair="REKLAM YOK, TAKİP YOK",
        fair_board=["Tüm oyunlar için tek satın alma: reklam yok, uygulama içi satın alma yok, hesap yok",
                    "Hiç veri toplanmaz; çevrimdışı çalışır", "22 dil"],
        fair_boardfree=["Reklam yok, uygulama içi satın alma yok, hesap yok", "Hiç veri toplanmaz; çevrimdışı çalışır",
                   "22 dil"],
        more="Tam sürüm RetroStone Board, {m} oyun daha, her bulmacanın tüm seviyelerini ve rastgele bulmacaları "
             "ekler: tek satın alma, reklamsız. Hepsini menüde görebilirsin:",
        credit="8BCraft yapımı."),
    "id": dict(
        short_board="{n} game papan, kartu, dan teka-teki: bersama teman atau lawan perangkat.",
        short_boardfree="Catur, backgammon, solitaire, dan {p} teka-teki: gratis, offline, tanpa iklan.",
        intro_board="RetroStone Board menghimpun {n} game klasik di satu meja hijau: catur dan dam, backgammon dan "
                    "go, mahjong, hati, belote, bridge dan tarot, sudoku, nonogram, dan solitaire. Papan kayu, kartu "
                    "dan dadu yang digambar dengan cermat, animasi yang halus, dan aturan setiap game di dalam "
                    "aplikasi.",
        intro_boardfree="RetroStone Board Free memberimu {f} game klasik dan semua {p} teka-teki RetroStone Board di satu "
                   "meja hijau: catur, dam, backgammon, hati, solitaire, sudoku, nonogram, dan banyak lagi. Papan "
                   "kayu, kartu dan dadu yang digambar dengan cermat, dan aturan setiap game di dalam aplikasi.",
        h_games="GAME-NYA", h_play="BERSAMA, ATAU MELAWAN PERANGKAT",
        friends="Bersama teman di satu perangkat yang berpindah tangan: 2 sampai 6 pemain, tergantung game-nya",
        bots_board="Atau biarkan perangkat mengisi kursi, dengan tiga tingkat (mudah, sedang, sulit): di {b} dari "
                   "{mp} game untuk dua orang atau lebih",
        bots_boardfree="Atau biarkan perangkat mengisi kursi, dengan tiga tingkat (mudah, sedang, sulit), di setiap game "
                  "untuk dua orang atau lebih",
        f2f="Berhadapan: letakkan ponsel atau tablet mendatar di antara kalian, dan bidak pemain di atas berputar "
            "menghadapnya",
        hidden="Game kartu menjaga setiap tangan tetap rahasia: layar penutup menyembunyikan kartu saat perangkat "
               "berpindah ke pemain berikutnya",
        tools="Batalkan langkah, putar papan, potret atau lanskap, dan aturan setiap game cukup satu ketukan",
        h_levels="TEKA-TEKI DAN LEVEL",
        levels_board=["{L} teka-teki dan solitaire dengan level yang dirancang satu per satu, kebanyakan 150: lebih "
                      "dari {lv} level",
                      "Hingga 3 bintang per level, dan petunjuk saat kamu buntu",
                      "Lalu teka-teki acak tanpa akhir"],
        levels_boardfree=["Setiap teka-teki dengan {fl} level pertamanya, hingga 3 bintang per level",
                     "Petunjuk saat kamu buntu"],
        h_fair="TANPA IKLAN, TANPA PELACAKAN",
        fair_board=["Sekali beli untuk semua game: tanpa iklan, tanpa pembelian dalam aplikasi, tanpa akun",
                    "Tidak ada data yang dikumpulkan; bisa dimainkan offline", "22 bahasa"],
        fair_boardfree=["Tanpa iklan, tanpa pembelian dalam aplikasi, tanpa akun",
                   "Tidak ada data yang dikumpulkan; bisa dimainkan offline", "22 bahasa"],
        more="RetroStone Board, versi lengkapnya, menambah {m} game lagi, semua level di setiap teka-teki, dan "
             "teka-teki acak: sekali beli, tanpa iklan. Kamu bisa melihat semuanya di menu:",
        credit="Dibuat oleh 8BCraft."),
    "ja": dict(
        short_board="ボードゲーム・カード・パズルなど{n}種。友だちと、または端末と対戦。オフラインOK。",
        short_boardfree="チェス、バックギャモン、ソリティアとパズル{p}種が無料。広告なし、オフラインOK。",
        intro_board="RetroStone Board は、緑のテーブルを囲むクラシックゲーム{n}種を収録。チェスとチェッカー、"
                    "バックギャモンと囲碁、麻雀、ハーツ、ベロット、ブリッジ、タロット、ナンバープレイス、ノノグラム、"
                    "ソリティア。木の盤、ていねいに描いたカードとダイス、なめらかなアニメーション、そしてすべての"
                    "ゲームのルールをアプリの中で読めます。",
        intro_boardfree="RetroStone Board Free は、RetroStone Board のクラシックゲーム{f}種とパズル全{p}種を無料で収録。"
                   "チェス、チェッカー、バックギャモン、ハーツ、ソリティア、ナンバープレイス、ノノグラムなど。木の盤、"
                   "ていねいに描いたカードとダイス、そしてすべてのゲームのルールをアプリの中で読めます。",
        h_games="収録ゲーム", h_play="みんなで、または端末と",
        friends="1台の端末を手渡しで回して友だちと：ゲームによって2〜6人",
        bots_board="端末に席をまかせることも。強さは3段階（やさしい・ふつう・むずかしい）：2人以上で遊ぶ{mp}種のうち"
                   "{b}種に対応",
        bots_boardfree="端末に席をまかせることも。強さは3段階（やさしい・ふつう・むずかしい）。2人以上で遊ぶすべてのゲームに"
                  "対応",
        f2f="向かい合わせ：スマホやタブレットを2人の間に平らに置くと、上側のプレイヤーの駒がその人の方を向きます",
        hidden="カードゲームでは手札は秘密：次のプレイヤーに端末を渡す間、カバーがカードを隠します",
        tools="待った、盤の回転、縦でも横でも。各ゲームのルールもワンタップで",
        h_levels="パズルとレベル",
        levels_board=["ひとつずつ作り込んだレベルのあるパズルとソリティアが{L}種、ほとんどが150レベル：合計{lv}"
                      "レベル以上",
                      "各レベル最大3つの星、行き詰まったらヒント",
                      "その後はランダムなパズルがいつまでも"],
        levels_boardfree=["すべてのパズルの最初の{fl}レベル、各レベル最大3つの星", "行き詰まったらヒント"],
        h_fair="広告なし・追跡なし",
        fair_board=["一度の購入ですべてのゲーム。広告なし、アプリ内課金なし、アカウント不要",
                    "データは一切収集しません。オフラインで遊べます", "22言語に対応"],
        fair_boardfree=["広告なし、アプリ内課金なし、アカウント不要", "データは一切収集しません。オフラインで遊べます",
                   "22言語に対応"],
        more="完全版の RetroStone Board には、さらに{m}種のゲーム、すべてのパズルの全レベル、ランダムなパズルが"
             "入っています。買い切りで広告なし。メニューですべてのゲームを見られます：",
        credit="制作：8BCraft"),
    "ko": dict(
        short_board="보드게임·카드·퍼즐 {n}종: 친구와 함께 또는 기기와 대결, 오프라인 플레이.",
        short_boardfree="체스, 백개먼, 솔리테어와 퍼즐 {p}종 무료. 광고 없음, 오프라인 플레이.",
        intro_board="RetroStone Board는 초록 테이블에 둘러앉아 즐기는 클래식 게임 {n}종을 담았습니다. 체스와 체커, "
                    "백개먼과 바둑, 마작, 하트, 블로트, 브리지, 타로, 스도쿠, 노노그램, 솔리테어까지. 나무 보드, "
                    "정성껏 그린 카드와 주사위, 부드러운 애니메이션, 그리고 모든 게임의 규칙을 앱 안에서 볼 수 "
                    "있습니다.",
        intro_boardfree="RetroStone Board Free는 RetroStone Board의 클래식 게임 {f}종과 퍼즐 {p}종 전부를 무료로 "
                   "담았습니다. 체스, 체커, 백개먼, 하트, 솔리테어, 스도쿠, 노노그램 등. 나무 보드, 정성껏 그린 "
                   "카드와 주사위, 그리고 모든 게임의 규칙을 앱 안에서 볼 수 있습니다.",
        h_games="수록 게임", h_play="함께, 또는 기기와 대결",
        friends="기기 하나를 돌려 가며 친구들과: 게임에 따라 2~6명",
        bots_board="기기에 자리를 맡길 수도 있습니다. 난이도 3단계(쉬움, 보통, 어려움): 2인 이상 게임 {mp}종 중 "
                   "{b}종 지원",
        bots_boardfree="기기에 자리를 맡길 수도 있습니다. 난이도 3단계(쉬움, 보통, 어려움), 2인 이상 모든 게임 지원",
        f2f="마주 보고: 휴대폰이나 태블릿을 두 사람 사이에 눕혀 두면 위쪽 플레이어의 말이 그 사람 쪽으로 "
            "돌아갑니다",
        hidden="카드 게임은 손패를 비밀로: 다음 플레이어에게 기기를 넘기는 동안 화면이 카드를 가립니다",
        tools="무르기, 보드 돌리기, 세로·가로 화면, 그리고 각 게임의 규칙을 한 번의 탭으로",
        h_levels="퍼즐과 레벨",
        levels_board=["하나하나 설계한 레벨이 있는 퍼즐과 솔리테어 {L}종, 대부분 150레벨: 총 {lv}레벨 이상",
                      "레벨마다 최대 별 3개, 막히면 힌트",
                      "그다음엔 끝없는 랜덤 퍼즐"],
        levels_boardfree=["모든 퍼즐의 처음 {fl}레벨, 레벨마다 최대 별 3개", "막히면 힌트"],
        h_fair="광고 없음, 추적 없음",
        fair_board=["한 번 구매로 모든 게임: 광고 없음, 인앱 결제 없음, 계정 없음",
                    "어떤 데이터도 수집하지 않음, 오프라인 플레이", "22개 언어"],
        fair_boardfree=["광고 없음, 인앱 결제 없음, 계정 없음", "어떤 데이터도 수집하지 않음, 오프라인 플레이", "22개 언어"],
        more="정식판 RetroStone Board에는 게임 {m}종, 모든 퍼즐의 전체 레벨, 랜덤 퍼즐이 더 있습니다. 한 번 구매, "
             "광고 없음. 메뉴에서 모든 게임을 볼 수 있습니다:",
        credit="제작: 8BCraft"),
    "zh_CN": dict(
        short_board="{n} 款棋类、纸牌和益智游戏：与朋友同玩或与设备对战，可离线游玩。",
        short_boardfree="国际象棋、双陆棋、纸牌接龙和 {p} 款益智游戏，免费、离线、无广告。",
        intro_board="RetroStone Board 汇集 {n} 款经典游戏，围坐在一张绿色牌桌旁：国际象棋和国际跳棋、双陆棋和围棋、"
                    "麻将、红心大战、贝洛特、桥牌和塔罗、数独、数织和纸牌接龙。木质棋盘，精心绘制的纸牌和骰子，"
                    "流畅的动画，每款游戏的规则都在应用里。",
        intro_boardfree="RetroStone Board Free 免费提供 RetroStone Board 的 {f} 款经典游戏和全部 {p} 款益智游戏，围坐在"
                   "一张绿色牌桌旁：国际象棋、国际跳棋、双陆棋、红心大战、纸牌接龙、数独、数织等。木质棋盘，精心"
                   "绘制的纸牌和骰子，每款游戏的规则都在应用里。",
        h_games="游戏列表", h_play="一起玩，或与设备对战",
        friends="朋友们轮流传递同一台设备：根据游戏 2 到 6 人",
        bots_board="也可以让设备坐上任意座位，三个难度（简单、中等、困难）：{mp} 款两人及以上游戏中的 {b} 款支持",
        bots_boardfree="也可以让设备坐上任意座位，三个难度（简单、中等、困难），所有两人及以上游戏都支持",
        f2f="面对面：把手机或平板平放在两人之间，上方玩家的棋子会转向他",
        hidden="纸牌游戏让每个人的手牌保密：设备传给下一位玩家时，遮挡画面会盖住纸牌",
        tools="悔棋、旋转棋盘、竖屏或横屏，每款游戏的规则一点即看",
        h_levels="益智游戏与关卡",
        levels_board=["{L} 款益智游戏和纸牌接龙带有精心设计的关卡，大多数各有 150 关：共 {lv} 多关",
                      "每关最多 3 颗星，卡住时有提示",
                      "之后还有无穷无尽的随机谜题"],
        levels_boardfree=["每款益智游戏的前 {fl} 关，每关最多 3 颗星", "卡住时有提示"],
        h_fair="无广告，无追踪",
        fair_board=["一次购买，畅玩全部游戏：无广告，无应用内购买，无需账号", "不收集任何数据；可离线游玩",
                    "支持 22 种语言"],
        fair_boardfree=["无广告，无应用内购买，无需账号", "不收集任何数据；可离线游玩", "支持 22 种语言"],
        more="完整版 RetroStone Board 还有 {m} 款游戏、每款益智游戏的全部关卡和随机谜题：一次购买，没有广告。"
             "在菜单中就能看到全部游戏：",
        credit="由 8BCraft 制作。"),
}

# the first release's notes (Google Play: 500 characters at most), as the Arcade's full/release-notes-1.0.0.txt:
# (paid, free); the same placeholders. The Arcade's Russian and Ukrainian notes say "вы", as here.
RELEASE = {
    "en": ("First release: {n} classic board, card and puzzle games in one app. Play with friends on one device or "
           "against it, offline, with no ads and no in-app purchases.",
           "First release: {f} classic games and {p} puzzles, free: chess, backgammon, hearts, solitaire, sudoku and "
           "more. Play with friends on one device or against it, offline, with no ads."),
    "cs": ("První vydání: {n} klasických deskových a karetních her a hlavolamů v jedné aplikaci. Hraj s přáteli na "
           "jednom zařízení nebo proti němu, offline, bez reklam a nákupů v aplikaci.",
           "První vydání: {f} klasických her a {p} hlavolamů zdarma: šachy, vrhcáby, srdce, solitér, sudoku a další. "
           "Hraj s přáteli na jednom zařízení nebo proti němu, offline, bez reklam."),
    "da": ("Første udgivelse: {n} klassiske bræt-, kort- og hjernevriderspil i én app. Spil med venner på én enhed "
           "eller mod den, offline, uden reklamer og uden køb i appen.",
           "Første udgivelse: {f} klassiske spil og {p} hjernevridere, gratis: skak, backgammon, hjerterfri, kabale, "
           "sudoku og mere. Spil med venner på én enhed eller mod den, offline, uden reklamer."),
    "de": ("Erste Version: {n} klassische Brett-, Karten- und Knobelspiele in einer App. Spiel mit Freunden an einem "
           "Gerät oder gegen es, offline, ohne Werbung und ohne In-App-Käufe.",
           "Erste Version: {f} klassische Spiele und {p} Knobelspiele, gratis: Schach, Backgammon, Hearts, Solitär, "
           "Sudoku und mehr. Spiel mit Freunden an einem Gerät oder gegen es, offline, ohne Werbung."),
    "el": ("Πρώτη έκδοση: {n} κλασικά επιτραπέζια, παιχνίδια με τράπουλα και γρίφοι σε μία εφαρμογή. Παίξε με φίλους "
           "σε μία συσκευή ή κόντρα σε αυτήν, χωρίς σύνδεση, χωρίς διαφημίσεις και χωρίς αγορές εντός εφαρμογής.",
           "Πρώτη έκδοση: {f} κλασικά παιχνίδια και {p} γρίφοι, δωρεάν: σκάκι, τάβλι, κούπες, πασιέντζα, σουντόκου "
           "και άλλα. Παίξε με φίλους σε μία συσκευή ή κόντρα σε αυτήν, χωρίς σύνδεση, χωρίς διαφημίσεις."),
    "es": ("Primera versión: {n} juegos clásicos de mesa, de cartas y rompecabezas en una app. Juega con amigos en un "
           "solo dispositivo o contra él, sin conexión, sin anuncios ni compras dentro de la app.",
           "Primera versión: {f} juegos clásicos y {p} rompecabezas, gratis: ajedrez, backgammon, corazones, "
           "solitario, sudoku y más. Juega con amigos en un solo dispositivo o contra él, sin conexión, sin anuncios."),
    "fi": ("Ensimmäinen julkaisu: {n} klassista lauta-, kortti- ja pulmapeliä yhdessä sovelluksessa. Pelaa ystävien "
           "kanssa yhdellä laitteella tai laitetta vastaan, offline, ilman mainoksia ja sovelluksen sisäisiä ostoja.",
           "Ensimmäinen julkaisu: {f} klassista peliä ja {p} pulmapeliä ilmaiseksi: shakki, backgammon, hertta, "
           "pasianssi, sudoku ja paljon muuta. Pelaa ystävien kanssa yhdellä laitteella tai laitetta vastaan, "
           "offline, ilman mainoksia."),
    "fr": ("Première version : {n} jeux de société, de cartes et casse-tête dans une appli. Joue entre amis sur un "
           "seul appareil ou contre lui, hors ligne, sans pub ni achat intégré.",
           "Première version : {f} jeux classiques et {p} casse-tête, gratuits : échecs, backgammon, dame de pique, "
           "réussite, sudoku et bien d'autres. Joue entre amis sur un seul appareil ou contre lui, hors ligne, sans "
           "pub."),
    "id": ("Rilis pertama: {n} game papan, kartu, dan teka-teki klasik dalam satu aplikasi. Main bersama teman di "
           "satu perangkat atau melawannya, offline, tanpa iklan dan tanpa pembelian dalam aplikasi.",
           "Rilis pertama: {f} game klasik dan {p} teka-teki, gratis: catur, backgammon, hati, solitaire, sudoku, dan "
           "banyak lagi. Main bersama teman di satu perangkat atau melawannya, offline, tanpa iklan."),
    "it": ("Prima versione: {n} giochi classici da tavolo, di carte e rompicapi in un'app. Gioca con gli amici su un "
           "solo dispositivo o contro di lui, offline, senza pubblicità né acquisti in-app.",
           "Prima versione: {f} giochi classici e {p} rompicapi, gratis: scacchi, backgammon, cuori, solitario, sudoku "
           "e altro ancora. Gioca con gli amici su un solo dispositivo o contro di lui, offline, senza pubblicità."),
    "ja": ("初回リリース：ボードゲーム・カードゲーム・パズルなどクラシックゲーム{n}種をひとつのアプリに。1台の端末で"
           "友だちと、または端末と対戦。オフラインで遊べて、広告なし、アプリ内課金なし。",
           "初回リリース：クラシックゲーム{f}種とパズル{p}種が無料。チェス、バックギャモン、ハーツ、ソリティア、"
           "ナンバープレイスなど。1台の端末で友だちと、または端末と対戦。オフラインで遊べて、広告なし。"),
    "ko": ("첫 출시: 보드게임·카드 게임·퍼즐 등 클래식 게임 {n}종을 앱 하나에. 기기 하나로 친구와 함께 또는 기기와 "
           "대결, 오프라인 플레이, 광고 없음, 인앱 결제 없음.",
           "첫 출시: 클래식 게임 {f}종과 퍼즐 {p}종 무료. 체스, 백개먼, 하트, 솔리테어, 스도쿠 등. 기기 하나로 친구와 "
           "함께 또는 기기와 대결, 오프라인 플레이, 광고 없음."),
    "nb": ("Første utgivelse: {n} klassiske brett-, kort- og tenkespill i én app. Spill med venner på én enhet eller "
           "mot den, offline, uten reklame og uten kjøp i appen.",
           "Første utgivelse: {f} klassiske spill og {p} nøtter, gratis: sjakk, backgammon, hjerter, kabal, sudoku og "
           "mer. Spill med venner på én enhet eller mot den, offline, uten reklame."),
    "nl": ("Eerste versie: {n} klassieke bord-, kaart- en puzzelspellen in één app. Speel met vrienden op één toestel "
           "of ertegen, offline, zonder advertenties en zonder in-app-aankopen.",
           "Eerste versie: {f} klassieke spellen en {p} puzzels, gratis: schaken, backgammon, hartenjagen, patience, "
           "sudoku en meer. Speel met vrienden op één toestel of ertegen, offline, zonder advertenties."),
    "pl": ("Pierwsze wydanie: {n} klasycznych gier planszowych, karcianych i łamigłówek w jednej aplikacji. Graj ze "
           "znajomymi na jednym urządzeniu albo przeciw niemu, offline, bez reklam i zakupów w aplikacji.",
           "Pierwsze wydanie: {f} klasycznych gier i {p} łamigłówek za darmo: szachy, backgammon, kierki, pasjans, "
           "sudoku i więcej. Graj ze znajomymi na jednym urządzeniu albo przeciw niemu, offline, bez reklam."),
    "pt_BR": ("Primeira versão: {n} jogos clássicos de tabuleiro, cartas e quebra-cabeças em um app. Jogue com amigos "
              "em um só aparelho ou contra ele, offline, sem anúncios e sem compras no app.",
              "Primeira versão: {f} jogos clássicos e {p} quebra-cabeças, grátis: xadrez, gamão, copas, paciência, "
              "sudoku e muito mais. Jogue com amigos em um só aparelho ou contra ele, offline, sem anúncios."),
    "pt_PT": ("Primeira versão: {n} jogos clássicos de tabuleiro, cartas e quebra-cabeças numa app. Joga com amigos "
              "num só dispositivo ou contra ele, offline, sem anúncios e sem compras na app.",
              "Primeira versão: {f} jogos clássicos e {p} quebra-cabeças, grátis: xadrez, gamão, copas, solitário, "
              "sudoku e muito mais. Joga com amigos num só dispositivo ou contra ele, offline, sem anúncios."),
    "ru": ("Первый выпуск: классические настольные и карточные игры и головоломки, всего {n}, в одном приложении. "
           "Играйте с друзьями на одном устройстве или против него, офлайн, без рекламы и встроенных покупок.",
           "Первый выпуск: классические игры и головоломки бесплатно — шахматы, нарды, червы, пасьянс, судоку и не "
           "только. Играйте с друзьями на одном устройстве или против него, офлайн, без рекламы."),
    "sv": ("Första versionen: {n} klassiska bräd-, kort- och pusselspel i en app. Spela med vänner på en enhet eller "
           "mot den, offline, utan reklam och utan köp i appen.",
           "Första versionen: {f} klassiska spel och {p} pussel, gratis: schack, backgammon, hjärter, patiens, sudoku "
           "och mer. Spela med vänner på en enhet eller mot den, offline, utan reklam."),
    "tr": ("İlk sürüm: Tek uygulamada {n} klasik masa, kart ve bulmaca oyunu. Tek cihazda arkadaşlarınla ya da cihaza "
           "karşı çevrimdışı oyna; reklam yok, uygulama içi satın alma yok.",
           "İlk sürüm: {f} klasik oyun ve {p} bulmaca ücretsiz: satranç, tavla, kupa, solitaire, sudoku ve daha "
           "fazlası. Tek cihazda arkadaşlarınla ya da cihaza karşı çevrimdışı oyna; reklam yok."),
    "uk": ("Перший випуск: класичні настільні й карткові ігри та головоломки, усього {n}, в одному застосунку. Грайте "
           "з друзями на одному пристрої або проти нього, офлайн, без реклами та покупок у застосунку.",
           "Перший випуск: класичні ігри та головоломки безкоштовно — шахи, нарди, чирви, пасьянс, судоку й не "
           "тільки. Грайте з друзями на одному пристрої або проти нього, офлайн, без реклами."),
    "zh_CN": ("首次发布：{n} 款经典棋类、纸牌和益智游戏，尽在一个应用。在同一台设备上与朋友同玩，或与设备对战；可离线"
              "游玩，无广告，无应用内购买。",
              "首次发布：{f} 款经典游戏和 {p} 款益智游戏，免费：国际象棋、双陆棋、红心大战、纸牌接龙、数独等。在同一台"
              "设备上与朋友同玩，或与设备对战；可离线游玩，无广告。"),
}


def write_release_notes(app, version, fmt):
    """<app>/release-notes-<version>.txt in the Arcade's format: <locale> ... </locale>, en-GB first (the English),
    then the Play Console's locales in order. Returns the problems (over 500 characters)."""
    notes = [("en-GB", RELEASE["en"][app != "board"].format(**fmt))]
    notes += sorted((LOCALES[lang], RELEASE[lang][app != "board"].format(**fmt)) for lang in LOCALES)
    with open(os.path.join(STORE, app, f"release-notes-{version}.txt"), "w", encoding="utf-8") as fh:
        fh.write("".join(f"<{loc}>\n{text}\n</{loc}>\n" for loc, text in notes))
    return [f"{app}/release notes {loc}: {len(t)} > 500" for loc, t in notes if len(t) > 500]


# how a list reads in each language: "a tab (count): name · name" (a middle dot: some names hold a comma)
COLON = {"fr": " : ", "ja": "：", "zh_CN": "："}
COMMA = {"ja": "、", "zh_CN": "、"}
# thousands: 6000 written as each language writes it
THOUSANDS = {"en": ",", "ja": ",", "ko": ",", "zh_CN": "", "es": "", "de": ".", "da": ".", "nl": ".", "it": ".",
             "pt_BR": ".", "el": ".", "tr": ".", "id": "."}


def thousands(lang, v):
    s = str(v)
    sep = THOUSANDS.get(lang, " ")              # the others: a no-break space (fr, cs, fi, nb, pl, ru, sv, uk, pt_PT)
    return s[:-3] + sep + s[-3:] if len(s) > 3 else s


# ---- the facts, from the app's code ---------------------------------------------------------------------------------
def src(path):
    return open(path, encoding="utf-8").read()


def game_ids():
    ids = set()
    for p in glob.glob(os.path.join(BG, "src", "games", "*.c")):
        s = src(p)
        ids.update(re.findall(r'\.id\s*=\s*"(\w+)"', s))
        ids.update(re.findall(r'^SUI_GAME\(\w+,\s*"(\w+)"', s, re.M))
    return sorted(ids)


def sections():
    s = src(os.path.join(BG, "src", "shell.c"))
    table = s[s.index("SECTION_OF[] = {"):]
    table = table[:table.index("};")]
    return {gid: sec for gid, sec in re.findall(r'\{"(\w+)",\s*SEC_(\w+)\}', table)}


def own_files(gid, ext):
    """a game's files: src/games/<id>.<ext> and <id>_*.<ext> (not another game's whose id starts the same: go, goose)"""
    games = os.path.join(BG, "src", "games")
    return sorted(glob.glob(os.path.join(games, f"{gid}.{ext}")) + glob.glob(os.path.join(games, f"{gid}_*.{ext}")))


def defines(gid):
    """the #defines of a game's files (src/games/<id>.c, <id>_*.c and their .h), as text"""
    d = {}
    for p in own_files(gid, "[ch]"):
        for k, v in re.findall(r"^#define[ \t]+(\w+)[ \t]+([^\n/]+)", src(p), re.M):
            d.setdefault(k, v.strip())
        for body in re.findall(r"\benum\s*\{([^}]*)\}", src(p)):         # enum { X_LEVELS = 150, ... } too
            for k, v in re.findall(r"\b([A-Z_][A-Z0-9_]*)\s*=\s*([^,]+)", body):
                d.setdefault(k, v.strip())
    return d


def resolve(expr, d):
    for _ in range(10):
        new = re.sub(r"\b([A-Z_][A-Z0-9_]*)\b", lambda m: "(" + d[m.group(1)] + ")" if m.group(1) in d else m.group(1),
                     expr)
        if new == expr:
            break
        expr = new
    if not re.fullmatch(r"[\d\s()*+\-u]+", expr):
        sys.exit(f"cannot resolve the levels: {expr}")
    return eval(expr.replace("u", ""))                 # digits and + - * ( ) only


def levels(gid):
    """the game's designed levels (0: none)"""
    p = os.path.join(BG, "src", "games", gid + ".c")
    s = src(p) if os.path.exists(p) else ""
    m = re.search(r"\.levels\s*=\s*(\w+)", s)
    if m:
        return resolve(m.group(1), defines(gid))
    m = re.search(r'^SUI_GAME\(\w+,\s*"\w+",\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*(\w+)', s, re.M)
    return resolve(m.group(1), defines(gid)) if m else 0


def solo(gid):
    """a game for one player only (.players: bit 1 alone)"""
    for p in own_files(gid, "c"):
        s = src(p)
        if re.search(r'^SUI_GAME\(', s, re.M):
            return True
        m = re.search(r"\.players\s*=\s*([^,\n]+),", s)
        if m:
            return m.group(1).replace(" ", "") in ("1u<<1", "1<<1")
    return False


def name(lang, gid):
    """the game's name in the menu (the first line of its rules), or NAMES' for the listing"""
    if (lang, gid) in NAMES:
        return NAMES[lang, gid]
    return open(os.path.join(BG, "help", lang, gid + ".txt"), encoding="utf-8-sig").readline().strip()


def tab_names(lang):
    return [l.strip() for l in open(os.path.join(BG, "help", lang, "_menu.txt"), encoding="utf-8-sig") if l.strip()]


def conf(path):
    c = {}
    for line in open(path, encoding="utf-8"):
        if "=" in line and not line.lstrip().startswith("#"):
            k, v = line.split("=", 1)
            c[k.strip()] = v.strip()
    return c


def sort_key(s):
    return "".join(ch for ch in unicodedata.normalize("NFD", s.casefold()) if not unicodedata.combining(ch))


def facts():
    ids = game_ids()
    sec = sections()
    tab = {g: SECTIONS.index(sec.get(g, "STRATEGY")) for g in ids}
    free_conf = conf(os.path.join(ROOT, "apps", "android", "app-boardfree.conf"))
    free_games = free_conf["FREE_GAMES"].split()
    unknown = [g for g in free_games if g not in ids]
    if unknown:
        sys.exit(f"FREE_GAMES: unknown {unknown}")
    lv = {g: levels(g) for g in ids}
    multi = [g for g in ids if not solo(g)]
    bots = [g for g in multi if os.path.exists(os.path.join(BG, "src", "games", g + "_bot.c"))]
    puzzles = [g for g in ids if tab[g] == 0]
    playable_free = set(puzzles) | set(free_games)
    return dict(ids=ids, tab=tab, levels=lv, multi=multi, bots=bots, puzzles=puzzles, free_games=free_games,
                locked=[g for g in ids if g not in playable_free], free_levels=int(free_conf["FREE_LEVELS"]))


def games_section(lang, F, show, by_tab=True):
    """a bullet a tab: "Puzzles (36): name, name...", the games of show only, sorted by their name"""
    colon, comma = COLON.get(lang, ": "), COMMA.get(lang, " · ")
    tabs = tab_names(lang)
    out = []
    for i, label in enumerate(tabs):
        names = sorted((name(lang, g) for g in show if F["tab"][g] == i), key=sort_key)
        if names:
            out.append(f"• {label} ({len(names)}){colon}{comma.join(names)}")
    return out


def full_description(lang, app, F, fmt):
    t = T[lang]
    out = [t["intro_" + app].format(**fmt), "", t["h_games"]]
    show = F["ids"] if app == "board" else F["puzzles"] + F["free_games"]
    out += games_section(lang, F, show)
    out += ["", t["h_play"], "• " + t["friends"], "• " + t["bots_" + app].format(**fmt), "• " + t["f2f"],
            "• " + t["hidden"], "• " + t["tools"]]
    out += ["", t["h_levels"]] + ["• " + s.format(**fmt) for s in t["levels_" + app]]
    out += ["", t["h_fair"]] + ["• " + s for s in t["fair_" + app]]
    if app == "boardfree":
        comma = COMMA.get(lang, " · ")
        out += ["", t["more"].format(**fmt),
                comma.join(sorted((name(lang, g) for g in F["locked"]), key=sort_key))]
    out += ["", t["credit"]]
    return "\n".join(out) + "\n"


def main():
    F = facts()
    n = len(F["ids"])
    with_levels = [g for g in F["ids"] if F["levels"][g]]
    total = sum(F["levels"].values())
    free_bots = [g for g in F["free_games"] if g in F["multi"] and g not in F["bots"]]
    if free_bots:
        sys.exit(f"the free listing says every free game for two or more has a bot: not {free_bots}")
    print(f"{n} games: tabs {[sum(1 for g in F['ids'] if F['tab'][g] == i) for i in range(len(SECTIONS))]}, "
          f"{len(F['multi'])} for two or more ({len(F['bots'])} with a bot), {len(with_levels)} with levels "
          f"({total} levels; {sum(1 for g in with_levels if F['levels'][g] == 150)} with 150), "
          f"free: {len(F['free_games'])} games + {len(F['puzzles'])} puzzles, {len(F['locked'])} locked")
    problems = []
    for app in ("board", "boardfree"):
        entries = []
        for lang in LOCALES:
            fmt = dict(n=n, f=len(F["free_games"]), p=len(F["puzzles"]), m=len(F["locked"]), b=len(F["bots"]),
                       mp=len(F["multi"]), L=len(with_levels), lv=thousands(lang, total // 1000 * 1000),
                       fl=F["free_levels"])
            title = title_for(lang, app, n)
            short = T[lang]["short_" + app].format(**fmt)
            full_d = full_description(lang, app, F, fmt)
            notes = RELEASE[lang][app != "board"].format(**fmt)
            for field, text, most in (("title", title, 30), ("short", short, 80), ("full", full_d, 4000),
                                      ("release notes", notes, 500)):
                if len(text.encode("utf-16-le")) // 2 > most:            # the same count in UTF-16 units
                    problems.append(f"{app}/{LOCALES[lang]}: {field} too long in UTF-16 units")
                low = text.casefold()
                for tm in TRADEMARKS + BY_LANG.get(lang, []):
                    if re.search(tm, low):
                        problems.append(f"{app}/{LOCALES[lang]}: {field}: a trademark: {tm}")
                for tm in CASED:
                    if re.search(tm, text):
                        problems.append(f"{app}/{LOCALES[lang]}: {field}: a trademark: {tm}")
            entries.append((lang, title, short, full_d))
        problems += write_listings(os.path.join(STORE, app), app, title_for("en", app, n), entries)
        version = conf(os.path.join(ROOT, "apps", "android", f"app-{app}.conf"))["APP_VERSION_NAME"]
        problems += write_release_notes(app, version, fmt)
    for p in problems:
        print("PROBLEM", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
