"""Map Quiz: the states' and regions' names that Natural Earth lacks or words otherwise (tools/mapquiz_gen.py).

FIX: a state's names by (language, key) where Natural Earth's are wrong or ambiguous (Moscow and Moscow Oblast, the
federal districts, Altai Krai...). CODE: the codes the levels use where the admin-1 file's ISO code is wrong (it
swaps Moscow's). GROUPS: the names of the regions made of several provinces (France's regions, Spain's autonomous
communities, Italy's regions), from a table of a line each:
    REGION field|local|en|de|fr|es|it|pt|pl|nl|sv|cs|da|fi|nb|tr|id|ru|uk|el|ja|ko|zh
("=": the local name). Japan's prefectures in the languages written in Latin letters: their plain names (Natural
Earth adds "Prefecture").
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""

import unicodedata

ORDER = ["local", "en", "de", "fr", "es", "it", "pt", "pl", "nl", "sv", "cs", "da", "fi", "nb", "tr", "id", "ru", "uk",
         "el", "ja", "ko", "zh_CN"]
LATIN = ["en", "cs", "da", "de", "es", "fi", "fr", "id", "it", "nb", "nl", "pl", "pt_BR", "pt_PT", "sv", "tr"]


def rows(text):
    out = {}
    for line in text.strip().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        cols = [c.strip() for c in line.split('|')]
        if len(cols) != len(ORDER) + 1:
            raise SystemExit('mapquiz_states: %d columns in %r' % (len(cols), line))
        key, vals = cols[0], cols[1:]
        local = vals[0]
        names = {}
        for lang, v in zip(ORDER[1:], vals[1:]):
            v = local if v == '=' else v
            if lang == 'pt':
                names['pt_BR'] = names['pt_PT'] = v
            else:
                names[lang] = v
        out[key] = names
    return out


FIX_ROWS = rows("""
# key|local|en|de|fr|es|it|pt|pl|nl|sv|cs|da|fi|nb|tr|id|ru|uk|el|ja|ko|zh
RU-MOS|Moskva|Moscow|Moskau|Moscou|Moscú|Mosca|Moscou|Moskwa|Moskou|Moskva|Moskva|Moskva|Moskova|Moskva|Moskova|Moskwa|Москва|Москва|Μόσχα|モスクワ|모스크바|莫斯科
RU-MOW|Moskovskaya|Moscow Oblast|Oblast Moskau|Oblast de Moscou|Óblast de Moscú|Oblast' di Mosca|Oblast de Moscou|Obwód moskiewski|Oblast Moskou|Moskva oblast|Moskevská oblast|Moskva oblast|Moskovan alue|Moskva oblast|Moskova Oblastı|Oblast Moskwa|Московская область|Московська область|Περιφέρεια της Μόσχας|モスクワ州|모스크바주|莫斯科州
RU-SPE|Sankt-Peterburg|Saint Petersburg|Sankt Petersburg|Saint-Pétersbourg|San Petersburgo|San Pietroburgo|São Petersburgo|Petersburg|Sint-Petersburg|Sankt Petersburg|Petrohrad|Sankt Petersborg|Pietari|St. Petersburg|Sankt-Peterburg|Sankt-Peterburg|Санкт-Петербург|Санкт-Петербург|Αγία Πετρούπολη|サンクトペテルブルク|상트페테르부르크|圣彼得堡
RU-LEN|Leningradskaya|Leningrad Oblast|Oblast Leningrad|Oblast de Léningrad|Óblast de Leningrado|Oblast' di Leningrado|Oblast de Leningrado|Obwód leningradzki|Oblast Leningrad|Leningrad oblast|Leningradská oblast|Leningrad oblast|Leningradin alue|Leningrad oblast|Leningrad Oblastı|Oblast Leningrad|Ленинградская область|Ленінградська область|Περιφέρεια του Λένινγκραντ|レニングラード州|레닌그라드주|列宁格勒州
RU-ALT|Altayskiy kray|Altai Krai|Region Altai|Kraï de l'Altaï|Krai de Altái|Territorio dell'Altaj|Krai de Altai|Kraj Ałtajski|Kraj Altaj|Altaj kraj|Altajský kraj|Altaj kraj|Altain aluepiiri|Altaj kraj|Altay Krayı|Kray Altai|Алтайский край|Алтайський край|Περιφέρεια Αλτάι|アルタイ地方|알타이 지방|阿尔泰边疆区
RU-AL|Altay|Altai Republic|Republik Altai|République de l'Altaï|República de Altái|Repubblica dell'Altaj|República de Altai|Republika Ałtaju|Republiek Altaj|Republiken Altaj|Altajská republika|Republikken Altaj|Altain tasavalta|Republikken Altaj|Altay Cumhuriyeti|Republik Altai|Республика Алтай|Республіка Алтай|Δημοκρατία του Αλτάι|アルタイ共和国|알타이 공화국|阿尔泰共和国
RU-YEV|Yevreyskaya|Jewish Autonomous Oblast|Jüdische Autonome Oblast|Oblast autonome juif|Óblast Autónomo Hebreo|Oblast' autonoma ebraica|Oblast Autónomo Judaico|Żydowski Obwód Autonomiczny|Joodse Autonome Oblast|Judiska autonoma oblastet|Židovská autonomní oblast|Den Jødiske Autonome Oblast|Juutalainen autonominen alue|Den jødiske autonome oblast|Yahudi Özerk Oblastı|Oblast Otonom Yahudi|Еврейская автономная область|Єврейська автономна область|Εβραϊκή Αυτόνομη Περιφέρεια|ユダヤ自治州|유대인 자치주|犹太自治州
MX-DIF|Ciudad de México|Mexico City|Mexiko-Stadt|Mexico|Ciudad de México|Città del Messico|Cidade do México|Meksyk (miasto)|Mexico-Stad|Mexico City|Ciudad de México|Mexico City|México|Mexico by|Meksiko City|Kota Meksiko|Мехико|Мехіко|Πόλη του Μεξικού|メキシコシティ|멕시코시티|墨西哥城
MX-MEX|México|State of Mexico|Bundesstaat México|État de Mexico|Estado de México|Stato del Messico|Estado do México|Meksyk (stan)|Mexico (staat)|Delstaten México|Stát México|Delstaten México|Méxicon osavaltio|Delstaten México|México Eyaleti|Negara Bagian Meksiko|Штат Мехико|Штат Мехіко|Πολιτεία του Μεξικού|メキシコ州|멕시코주|墨西哥州
BR-DF|Distrito Federal|Federal District|Bundesdistrikt|District fédéral|Distrito Federal|Distretto Federale|Distrito Federal|Dystrykt Federalny|Federaal District|Federala distriktet|Federální distrikt|Forbundsdistriktet|Liittovaltion piirikunta|Forbundsdistriktet|Federal Bölge|Distrik Federal|Федеральный округ|Федеральний округ|Ομοσπονδιακή Περιφέρεια|ブラジリア連邦直轄区|브라질 연방구|巴西联邦区
AR-C|Ciudad de Buenos Aires|City of Buenos Aires|Buenos Aires (Stadt)|Ville de Buenos Aires|Ciudad de Buenos Aires|Città di Buenos Aires|Cidade de Buenos Aires|Miasto Buenos Aires|Buenos Aires (stad)|Buenos Aires (stad)|Buenos Aires (město)|Buenos Aires (by)|Buenos Airesin kaupunki|Buenos Aires (by)|Buenos Aires Şehri|Kota Buenos Aires|Город Буэнос-Айрес|Місто Буенос-Айрес|Πόλη του Μπουένος Άιρες|ブエノスアイレス市|부에노스아이레스시|布宜诺斯艾利斯市
US-DC|Washington, D.C.|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Вашингтон (округ Колумбия)|Вашингтон (округ Колумбія)|Ουάσινγκτον, Π.Κ.|ワシントンD.C.|워싱턴 D.C.|华盛顿哥伦比亚特区
DE-HB|Bremen|=|=|Brême|=|Brema|=|Brema|=|=|Brémy|=|=|=|=|=|Бремен|Бремен|Βρέμη|ブレーメン州|브레멘|不来梅
AR-B|Buenos Aires|Buenos Aires Province|Provinz Buenos Aires|Province de Buenos Aires|Provincia de Buenos Aires|Provincia di Buenos Aires|Província de Buenos Aires|Prowincja Buenos Aires|Buenos Aires (provincie)|Buenos Aires (provins)|Provincie Buenos Aires|Buenos Aires (provins)|Buenos Airesin provinssi|Buenos Aires (provins)|Buenos Aires Eyaleti|Provinsi Buenos Aires|Провинция Буэнос-Айрес|Провінція Буенос-Айрес|Επαρχία του Μπουένος Άιρες|ブエノスアイレス州|부에노스아이레스주|布宜诺斯艾利斯省
""")

FIX = {}
for _key, _names in FIX_ROWS.items():
    for _lang, _n in _names.items():
        FIX[(_lang, _key)] = _n
FIX[('pt_PT', 'RU-MOS')] = 'Moscovo'
FIX[('pt_PT', 'RU-MOW')] = 'Oblast de Moscovo'

# the admin-1 file swaps Moscow (MOW) and Moscow Oblast (MOS): the levels use the right codes
CODE = {"RU-MOW": "RU-MOS", "RU-MOS": "RU-MOW"}

GROUP_ROWS = {
    "france": rows("""
