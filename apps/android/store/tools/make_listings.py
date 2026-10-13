#!/usr/bin/env python3
"""The Google Play store listings of both apps, in the 22 languages: apps/android/store/<free|full>/listings/<locale>/
title.txt, short_description.txt (80 characters at most) and full_description.txt (4000 at most): the fields of the
Play Console's "Main store listing" (and fastlane's layout, fastlane/metadata/android/<locale>/, if it is ever used).

    python3 apps/android/store/tools/make_listings.py

The texts are here, in one place. English is the source; French is written with "tu"; the 20 other languages were
translated by an AI (Claude) and need a native speaker's review (apps/android/store/CHECKLIST.md). The game names and
the app names are never translated. The game count and the list come from the apps' configs (gen_catalog.py).
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.dirname(HERE)
ROOT = os.path.abspath(os.path.join(STORE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "apps", "launcher", "tools"))
import gen_catalog as gc  # noqa: E402

# the Play Console's language codes
LOCALES = {"en": "en-US", "cs": "cs-CZ", "da": "da-DK", "de": "de-DE", "el": "el-GR", "es": "es-ES", "fi": "fi-FI",
           "fr": "fr-FR", "id": "id", "it": "it-IT", "ja": "ja-JP", "ko": "ko-KR", "nb": "no-NO", "nl": "nl-NL",
           "pl": "pl-PL", "pt_BR": "pt-BR", "pt_PT": "pt-PT", "ru": "ru-RU", "sv": "sv-SE", "tr": "tr-TR", "uk": "uk",
           "zh_CN": "zh-CN"}

# the languages' names as the Play Console lists them (listings-all-languages.txt)
LANGUAGE_NAMES = {"en": "English (United States)", "cs": "Czech", "da": "Danish", "de": "German", "el": "Greek",
                  "es": "Spanish (Spain)", "fi": "Finnish", "fr": "French (France)", "id": "Indonesian",
                  "it": "Italian", "ja": "Japanese", "ko": "Korean", "nb": "Norwegian", "nl": "Dutch",
                  "pl": "Polish", "pt_BR": "Portuguese (Brazil)", "pt_PT": "Portuguese (Portugal)",
                  "ru": "Russian", "sv": "Swedish", "tr": "Turkish", "uk": "Ukrainian",
                  "zh_CN": "Chinese (Simplified)"}

# the store titles (the Play Console's "App name", 30 characters at most): "RetroStone" (a brand, never translated) and
# what people search ("offline games"); (full app, free app). A language without its own (too long: Greek, ...) uses
# the English one. The name under the phone's icon stays the configs' APP_NAME.
TITLES = {
    "en": ("RetroStone: {n} Offline Games", "RetroStone: Offline Games Lite"),
    "fr": ("RetroStone: {n} jeux hors ligne", None),
    "de": ("RetroStone: {n} Offline-Spiele", None),
    "es": ("RetroStone: {n} juegos offline", None),
    "it": ("RetroStone: {n} giochi offline", None),
    "pt_BR": ("RetroStone: {n} jogos offline", "RetroStone: jogos offline Lite"),
    "pt_PT": ("RetroStone: {n} jogos offline", "RetroStone: jogos offline Lite"),
    "nl": ("RetroStone: {n} offline games", "RetroStone: offline games Lite"),
    "sv": ("RetroStone: {n} offlinespel", "RetroStone: offlinespel Lite"),
    "da": ("RetroStone: {n} offline-spil", "RetroStone: offline-spil Lite"),
    "nb": ("RetroStone: {n} offline-spill", "RetroStone: offline-spill Lite"),
    "fi": ("RetroStone: {n} offline-peliä", "RetroStone: offline-pelit Lite"),
    "pl": ("RetroStone: {n} gier offline", "RetroStone: gry offline Lite"),
    "cs": ("RetroStone: {n} offline her", "RetroStone: offline hry Lite"),
    "ru": ("RetroStone: {n} офлайн-игр", "RetroStone: офлайн-игры Lite"),
    "uk": ("RetroStone: {n} офлайн-ігор", "RetroStone: офлайн-ігри Lite"),
    "tr": ("RetroStone: {n} Çevrimdışı Oyun", None),
    "id": ("RetroStone: {n} Game Offline", "RetroStone: Game Offline Lite"),
    "ja": ("RetroStone: オフラインゲーム{n}本", "RetroStone: オフラインゲーム Lite"),
    "ko": ("RetroStone: 오프라인 게임 {n}종", "RetroStone: 오프라인 게임 Lite"),
    "zh_CN": ("RetroStone: {n}款离线游戏", "RetroStone: 离线游戏 Lite"),
}


def title_for(lang, app, n):
    full, free = TITLES.get(lang, (None, None))
    t = (full if app == "full" else free) or TITLES["en"][app == "free"]
    t = t.format(n=n)
    return t if len(t) <= 30 else TITLES["en"][app == "free"].format(n=n)


# one line per game (English and French only; the other languages list the names)
LINES = {
    "en": {
        "beaverrush": "gnaw a giant tree log by log, from the left or the right",
        "biscuitbandit": "catch the cookies falling from the bakery belts, but never the cups",
        "blueberrytumble": "roll down the mountain and jump on the beat of the music",
        "crabcoconut": "bounce a coconut off your claws and break the falling shells",
        "duckparade": "hop across roads and rivers and lead the lost ducklings home",
        "frogreach": "stretch the frog's tongue just far enough to cross the swamp",
        "hedgehoghills": "curl up downhill, let go uphill and fly over the woodland hills",
        "leadysquid": "tap to keep a heavy little squid afloat and swim through the gaps",
        "molemarbles": "split the bouncing gemstones with your harpoon",
        "motharound": "circle the lantern and slip through the closing rings",
        "mouseespresso": "fly a mouse wearing an espresso machine through the cafe",
        "nuttybranch": "run along an endless branch without sliding off its sides",
        "otterrapids": "paddle down the river and catch every golden fish",
        "pancaketower": "drop the pancakes and stack the tallest tower",
        "penguinslalom": "ski between the flags down an endless snowy slope",
        "pogomamie": "chase a runaway cat across the rooftops of Paris on a pogo stick",
        "raccoonrecycler": "sort the toys by kind or by colour while the rules keep changing",
        "radishrascals": "shoo the rabbits from your vegetable beds, spare the hedgehogs",
        "snailswitch": "flip above or below the vine to dodge thorns and caterpillars",
        "spiderswing": "swing from flower to flower and climb ever higher",
        "unicornhop": "hop up from cloud to cloud, one tap at just the right moment",
    },
    "fr": {
        "beaverrush": "ronge l'arbre géant bûche après bûche, à gauche ou à droite",
        "biscuitbandit": "attrape les biscuits qui tombent des tapis de la boulangerie, mais jamais les tasses",
        "blueberrytumble": "dévale la montagne et saute en rythme avec la musique",
        "crabcoconut": "renvoie ta noix de coco avec tes pinces et casse les coquillages",
        "duckparade": "traverse routes et rivières et ramène les canetons perdus à la maison",
        "frogreach": "étire la langue de la grenouille juste assez pour traverser le marais",
        "hedgehoghills": "roule-toi en boule dans les descentes et envole-toi au-dessus des collines",
        "leadysquid": "tape pour garder à flot un petit calmar trop lesté et nager entre les obstacles",
        "molemarbles": "fais éclater les pierres précieuses qui rebondissent avec ton harpon",
        "motharound": "tourne autour de la lanterne et faufile-toi entre les anneaux",
        "mouseespresso": "fais voler une souris équipée d'une machine à expresso à travers le café",
        "nuttybranch": "cours sur une branche sans fin sans glisser sur les côtés",
        "otterrapids": "descends la rivière et attrape tous les poissons dorés",
        "pancaketower": "lâche les crêpes au bon moment et monte la plus haute pile",
        "penguinslalom": "slalome entre les drapeaux sur une pente enneigée sans fin",
        "pogomamie": "poursuis un chat fugueur sur les toits de Paris, en bâton sauteur",
        "raccoonrecycler": "trie les jouets par espèce ou par couleur, alors que les règles changent",
        "radishrascals": "chasse les lapins de ton potager, mais épargne les hérissons",
        "snailswitch": "passe dessus ou dessous la liane pour éviter épines et chenilles",
        "spiderswing": "balance-toi de fleur en fleur et grimpe toujours plus haut",
        "unicornhop": "saute de nuage en nuage vers le ciel, en tapant juste au bon moment",
    },
}

# {n}: the full app's game count, {m}: the games only in the full app, {f}: the free app's game count
T = {
    "en": dict(
        short_free="{f} retro arcade games in 16-bit style, free. No ads, no tracking, plays offline.",
        short_full="{n} retro arcade games in one app. Play offline: no ads, no in-app purchases.",
        intro_free="RetroStone Arcade Free brings you {f} original arcade games made in the spirit of the 16-bit "
                   "consoles: crisp pixel art, catchy music and quick rounds you can pick up in a second.",
        intro_full="RetroStone Arcade gathers {n} original arcade games made in the spirit of the 16-bit consoles: "
                   "crisp pixel art, catchy music and quick rounds you can pick up in a second.",
        h_games="THE GAMES", h_play="PLAY YOUR WAY", h_fair="NO ADS, NO TRACKING",
        play=["Simple touch controls, made for each game",
              "Bluetooth and USB gamepads work too, with up to 4 players in many games",
              "Leave a game at any moment: it waits for you right where you were",
              "Your best scores stay on your phone"],
        fair_free=["No ads, no in-app purchases, no account", "No data collected; it plays offline", "22 languages"],
        fair_full=["One purchase for every game: no ads, no in-app purchases, no account",
                   "No data collected; it plays offline", "22 languages"],
        more="{m} more games are waiting in RetroStone Arcade, the full version: one purchase, no ads. You can see "
             "them all in the menu.",
        credit="Made by 8BCraft. Code under the MIT licence, art and music under CC BY-NC-SA 4.0: see Credits & "
               "licences in the app."),
    "fr": dict(
        short_free="{f} jeux d'arcade rétro style 16 bits, gratuits. Sans pub ni pistage, hors ligne.",
        short_full="{n} jeux d'arcade rétro dans une appli. Hors ligne, sans pub ni achat intégré.",
        intro_free="RetroStone Arcade Free t'offre {f} jeux d'arcade originaux dans l'esprit des consoles 16 bits : "
                   "du pixel art soigné, des musiques entraînantes et des parties courtes que tu prends en main en "
                   "une seconde.",
        intro_full="RetroStone Arcade réunit {n} jeux d'arcade originaux dans l'esprit des consoles 16 bits : du "
                   "pixel art soigné, des musiques entraînantes et des parties courtes que tu prends en main en une "
                   "seconde.",
        h_games="LES JEUX", h_play="JOUE COMME TU VEUX", h_fair="SANS PUB, SANS PISTAGE",
        play=["Des commandes tactiles simples, pensées pour chaque jeu",
              "Les manettes Bluetooth et USB marchent aussi, jusqu'à 4 joueurs dans beaucoup de jeux",
              "Quitte une partie quand tu veux : elle t'attend exactement où tu l'as laissée",
              "Tes meilleurs scores restent sur ton téléphone"],
        fair_free=["Pas de pub, pas d'achat intégré, pas de compte", "Aucune donnée collectée ; ça marche hors ligne",
                   "22 langues"],
        fair_full=["Un seul achat pour tous les jeux : pas de pub, pas d'achat intégré, pas de compte",
                   "Aucune donnée collectée ; ça marche hors ligne", "22 langues"],
        more="{m} autres jeux t'attendent dans RetroStone Arcade, la version complète : un seul achat, pas de pub. "
             "Tu les vois tous dans le menu.",
        credit="Créé par 8BCraft. Code sous licence MIT, graphismes et musiques sous CC BY-NC-SA 4.0 : voir "
               "Crédits et licences dans l'appli."),
    "de": dict(
        short_free="{f} Retro-Arcade-Spiele im 16-Bit-Stil, gratis. Ohne Werbung, ohne Tracking.",
        short_full="{n} Retro-Arcade-Spiele in einer App. Offline spielbar, ohne Werbung und Käufe.",
        intro_free="RetroStone Arcade Free bringt dir {f} originelle Arcade-Spiele im Geist der 16-Bit-Konsolen: "
                   "knackige Pixel-Art, eingängige Musik und kurze Runden, die du sofort verstehst.",
        intro_full="RetroStone Arcade vereint {n} originelle Arcade-Spiele im Geist der 16-Bit-Konsolen: knackige "
                   "Pixel-Art, eingängige Musik und kurze Runden, die du sofort verstehst.",
        h_games="DIE SPIELE", h_play="SPIEL, WIE DU WILLST", h_fair="OHNE WERBUNG, OHNE TRACKING",
        play=["Einfache Touch-Steuerung, für jedes Spiel gemacht",
              "Bluetooth- und USB-Gamepads gehen auch, in vielen Spielen mit bis zu 4 Spielern",
              "Verlass ein Spiel jederzeit: Es wartet genau dort auf dich, wo du warst",
              "Deine Bestwerte bleiben auf deinem Handy"],
        fair_free=["Keine Werbung, keine In-App-Käufe, kein Konto", "Keine Datenerhebung; spielbar offline",
                   "22 Sprachen"],
        fair_full=["Ein Kauf für alle Spiele: keine Werbung, keine In-App-Käufe, kein Konto",
                   "Keine Datenerhebung; spielbar offline", "22 Sprachen"],
        more="{m} weitere Spiele warten in RetroStone Arcade, der Vollversion: ein Kauf, keine Werbung. Im Menü "
             "siehst du sie alle.",
        credit="Von 8BCraft. Code unter der MIT-Lizenz, Grafik und Musik unter CC BY-NC-SA 4.0: siehe "
               "„Mitwirkende & Lizenzen“ in der App."),
    "es": dict(
        short_free="{f} juegos arcade retro con estilo de 16 bits, gratis. Sin anuncios, sin conexión.",
        short_full="{n} juegos arcade retro en una app. Sin conexión, sin anuncios ni compras.",
        intro_free="RetroStone Arcade Free te trae {f} juegos arcade originales con el espíritu de las consolas de "
                   "16 bits: pixel art nítido, música pegadiza y partidas cortas que entiendes en un segundo.",
        intro_full="RetroStone Arcade reúne {n} juegos arcade originales con el espíritu de las consolas de 16 bits: "
                   "pixel art nítido, música pegadiza y partidas cortas que entiendes en un segundo.",
        h_games="LOS JUEGOS", h_play="JUEGA A TU MANERA", h_fair="SIN ANUNCIOS NI RASTREO",
        play=["Controles táctiles sencillos, pensados para cada juego",
              "También funcionan los mandos Bluetooth y USB, con hasta 4 jugadores en muchos juegos",
              "Sal de una partida cuando quieras: te espera justo donde la dejaste",
              "Tus mejores puntuaciones se quedan en tu teléfono"],
        fair_free=["Sin anuncios, sin compras dentro de la app, sin cuenta",
                   "No recoge ningún dato; funciona sin conexión", "22 idiomas"],
        fair_full=["Una sola compra para todos los juegos: sin anuncios, sin compras dentro de la app, sin cuenta",
                   "No recoge ningún dato; funciona sin conexión", "22 idiomas"],
        more="Otros {m} juegos te esperan en RetroStone Arcade, la versión completa: una sola compra, sin anuncios. "
             "Los ves todos en el menú.",
        credit="Creado por 8BCraft. Código con licencia MIT, gráficos y música con CC BY-NC-SA 4.0: mira Créditos y "
               "licencias en la app."),
    "it": dict(
        short_free="{f} giochi arcade retrò in stile 16 bit, gratis. Senza pubblicità, anche offline.",
        short_full="{n} giochi arcade retrò in un'app. Anche offline, senza pubblicità né acquisti.",
        intro_free="RetroStone Arcade Free ti porta {f} giochi arcade originali nello spirito delle console a 16 bit: "
                   "pixel art curata, musiche orecchiabili e partite brevi che capisci in un attimo.",
        intro_full="RetroStone Arcade riunisce {n} giochi arcade originali nello spirito delle console a 16 bit: "
                   "pixel art curata, musiche orecchiabili e partite brevi che capisci in un attimo.",
        h_games="I GIOCHI", h_play="GIOCA COME VUOI", h_fair="NIENTE PUBBLICITÀ, NIENTE TRACCIAMENTO",
        play=["Comandi touch semplici, pensati per ogni gioco",
              "Funzionano anche i controller Bluetooth e USB, fino a 4 giocatori in molti giochi",
              "Esci da una partita quando vuoi: ti aspetta esattamente dove l'hai lasciata",
              "I tuoi record restano sul tuo telefono"],
        fair_free=["Niente pubblicità, niente acquisti in-app, niente account",
                   "Nessun dato raccolto; funziona offline", "22 lingue"],
        fair_full=["Un solo acquisto per tutti i giochi: niente pubblicità, niente acquisti in-app, niente account",
                   "Nessun dato raccolto; funziona offline", "22 lingue"],
        more="Altri {m} giochi ti aspettano in RetroStone Arcade, la versione completa: un solo acquisto, niente "
             "pubblicità. Li vedi tutti nel menu.",
        credit="Creato da 8BCraft. Codice con licenza MIT, grafica e musica con CC BY-NC-SA 4.0: vedi Crediti e "
               "licenze nell'app."),
    "pt_BR": dict(
        short_free="{f} jogos de fliperama retrô em estilo 16 bits, grátis. Sem anúncios, offline.",
        short_full="{n} jogos de fliperama retrô num app. Jogue offline, sem anúncios nem compras.",
        intro_free="RetroStone Arcade Free traz {f} jogos de fliperama originais no espírito dos consoles de 16 bits: "
                   "pixel art caprichada, músicas que grudam e partidas rápidas que você entende num segundo.",
        intro_full="RetroStone Arcade reúne {n} jogos de fliperama originais no espírito dos consoles de 16 bits: "
                   "pixel art caprichada, músicas que grudam e partidas rápidas que você entende num segundo.",
        h_games="OS JOGOS", h_play="JOGUE DO SEU JEITO", h_fair="SEM ANÚNCIOS, SEM RASTREAMENTO",
        play=["Controles de toque simples, feitos para cada jogo",
              "Controles Bluetooth e USB também funcionam, com até 4 jogadores em muitos jogos",
              "Saia de uma partida quando quiser: ela espera você exatamente onde parou",
              "Seus recordes ficam no seu celular"],
        fair_free=["Sem anúncios, sem compras no app, sem conta", "Nenhum dado coletado; funciona offline",
                   "22 idiomas"],
        fair_full=["Uma compra para todos os jogos: sem anúncios, sem compras no app, sem conta",
                   "Nenhum dado coletado; funciona offline", "22 idiomas"],
        more="Mais {m} jogos esperam por você no RetroStone Arcade, a versão completa: uma compra só, sem anúncios. "
             "Você vê todos eles no menu.",
        credit="Feito por 8BCraft. Código sob a licença MIT, arte e música sob CC BY-NC-SA 4.0: veja Créditos e "
               "licenças no app."),
    "pt_PT": dict(
        short_free="{f} jogos arcade retro em estilo 16 bits, grátis. Sem anúncios, joga offline.",
        short_full="{n} jogos arcade retro numa app. Joga offline, sem anúncios nem compras.",
        intro_free="O RetroStone Arcade Free traz-te {f} jogos arcade originais no espírito das consolas de 16 bits: "
                   "pixel art cuidada, músicas que ficam no ouvido e partidas curtas que percebes num segundo.",
        intro_full="O RetroStone Arcade reúne {n} jogos arcade originais no espírito das consolas de 16 bits: pixel "
                   "art cuidada, músicas que ficam no ouvido e partidas curtas que percebes num segundo.",
        h_games="OS JOGOS", h_play="JOGA À TUA MANEIRA", h_fair="SEM ANÚNCIOS, SEM RASTREIO",
        play=["Controlos tácteis simples, pensados para cada jogo",
              "Os comandos Bluetooth e USB também funcionam, com até 4 jogadores em muitos jogos",
              "Sai de uma partida quando quiseres: ela espera por ti exatamente onde ficaste",
              "Os teus recordes ficam no teu telemóvel"],
        fair_free=["Sem anúncios, sem compras na app, sem conta", "Nenhum dado recolhido; funciona offline",
                   "22 idiomas"],
        fair_full=["Uma compra para todos os jogos: sem anúncios, sem compras na app, sem conta",
                   "Nenhum dado recolhido; funciona offline", "22 idiomas"],
        more="Mais {m} jogos esperam por ti no RetroStone Arcade, a versão completa: uma só compra, sem anúncios. "
             "Vês todos no menu.",
        credit="Feito por 8BCraft. Código sob a licença MIT, arte e música sob CC BY-NC-SA 4.0: vê Créditos e "
               "licenças na app."),
    "nl": dict(
        short_free="{f} retro-arcadegames in 16-bitstijl, gratis. Geen advertenties, speelt offline.",
        short_full="{n} retro-arcadegames in één app. Speel offline, zonder advertenties of aankopen.",
        intro_free="RetroStone Arcade Free brengt je {f} originele arcadegames in de geest van de 16-bitconsoles: "
                   "strakke pixelart, pakkende muziek en korte potjes die je meteen snapt.",
        intro_full="RetroStone Arcade bundelt {n} originele arcadegames in de geest van de 16-bitconsoles: strakke "
                   "pixelart, pakkende muziek en korte potjes die je meteen snapt.",
        h_games="DE GAMES", h_play="SPEEL ZOALS JIJ WILT", h_fair="GEEN ADVERTENTIES, GEEN TRACKING",
        play=["Eenvoudige aanraakbediening, gemaakt voor elke game",
              "Bluetooth- en USB-controllers werken ook, met tot 4 spelers in veel games",
              "Stop wanneer je wilt: de game wacht precies waar je was",
              "Je beste scores blijven op je telefoon"],
        fair_free=["Geen advertenties, geen in-app-aankopen, geen account",
                   "Er worden geen gegevens verzameld; speelt offline", "22 talen"],
        fair_full=["Eén aankoop voor alle games: geen advertenties, geen in-app-aankopen, geen account",
                   "Er worden geen gegevens verzameld; speelt offline", "22 talen"],
        more="Nog {m} games wachten op je in RetroStone Arcade, de volledige versie: één aankoop, geen "
             "advertenties. Je ziet ze allemaal in het menu.",
        credit="Gemaakt door 8BCraft. Code onder de MIT-licentie, beeld en muziek onder CC BY-NC-SA 4.0: zie "
               "Credits en licenties in de app."),
    "sv": dict(
        short_free="{f} retroarkadspel i 16-bitarsstil, gratis. Ingen reklam, ingen spårning, offline.",
        short_full="{n} retroarkadspel i en app. Spela offline, ingen reklam, inga köp i appen.",
        intro_free="RetroStone Arcade Free ger dig {f} originella arkadspel i 16-bitarskonsolernas anda: skarp "
                   "pixelgrafik, medryckande musik och korta omgångar som du lär dig på en sekund.",
        intro_full="RetroStone Arcade samlar {n} originella arkadspel i 16-bitarskonsolernas anda: skarp "
                   "pixelgrafik, medryckande musik och korta omgångar som du lär dig på en sekund.",
        h_games="SPELEN", h_play="SPELA SOM DU VILL", h_fair="INGEN REKLAM, INGEN SPÅRNING",
        play=["Enkla pekkontroller, gjorda för varje spel",
              "Bluetooth- och USB-handkontroller fungerar också, med upp till 4 spelare i många spel",
              "Lämna ett spel när du vill: det väntar precis där du var",
              "Dina rekord stannar i din telefon"],
        fair_free=["Ingen reklam, inga köp i appen, inget konto", "Ingen data samlas in; spelas offline",
                   "22 språk"],
        fair_full=["Ett köp för alla spel: ingen reklam, inga köp i appen, inget konto",
                   "Ingen data samlas in; spelas offline", "22 språk"],
        more="{m} spel till väntar i RetroStone Arcade, den fullständiga versionen: ett köp, ingen reklam. Du ser "
             "alla i menyn.",
        credit="Gjort av 8BCraft. Kod under MIT-licensen, grafik och musik under CC BY-NC-SA 4.0: se Medverkande "
               "och licenser i appen."),
    "da": dict(
        short_free="{f} retro-arkadespil i 16-bit-stil, gratis. Uden reklamer og sporing, offline.",
        short_full="{n} retro-arkadespil i én app. Spil offline, ingen reklamer, ingen køb i appen.",
        intro_free="RetroStone Arcade Free giver dig {f} originale arkadespil i 16-bit-konsollernes ånd: skarp "
                   "pixelgrafik, fængende musik og korte runder, du forstår på et sekund.",
        intro_full="RetroStone Arcade samler {n} originale arkadespil i 16-bit-konsollernes ånd: skarp pixelgrafik, "
                   "fængende musik og korte runder, du forstår på et sekund.",
        h_games="SPILLENE", h_play="SPIL, SOM DU VIL", h_fair="INGEN REKLAMER, INGEN SPORING",
        play=["Enkel touchstyring, lavet til hvert spil",
              "Bluetooth- og USB-controllere virker også, med op til 4 spillere i mange spil",
              "Forlad et spil, når du vil: det venter præcis, hvor du var",
              "Dine rekorder bliver på din telefon"],
        fair_free=["Ingen reklamer, ingen køb i appen, ingen konto", "Ingen data indsamles; spilles offline",
                   "22 sprog"],
        fair_full=["Ét køb til alle spil: ingen reklamer, ingen køb i appen, ingen konto",
                   "Ingen data indsamles; spilles offline", "22 sprog"],
        more="{m} spil mere venter i RetroStone Arcade, den fulde version: ét køb, ingen reklamer. Du kan se dem "
             "alle i menuen.",
        credit="Lavet af 8BCraft. Kode under MIT-licensen, grafik og musik under CC BY-NC-SA 4.0: se Krediter og "
               "licenser i appen."),
    "nb": dict(
        short_free="{f} retro-arkadespill i 16-biters stil, gratis. Uten reklame og sporing, offline.",
        short_full="{n} retro-arkadespill i én app. Spill offline, uten reklame og kjøp i appen.",
        intro_free="RetroStone Arcade Free gir deg {f} originale arkadespill i 16-bitskonsollenes ånd: skarp "
                   "pikselgrafikk, fengende musikk og korte runder du skjønner på et sekund.",
        intro_full="RetroStone Arcade samler {n} originale arkadespill i 16-bitskonsollenes ånd: skarp "
                   "pikselgrafikk, fengende musikk og korte runder du skjønner på et sekund.",
        h_games="SPILLENE", h_play="SPILL SOM DU VIL", h_fair="INGEN REKLAME, INGEN SPORING",
        play=["Enkel berøringsstyring, laget for hvert spill",
              "Bluetooth- og USB-kontrollere virker også, med opptil 4 spillere i mange spill",
              "Gå ut av et spill når du vil: det venter akkurat der du var",
              "Rekordene dine blir på telefonen din"],
        fair_free=["Ingen reklame, ingen kjøp i appen, ingen konto", "Ingen data samles inn; spilles offline",
                   "22 språk"],
        fair_full=["Ett kjøp for alle spillene: ingen reklame, ingen kjøp i appen, ingen konto",
                   "Ingen data samles inn; spilles offline", "22 språk"],
        more="{m} spill til venter i RetroStone Arcade, fullversjonen: ett kjøp, ingen reklame. Du ser alle i "
             "menyen.",
        credit="Laget av 8BCraft. Kode under MIT-lisensen, grafikk og musikk under CC BY-NC-SA 4.0: se Kreditter "
               "og lisenser i appen."),
    "fi": dict(
        short_free="{f} retrohenkistä arcade-peliä 16-bittisellä tyylillä, ilmaiseksi. Ei mainoksia.",
        short_full="{n} retro-arcade-peliä yhdessä sovelluksessa. Pelaa offline, ei mainoksia.",
        intro_free="RetroStone Arcade Free tuo sinulle {f} alkuperäistä arcade-peliä 16-bittisten konsolien "
                   "hengessä: terävää pikseligrafiikkaa, tarttuvaa musiikkia ja lyhyitä pelejä, jotka opit hetkessä.",
        intro_full="RetroStone Arcade kokoaa yhteen {n} alkuperäistä arcade-peliä 16-bittisten konsolien hengessä: "
                   "terävää pikseligrafiikkaa, tarttuvaa musiikkia ja lyhyitä pelejä, jotka opit hetkessä.",
        h_games="PELIT", h_play="PELAA OMALLA TAVALLASI", h_fair="EI MAINOKSIA, EI SEURANTAA",
        play=["Helpot kosketusohjaimet, jokaiselle pelille omansa",
              "Myös Bluetooth- ja USB-ohjaimet toimivat, monessa pelissä jopa 4 pelaajaa",
              "Lopeta milloin vain: peli odottaa juuri siinä, mihin jäit",
              "Ennätyksesi pysyvät puhelimessasi"],
        fair_free=["Ei mainoksia, ei sovelluksen sisäisiä ostoja, ei tiliä",
                   "Tietoja ei kerätä; toimii ilman verkkoa", "22 kieltä"],
        fair_full=["Yksi osto kaikille peleille: ei mainoksia, ei sovelluksen sisäisiä ostoja, ei tiliä",
                   "Tietoja ei kerätä; toimii ilman verkkoa", "22 kieltä"],
        more="Vielä {m} peliä odottaa RetroStone Arcadessa, täydessä versiossa: yksi osto, ei mainoksia. Näet ne "
             "kaikki valikossa.",
        credit="Tekijä 8BCraft. Koodi MIT-lisenssillä, grafiikka ja musiikki CC BY-NC-SA 4.0 -lisenssillä: katso "
               "Tekijät ja lisenssit sovelluksessa."),
    "pl": dict(
        short_free="{f} gry zręcznościowe retro w stylu 16-bit, za darmo. Bez reklam, działa offline.",
        short_full="{n} gier zręcznościowych retro w jednej aplikacji. Działa offline, bez reklam.",
        intro_free="RetroStone Arcade Free to {f} oryginalne gry zręcznościowe w duchu konsol 16-bitowych: wyraźny "
                   "pixel art, wpadająca w ucho muzyka i krótkie rozgrywki, które zrozumiesz w sekundę.",
        intro_full="RetroStone Arcade zbiera {n} oryginalnych gier zręcznościowych w duchu konsol 16-bitowych: "
                   "wyraźny pixel art, wpadająca w ucho muzyka i krótkie rozgrywki, które zrozumiesz w sekundę.",
        h_games="GRY", h_play="GRAJ PO SWOJEMU", h_fair="BEZ REKLAM, BEZ ŚLEDZENIA",
        play=["Proste sterowanie dotykiem, zaprojektowane dla każdej gry",
              "Działają też pady Bluetooth i USB, w wielu grach nawet dla 4 graczy",
              "Wyjdź z gry, kiedy chcesz: poczeka dokładnie tam, gdzie skończyłeś",
              "Twoje rekordy zostają w telefonie"],
        fair_free=["Bez reklam, bez zakupów w aplikacji, bez konta", "Żadne dane nie są zbierane; działa offline",
                   "22 języki"],
        fair_full=["Jeden zakup, wszystkie gry: bez reklam, bez zakupów w aplikacji, bez konta",
                   "Żadne dane nie są zbierane; działa offline", "22 języki"],
        more="Kolejne gry ({m}) czekają w RetroStone Arcade, pełnej wersji: jeden zakup, bez reklam. Wszystkie "
             "zobaczysz w menu.",
        credit="Autor: 8BCraft. Kod na licencji MIT, grafika i muzyka na licencji CC BY-NC-SA 4.0: zobacz Autorzy "
               "i licencje w aplikacji."),
    "cs": dict(
        short_free="{f} retro arkádové hry ve stylu 16 bitů, zdarma. Bez reklam a sledování, offline.",
        short_full="{n} retro arkádových her v jedné aplikaci. Hraj offline, bez reklam a nákupů.",
        intro_free="RetroStone Arcade Free ti přináší {f} původní arkádové hry v duchu 16bitových konzolí: ostrý "
                   "pixel art, chytlavou hudbu a krátká kola, která pochopíš za vteřinu.",
        intro_full="RetroStone Arcade spojuje {n} původních arkádových her v duchu 16bitových konzolí: ostrý pixel "
                   "art, chytlavou hudbu a krátká kola, která pochopíš za vteřinu.",
        h_games="HRY", h_play="HRAJ PO SVÉM", h_fair="BEZ REKLAM, BEZ SLEDOVÁNÍ",
        play=["Jednoduché dotykové ovládání, navržené pro každou hru",
              "Fungují i ovladače Bluetooth a USB, v mnoha hrách až pro 4 hráče",
              "Odejdi ze hry, kdy chceš: počká přesně tam, kde jsi skončil",
              "Tvé rekordy zůstávají v telefonu"],
        fair_free=["Žádné reklamy, žádné nákupy v aplikaci, žádný účet", "Neshromažďuje žádná data; funguje offline",
                   "22 jazyků"],
        fair_full=["Jeden nákup za všechny hry: žádné reklamy, žádné nákupy v aplikaci, žádný účet",
                   "Neshromažďuje žádná data; funguje offline", "22 jazyků"],
        more="Dalších {m} her čeká v RetroStone Arcade, plné verzi: jeden nákup, žádné reklamy. Všechny uvidíš v "
             "nabídce.",
        credit="Vytvořil 8BCraft. Kód pod licencí MIT, grafika a hudba pod CC BY-NC-SA 4.0: viz Autoři a licence v "
               "aplikaci."),
    "el": dict(
        short_free="{f} ρετρό παιχνίδια arcade σε στιλ 16-bit, δωρεάν. Χωρίς διαφημίσεις, offline.",
        short_full="{n} ρετρό παιχνίδια arcade σε μία εφαρμογή. Παίζει offline, χωρίς διαφημίσεις.",
        intro_free="Το RetroStone Arcade Free σού φέρνει {f} πρωτότυπα παιχνίδια arcade στο πνεύμα των κονσολών "
                   "16-bit: καθαρό pixel art, μουσική που κολλάει και σύντομους γύρους που μαθαίνεις αμέσως.",
        intro_full="Το RetroStone Arcade συγκεντρώνει {n} πρωτότυπα παιχνίδια arcade στο πνεύμα των κονσολών 16-bit: "
                   "καθαρό pixel art, μουσική που κολλάει και σύντομους γύρους που μαθαίνεις αμέσως.",
        h_games="ΤΑ ΠΑΙΧΝΙΔΙΑ", h_play="ΠΑΙΞΕ ΟΠΩΣ ΘΕΛΕΙΣ", h_fair="ΧΩΡΙΣ ΔΙΑΦΗΜΙΣΕΙΣ, ΧΩΡΙΣ ΠΑΡΑΚΟΛΟΥΘΗΣΗ",
        play=["Απλά χειριστήρια αφής, φτιαγμένα για κάθε παιχνίδι",
              "Λειτουργούν και χειριστήρια Bluetooth και USB, με έως 4 παίκτες σε πολλά παιχνίδια",
              "Βγες από ένα παιχνίδι όποτε θέλεις: σε περιμένει ακριβώς εκεί που το άφησες",
              "Τα ρεκόρ σου μένουν στο κινητό σου"],
        fair_free=["Χωρίς διαφημίσεις, χωρίς αγορές εντός εφαρμογής, χωρίς λογαριασμό",
                   "Δεν συλλέγονται δεδομένα· παίζει χωρίς σύνδεση", "22 γλώσσες"],
        fair_full=["Μία αγορά για όλα τα παιχνίδια: χωρίς διαφημίσεις, χωρίς αγορές εντός εφαρμογής, χωρίς "
                   "λογαριασμό", "Δεν συλλέγονται δεδομένα· παίζει χωρίς σύνδεση", "22 γλώσσες"],
        more="Άλλα {m} παιχνίδια σε περιμένουν στο RetroStone Arcade, την πλήρη έκδοση: μία αγορά, χωρίς "
             "διαφημίσεις. Τα βλέπεις όλα στο μενού.",
        credit="Από την 8BCraft. Κώδικας με άδεια MIT, γραφικά και μουσική με CC BY-NC-SA 4.0: δες «Συντελεστές "
               "και άδειες» στην εφαρμογή."),
    "ru": dict(
        short_free="{f} ретро-аркады в стиле 16 бит, бесплатно. Без рекламы и слежки, офлайн.",
        short_full="{n} ретро-аркад в одном приложении. Работает офлайн, без рекламы и покупок.",
        intro_free="RetroStone Arcade Free — это {f} оригинальные аркады в духе 16-битных консолей: чёткий пиксель-арт, "
                   "запоминающаяся музыка и короткие забеги, в которых разберёшься за секунду.",
        intro_full="RetroStone Arcade собрал {n} оригинальных аркад в духе 16-битных консолей: чёткий пиксель-арт, "
                   "запоминающаяся музыка и короткие забеги, в которых разберёшься за секунду.",
        h_games="ИГРЫ", h_play="ИГРАЙ КАК ХОЧЕШЬ", h_fair="БЕЗ РЕКЛАМЫ И СЛЕЖКИ",
        play=["Простое сенсорное управление, продуманное для каждой игры",
              "Работают и геймпады Bluetooth и USB, во многих играх — до 4 игроков",
              "Выходи из игры когда угодно: она подождёт тебя там, где ты остановился",
              "Твои рекорды хранятся на телефоне"],
        fair_free=["Без рекламы, без покупок в приложении, без аккаунта",
                   "Никакие данные не собираются; работает без интернета", "22 языка"],
        fair_full=["Одна покупка — все игры: без рекламы, без покупок в приложении, без аккаунта",
                   "Никакие данные не собираются; работает без интернета", "22 языка"],
        more="Ещё {m} игр ждут тебя в RetroStone Arcade, полной версии: одна покупка, без рекламы. Все они видны в "
             "меню.",
        credit="Создано 8BCraft. Код — под лицензией MIT, графика и музыка — под CC BY-NC-SA 4.0: смотри «Авторы и "
               "лицензии» в приложении."),
    "uk": dict(
        short_free="{f} ретро-аркади у стилі 16 біт, безкоштовно. Без реклами й стеження, офлайн.",
        short_full="{n} ретро-аркад в одному застосунку. Працює офлайн, без реклами й покупок.",
        intro_free="RetroStone Arcade Free — це {f} оригінальні аркади в дусі 16-бітних консолей: чіткий піксель-арт, "
                   "музика, що запам'ятовується, і короткі забіги, у яких розберешся за секунду.",
        intro_full="RetroStone Arcade зібрав {n} оригінальних аркад у дусі 16-бітних консолей: чіткий піксель-арт, "
                   "музика, що запам'ятовується, і короткі забіги, у яких розберешся за секунду.",
        h_games="ІГРИ", h_play="ГРАЙ ЯК ХОЧЕШ", h_fair="БЕЗ РЕКЛАМИ Й СТЕЖЕННЯ",
        play=["Просте сенсорне керування, продумане для кожної гри",
              "Працюють і геймпади Bluetooth та USB, у багатьох іграх — до 4 гравців",
              "Виходь із гри будь-коли: вона чекатиме там, де ти зупинився",
              "Твої рекорди зберігаються на телефоні"],
        fair_free=["Без реклами, без покупок у застосунку, без облікового запису",
                   "Жодні дані не збираються; працює без інтернету", "22 мови"],
        fair_full=["Одна покупка — усі ігри: без реклами, без покупок у застосунку, без облікового запису",
                   "Жодні дані не збираються; працює без інтернету", "22 мови"],
        more="Ще {m} ігор чекають на тебе в RetroStone Arcade, повній версії: одна покупка, без реклами. Усі вони "
             "видно в меню.",
        credit="Створено 8BCraft. Код — за ліцензією MIT, графіка й музика — за CC BY-NC-SA 4.0: дивись «Автори та "
               "ліцензії» в застосунку."),
    "tr": dict(
        short_free="16-bit tarzında {f} retro arcade oyunu, ücretsiz. Reklamsız, takipsiz, çevrimdışı.",
        short_full="{n} retro arcade oyunu tek uygulamada. Çevrimdışı oyna, reklam ve satın alma yok.",
        intro_free="RetroStone Arcade Free, 16-bit konsolların ruhunu taşıyan {f} özgün arcade oyununu sunuyor: net "
                   "piksel sanatı, akılda kalan müzikler ve bir saniyede kavrayacağın kısa turlar.",
        intro_full="RetroStone Arcade, 16-bit konsolların ruhunu taşıyan {n} özgün arcade oyununu bir araya "
                   "getiriyor: net piksel sanatı, akılda kalan müzikler ve bir saniyede kavrayacağın kısa turlar.",
        h_games="OYUNLAR", h_play="İSTEDİĞİN GİBİ OYNA", h_fair="REKLAM YOK, TAKİP YOK",
        play=["Her oyun için tasarlanmış basit dokunmatik kontroller",
              "Bluetooth ve USB oyun kumandaları da çalışır, birçok oyunda 4 oyuncuya kadar",
              "Oyundan istediğin an çık: tam kaldığın yerde seni bekler",
              "En yüksek skorların telefonunda kalır"],
        fair_free=["Reklam yok, uygulama içi satın alma yok, hesap yok", "Hiç veri toplanmaz; çevrimdışı çalışır",
                   "22 dil"],
        fair_full=["Tüm oyunlar için tek satın alma: reklam yok, uygulama içi satın alma yok, hesap yok",
                   "Hiç veri toplanmaz; çevrimdışı çalışır", "22 dil"],
        more="{m} oyun daha tam sürüm RetroStone Arcade'de seni bekliyor: tek satın alma, reklam yok. Hepsini "
             "menüde görebilirsin.",
        credit="8BCraft yapımı. Kod MIT lisansıyla, grafik ve müzik CC BY-NC-SA 4.0 lisansıyla: uygulamadaki Emeği "
               "Geçenler ve Lisanslar bölümüne bak."),
    "id": dict(
        short_free="{f} game arcade retro bergaya 16-bit, gratis. Tanpa iklan & pelacakan, offline.",
        short_full="{n} game arcade retro dalam 1 aplikasi. Main offline, tanpa iklan & pembelian.",
        intro_free="RetroStone Arcade Free menghadirkan {f} game arcade orisinal dengan semangat konsol 16-bit: "
                   "pixel art yang tajam, musik yang mudah diingat, dan ronde singkat yang langsung kamu pahami.",
        intro_full="RetroStone Arcade mengumpulkan {n} game arcade orisinal dengan semangat konsol 16-bit: pixel art "
                   "yang tajam, musik yang mudah diingat, dan ronde singkat yang langsung kamu pahami.",
        h_games="GAME-NYA", h_play="MAIN SESUKAMU", h_fair="TANPA IKLAN, TANPA PELACAKAN",
        play=["Kontrol sentuh sederhana, dibuat untuk tiap game",
              "Gamepad Bluetooth dan USB juga bisa, hingga 4 pemain di banyak game",
              "Keluar dari game kapan saja: game menunggumu tepat di tempat kamu berhenti",
              "Skor terbaikmu tersimpan di ponselmu"],
        fair_free=["Tanpa iklan, tanpa pembelian dalam aplikasi, tanpa akun",
                   "Tidak ada data yang dikumpulkan; bisa dimainkan offline", "22 bahasa"],
        fair_full=["Sekali beli untuk semua game: tanpa iklan, tanpa pembelian dalam aplikasi, tanpa akun",
                   "Tidak ada data yang dikumpulkan; bisa dimainkan offline", "22 bahasa"],
        more="{m} game lainnya menunggumu di RetroStone Arcade, versi lengkapnya: sekali beli, tanpa iklan. Semuanya "
             "terlihat di menu.",
        credit="Dibuat oleh 8BCraft. Kode berlisensi MIT, gambar dan musik berlisensi CC BY-NC-SA 4.0: lihat Kredit "
               "& lisensi di aplikasi."),
    "ja": dict(
        short_free="16ビット風のレトロなアーケードゲーム{f}本が無料。広告なし、追跡なし、オフラインOK。",
        short_full="レトロなアーケードゲーム{n}本をひとつのアプリに。オフラインで遊べて、広告なし、課金なし。",
        intro_free="RetroStone Arcade Free は、16ビット機の魂を受け継いだオリジナルのアーケードゲーム{f}本を収録。"
                   "くっきりしたドット絵、耳に残る音楽、一瞬でわかる短いプレイが楽しめます。",
        intro_full="RetroStone Arcade は、16ビット機の魂を受け継いだオリジナルのアーケードゲーム{n}本を収録。"
                   "くっきりしたドット絵、耳に残る音楽、一瞬でわかる短いプレイが楽しめます。",
        h_games="収録ゲーム", h_play="好きなように遊べる", h_fair="広告なし・追跡なし",
        play=["ゲームごとに考えられたシンプルなタッチ操作",
              "Bluetooth・USBのゲームパッドにも対応。多くのゲームで最大4人まで遊べます",
              "いつでも中断OK。やめたところからそのまま再開できます",
              "ベストスコアはスマホの中に保存されます"],
        fair_free=["広告なし、アプリ内課金なし、アカウント不要", "データは一切収集しません。オフラインで遊べます",
                   "22言語に対応"],
        fair_full=["一度の購入ですべてのゲーム。広告なし、アプリ内課金なし、アカウント不要",
                   "データは一切収集しません。オフラインで遊べます", "22言語に対応"],
        more="完全版の RetroStone Arcade には、さらに{m}本のゲームが待っています。買い切りで広告なし。"
             "メニューですべてのゲームを見られます。",
        credit="制作：8BCraft。コードは MIT ライセンス、グラフィックと音楽は CC BY-NC-SA 4.0 です。"
               "詳しくはアプリ内の「クレジットとライセンス」をご覧ください。"),
    "ko": dict(
        short_free="16비트 감성의 레트로 아케이드 게임 {f}종 무료. 광고 없음, 추적 없음, 오프라인 플레이.",
        short_full="레트로 아케이드 게임 {n}종을 앱 하나에. 오프라인 플레이, 광고 없음, 인앱 결제 없음.",
        intro_free="RetroStone Arcade Free는 16비트 콘솔의 정신을 담은 오리지널 아케이드 게임 {f}종을 담았습니다. "
                   "선명한 픽셀 아트, 귀에 쏙 들어오는 음악, 바로 이해되는 짧은 판을 즐겨 보세요.",
        intro_full="RetroStone Arcade는 16비트 콘솔의 정신을 담은 오리지널 아케이드 게임 {n}종을 모았습니다. "
                   "선명한 픽셀 아트, 귀에 쏙 들어오는 음악, 바로 이해되는 짧은 판을 즐겨 보세요.",
        h_games="수록 게임", h_play="원하는 대로 플레이", h_fair="광고 없음, 추적 없음",
        play=["게임마다 맞춘 간단한 터치 조작",
              "블루투스·USB 게임패드도 지원, 많은 게임에서 최대 4명까지",
              "언제든 게임을 나가도 멈춘 그 자리에서 기다립니다",
              "최고 기록은 휴대폰에 저장됩니다"],
        fair_free=["광고 없음, 인앱 결제 없음, 계정 없음", "어떤 데이터도 수집하지 않음, 오프라인 플레이", "22개 언어"],
        fair_full=["한 번 구매로 모든 게임: 광고 없음, 인앱 결제 없음, 계정 없음",
                   "어떤 데이터도 수집하지 않음, 오프라인 플레이", "22개 언어"],
        more="정식판 RetroStone Arcade에서 {m}종의 게임이 더 기다리고 있습니다. 한 번 구매, 광고 없음. "
             "메뉴에서 모든 게임을 볼 수 있습니다.",
        credit="제작: 8BCraft. 코드는 MIT 라이선스, 그래픽과 음악은 CC BY-NC-SA 4.0입니다. 앱의 '크레딧 및 "
               "라이선스'를 확인하세요."),
    "zh_CN": dict(
        short_free="{f} 款 16 位风格的复古街机游戏，免费。无广告、无追踪，可离线游玩。",
        short_full="{n} 款复古街机游戏，尽在一个应用。可离线游玩，无广告，无应用内购买。",
        intro_free="RetroStone Arcade Free 收录 {f} 款原创街机游戏，延续 16 位游戏机的精神：清晰的像素画、"
                   "朗朗上口的音乐，还有一秒就能上手的短局。",
        intro_full="RetroStone Arcade 汇集 {n} 款原创街机游戏，延续 16 位游戏机的精神：清晰的像素画、"
                   "朗朗上口的音乐，还有一秒就能上手的短局。",
        h_games="游戏列表", h_play="随心畅玩", h_fair="无广告，无追踪",
        play=["为每款游戏量身设计的简单触控操作",
              "也支持蓝牙和 USB 手柄，许多游戏最多可 4 人同玩",
              "随时退出游戏：它会在你离开的地方等你",
              "最高分保存在你的手机上"],
        fair_free=["无广告，无应用内购买，无需账号", "不收集任何数据；可离线游玩", "支持 22 种语言"],
        fair_full=["一次购买，畅玩全部游戏：无广告，无应用内购买，无需账号", "不收集任何数据；可离线游玩",
                   "支持 22 种语言"],
        more="完整版 RetroStone Arcade 中还有 {m} 款游戏等着你：一次购买，没有广告。在菜单中就能看到全部游戏。",
        credit="由 8BCraft 制作。代码采用 MIT 许可证，美术和音乐采用 CC BY-NC-SA 4.0：详见应用中的“致谢与许可”。"),
}


def games(conf):
    return [(g, gc.game_name(g), locked) for g, locked in gc.list_games(gc.read_conf(conf))]


def full_description(lang, app, free_games, all_games):
    t = T[lang]
    n, f = len(all_games), len(free_games)
    fmt = dict(n=n, f=f, m=n - f)
    out = [t["intro_" + app].format(**fmt), "", t["h_games"]]
    for gid, name in (free_games if app == "free" else all_games):
        line = LINES.get(lang, {}).get(gid)
        out.append(f"• {name}" + (f": {line}" if line and lang not in ("fr",) else f" : {line}" if line else ""))
    out += ["", t["h_play"]] + ["• " + s for s in t["play"]]
    out += ["", t["h_fair"]] + ["• " + s for s in t["fair_" + app]]
    if app == "free":
        out += ["", t["more"].format(**fmt), "", ", ".join(name for gid, name in all_games
                                                             if gid not in dict(free_games))]
    out += ["", t["credit"]]
    return "\n".join(out) + "\n"


def write_listings(app_dir, label, title_en, entries):
    """one app's listings: <app_dir>/listings/<locale>/title.txt, short_description.txt and full_description.txt, and
    <app_dir>/listings-all-languages.txt (every language in one plain UTF-8 file: the Play Console's "Import
    translations with AI" takes one file). entries: (lang, title, short description, full description), in LOCALES'
    order. Returns the problems (a field too long for Google Play), each starting with label. Also used by
    make_listings_board.py (RetroStone Board)."""
    problems = []
    every = [f"{title_en}: Google Play store listing, {len(LOCALES)} languages. Each language starts with a "
             f"\"=== LANGUAGE ===\" line, then its three fields: App name, Short description, Full description.\n"]
    for lang, title, short, full_d in entries:
        loc = LOCALES[lang]
        d = os.path.join(app_dir, "listings", loc)
        os.makedirs(d, exist_ok=True)
        for name, text in (("title.txt", title + "\n"), ("short_description.txt", short + "\n"),
                           ("full_description.txt", full_d)):
            with open(os.path.join(d, name), "w", encoding="utf-8") as fh:
                fh.write(text)
        every.append(f"=== {LANGUAGE_NAMES[lang]} ({loc}) ===\n\nApp name:\n{title}\n\n"
                     f"Short description:\n{short}\n\nFull description:\n{full_d}")
        if len(short) > 80:
            problems.append(f"{label}/{loc}: short description {len(short)} > 80: {short}")
        if len(full_d) > 4000:
            problems.append(f"{label}/{loc}: full description {len(full_d)} > 4000")
        if len(title) > 30:
            problems.append(f"{label}/{loc}: title too long")
    with open(os.path.join(app_dir, "listings-all-languages.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n\n".join(every))
    return problems


def main():
    free = games(os.path.join(ROOT, "apps", "android", "app.conf"))
    full = games(os.path.join(ROOT, "apps", "android", "app-full.conf"))
    free_games = [(g, nm) for g, nm, locked in free if not locked]
    all_games = [(g, nm) for g, nm, locked in full]
    problems = []
    fmt = dict(n=len(all_games), f=len(free_games), m=len(all_games) - len(free_games))
    for app in ("free", "full"):
        entries = [(lang, title_for(lang, app, len(all_games)), T[lang]["short_" + app].format(**fmt),
                    full_description(lang, app, free_games, all_games)) for lang in LOCALES]
        problems += write_listings(os.path.join(STORE, app), app, title_for("en", app, len(all_games)), entries)
    print(f"{len(LOCALES)} languages x 2 apps: {len(all_games)} games, {len(free_games)} free")
    for p in problems:
        print("PROBLEM", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
