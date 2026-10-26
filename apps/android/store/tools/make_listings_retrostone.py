#!/usr/bin/env python3
"""The Google Play store listing of RetroStone (the one free app: apps/android/app-retrostone.conf), in the app's 22
languages: apps/android/store/retrostone/listings/<locale>/title.txt, short_description.txt (80 characters at most),
full_description.txt (4000 at most), retrostone/listings-all-languages.txt (the Arcade's format: make_listings.py's
write_listings()) and retrostone/release-notes-<APP_VERSION_NAME>.txt (<locale> ... </locale>, 500 at most).

    python3 apps/android/store/tools/make_listings_retrostone.py

The facts come from the app's code, as make_listings_board.py reads them (its helpers): the board games (src/games),
their tab (shell.c's SECTION_OF, the tabs in SEC_SHOWN's order), their names in each language (help/<lang>/<id>.txt),
the tabs' names (help/<lang>/_menu.txt), the bots, the designed levels; and the Arcade tab's games (the .conf's
ARCADE and ARCADE_HIDDEN, as tools/gen_arcade.py lists them; their names stay as they are in every language).
Over 250 games do not fit in 4000 characters: each board tab lists its best-known games (FEATURED) and "and N more";
the Arcade tab lists every game. The texts shared with RetroStone Board (the play together lines, the levels, no
data, the languages, the credit) are make_listings_board.py's own; English is the source, French is written with
"tu"; the 20 other languages were written by an AI (Claude) and need a native speaker's review (CHECKLIST-retrostone.md).
Polish, Czech, Russian and Ukrainian never put a count before a noun (the plural's form would change with the number).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_listings_board as B  # noqa: E402  (the board games' facts, the shared texts, the checks)
from make_listings import LOCALES, gc, write_listings  # noqa: E402  (the languages, the Arcade's catalogue)

STORE, ROOT = B.STORE, B.ROOT
APP = "retrostone"
CONF = os.path.join(ROOT, "apps", "android", "app-retrostone.conf")

# shell.c's sections by number (enum SEC_*), and the order of the tabs in the menu (SEC_SHOWN)
SECTIONS = ["PUZZLE", "STRATEGY", "CHESS", "RACE", "CARDS", "GUESS", "ARCADE", "LOGIC"]
SHOWN = ["PUZZLE", "LOGIC", "STRATEGY", "CHESS", "RACE", "CARDS", "GUESS", "ARCADE"]

# the games each board tab names in the listing, the best known first (no brand-like names: see B.TRADEMARKS); a tab
# not here names all its games
FEATURED = {
    "PUZZLE": ["ballsort", "watersort", "blocks", "jigsaw", "mines", "pipes", "taquin", "tangram", "spotdiff",
               "chesspuzzles"],
    "LOGIC": ["sudoku", "nonogram", "killersudoku", "kakuro", "calcudoku", "binairo", "starbattle", "hashi", "tents",
              "futoshiki"],
    "STRATEGY": ["draughts", "go", "reversi", "fourrow", "mancala", "morris", "gomoku", "halma", "hex", "pool"],
    "RACE": ["backgammon", "ludo", "yacht", "farkle", "snakes", "ur", "shutthebox", "liarsdice"],
    "CARDS": ["klondike", "spider", "freecells", "mahjong4", "mahjong", "hearts", "spades", "belote", "bridge",
              "tarot", "ginrummy", "canasta"],
}

# the store titles (30 at most), the owner's choice (26 October 2026): what the app is, not its brand ("RetroStone" stays
# the name under the icon): "250 in 1" and the three kinds of games; the next one when the first is too long. {c}: the
# games rounded down to 50 (the title stays true as games are added). "N in 1" keeps the Slavic titles free of a noun
# after the number.
TITLES = {
    "en": ["{c} in 1 Puzzle, Board, Arcade"],
    "fr": ["{c} en 1 : société et arcade"],
    "de": ["{c} in 1 Rätsel, Brett, Arcade"],
    "es": ["{c} en 1: puzles, mesa, arcade"],
    "it": ["{c} in 1 puzzle, tavolo, arcade", "{c} in 1: tavolo e arcade"],
    "pt_BR": ["{c} em 1: tabuleiro e arcade"],
    "pt_PT": ["{c} em 1: tabuleiro e arcade"],
    "nl": ["{c} in 1: puzzel, bord, arcade"],
    "sv": ["{c} i 1: brädspel och arkad"],
    "da": ["{c} i 1: brætspil og arkade"],
    "nb": ["{c} i 1: puslespill, brett, arkade", "{c} i 1: brettspill og arkade"],
    "fi": ["{c} in 1: pulma, lauta, arcade"],
    "pl": ["{c} w 1: łamigłówki, planszówki", "{c} w 1: planszówki i arcade"],
    "cs": ["{c} v 1: hlavolamy, deskovky", "{c} v 1: deskovky a arkády"],
    "ru": ["{c} в 1: головоломки, настолки", "{c} в 1: настолки и аркады"],
    "uk": ["{c} в 1: головоломки, настільні", "{c} в 1: настільні й аркади"],
    "el": ["{c} σε 1: γρίφοι, επιτραπέζια", "{c} σε 1: επιτραπέζια & arcade"],
    "tr": ["{c} Bir Arada: Masa ve Arcade", "{c} Oyun Bir Arada"],
    "id": ["{c} in 1: Puzzle, Papan, Arcade", "{c} in 1: Game Papan & Arcade"],
    "ja": ["{c} in 1 パズル・ボード・アーケード"],
    "ko": ["{c} in 1 퍼즐·보드·아케이드"],
    "zh_CN": ["{c}合1 益智·棋牌·街机"],
}

# this app's own texts. {n} every game, {nb} the board, card and puzzle games, {a} the Arcade's, {k} a tab's games
# not named. arcade: the Arcade's lines; daily: the Stats and Settings pages; fair: the first of the free lines (the
# others: Board's "no data" and "22 languages"), tip: the Settings page's tips.
T = {
    "en": dict(
        short="{n} puzzle, board, card and arcade games. Free, open source, no ads, offline.",
        intro="RetroStone gathers {n} games in one free app: {nb} board, card and puzzle games around a green table "
              "(chess, go, backgammon, mahjong, solitaire, belote, bridge, sudoku, nonograms...) and {a} pixel-art "
              "arcade games made for the touch screen. No ads, nothing locked: every game and every level is free.",
        h_arcade="THE ARCADE",
        arcade=["{a} pixel-art arcade games made for the touch screen, from one-finger games to platformers",
                "Your best score is kept, and every game waits where you left it",
                "Gamepads work too: up to 4 players in some games"],
        h_daily="EVERY DAY",
        daily=["A daily streak, experience, levels and achievements, and your progress in every tab",
               "Light or dark theme, and a search to find any game by its name"],
        h_fair="FREE, WITH NO ADS",
        fair="Every game and every level, free: no ads, nothing locked, no account",
        tip="Like it? An optional tip in the settings helps make new games",
        more="and {k} more",
        release="First release: {n} board, card, puzzle and arcade games in one free app, with no ads and nothing "
                "locked. Play offline, with friends on one device or against it."),
    "fr": dict(
        short="{n} casse-tête, jeux de société et d'arcade : gratuit, open source, sans pub.",
        intro="RetroStone réunit {n} jeux dans une appli gratuite : {nb} jeux de société, de cartes et casse-tête "
              "autour d'un tapis vert (échecs, go, backgammon, mahjong, réussites, belote, bridge, sudoku, "
              "nonogrammes…) et {a} jeux d'arcade en pixel art faits pour l'écran tactile. Sans pub, rien de "
              "verrouillé : chaque jeu et chaque niveau est gratuit.",
        h_arcade="L'ARCADE",
        arcade=["{a} jeux d'arcade en pixel art faits pour l'écran tactile, des jeux à un doigt aux jeux de "
                "plateformes",
                "Ton meilleur score est gardé, et chaque partie t'attend là où tu l'as laissée",
                "Les manettes marchent aussi : jusqu'à 4 joueurs dans certains jeux"],
        h_daily="CHAQUE JOUR",
        daily=["Une série de jours, de l'expérience, des niveaux et des succès, et ta progression dans chaque onglet",
               "Thème clair ou sombre, et une recherche pour trouver un jeu par son nom"],
        h_fair="GRATUIT, SANS PUB",
        fair="Tous les jeux et tous les niveaux, gratuits : sans pub, rien de verrouillé, sans compte",
        tip="Ça te plaît ? Un pourboire facultatif, dans les réglages, aide à créer de nouveaux jeux",
        more="et {k} autres",
        release="Première version : {n} jeux de société, de cartes, casse-tête et d'arcade dans une appli gratuite, "
                "sans pub et sans rien de verrouillé. Joue hors ligne, entre amis sur un seul appareil ou contre lui."),
    "de": dict(
        short="Brett-, Karten-, Knobel- und Arcade-Spiele: {n} Spiele, ohne Werbung, offline.",
        intro="RetroStone vereint {n} Spiele in einer kostenlosen App: {nb} Brett-, Karten- und Knobelspiele an "
              "einem grünen Spieltisch (Schach, Go, Backgammon, Mahjong, Solitär, Belote, Bridge, Sudoku, "
              "Nonogramme …) und {a} Arcade-Spiele in Pixel-Art, gemacht für den Touchscreen. Keine Werbung, nichts "
              "gesperrt: jedes Spiel und jedes Level ist kostenlos.",
        h_arcade="DIE ARCADE",
        arcade=["{a} Arcade-Spiele in Pixel-Art für den Touchscreen, von Ein-Finger-Spielen bis zu Jump 'n' Runs",
                "Dein Rekord wird gespeichert, und jedes Spiel wartet dort, wo du aufgehört hast",
                "Gamepads gehen auch: bis zu 4 Spieler in manchen Spielen"],
        h_daily="JEDEN TAG",
        daily=["Eine Tagesserie, Erfahrungspunkte, Stufen und Erfolge, und dein Fortschritt in jedem Reiter",
               "Helles oder dunkles Design, und eine Suche, die jedes Spiel nach seinem Namen findet"],
        h_fair="KOSTENLOS, OHNE WERBUNG",
        fair="Alle Spiele und alle Level kostenlos: keine Werbung, nichts gesperrt, kein Konto",
        tip="Gefällt's dir? Ein freiwilliges Trinkgeld in den Einstellungen hilft, neue Spiele zu machen",
        more="und {k} weitere",
        release="Erste Version: {n} Brett-, Karten-, Knobel- und Arcade-Spiele in einer kostenlosen App, ohne "
                "Werbung und ohne Sperren. Spiel offline, mit Freunden an einem Gerät oder gegen es."),
    "es": dict(
        short="Juegos de mesa, cartas, puzles y arcade: {n} juegos sin anuncios y sin conexión.",
        intro="RetroStone reúne {n} juegos en una app gratuita: {nb} juegos de mesa, de cartas y rompecabezas "
              "alrededor de un tapete verde (ajedrez, go, backgammon, mahjong, solitario, belote, bridge, sudoku, "
              "nonogramas…) y {a} juegos arcade en pixel art hechos para la pantalla táctil. Sin anuncios y sin nada "
              "bloqueado: todos los juegos y todos los niveles son gratis.",
        h_arcade="EL ARCADE",
        arcade=["{a} juegos arcade en pixel art hechos para la pantalla táctil, desde juegos de un dedo hasta "
                "plataformas",
                "Tu mejor puntuación se guarda, y cada partida te espera donde la dejaste",
                "También con mando: hasta 4 jugadores en algunos juegos"],
        h_daily="CADA DÍA",
        daily=["Una racha diaria, experiencia, niveles y logros, y tu progreso en cada pestaña",
               "Tema claro u oscuro, y un buscador para encontrar cualquier juego por su nombre"],
        h_fair="GRATIS, SIN ANUNCIOS",
        fair="Todos los juegos y todos los niveles, gratis: sin anuncios, nada bloqueado, sin cuenta",
        tip="¿Te gusta? Una propina opcional en los ajustes ayuda a crear nuevos juegos",
        more="y {k} más",
        release="Primera versión: {n} juegos de mesa, de cartas, rompecabezas y arcade en una app gratuita, sin "
                "anuncios y sin nada bloqueado. Juega sin conexión, con amigos en un solo dispositivo o contra él."),
    "it": dict(
        short="Giochi da tavolo, di carte, rompicapi e arcade: {n} giochi, senza pubblicità.",
        intro="RetroStone riunisce {n} giochi in un'app gratuita: {nb} giochi da tavolo, di carte e rompicapi "
              "attorno a un tappeto verde (scacchi, go, backgammon, mahjong, solitario, belote, bridge, sudoku, "
              "nonogrammi…) e {a} giochi arcade in pixel art pensati per il touch screen. Niente pubblicità, niente "
              "di bloccato: ogni gioco e ogni livello è gratis.",
        h_arcade="L'ARCADE",
        arcade=["{a} giochi arcade in pixel art pensati per il touch screen, dai giochi a un dito ai platform",
                "Il tuo record viene salvato, e ogni partita ti aspetta dove l'hai lasciata",
                "Funzionano anche i controller: fino a 4 giocatori in alcuni giochi"],
        h_daily="OGNI GIORNO",
        daily=["Una serie di giorni, esperienza, livelli e obiettivi, e i tuoi progressi in ogni scheda",
               "Tema chiaro o scuro, e una ricerca per trovare qualsiasi gioco per nome"],
        h_fair="GRATIS, SENZA PUBBLICITÀ",
        fair="Tutti i giochi e tutti i livelli, gratis: niente pubblicità, niente di bloccato, nessun account",
        tip="Ti piace? Una mancia facoltativa nelle impostazioni aiuta a creare nuovi giochi",
        more="e altri {k}",
        release="Prima versione: {n} giochi da tavolo, di carte, rompicapi e arcade in un'app gratuita, senza "
                "pubblicità e senza nulla di bloccato. Gioca offline, con gli amici su un solo dispositivo o contro "
                "di lui."),
    "pt_BR": dict(
        short="Jogos de tabuleiro, cartas, quebra-cabeças e arcade: {n} jogos, sem anúncios.",
        intro="O RetroStone reúne {n} jogos em um app gratuito: {nb} jogos de tabuleiro, de cartas e "
              "quebra-cabeças em volta de uma mesa verde (xadrez, go, gamão, mahjong, paciência, belote, bridge, "
              "sudoku, nonogramas…) e {a} jogos arcade em pixel art feitos para a tela de toque. Sem anúncios, nada "
              "bloqueado: todos os jogos e todos os níveis são grátis.",
        h_arcade="O ARCADE",
        arcade=["{a} jogos arcade em pixel art feitos para a tela de toque, de jogos de um dedo a jogos de "
                "plataforma",
                "Seu recorde fica salvo, e cada partida espera por você onde parou",
                "Controles também funcionam: até 4 jogadores em alguns jogos"],
        h_daily="TODO DIA",
        daily=["Uma sequência de dias, experiência, níveis e conquistas, e seu progresso em cada aba",
               "Tema claro ou escuro, e uma busca para achar qualquer jogo pelo nome"],
        h_fair="GRÁTIS, SEM ANÚNCIOS",
        fair="Todos os jogos e todos os níveis, grátis: sem anúncios, nada bloqueado, sem conta",
        tip="Gostou? Uma gorjeta opcional nas configurações ajuda a criar novos jogos",
        more="e mais {k}",
        release="Primeira versão: {n} jogos de tabuleiro, de cartas, quebra-cabeças e arcade em um app gratuito, "
                "sem anúncios e sem nada bloqueado. Jogue offline, com amigos em um só aparelho ou contra ele."),
    "pt_PT": dict(
        short="Jogos de tabuleiro, cartas, quebra-cabeças e arcade: {n} jogos, sem anúncios.",
        intro="O RetroStone reúne {n} jogos numa app gratuita: {nb} jogos de tabuleiro, de cartas e quebra-cabeças "
              "à volta de uma mesa verde (xadrez, go, gamão, mahjong, solitário, belote, bridge, sudoku, "
              "nonogramas…) e {a} jogos arcade em pixel art feitos para o ecrã tátil. Sem anúncios, nada bloqueado: "
              "todos os jogos e todos os níveis são grátis.",
        h_arcade="O ARCADE",
        arcade=["{a} jogos arcade em pixel art feitos para o ecrã tátil, de jogos de um dedo a jogos de plataformas",
                "O teu recorde fica guardado, e cada partida espera por ti onde a deixaste",
                "Os comandos também funcionam: até 4 jogadores em alguns jogos"],
        h_daily="TODOS OS DIAS",
        daily=["Uma sequência de dias, experiência, níveis e conquistas, e o teu progresso em cada separador",
               "Tema claro ou escuro, e uma pesquisa para encontrar qualquer jogo pelo nome"],
        h_fair="GRÁTIS, SEM ANÚNCIOS",
        fair="Todos os jogos e todos os níveis, grátis: sem anúncios, nada bloqueado, sem conta",
        tip="Gostas? Uma gorjeta opcional nas definições ajuda a criar novos jogos",
        more="e mais {k}",
        release="Primeira versão: {n} jogos de tabuleiro, de cartas, quebra-cabeças e arcade numa app gratuita, sem "
                "anúncios e sem nada bloqueado. Joga offline, com amigos num só dispositivo ou contra ele."),
    "nl": dict(
        short="Bord-, kaart-, puzzel- en arcadespellen: {n} spellen, zonder advertenties.",
        intro="RetroStone bundelt {n} spellen in één gratis app: {nb} bord-, kaart- en puzzelspellen rond een "
              "groene speltafel (schaken, go, backgammon, mahjong, patience, belote, bridge, sudoku, nonogrammen…) "
              "en {a} arcadespellen in pixelart, gemaakt voor het touchscreen. Geen advertenties, niets op slot: elk "
              "spel en elk level is gratis.",
        h_arcade="DE ARCADE",
        arcade=["{a} arcadespellen in pixelart, gemaakt voor het touchscreen, van spellen met één vinger tot "
                "platformspellen",
                "Je beste score wordt bewaard, en elk spel wacht waar je gebleven was",
                "Gamepads werken ook: tot 4 spelers in sommige spellen"],
        h_daily="ELKE DAG",
        daily=["Een dagenreeks, ervaringspunten, niveaus en prestaties, en je voortgang in elk tabblad",
               "Licht of donker thema, en een zoekfunctie om elk spel op naam te vinden"],
        h_fair="GRATIS, ZONDER ADVERTENTIES",
        fair="Alle spellen en alle levels gratis: geen advertenties, niets op slot, geen account",
        tip="Vind je het leuk? Een vrijwillige fooi in de instellingen helpt nieuwe spellen te maken",
        more="en nog {k}",
        release="Eerste versie: {n} bord-, kaart-, puzzel- en arcadespellen in één gratis app, zonder advertenties "
                "en zonder iets op slot. Speel offline, met vrienden op één toestel of ertegen."),
    "sv": dict(
        short="Bräd-, kort-, pussel- och arkadspel: {n} spel i en gratis app, utan reklam.",
        intro="RetroStone samlar {n} spel i en gratis app: {nb} bräd-, kort- och pusselspel kring ett grönt "
              "spelbord (schack, go, backgammon, mahjong, patiens, belote, bridge, sudoku, nonogram …) och {a} "
              "arkadspel i pixelgrafik, gjorda för pekskärmen. Ingen reklam, inget låst: varje spel och varje nivå "
              "är gratis.",
        h_arcade="ARKADEN",
        arcade=["{a} arkadspel i pixelgrafik gjorda för pekskärmen, från enfingerspel till plattformsspel",
                "Ditt rekord sparas, och varje spel väntar där du slutade",
                "Handkontroller fungerar också: upp till 4 spelare i vissa spel"],
        h_daily="VARJE DAG",
        daily=["En dagsserie, erfarenhetspoäng, nivåer och prestationer, och dina framsteg i varje flik",
               "Ljust eller mörkt tema, och en sökning som hittar vilket spel som helst på namnet"],
        h_fair="GRATIS, UTAN REKLAM",
        fair="Alla spel och alla nivåer gratis: ingen reklam, inget låst, inget konto",
        tip="Gillar du det? En frivillig dricks i inställningarna hjälper till att skapa nya spel",
        more="och {k} till",
        release="Första versionen: {n} bräd-, kort-, pussel- och arkadspel i en gratis app, utan reklam och utan "
                "något låst. Spela offline, med vänner på en enhet eller mot den."),
    "da": dict(
        short="Bræt-, kort-, hjernevrider- og arkadespil: {n} spil, uden reklamer, offline.",
        intro="RetroStone samler {n} spil i én gratis app: {nb} bræt-, kort- og hjernevriderspil omkring et grønt "
              "spillebord (skak, go, backgammon, mahjong, kabale, belote, bridge, sudoku, nonogrammer …) og {a} "
              "arkadespil i pixelgrafik, lavet til touchskærmen. Ingen reklamer, intet låst: hvert spil og hvert "
              "niveau er gratis.",
        h_arcade="ARKADEN",
        arcade=["{a} arkadespil i pixelgrafik lavet til touchskærmen, fra spil med én finger til platformspil",
                "Din rekord gemmes, og hvert spil venter, hvor du slap",
                "Gamepads virker også: op til 4 spillere i nogle spil"],
        h_daily="HVER DAG",
        daily=["En stribe af dage, erfaringspoint, niveauer og præstationer, og dine fremskridt i hver fane",
               "Lyst eller mørkt tema, og en søgning, der finder ethvert spil ud fra navnet"],
        h_fair="GRATIS, UDEN REKLAMER",
        fair="Alle spil og alle niveauer gratis: ingen reklamer, intet låst, ingen konto",
        tip="Kan du lide det? Frivillige drikkepenge i indstillingerne hjælper nye spil på vej",
        more="og {k} til",
        release="Første udgivelse: {n} bræt-, kort-, hjernevrider- og arkadespil i én gratis app, uden reklamer og "
                "uden noget låst. Spil offline, med venner på én enhed eller mod den."),
    "nb": dict(
        short="Brett-, kort-, tenke- og arkadespill: {n} spill, uten reklame, offline.",
        intro="RetroStone samler {n} spill i én gratis app: {nb} brett-, kort- og tenkespill rundt et grønt "
              "spillebord (sjakk, go, backgammon, mahjong, kabal, belote, bridge, sudoku, nonogram …) og {a} "
              "arkadespill i pikselgrafikk, laget for berøringsskjermen. Ingen reklame, ingenting låst: hvert spill "
              "og hvert nivå er gratis.",
        h_arcade="ARKADEN",
        arcade=["{a} arkadespill i pikselgrafikk laget for berøringsskjermen, fra énfingerspill til plattformspill",
                "Rekorden din lagres, og hvert spill venter der du slapp",
                "Spillkontrollere virker også: opptil 4 spillere i noen spill"],
        h_daily="HVER DAG",
        daily=["En dagsrekke, erfaringspoeng, nivåer og prestasjoner, og fremgangen din i hver fane",
               "Lyst eller mørkt tema, og et søk som finner ethvert spill på navnet"],
        h_fair="GRATIS, UTEN REKLAME",
        fair="Alle spill og alle nivåer gratis: ingen reklame, ingenting låst, ingen konto",
        tip="Liker du det? Et frivillig tips i innstillingene hjelper nye spill på vei",
        more="og {k} til",
        release="Første utgivelse: {n} brett-, kort-, tenke- og arkadespill i én gratis app, uten reklame og uten "
                "noe låst. Spill offline, med venner på én enhet eller mot den."),
    "fi": dict(
        short="Lauta-, kortti-, pulma- ja arcadepelejä: {n} peliä ilman mainoksia, offline.",
        intro="RetroStone kokoaa {n} peliä yhteen ilmaiseen sovellukseen: {nb} lauta-, kortti- ja pulmapeliä "
              "vihreän pelipöydän ääressä (shakki, go, backgammon, mahjong, pasianssi, belote, bridge, sudoku, "
              "nonogrammit…) ja {a} pikseligrafiikalla tehtyä arcadepeliä kosketusnäytölle. Ei mainoksia, ei mitään "
              "lukittuna: jokainen peli ja jokainen taso on ilmainen.",
        h_arcade="ARCADE",
        arcade=["{a} pikseligrafiikalla tehtyä arcadepeliä kosketusnäytölle, yhden sormen peleistä tasohyppelyihin",
                "Ennätyksesi tallentuu, ja jokainen peli odottaa siinä, mihin jäit",
                "Myös peliohjaimet toimivat: jopa 4 pelaajaa joissakin peleissä"],
        h_daily="JOKA PÄIVÄ",
        daily=["Päiväputki, kokemuspisteet, tasot ja saavutukset sekä edistymisesi jokaisella välilehdellä",
               "Vaalea tai tumma teema ja haku, joka löytää minkä tahansa pelin nimellä"],
        h_fair="ILMAINEN, ILMAN MAINOKSIA",
        fair="Kaikki pelit ja kaikki tasot ilmaiseksi: ei mainoksia, ei lukituksia, ei tiliä",
        tip="Pidätkö siitä? Vapaaehtoinen tippi asetuksissa auttaa tekemään uusia pelejä",
        more="ja {k} muuta",
        release="Ensimmäinen julkaisu: {n} lauta-, kortti-, pulma- ja arcadepeliä yhdessä ilmaisessa "
                "sovelluksessa, ilman mainoksia ja ilman lukituksia. Pelaa offline, ystävien kanssa yhdellä "
                "laitteella tai laitetta vastaan."),
    "pl": dict(
        short="Gry planszowe, karciane, łamigłówki i arcade w jednej aplikacji. Bez reklam.",
        intro="RetroStone łączy w jednej darmowej aplikacji gry planszowe, karciane i łamigłówki przy zielonym "
              "stole (szachy, go, backgammon, mahjong, pasjans, belote, brydż, sudoku, nonogramy…) oraz gry "
              "zręcznościowe w pixel arcie, stworzone na ekran dotykowy: łącznie {n}. Bez reklam, nic "
              "zablokowanego: każda gra i każdy poziom są za darmo.",
        h_arcade="ARCADE",
        arcade=["Gry zręcznościowe w pixel arcie na ekran dotykowy ({a}), od gier na jeden palec po platformówki",
                "Twój rekord jest zapisywany, a każda gra czeka w miejscu, w którym ją zostawiono",
                "Działają też pady: do 4 graczy w niektórych grach"],
        h_daily="CODZIENNIE",
        daily=["Seria dni, doświadczenie, poziomy i osiągnięcia oraz twoje postępy w każdej zakładce",
               "Jasny lub ciemny motyw i wyszukiwarka, która znajdzie każdą grę po nazwie"],
        h_fair="ZA DARMO, BEZ REKLAM",
        fair="Wszystkie gry i wszystkie poziomy za darmo: bez reklam, nic zablokowanego, bez konta",
        tip="Podoba ci się? Dobrowolny napiwek w ustawieniach pomaga tworzyć nowe gry",
        more="i inne",
        release="Pierwsze wydanie: gry planszowe, karciane, łamigłówki i gry zręcznościowe w jednej darmowej "
                "aplikacji, łącznie {n}, bez reklam i bez blokad. Graj offline, ze znajomymi na jednym urządzeniu "
                "albo przeciw niemu."),
    "cs": dict(
        short="Deskové a karetní hry, hlavolamy a arkády v jedné aplikaci. Bez reklam, offline.",
        intro="RetroStone spojuje v jedné bezplatné aplikaci deskové a karetní hry a hlavolamy u zeleného stolu "
              "(šachy, go, vrhcáby, mahjong, solitér, belote, bridž, sudoku, malované křížovky…) a arkády v pixel "
              "artu, vytvořené pro dotykovou obrazovku: celkem {n}. Bez reklam, nic zamčeného: každá hra a každá "
              "úroveň je zdarma.",
        h_arcade="ARKÁDY",
        arcade=["Arkády v pixel artu pro dotykovou obrazovku ({a}), od her na jeden prst po plošinovky",
                "Tvůj rekord se ukládá a každá rozehraná hra na tebe počká",
                "Fungují i herní ovladače: až 4 hráči v některých hrách"],
        h_daily="KAŽDÝ DEN",
        daily=["Série dní, zkušenosti, úrovně a úspěchy a tvůj postup v každé záložce",
               "Světlý nebo tmavý motiv a hledání, které najde každou hru podle názvu"],
        h_fair="ZDARMA, BEZ REKLAM",
        fair="Všechny hry a všechny úrovně zdarma: bez reklam, nic zamčeného, bez účtu",
        tip="Líbí se ti? Dobrovolné spropitné v nastavení pomáhá vzniku nových her",
        more="a další",
        release="První vydání: deskové a karetní hry, hlavolamy a arkády v jedné bezplatné aplikaci, celkem {n}, "
                "bez reklam a bez zámků. Hraj offline, s přáteli na jednom zařízení nebo proti němu."),
    "ru": dict(
        short="Настольные и карточные игры, головоломки и аркады в одном приложении без рекламы",
        intro="RetroStone собирает в одном бесплатном приложении настольные и карточные игры и головоломки за "
              "зелёным столом (шахматы, го, нарды, маджонг, пасьянсы, белот, бридж, судоку, японские кроссворды…) "
              "и аркадные игры в пиксель-арте, сделанные для сенсорного экрана: всего {n}. Без рекламы, ничего не "
              "заблокировано: все игры и все уровни бесплатны.",
        h_arcade="АРКАДА",
        arcade=["Аркадные игры в пиксель-арте для сенсорного экрана ({a}), от игр в одно касание до платформеров",
                "Твой рекорд сохраняется, а каждая начатая игра тебя дождётся",
                "Геймпады тоже работают: до 4 игроков в некоторых играх"],
        h_daily="КАЖДЫЙ ДЕНЬ",
        daily=["Серия дней, опыт, уровни и достижения, и твой прогресс в каждой вкладке",
               "Светлая или тёмная тема и поиск, который найдёт любую игру по названию"],
        h_fair="БЕСПЛАТНО, БЕЗ РЕКЛАМЫ",
        fair="Все игры и все уровни бесплатно: без рекламы, ничего не заблокировано, без аккаунта",
        tip="Нравится? Добровольные чаевые в настройках помогают делать новые игры",
        more="и другие",
        release="Первый выпуск: настольные, карточные, аркадные игры и головоломки в одном бесплатном приложении, "
                "всего {n}, без рекламы и без блокировок. Играйте офлайн, с друзьями на одном устройстве или против "
                "него."),
    "uk": dict(
        short="Настільні й карткові ігри, головоломки та аркади в одному застосунку без реклами",
        intro="RetroStone збирає в одному безкоштовному застосунку настільні й карткові ігри та головоломки за "
              "зеленим столом (шахи, го, нарди, маджонг, пасьянси, белот, бридж, судоку, японські кросворди…) і "
              "аркадні ігри в піксель-арті, створені для сенсорного екрана: усього {n}. Без реклами, нічого не "
              "заблоковано: усі ігри й усі рівні безкоштовні.",
        h_arcade="АРКАДА",
        arcade=["Аркадні ігри в піксель-арті для сенсорного екрана ({a}), від ігор в один дотик до платформерів",
                "Твій рекорд зберігається, а кожна розпочата гра на тебе зачекає",
                "Геймпади теж працюють: до 4 гравців у деяких іграх"],
        h_daily="ЩОДНЯ",
        daily=["Серія днів, досвід, рівні й досягнення, і твій поступ у кожній вкладці",
               "Світла або темна тема та пошук, що знайде будь-яку гру за назвою"],
        h_fair="БЕЗКОШТОВНО, БЕЗ РЕКЛАМИ",
        fair="Усі ігри й усі рівні безкоштовно: без реклами, нічого не заблоковано, без акаунта",
        tip="Подобається? Добровільні чайові в налаштуваннях допомагають створювати нові ігри",
        more="та інші",
        release="Перший випуск: настільні, карткові, аркадні ігри та головоломки в одному безкоштовному "
                "застосунку, усього {n}, без реклами й без блокувань. Грайте офлайн, із друзями на одному пристрої "
                "або проти нього."),
    "el": dict(
        short="Επιτραπέζια, τράπουλα, γρίφοι και arcade: {n} παιχνίδια χωρίς διαφημίσεις.",
        intro="Το RetroStone συγκεντρώνει {n} παιχνίδια σε μία δωρεάν εφαρμογή: {nb} επιτραπέζια, παιχνίδια με "
              "τράπουλα και γρίφους γύρω από ένα πράσινο τραπέζι (σκάκι, γκο, τάβλι, μαζόνγκ, πασιέντζα, μπελότ, "
              "μπριτζ, σουντόκου, νονόγκραμ…) και {a} παιχνίδια arcade σε pixel art, φτιαγμένα για την οθόνη αφής. "
              "Χωρίς διαφημίσεις, τίποτα κλειδωμένο: κάθε παιχνίδι και κάθε επίπεδο είναι δωρεάν.",
        h_arcade="ΤΟ ARCADE",
        arcade=["{a} παιχνίδια arcade σε pixel art για την οθόνη αφής, από παιχνίδια με ένα δάχτυλο ως πλατφόρμες",
                "Το ρεκόρ σου κρατιέται, και κάθε παιχνίδι σε περιμένει εκεί που το άφησες",
                "Δουλεύουν και χειριστήρια: έως 4 παίκτες σε κάποια παιχνίδια"],
        h_daily="ΚΑΘΕ ΜΕΡΑ",
        daily=["Σερί ημερών, εμπειρία, επίπεδα και επιτεύγματα, και η πρόοδός σου σε κάθε καρτέλα",
               "Ανοιχτό ή σκούρο θέμα, και αναζήτηση για να βρεις κάθε παιχνίδι με το όνομά του"],
        h_fair="ΔΩΡΕΑΝ, ΧΩΡΙΣ ΔΙΑΦΗΜΙΣΕΙΣ",
        fair="Όλα τα παιχνίδια και όλα τα επίπεδα δωρεάν: χωρίς διαφημίσεις, τίποτα κλειδωμένο, χωρίς λογαριασμό",
        tip="Σου αρέσει; Ένα προαιρετικό φιλοδώρημα στις ρυθμίσεις βοηθά να φτιάχνονται νέα παιχνίδια",
        more="και {k} ακόμη",
        release="Πρώτη έκδοση: {n} επιτραπέζια, παιχνίδια με τράπουλα, γρίφοι και arcade σε μία δωρεάν εφαρμογή, "
                "χωρίς διαφημίσεις και χωρίς τίποτα κλειδωμένο. Παίξε χωρίς σύνδεση, με φίλους σε μία συσκευή ή "
                "κόντρα σε αυτήν."),
    "tr": dict(
        short="Masa, kart, bulmaca ve arcade oyunları: {n} oyun, reklamsız ve çevrimdışı.",
        intro="RetroStone, {n} oyunu tek bir ücretsiz uygulamada toplar: yeşil bir masanın etrafında {nb} masa, kart "
              "ve bulmaca oyunu (satranç, go, tavla, mahjong, solitaire, belot, briç, sudoku, nonogram…) ve "
              "dokunmatik ekran için yapılmış {a} piksel sanatı arcade oyunu. Reklam yok, kilitli hiçbir şey yok: her "
              "oyun ve her seviye ücretsiz.",
        h_arcade="ARCADE",
        arcade=["Dokunmatik ekran için yapılmış {a} piksel sanatı arcade oyunu: tek parmakla oynananlardan platform "
                "oyunlarına",
                "En yüksek skorun saklanır, her oyun kaldığın yerde seni bekler",
                "Oyun kumandaları da çalışır: bazı oyunlarda 4 oyuncuya kadar"],
        h_daily="HER GÜN",
        daily=["Günlük seri, deneyim puanı, seviyeler ve başarımlar, her sekmede ilerlemen",
               "Açık veya koyu tema ve her oyunu adıyla bulan bir arama"],
        h_fair="ÜCRETSİZ, REKLAMSIZ",
        fair="Tüm oyunlar ve tüm seviyeler ücretsiz: reklam yok, kilit yok, hesap yok",
        tip="Beğendin mi? Ayarlardaki isteğe bağlı bahşiş yeni oyunların yapılmasına yardım eder",
        more="ve {k} oyun daha",
        release="İlk sürüm: Tek ücretsiz uygulamada {n} masa, kart, bulmaca ve arcade oyunu; reklam yok, kilitli "
                "hiçbir şey yok. Tek cihazda arkadaşlarınla ya da cihaza karşı çevrimdışı oyna."),
    "id": dict(
        short="Game papan, kartu, teka-teki, dan arcade: {n} game tanpa iklan, bisa offline.",
        intro="RetroStone mengumpulkan {n} game dalam satu aplikasi gratis: {nb} game papan, kartu, dan teka-teki "
              "di sekitar meja hijau (catur, go, backgammon, mahjong, solitaire, belote, bridge, sudoku, nonogram…) "
              "dan {a} game arcade bergaya pixel art yang dibuat untuk layar sentuh. Tanpa iklan, tanpa yang "
              "terkunci: setiap game dan setiap level gratis.",
        h_arcade="ARCADE",
        arcade=["{a} game arcade pixel art untuk layar sentuh, dari game satu jari sampai game platform",
                "Skor terbaikmu disimpan, dan setiap game menunggu di tempat kamu berhenti",
                "Gamepad juga bisa: sampai 4 pemain di beberapa game"],
        h_daily="SETIAP HARI",
        daily=["Rentetan harian, poin pengalaman, level dan pencapaian, serta kemajuanmu di setiap tab",
               "Tema terang atau gelap, dan pencarian untuk menemukan game apa pun lewat namanya"],
        h_fair="GRATIS, TANPA IKLAN",
        fair="Semua game dan semua level gratis: tanpa iklan, tanpa yang terkunci, tanpa akun",
        tip="Suka? Tip sukarela di pengaturan membantu membuat game baru",
        more="dan {k} lainnya",
        release="Rilis pertama: {n} game papan, kartu, teka-teki, dan arcade dalam satu aplikasi gratis, tanpa "
                "iklan dan tanpa yang terkunci. Main offline, bersama teman di satu perangkat atau melawannya."),
    "ja": dict(
        short="ボードゲーム、カード、パズル、アーケード：{n}種のゲームを広告なし・オフラインで。",
        intro="RetroStoneは{n}種のゲームをひとつの無料アプリに。緑のテーブルを囲むボードゲーム・カードゲーム・"
              "パズルが{nb}種（チェス、囲碁、バックギャモン、麻雀、ソリティア、ブロット、ブリッジ、ナンバープレイス、"
              "お絵かきロジック…）、そしてタッチ画面のために作ったドット絵のアーケードゲームが{a}種。広告なし、"
              "ロックなし：すべてのゲームとすべてのレベルが無料です。",
        h_arcade="アーケード",
        arcade=["タッチ画面向けのドット絵アーケードゲーム{a}種：指一本で遊べるゲームからアクションゲームまで",
                "ハイスコアは保存され、中断したゲームはそのまま続きから",
                "ゲームパッドにも対応：一部のゲームは最大4人で"],
        h_daily="毎日の楽しみ",
        daily=["連続プレイ日数、経験値、レベル、実績、そして各タブの進み具合",
               "ライト／ダークテーマ、ゲーム名で探せる検索"],
        h_fair="無料、広告なし",
        fair="すべてのゲームとすべてのレベルが無料：広告なし、ロックなし、アカウント不要",
        tip="気に入ったら、設定画面から任意のチップで新しいゲーム作りを応援できます",
        more="ほか{k}種",
        release="初回リリース：ボードゲーム・カード・パズル・アーケードの{n}種をひとつの無料アプリに。広告なし、"
                "ロックなし。オフラインで、1台の端末で友だちと、または端末と対戦。"),
    "ko": dict(
        short="보드게임, 카드, 퍼즐, 아케이드: {n}종의 게임을 광고 없이 오프라인으로.",
        intro="RetroStone은 {n}종의 게임을 하나의 무료 앱에 담았습니다. 초록 테이블에 둘러앉아 즐기는 보드게임·카드 "
              "게임·퍼즐 {nb}종(체스, 바둑, 백개먼, 마작, 솔리테어, 블로트, 브리지, 스도쿠, 노노그램…)과 "
              "터치스크린을 위해 만든 픽셀 아트 아케이드 게임 {a}종. 광고 없음, 잠금 없음: 모든 게임과 모든 레벨이 "
              "무료입니다.",
        h_arcade="아케이드",
        arcade=["터치스크린용 픽셀 아트 아케이드 게임 {a}종: 한 손가락 게임부터 플랫폼 게임까지",
                "최고 기록이 저장되고, 하던 게임은 멈춘 곳에서 기다립니다",
                "게임패드도 지원: 일부 게임은 최대 4인"],
        h_daily="매일",
        daily=["연속 플레이 일수, 경험치, 레벨과 업적, 탭마다의 진행도",
               "라이트/다크 테마, 이름으로 게임을 찾는 검색"],
        h_fair="무료, 광고 없음",
        fair="모든 게임과 모든 레벨 무료: 광고 없음, 잠금 없음, 계정 없음",
        tip="마음에 드셨나요? 설정에서 선택적으로 팁을 보내 새 게임 제작을 응원할 수 있습니다",
        more="외 {k}종",
        release="첫 출시: 보드게임·카드·퍼즐·아케이드 게임 {n}종을 하나의 무료 앱에. 광고 없음, 잠금 없음. "
                "오프라인으로, 기기 하나로 친구와 함께 또는 기기와 대결."),
    "zh_CN": dict(
        short="棋类、纸牌、益智和街机游戏：{n} 款游戏，无广告，可离线游玩。",
        intro="RetroStone 把 {n} 款游戏装进一个免费应用：围绕绿色牌桌的 {nb} 款棋类、纸牌和益智游戏（国际象棋、围棋、"
              "双陆棋、麻将、纸牌接龙、勃洛特、桥牌、数独、数织……），以及为触屏打造的 {a} 款像素风街机游戏。无广告，"
              "无锁定：所有游戏和所有关卡全部免费。",
        h_arcade="街机",
        arcade=["{a} 款为触屏打造的像素风街机游戏，从单指小游戏到平台跳跃",
                "保存你的最高分，每局游戏都在你离开的地方等你",
                "也支持手柄：部分游戏最多 4 人同玩"],
        h_daily="每一天",
        daily=["连续天数、经验值、等级和成就，以及每个分类的进度",
               "浅色或深色主题，还能按名称搜索任意游戏"],
        h_fair="免费，无广告",
        fair="所有游戏和所有关卡免费：无广告，无锁定，无需账号",
        tip="喜欢吗？可在设置中自愿打赏，支持我们制作新游戏",
        more="等另外 {k} 款",
        release="首次发布：{n} 款棋类、纸牌、益智和街机游戏，尽在一个免费应用，无广告，无锁定。可离线游玩，在同一台"
                "设备上与朋友同玩，或与设备对战。"),
}


# the free lines' open source one (the source: SOURCE_URL, the Settings page's Star on GitHub)
OPEN = {
    "en": "Open source: the whole code is on GitHub, free to read",
    "fr": "Open source : tout le code est sur GitHub, libre à lire",
    "de": "Open Source: der ganze Code liegt offen auf GitHub",
    "es": "Código abierto: todo el código está en GitHub, libre para leer",
    "it": "Open source: tutto il codice è su GitHub, libero da leggere",
    "pt_BR": "Código aberto: todo o código está no GitHub, livre para ler",
    "pt_PT": "Código aberto: todo o código está no GitHub, livre para ler",
    "nl": "Open source: alle code staat op GitHub, vrij te lezen",
    "sv": "Öppen källkod: all kod finns på GitHub, fri att läsa",
    "da": "Åben kildekode: al koden ligger på GitHub, fri at læse",
    "nb": "Åpen kildekode: all koden ligger på GitHub, fri å lese",
    "fi": "Avoin lähdekoodi: koko koodi on GitHubissa vapaasti luettavana",
    "pl": "Otwarte źródło: cały kod jest na GitHubie, do swobodnego wglądu",
    "cs": "Otevřený zdrojový kód: celý kód je volně na GitHubu",
    "ru": "Открытый исходный код: весь код открыт на GitHub",
    "uk": "Відкритий код: увесь код відкрито на GitHub",
    "el": "Ανοιχτός κώδικας: όλος ο κώδικας είναι ελεύθερα στο GitHub",
    "tr": "Açık kaynak: kodun tamamı GitHub'da, herkes okuyabilir",
    "id": "Sumber terbuka: seluruh kodenya ada di GitHub, bebas dibaca",
    "ja": "オープンソース：コードはすべてGitHubで公開",
    "ko": "오픈 소스: 모든 코드를 GitHub에 공개",
    "zh_CN": "开源：全部代码公开在 GitHub 上",
}


def title_for(lang, n):
    c = n // 50 * 50
    for t in TITLES[lang]:
        t = t.format(c=c)
        if len(t) <= 30:
            return t
    return TITLES["en"][0].format(c=c)


def arcade_games(conf):
    """the Arcade tab's games, as tools/gen_arcade.py lists them: (id, name)"""
    hidden = " ".join(conf.get("ARCADE_HIDDEN", "").split())
    return [(g, gc.game_name(g)) for g, locked in gc.list_games({"FREE_GAMES": "all", "HIDDEN_GAMES": hidden})]