Auvergne-Rhône-Alpes|Auvergne-Rhône-Alpes|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Овернь — Рона — Альпы|Овернь — Рона — Альпи|Ωβέρνη-Ροδανός-Άλπεις|オーヴェルニュ＝ローヌ＝アルプ|오베르뉴론알프|奥弗涅-罗讷-阿尔卑斯
Bourgogne-Franche-Comté|Bourgogne-Franche-Comté|Burgundy-Franche-Comté|=|=|Borgoña-Franco Condado|Borgogna-Franca Contea|Borgonha-Franco-Condado|Burgundia-Franche-Comté|=|=|Burgundsko-Franche-Comté|=|=|=|=|=|Бургундия — Франш-Конте|Бургундія — Франш-Конте|Βουργουνδία-Φρανς-Κοντέ|ブルゴーニュ＝フランシュ＝コンテ|부르고뉴프랑슈콩테|勃艮第-弗朗什-孔泰
Bretagne|Bretagne|Brittany|=|=|Bretaña|Bretagna|Bretanha|Bretania|=|=|Bretaň|=|=|=|Brötanya|=|Бретань|Бретань|Βρετάνη|ブルターニュ|브르타뉴|布列塔尼
Centre-Val de Loire|Centre-Val de Loire|=|=|=|Centro-Valle de Loira|Centro-Valle della Loira|Centro-Vale do Loire|Centre-Dolina Loary|=|=|=|=|=|=|=|=|Центр — Долина Луары|Центр — Долина Луари|Κέντρο-Κοιλάδα του Λίγηρα|サントル＝ヴァル・ド・ロワール|상트르발드루아르|中央-卢瓦尔河谷
Corse|Corse|Corsica|Korsika|=|Córcega|Corsica|Córsega|Korsyka|Corsica|Korsika|Korsika|Korsika|Korsika|Korsika|Korsika|Korsika|Корсика|Корсика|Κορσική|コルシカ|코르시카|科西嘉
Grand Est|Grand Est|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Гранд-Эст|Гранд-Ест|Γκραντ Εστ|グラン・テスト|그랑테스트|大东部
Hauts-de-France|Hauts-de-France|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|О-де-Франс|О-де-Франс|Ο-ντε-Φρανς|オー＝ド＝フランス|오드프랑스|上法兰西
Île-de-France|Île-de-France|=|=|=|Isla de Francia|=|Ilha de França|=|=|=|=|=|=|=|=|=|Иль-де-Франс|Іль-де-Франс|Ιλ-ντε-Φρανς|イル＝ド＝フランス|일드프랑스|法兰西岛
Normandie|Normandie|Normandy|=|=|Normandía|Normandia|Normandia|Normandia|Normandië|=|=|Normandiet|Normandia|=|Normandiya|Normandia|Нормандия|Нормандія|Νορμανδία|ノルマンディー|노르망디|诺曼底
Nouvelle-Aquitaine|Nouvelle-Aquitaine|=|=|=|Nueva Aquitania|Nuova Aquitania|Nova Aquitânia|Nowa Akwitania|=|=|Nová Akvitánie|=|=|=|=|=|Новая Аквитания|Нова Аквітанія|Νέα Ακουιτανία|ヌーヴェル＝アキテーヌ|누벨아키텐|新阿基坦
Occitanie|Occitanie|Occitania|Okzitanien|=|Occitania|Occitania|Occitânia|Oksytania|=|Occitanien|Okcitánie|Occitanien|Oksitania|Occitania|Oksitanya|=|Окситания|Окситанія|Οξιτανία|オクシタニー|옥시타니|奥克西塔尼
Pays de la Loire|Pays de la Loire|=|=|=|País del Loira|Paesi della Loira|Países do Loire|Kraj Loary|=|=|=|=|=|=|=|=|Земли Луары|Пеї-де-ла-Луар|Χώρες του Λίγηρα|ペイ・ド・ラ・ロワール|페이드라루아르|卢瓦尔河地区
Provence-Alpes-Côte-d'Azur|Provence-Alpes-Côte d'Azur|=|=|=|Provenza-Alpes-Costa Azul|Provenza-Alpi-Costa Azzurra|Provença-Alpes-Costa Azul|Prowansja-Alpy-Lazurowe Wybrzeże|=|=|=|=|=|=|=|=|Прованс — Альпы — Лазурный Берег|Прованс — Альпи — Лазуровий Берег|Προβηγκία-Άλπεις-Κυανή Ακτή|プロヴァンス＝アルプ＝コート・ダジュール|프로방스알프코트다쥐르|普罗旺斯-阿尔卑斯-蓝色海岸
"""),
    "spain": rows("""
Andalucía|Andalucía|Andalusia|Andalusien|Andalousie|=|Andalusia|Andaluzia|Andaluzja|Andalusië|Andalusien|Andalusie|Andalusien|Andalusia|=|Endülüs|Andalusia|Андалусия|Андалусія|Ανδαλουσία|アンダルシア州|안달루시아|安达卢西亚
Aragón|Aragón|Aragon|Aragonien|Aragon|=|Aragona|Aragão|Aragonia|=|Aragonien|Aragonie|Aragonien|Aragonia|=|Aragon|Aragon|Арагон|Арагон|Αραγωνία|アラゴン州|아라곤|阿拉贡
Asturias|Asturias|=|Asturien|Asturies|=|Asturie|Astúrias|Asturia|Asturië|Asturien|Asturie|Asturien|=|=|=|=|Астурия|Астурія|Αστούριας|アストゥリアス州|아스투리아스|阿斯图里亚斯
Canary Is.|Canarias|Canary Islands|Kanarische Inseln|Îles Canaries|=|Canarie|Canárias|Wyspy Kanaryjskie|Canarische Eilanden|Kanarieöarna|Kanárské ostrovy|De Kanariske Øer|Kanariansaaret|Kanariøyene|Kanarya Adaları|Kepulauan Canaria|Канарские острова|Канарські острови|Κανάριοι Νήσοι|カナリア諸島|카나리아 제도|加那利群岛
Cantabria|Cantabria|=|Kantabrien|Cantabrie|=|=|Cantábria|Kantabria|Cantabrië|Kantabrien|Kantábrie|Kantabrien|Kantabria|=|Kantabria|Kantabria|Кантабрия|Кантабрія|Κανταβρία|カンタブリア州|칸타브리아|坎塔布里亚
Castilla y León|Castilla y León|Castile and León|Kastilien und León|Castille-et-León|=|Castiglia e León|Castela e Leão|Kastylia i León|Castilië en León|Kastilien och León|Kastilie a León|=|Kastilia ja León|=|Kastilya ve Leon|Kastila dan León|Кастилия и Леон|Кастилія і Леон|Καστίλλη και Λεόν|カスティーリャ・イ・レオン州|카스티야이레온|卡斯蒂利亚-莱昂
Castilla-La Mancha|Castilla-La Mancha|Castile-La Mancha|Kastilien-La Mancha|Castille-La Manche|=|Castiglia-La Mancia|Castela-Mancha|Kastylia-La Mancha|Castilië-La Mancha|Kastilien-La Mancha|Kastilie-La Mancha|=|Kastilia-La Mancha|=|Kastilya-La Mancha|Kastila-La Mancha|Кастилия — Ла-Манча|Кастилія — Ла-Манча|Καστίλλη-Λα Μάντσα|カスティーリャ＝ラ・マンチャ州|카스티야라만차|卡斯蒂利亚-拉曼恰
Cataluña|Cataluña|Catalonia|Katalonien|Catalogne|=|Catalogna|Catalunha|Katalonia|Catalonië|Katalonien|Katalánsko|Catalonien|Katalonia|Catalonia|Katalonya|Katalonia|Каталония|Каталонія|Καταλονία|カタルーニャ州|카탈루냐|加泰罗尼亚
Ceuta|Ceuta|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Сеута|Сеута|Θέουτα|セウタ|세우타|休达
Extremadura|Extremadura|=|=|Estrémadure|=|Estremadura|Estremadura|Estremadura|=|=|=|=|=|=|=|=|Эстремадура|Естремадура|Εξτρεμαδούρα|エストレマドゥーラ州|엑스트레마두라|埃斯特雷马杜拉
Foral de Navarra|Navarra|Navarre|=|Navarre|=|=|=|Nawarra|=|=|=|=|=|=|=|=|Наварра|Наварра|Ναβάρρα|ナバーラ州|나바라|纳瓦拉
Galicia|Galicia|=|Galicien|Galice|=|Galizia|Galiza|Galicja|Galicië|Galicien|Galicie|Galicien|=|=|Galiçya|=|Галисия|Галісія|Γαλικία|ガリシア州|갈리시아|加利西亚
Islas Baleares|Islas Baleares|Balearic Islands|Balearen|Îles Baléares|=|Isole Baleari|Ilhas Baleares|Baleary|Balearen|Balearerna|Baleáry|Balearerne|Baleaarit|Balearene|Balear Adaları|Kepulauan Balears|Балеарские острова|Балеарські острови|Βαλεαρίδες Νήσοι|バレアレス諸島|발레아레스 제도|巴利阿里群岛
La Rioja|La Rioja|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Риоха|Ла-Ріоха|Λα Ριόχα|ラ・リオハ州|라리오하|拉里奥哈
Madrid|Comunidad de Madrid|Community of Madrid|Autonome Gemeinschaft Madrid|Communauté de Madrid|=|Comunità di Madrid|Comunidade de Madrid|Wspólnota Madrytu|Madrid (regio)|Madrid (region)|Madridské autonomní společenství|Madrid (region)|Madridin itsehallintoalue|Madrid (region)|Madrid Özerk Topluluğu|Komunitas Madrid|Мадрид (автономное сообщество)|Мадрид (автономна спільнота)|Κοινότητα της Μαδρίτης|マドリード州|마드리드 지방|马德里自治区
Melilla|Melilla|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Мелилья|Мелілья|Μελίγια|メリリャ|멜리야|梅利利亚
Murcia|Región de Murcia|Murcia|=|Murcie|=|=|Múrcia|Murcja|=|=|=|=|=|=|=|=|Мурсия|Мурсія|Μούρθια|ムルシア州|무르시아|穆尔西亚
País Vasco|País Vasco|Basque Country|Baskenland|Pays basque|=|Paesi Baschi|País Basco|Kraj Basków|Baskenland|Baskien|Baskicko|Baskerlandet|Baskimaa|Baskerland|Bask Özerk Topluluğu|Negeri Basque|Страна Басков|Країна Басків|Χώρα των Βάσκων|バスク州|바스크|巴斯克
Valenciana|Comunidad Valenciana|Valencian Community|Valencianische Gemeinschaft|Communauté valencienne|=|Comunità Valenciana|Comunidade Valenciana|Wspólnota Walencka|Valencia (regio)|Valencia (region)|Valencijské společenství|Valencia (region)|Valencian itsehallintoalue|Valencia (region)|Valensiya Topluluğu|Komunitas Valencia|Валенсия (автономное сообщество)|Валенсія (автономна спільнота)|Κοινότητα της Βαλένθια|バレンシア州|발렌시아 지방|巴伦西亚自治区
"""),
    "italy": rows("""