def games_section(lang, F, arcade):
    """a bullet a tab, in the menu's order: "Puzzles (52): name · name ... and 42 more"; the Arcade: every name"""
    colon, comma = B.COLON.get(lang, ": "), B.COMMA.get(lang, " · ")
    tabs = B.tab_names(lang)
    out = []
    for sec in SHOWN:
        label = tabs[SECTIONS.index(sec)]
        if sec == "ARCADE":
            names = sorted((nm for g, nm in arcade), key=B.sort_key)
            out.append(f"• {label} ({len(arcade)}){colon}{comma.join(names)}")
            continue
        ids = [g for g in F["ids"] if F["sec"][g] == sec]
        shown = [g for g in FEATURED.get(sec, sorted(ids, key=lambda g: B.sort_key(B.name(lang, g)))) if g in ids]
        line = f"• {label} ({len(ids)}){colon}{comma.join(B.name(lang, g) for g in shown)}"
        if len(ids) > len(shown):
            line += (comma if lang in ("ja", "zh_CN") else " ") + \
                T[lang]["more"].format(k=len(ids) - len(shown), t=len(ids))
        out.append(line)
    return out


def full_description(lang, F, arcade, fmt):
    t, b = T[lang], B.T[lang]
    out = [t["intro"].format(**fmt), "", b["h_games"]] + games_section(lang, F, arcade)
    out += ["", t["h_arcade"]] + ["• " + s.format(**fmt) for s in t["arcade"]]
    out += ["", b["h_play"], "• " + b["friends"], "• " + b["bots_board"].format(**fmt), "• " + b["f2f"],
            "• " + b["hidden"], "• " + b["tools"]]
    out += ["", b["h_levels"]] + ["• " + s.format(**fmt) for s in b["levels_board"]]
    out += ["", t["h_daily"]] + ["• " + s for s in t["daily"]]
    out += ["", t["h_fair"], "• " + t["fair"], "• " + OPEN[lang]] + ["• " + s for s in b["fair_board"][1:]]
    if fmt["tips"]:
        out.append("• " + t["tip"])
    out += ["", b["credit"]]
    return "\n".join(out) + "\n"


def facts():
    ids = B.game_ids()
    sec = B.sections()
    lv = {g: B.levels(g) for g in ids}
    multi = [g for g in ids if not B.solo(g)]
    bots = [g for g in multi if os.path.exists(os.path.join(B.BG, "src", "games", g + "_bot.c"))]
    for s, feat in FEATURED.items():
        wrong = [g for g in feat if sec.get(g, "STRATEGY") != s]
        if wrong:
            sys.exit(f"FEATURED[{s}]: not in that tab: {wrong}")
    return dict(ids=ids, sec={g: sec.get(g, "STRATEGY") for g in ids}, levels=lv, multi=multi, bots=bots)


def write_release_notes(version, notes):
    """retrostone/release-notes-<version>.txt: <locale> ... </locale>, en-GB first, then the Play Console's locales"""
    rows = [("en-GB", notes["en"])] + sorted((LOCALES[lang], notes[lang]) for lang in LOCALES)
    with open(os.path.join(STORE, APP, f"release-notes-{version}.txt"), "w", encoding="utf-8") as fh:
        fh.write("".join(f"<{loc}>\n{text}\n</{loc}>\n" for loc, text in rows))


def write_products(conf):
    """retrostone/in-app-products.txt: the tips (TIP_PRODUCTS, in tier order) as the Play Console's one-time products
    ask for them, in every language: the name (55 at most) and the description (200 at most) are the Settings page's
    own words (help/<lang>/_ui.txt: the tip panel's line and each tier's name, from TIP_UI on). Returns the problems."""
    ids = conf.get("TIP_PRODUCTS", "").split()
    if not ids:
        return []
    problems = []
    out = [f"RetroStone: the tips, {len(ids)} consumable one-time products (Play Console > Monetize > Products > "
           f"One-time products). Each language: the product's name, then its description. The prices: "
           f"CHECKLIST-retrostone.md.\n"]
    for k, pid in enumerate(ids):
        out.append(f"######## {pid}\n")
        for lang in LOCALES:
            ui = [l.rstrip("\n") for l in open(os.path.join(B.BG, "help", lang, "_ui.txt"), encoding="utf-8-sig")]
            if TIP_UI + 2 + k >= len(ui) or ui[TIP_UI + 2 + k] == ui[TIP_UI + 5]:
                sys.exit(f"help/{lang}/_ui.txt: no name for the tip tier {k + 1} ({pid})")
            name, desc = ui[TIP_UI + 2 + k], ui[TIP_UI + 1]
            out.append(f"{LOCALES[lang]}\n  name: {name}\n  description: {desc}\n")
            if len(name) > 55 or len(desc) > 200:
                problems.append(f"{pid} {LOCALES[lang]}: the name or the description is too long")
    with open(os.path.join(STORE, APP, "in-app-products.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(out))
    return problems


# the tip panel's first line in help/<lang>/_ui.txt (0-based: "Leave a tip", the panel's line, then the tiers' names,
# then the thanks): pages.c's U_* order
TIP_UI = 42


def main():
    conf = B.conf(CONF)
    F = facts()
    arcade = arcade_games(conf)
    nb, a = len(F["ids"]), len(arcade)
    with_levels = [g for g in F["ids"] if F["levels"][g]]
    total = sum(F["levels"].values())
    print(f"{nb + a} games: {nb} board, card and puzzle games "
          f"({', '.join(f'{s} {sum(1 for g in F['ids'] if F['sec'][g] == s)}' for s in SHOWN[:-1])}), {a} arcade; "
          f"{len(F['multi'])} for two or more ({len(F['bots'])} with a bot), {len(with_levels)} with levels "
          f"({total} levels)")
    problems, entries, notes = [], [], {}
    for lang in LOCALES:
        fmt = dict(n=nb + a, nb=nb, a=a, b=len(F["bots"]), mp=len(F["multi"]), L=len(with_levels),
                   lv=B.thousands(lang, total // 1000 * 1000), tips=bool(conf.get("TIP_PRODUCTS")))
        title = title_for(lang, nb + a)
        short = T[lang]["short"].format(**fmt)
        full_d = full_description(lang, F, arcade, fmt)
        notes[lang] = T[lang]["release"].format(**fmt)
        for field, text, most in (("title", title, 30), ("short", short, 80), ("full", full_d, 4000),
                                  ("release notes", notes[lang], 500)):
            if len(text.encode("utf-16-le")) // 2 > most:
                problems.append(f"{LOCALES[lang]}: {field} too long ({len(text)} > {most})")
            for tm in B.TRADEMARKS + B.BY_LANG.get(lang, []):
                if re.search(tm, text.casefold()):
                    problems.append(f"{LOCALES[lang]}: {field}: a trademark: {tm}")
            for tm in B.CASED:
                if re.search(tm, text):
                    problems.append(f"{LOCALES[lang]}: {field}: a trademark: {tm}")
        entries.append((lang, title, short, full_d))
    problems += write_listings(os.path.join(STORE, APP), APP, title_for("en", nb + a), entries)
    write_release_notes(conf["APP_VERSION_NAME"], notes)
    problems += write_products(conf)
    for p in problems:
        print("PROBLEM", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