Abruzzo|Abruzzo|=|Abruzzen|Abruzzes|Abruzos|=|Abruzos|Abruzja|Abruzzen|=|=|=|=|=|=|=|Абруцци|Абруццо|Αμπρούτσο|アブルッツォ州|아브루초|阿布鲁佐
Apulia|Puglia|Apulia|Apulien|Pouilles|Apulia|=|Apúlia|Apulia|Apulië|Apulien|Apulie|Apulien|Apulia|Apulia|Apulia|Apulia|Апулия|Апулія|Απουλία|プッリャ州|풀리아|普利亚
Basilicata|Basilicata|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Базиликата|Базиліката|Βασιλικάτα|バジリカータ州|바실리카타|巴西利卡塔
Calabria|Calabria|=|Kalabrien|Calabre|=|=|Calábria|Kalabria|Calabrië|Kalabrien|Kalábrie|Calabrien|=|=|Kalabriya|=|Калабрия|Калабрія|Καλαβρία|カラブリア州|칼라브리아|卡拉布里亚
Campania|Campania|=|Kampanien|Campanie|=|=|Campânia|Kampania|Campanië|Kampanien|Kampánie|Campanien|=|=|Campania|=|Кампания|Кампанія|Καμπανία|カンパニア州|캄파니아|坎帕尼亚
Emilia-Romagna|Emilia-Romagna|=|=|Émilie-Romagne|Emilia-Romaña|=|Emília-Romanha|Emilia-Romania|=|=|=|=|=|=|=|=|Эмилия-Романья|Емілія-Романья|Εμίλια-Ρομάνια|エミリア＝ロマーニャ州|에밀리아로마냐|艾米利亚-罗马涅
Friuli-Venezia Giulia|Friuli-Venezia Giulia|=|Friaul-Julisch Venetien|Frioul-Vénétie julienne|Friuli-Venecia Julia|=|Friul-Veneza Júlia|Friuli-Wenecja Julijska|=|=|Furlansko-Julské Benátsko|=|=|=|=|=|Фриули — Венеция-Джулия|Фріулі — Венеція-Джулія|Φρίουλι-Βενέτσια Τζούλια|フリウリ＝ヴェネツィア・ジュリア州|프리울리베네치아줄리아|弗留利-威尼斯朱利亚
Lazio|Lazio|=|Latium|Latium|Lacio|=|Lácio|Lacjum|=|=|=|=|=|=|=|=|Лацио|Лаціо|Λάτσιο|ラツィオ州|라치오|拉齐奥
Liguria|Liguria|=|Ligurien|Ligurie|=|=|Ligúria|=|Ligurië|Ligurien|Ligurie|Ligurien|=|=|=|=|Лигурия|Лігурія|Λιγουρία|リグーリア州|리구리아|利古里亚
Lombardia|Lombardia|Lombardy|Lombardei|Lombardie|Lombardía|=|=|=|Lombardije|Lombardiet|Lombardie|Lombardiet|=|=|Lombardiya|=|Ломбардия|Ломбардія|Λομβαρδία|ロンバルディア州|롬바르디아|伦巴第
Marche|Marche|=|Marken|Marches|Marcas|=|Marcas|=|=|=|Marche|=|=|=|=|=|Марке|Марке|Μάρκε|マルケ州|마르케|马尔凯
Molise|Molise|=|=|=|=|=|=|=|=|=|=|=|=|=|=|=|Молизе|Молізе|Μολίζε|モリーゼ州|몰리세|莫利塞
Piemonte|Piemonte|Piedmont|Piemont|Piémont|Piamonte|=|=|Piemont|Piëmont|=|Piemont|=|=|=|Piyemonte|=|Пьемонт|П'ємонт|Πεδεμόντιο|ピエモンテ州|피에몬테|皮埃蒙特
Sardegna|Sardegna|Sardinia|Sardinien|Sardaigne|Cerdeña|=|Sardenha|Sardynia|Sardinië|Sardinien|Sardinie|Sardinien|Sardinia|Sardinia|Sardinya|Sardinia|Сардиния|Сардинія|Σαρδηνία|サルデーニャ州|사르데냐|撒丁
Sicily|Sicilia|Sicily|Sizilien|Sicile|=|=|Sicília|Sycylia|Sicilië|Sicilien|Sicílie|Sicilien|Sisilia|=|Sicilya|Sisilia|Сицилия|Сицилія|Σικελία|シチリア州|시칠리아|西西里
Toscana|Toscana|Tuscany|Toskana|Toscane|=|=|=|Toskania|Toscane|=|Toskánsko|=|=|=|Toskana|Toskana|Тоскана|Тоскана|Τοσκάνη|トスカーナ州|토스카나|托斯卡纳
Trentino-Alto Adige|Trentino-Alto Adige|Trentino-South Tyrol|Trentino-Südtirol|Trentin-Haut-Adige|Trentino-Alto Adigio|=|Trentino-Alto Ádige|Trydent-Górna Adyga|Trentino-Zuid-Tirol|=|Tridentsko-Horní Adiže|=|=|=|=|=|Трентино — Альто-Адидже|Трентіно — Альто-Адідже|Τρεντίνο-Άλτο Άντιτζε|トレンティーノ＝アルト・アディジェ州|트렌티노알토아디제|特伦蒂诺-上阿迪杰
Umbria|Umbria|=|Umbrien|Ombrie|=|=|Úmbria|=|Umbrië|Umbrien|Umbrie|Umbrien|=|=|=|=|Умбрия|Умбрія|Ούμπρια|ウンブリア州|움브리아|翁布里亚
Valle d'Aosta|Valle d'Aosta|Aosta Valley|Aostatal|Vallée d'Aoste|Valle de Aosta|=|Vale de Aosta|Dolina Aosty|=|Aostadalen|Údolí Aosty|Aostadalen|Aostanlaakso|Aostadalen|Aosta Vadisi|Lembah Aosta|Валле-д’Аоста|Валле-д'Аоста|Κοιλάδα της Αόστας|ヴァッレ・ダオスタ州|발레다오스타|瓦莱达奥斯塔
Veneto|Veneto|=|Venetien|Vénétie|Véneto|=|Vêneto|Wenecja Euganejska|=|=|Benátsko|=|=|=|=|=|Венето|Венето|Βένετο|ヴェネト州|베네토|威尼托
"""),
}
GROUPS = {}
GROUP_CODE = {}
for _mid, _tab in GROUP_ROWS.items():
    for _g, _names in _tab.items():
        GROUPS[(_mid, _g)] = _names
        GROUP_CODE[(_mid, _g)] = {"france": "FR-", "spain": "ES-", "italy": "IT-"}[_mid] + \
            ''.join(c for c in unicodedata.normalize('NFKD', _g.upper()) if 'A' <= c <= 'Z')
GROUPS[("italy", "Veneto")]["pt_PT"] = "Véneto"
