"""Map Quiz's names that Natural Earth lacks or words otherwise (tools/mapquiz_gen.py reads them).

Natural Earth gives the countries' and the states' names in many of the app's languages (NAME_DE, NAME_FR...). This
file fills the four it has none for (Czech, Danish, Finnish, Norwegian Bokmal), Brazilian Portuguese where it differs
from Natural Earth's (European) Portuguese, the short everyday forms where Natural Earth gives an official one
(People's Republic of China: China), and the names of the regions made of several provinces (France's, Spain's,
Italy's). A code is Natural Earth's ADM0_A3 (a country), or the generator's own for a region.
MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
"""

LANGS = ["en", "cs", "da", "de", "el", "es", "fi", "fr", "id", "it", "ja", "ko", "nb", "nl", "pl", "pt_BR",
         "pt_PT", "ru", "sv", "tr", "uk", "zh_CN"]


def table(text):
    """lines 'CODE|name' into {code: name}"""
    out = {}
    for line in text.strip().splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        code, name = line.split('|', 1)
        out[code.strip()] = name.strip()
    return out


CS = table("""
ABW|Aruba
AFG|Afghánistán
AGO|Angola
AIA|Anguilla
ALB|Albánie
ALD|Alandy
AND|Andorra
ARE|Spojené arabské emiráty
ARG|Argentina
ARM|Arménie
ASM|Americká Samoa
ATC|Ashmoreovy a Cartierovy ostrovy
ATF|Francouzská jižní a antarktická území
ATG|Antigua a Barbuda
AUS|Austrálie
AUT|Rakousko
AZE|Ázerbájdžán
BDI|Burundi
BEL|Belgie
BEN|Benin
BFA|Burkina Faso
BGD|Bangladéš
BGR|Bulharsko
BHR|Bahrajn
BHS|Bahamy
BIH|Bosna a Hercegovina
BLM|Svatý Bartoloměj
BLR|Bělorusko
BLZ|Belize
BMU|Bermudy
BOL|Bolívie
BRA|Brazílie
BRB|Barbados
BRN|Brunej
BTN|Bhútán
BWA|Botswana
CAF|Středoafrická republika
CAN|Kanada
CHE|Švýcarsko
CHL|Chile
CHN|Čína
CIV|Pobřeží slonoviny
CMR|Kamerun
COD|Demokratická republika Kongo
COG|Kongo
COK|Cookovy ostrovy
COL|Kolumbie
COM|Komory
CPV|Kapverdy
CRI|Kostarika
CUB|Kuba
CUW|Curaçao
CYM|Kajmanské ostrovy
CYP|Kypr
CZE|Česko
DEU|Německo
DJI|Džibutsko
DMA|Dominika
DNK|Dánsko
DOM|Dominikánská republika
DZA|Alžírsko
ECU|Ekvádor
EGY|Egypt
ERI|Eritrea
ESP|Španělsko
EST|Estonsko
ETH|Etiopie
FIN|Finsko
FJI|Fidži
FRA|Francie
FRO|Faerské ostrovy
FSM|Mikronésie
GAB|Gabon
GBR|Spojené království
GEO|Gruzie
GGY|Guernsey
GHA|Ghana
GIN|Guinea
GMB|Gambie
GNB|Guinea-Bissau
GNQ|Rovníková Guinea
GRC|Řecko
GRD|Grenada
GRL|Grónsko
GTM|Guatemala
GUM|Guam
GUY|Guyana
HKG|Hongkong
HMD|Heardův ostrov a McDonaldovy ostrovy
HND|Honduras
HRV|Chorvatsko
HTI|Haiti
HUN|Maďarsko
IDN|Indonésie
IMN|Ostrov Man
IND|Indie
IOA|Australská území v Indickém oceánu
IRL|Irsko
IRN|Írán
IRQ|Irák
ISL|Island
ISR|Izrael
ITA|Itálie
JAM|Jamajka
JEY|Jersey
JOR|Jordánsko
JPN|Japonsko
KAZ|Kazachstán
KEN|Keňa
KGZ|Kyrgyzstán
KHM|Kambodža
KIR|Kiribati
KNA|Svatý Kryštof a Nevis
KOR|Jižní Korea
KWT|Kuvajt
LAO|Laos
LBN|Libanon
LBR|Libérie
LBY|Libye
LCA|Svatá Lucie
LIE|Lichtenštejnsko
LKA|Srí Lanka
LSO|Lesotho
LTU|Litva
LUX|Lucembursko
LVA|Lotyšsko
MAC|Macao
MAF|Svatý Martin
MAR|Maroko
MCO|Monako
MDA|Moldavsko
MDG|Madagaskar
MDV|Maledivy
MEX|Mexiko
MHL|Marshallovy ostrovy
MKD|Severní Makedonie
MLI|Mali
MLT|Malta
MMR|Myanmar
MNE|Černá Hora
MNG|Mongolsko
MNP|Severní Mariany
MOZ|Mosambik
MRT|Mauritánie
MSR|Montserrat
MUS|Mauricius
MWI|Malawi
MYS|Malajsie
NAM|Namibie
NCL|Nová Kaledonie
NER|Niger
NFK|Norfolk
NGA|Nigérie
NIC|Nikaragua
NIU|Niue
NLD|Nizozemsko
NOR|Norsko
NPL|Nepál
NRU|Nauru
NZL|Nový Zéland
OMN|Omán
PAK|Pákistán
PAN|Panama
PCN|Pitcairnovy ostrovy
PER|Peru
PHL|Filipíny
PLW|Palau
PNG|Papua Nová Guinea
POL|Polsko
PRI|Portoriko
PRK|Severní Korea
PRT|Portugalsko
PRY|Paraguay
PYF|Francouzská Polynésie
QAT|Katar
ROU|Rumunsko
RUS|Rusko
RWA|Rwanda
SAU|Saúdská Arábie
SDN|Súdán
SDS|Jižní Súdán
SEN|Senegal
SGP|Singapur
SGS|Jižní Georgie a Jižní Sandwichovy ostrovy
SHN|Svatá Helena
SLB|Šalamounovy ostrovy
SLE|Sierra Leone
SLV|Salvador
SMR|San Marino
SOM|Somálsko
SPM|Saint-Pierre a Miquelon
SRB|Srbsko
STP|Svatý Tomáš a Princův ostrov
SUR|Surinam
SVK|Slovensko
SVN|Slovinsko
SWE|Švédsko
SWZ|Eswatini
SXM|Svatý Martin (Nizozemsko)
SYC|Seychely
SYR|Sýrie
TCA|Turks a Caicos
TCD|Čad
TGO|Togo
THA|Thajsko
TJK|Tádžikistán
TKM|Turkmenistán
TLS|Východní Timor
TON|Tonga
TTO|Trinidad a Tobago
TUN|Tunisko
TUR|Turecko
TUV|Tuvalu
TZA|Tanzanie
UGA|Uganda
UKR|Ukrajina
URY|Uruguay
USA|Spojené státy
UZB|Uzbekistán
VAT|Vatikán
VCT|Svatý Vincenc a Grenadiny
VEN|Venezuela
VGB|Britské Panenské ostrovy
VIR|Americké Panenské ostrovy
VNM|Vietnam
VUT|Vanuatu
WLF|Wallis a Futuna
WSM|Samoa
YEM|Jemen
ZAF|Jihoafrická republika
ZMB|Zambie
ZWE|Zimbabwe
CLP|Clippertonův ostrov
CSI|Ostrovy Korálového moře
ESB|Dhekelia
GIB|Gibraltar
UMI|Menší odlehlé ostrovy USA
USG|Guantánamo
WSB|Akrotiri
""")

DA = table("""
ABW|Aruba
AFG|Afghanistan
AGO|Angola
AIA|Anguilla
ALB|Albanien
ALD|Åland
AND|Andorra
ARE|De Forenede Arabiske Emirater
ARG|Argentina
ARM|Armenien
ASM|Amerikansk Samoa
ATC|Ashmore- og Cartierøerne
ATF|De Franske Sydlige og Antarktiske Territorier
ATG|Antigua og Barbuda
AUS|Australien
AUT|Østrig
AZE|Aserbajdsjan
BDI|Burundi
BEL|Belgien
BEN|Benin
BFA|Burkina Faso
BGD|Bangladesh
BGR|Bulgarien
BHR|Bahrain
BHS|Bahamas
BIH|Bosnien-Hercegovina
BLM|Saint-Barthélemy
BLR|Hviderusland
BLZ|Belize
BMU|Bermuda
BOL|Bolivia
BRA|Brasilien
BRB|Barbados
BRN|Brunei
BTN|Bhutan
BWA|Botswana
CAF|Den Centralafrikanske Republik
CAN|Canada
CHE|Schweiz
CHL|Chile
CHN|Kina
CIV|Elfenbenskysten
CMR|Cameroun
COD|Den Demokratiske Republik Congo
COG|Republikken Congo
COK|Cookøerne
COL|Colombia
COM|Comorerne
CPV|Kap Verde
CRI|Costa Rica
CUB|Cuba
CUW|Curaçao
CYM|Caymanøerne
CYP|Cypern
CZE|Tjekkiet
DEU|Tyskland
DJI|Djibouti
DMA|Dominica
DNK|Danmark
DOM|Den Dominikanske Republik
DZA|Algeriet
ECU|Ecuador
EGY|Egypten
ERI|Eritrea
ESP|Spanien
EST|Estland
ETH|Etiopien
FIN|Finland
FJI|Fiji
FRA|Frankrig
FRO|Færøerne
FSM|Mikronesien
GAB|Gabon
GBR|Storbritannien
GEO|Georgien
GGY|Guernsey
GHA|Ghana
GIN|Guinea
GMB|Gambia
GNB|Guinea-Bissau
GNQ|Ækvatorialguinea
GRC|Grækenland
GRD|Grenada
GRL|Grønland
GTM|Guatemala
GUM|Guam
GUY|Guyana
HKG|Hongkong
HMD|Heard- og McDonaldøerne
HND|Honduras
HRV|Kroatien
HTI|Haiti
HUN|Ungarn
IDN|Indonesien
IMN|Isle of Man
IND|Indien
IOA|Australske territorier i Det Indiske Ocean
IRL|Irland
IRN|Iran
IRQ|Irak
ISL|Island
ISR|Israel
ITA|Italien
JAM|Jamaica
JEY|Jersey
JOR|Jordan
JPN|Japan
KAZ|Kasakhstan
KEN|Kenya
KGZ|Kirgisistan
KHM|Cambodja
KIR|Kiribati
KNA|Saint Kitts og Nevis
KOR|Sydkorea
KWT|Kuwait
LAO|Laos
LBN|Libanon
LBR|Liberia
LBY|Libyen
LCA|Saint Lucia
LIE|Liechtenstein
LKA|Sri Lanka
LSO|Lesotho
LTU|Litauen
LUX|Luxembourg
LVA|Letland
MAC|Macao
MAF|Saint-Martin
MAR|Marokko
MCO|Monaco
MDA|Moldova
MDG|Madagaskar
MDV|Maldiverne
MEX|Mexico
MHL|Marshalløerne
MKD|Nordmakedonien
MLI|Mali
MLT|Malta
MMR|Myanmar
MNE|Montenegro
MNG|Mongoliet
MNP|Nordmarianerne
MOZ|Mozambique
MRT|Mauretanien
MSR|Montserrat
MUS|Mauritius
MWI|Malawi
MYS|Malaysia
NAM|Namibia
NCL|Ny Kaledonien
NER|Niger
NFK|Norfolk Island
NGA|Nigeria
NIC|Nicaragua
NIU|Niue
NLD|Nederlandene
NOR|Norge
NPL|Nepal
NRU|Nauru
NZL|New Zealand
OMN|Oman
PAK|Pakistan
PAN|Panama
PCN|Pitcairn
PER|Peru
PHL|Filippinerne
PLW|Palau
PNG|Papua Ny Guinea
POL|Polen
PRI|Puerto Rico
PRK|Nordkorea
PRT|Portugal
PRY|Paraguay
PYF|Fransk Polynesien
QAT|Qatar
ROU|Rumænien
RUS|Rusland
RWA|Rwanda
SAU|Saudi-Arabien
SDN|Sudan
SDS|Sydsudan
SEN|Senegal
SGP|Singapore
SGS|South Georgia og South Sandwich Islands
SHN|Sankt Helena
SLB|Salomonøerne
SLE|Sierra Leone
SLV|El Salvador
SMR|San Marino
SOM|Somalia
SPM|Saint-Pierre og Miquelon
SRB|Serbien
STP|São Tomé og Príncipe
SUR|Surinam
SVK|Slovakiet
SVN|Slovenien
SWE|Sverige
SWZ|Eswatini
SXM|Sint Maarten
SYC|Seychellerne
SYR|Syrien
TCA|Turks- og Caicosøerne
TCD|Tchad
TGO|Togo
THA|Thailand
TJK|Tadsjikistan
TKM|Turkmenistan
TLS|Østtimor
TON|Tonga
TTO|Trinidad og Tobago
TUN|Tunesien
TUR|Tyrkiet
TUV|Tuvalu
TZA|Tanzania
UGA|Uganda
UKR|Ukraine
URY|Uruguay
USA|USA
UZB|Usbekistan
VAT|Vatikanstaten
VCT|Saint Vincent og Grenadinerne
VEN|Venezuela
VGB|De Britiske Jomfruøer
VIR|De Amerikanske Jomfruøer
VNM|Vietnam
VUT|Vanuatu
WLF|Wallis og Futuna
WSM|Samoa
YEM|Yemen
ZAF|Sydafrika
ZMB|Zambia
ZWE|Zimbabwe
CLP|Clippertonøen
CSI|Koralhavsøerne
ESB|Dhekelia
GIB|Gibraltar
UMI|USA's ydre småøer
USG|Guantánamo
WSB|Akrotiri
""")

FI = table("""
ABW|Aruba
AFG|Afganistan
AGO|Angola
AIA|Anguilla
ALB|Albania
ALD|Ahvenanmaa
AND|Andorra
ARE|Arabiemiirikunnat
ARG|Argentiina
ARM|Armenia
ASM|Amerikan Samoa
ATC|Ashmore- ja Cartiersaaret
ATF|Ranskan eteläiset ja antarktiset alueet
ATG|Antigua ja Barbuda
AUS|Australia
AUT|Itävalta
AZE|Azerbaidžan
BDI|Burundi
BEL|Belgia
BEN|Benin
BFA|Burkina Faso
BGD|Bangladesh
BGR|Bulgaria
BHR|Bahrain
BHS|Bahama
BIH|Bosnia ja Hertsegovina
BLM|Saint-Barthélemy
BLR|Valko-Venäjä
BLZ|Belize
BMU|Bermuda
BOL|Bolivia
BRA|Brasilia
BRB|Barbados
BRN|Brunei
BTN|Bhutan
BWA|Botswana
CAF|Keski-Afrikan tasavalta
CAN|Kanada
CHE|Sveitsi
CHL|Chile
CHN|Kiina
CIV|Norsunluurannikko
CMR|Kamerun
COD|Kongon demokraattinen tasavalta
COG|Kongon tasavalta
COK|Cookinsaaret
COL|Kolumbia
COM|Komorit
CPV|Kap Verde
CRI|Costa Rica
CUB|Kuuba
CUW|Curaçao
CYM|Caymansaaret
CYP|Kypros
CZE|Tšekki
DEU|Saksa
DJI|Djibouti
DMA|Dominica
DNK|Tanska
DOM|Dominikaaninen tasavalta
DZA|Algeria
ECU|Ecuador
EGY|Egypti
ERI|Eritrea
ESP|Espanja
EST|Viro
ETH|Etiopia
FIN|Suomi
FJI|Fidži
FRA|Ranska
FRO|Färsaaret
FSM|Mikronesia
GAB|Gabon
GBR|Yhdistynyt kuningaskunta
GEO|Georgia
GGY|Guernsey
GHA|Ghana
GIN|Guinea
GMB|Gambia
GNB|Guinea-Bissau
GNQ|Päiväntasaajan Guinea
GRC|Kreikka
GRD|Grenada
GRL|Grönlanti
GTM|Guatemala
GUM|Guam
GUY|Guyana
HKG|Hongkong
HMD|Heard ja McDonaldinsaaret
HND|Honduras
HRV|Kroatia
HTI|Haiti
HUN|Unkari
IDN|Indonesia
IMN|Mansaari
IND|Intia
IOA|Australian Intian valtameren alueet
IRL|Irlanti
IRN|Iran
IRQ|Irak
ISL|Islanti
ISR|Israel
ITA|Italia
JAM|Jamaika
JEY|Jersey
JOR|Jordania
JPN|Japani
KAZ|Kazakstan
KEN|Kenia
KGZ|Kirgisia
KHM|Kambodža
KIR|Kiribati
KNA|Saint Kitts ja Nevis
KOR|Etelä-Korea
KWT|Kuwait
LAO|Laos
LBN|Libanon
LBR|Liberia
LBY|Libya
LCA|Saint Lucia
LIE|Liechtenstein
LKA|Sri Lanka
LSO|Lesotho
LTU|Liettua
LUX|Luxemburg
LVA|Latvia
MAC|Macao
MAF|Saint-Martin
MAR|Marokko
MCO|Monaco
MDA|Moldova
MDG|Madagaskar
MDV|Malediivit
MEX|Meksiko
MHL|Marshallinsaaret
MKD|Pohjois-Makedonia
MLI|Mali
MLT|Malta
MMR|Myanmar
MNE|Montenegro
MNG|Mongolia
MNP|Pohjois-Mariaanit
MOZ|Mosambik
MRT|Mauritania
MSR|Montserrat
MUS|Mauritius
MWI|Malawi
MYS|Malesia
NAM|Namibia
NCL|Uusi-Kaledonia
NER|Niger
NFK|Norfolkinsaari
NGA|Nigeria
NIC|Nicaragua
NIU|Niue
NLD|Alankomaat
NOR|Norja
NPL|Nepal
NRU|Nauru
NZL|Uusi-Seelanti
OMN|Oman
PAK|Pakistan
PAN|Panama
PCN|Pitcairn
PER|Peru
PHL|Filippiinit
PLW|Palau
PNG|Papua-Uusi-Guinea
POL|Puola
PRI|Puerto Rico
PRK|Pohjois-Korea
PRT|Portugali
PRY|Paraguay
PYF|Ranskan Polynesia
QAT|Qatar
ROU|Romania
RUS|Venäjä
RWA|Ruanda
SAU|Saudi-Arabia
SDN|Sudan
SDS|Etelä-Sudan
SEN|Senegal
SGP|Singapore
SGS|Etelä-Georgia ja Eteläiset Sandwichsaaret
SHN|Saint Helena
SLB|Salomonsaaret
SLE|Sierra Leone
SLV|El Salvador
SMR|San Marino
SOM|Somalia
SPM|Saint-Pierre ja Miquelon
SRB|Serbia
STP|São Tomé ja Príncipe
SUR|Suriname
SVK|Slovakia
SVN|Slovenia
SWE|Ruotsi
SWZ|Eswatini
SXM|Sint Maarten
SYC|Seychellit
SYR|Syyria
TCA|Turks- ja Caicossaaret
TCD|Tšad
TGO|Togo
THA|Thaimaa
TJK|Tadžikistan
TKM|Turkmenistan
TLS|Itä-Timor
TON|Tonga
TTO|Trinidad ja Tobago
TUN|Tunisia
TUR|Turkki
TUV|Tuvalu
TZA|Tansania
UGA|Uganda
UKR|Ukraina
URY|Uruguay
USA|Yhdysvallat
UZB|Uzbekistan
VAT|Vatikaani
VCT|Saint Vincent ja Grenadiinit
VEN|Venezuela
VGB|Brittiläiset Neitsytsaaret
VIR|Yhdysvaltain Neitsytsaaret
VNM|Vietnam
VUT|Vanuatu
WLF|Wallis ja Futuna
WSM|Samoa
YEM|Jemen
ZAF|Etelä-Afrikka
ZMB|Sambia
ZWE|Zimbabwe
CLP|Clippertoninsaari
CSI|Korallimeren saaret
ESB|Dhekelia
GIB|Gibraltar
UMI|Yhdysvaltain pienet erillissaaret
USG|Guantánamo
WSB|Akrotiri
""")

NB = table("""
ABW|Aruba
AFG|Afghanistan
AGO|Angola
AIA|Anguilla
ALB|Albania
ALD|Åland
AND|Andorra
ARE|De forente arabiske emirater
ARG|Argentina
ARM|Armenia
ASM|Amerikansk Samoa
ATC|Ashmore- og Cartierøyene
ATF|De franske sørterritorier
ATG|Antigua og Barbuda
AUS|Australia
AUT|Østerrike
AZE|Aserbajdsjan
BDI|Burundi
BEL|Belgia
BEN|Benin
BFA|Burkina Faso
BGD|Bangladesh
BGR|Bulgaria
BHR|Bahrain
BHS|Bahamas
BIH|Bosnia-Hercegovina
BLM|Saint-Barthélemy
BLR|Hviterussland
BLZ|Belize
BMU|Bermuda
BOL|Bolivia
BRA|Brasil
BRB|Barbados
BRN|Brunei
BTN|Bhutan
BWA|Botswana
CAF|Den sentralafrikanske republikk
CAN|Canada
CHE|Sveits
CHL|Chile
CHN|Kina
CIV|Elfenbenskysten
CMR|Kamerun
COD|Den demokratiske republikken Kongo
COG|Republikken Kongo
COK|Cookøyene
COL|Colombia
COM|Komorene
CPV|Kapp Verde
CRI|Costa Rica
CUB|Cuba
CUW|Curaçao
CYM|Caymanøyene
CYP|Kypros
CZE|Tsjekkia
DEU|Tyskland
DJI|Djibouti
DMA|Dominica
DNK|Danmark
DOM|Den dominikanske republikk
DZA|Algerie
ECU|Ecuador
EGY|Egypt
ERI|Eritrea
ESP|Spania
EST|Estland
ETH|Etiopia
FIN|Finland
FJI|Fiji
FRA|Frankrike
FRO|Færøyene
FSM|Mikronesiaføderasjonen
GAB|Gabon
GBR|Storbritannia
GEO|Georgia
GGY|Guernsey
GHA|Ghana
GIN|Guinea
GMB|Gambia
GNB|Guinea-Bissau
GNQ|Ekvatorial-Guinea
GRC|Hellas
GRD|Grenada
GRL|Grønland
GTM|Guatemala
GUM|Guam
GUY|Guyana
HKG|Hongkong
HMD|Heard- og McDonaldøyene
HND|Honduras
HRV|Kroatia
HTI|Haiti
HUN|Ungarn
IDN|Indonesia
IMN|Man
IND|India
IOA|Australias territorier i Indiahavet
IRL|Irland
IRN|Iran
IRQ|Irak
ISL|Island
ISR|Israel
ITA|Italia
JAM|Jamaica
JEY|Jersey
JOR|Jordan
JPN|Japan
KAZ|Kasakhstan
KEN|Kenya
KGZ|Kirgisistan
KHM|Kambodsja
KIR|Kiribati
KNA|Saint Kitts og Nevis
KOR|Sør-Korea
KWT|Kuwait
LAO|Laos
LBN|Libanon
LBR|Liberia
LBY|Libya
LCA|Saint Lucia
LIE|Liechtenstein
LKA|Sri Lanka
LSO|Lesotho
LTU|Litauen
LUX|Luxembourg
LVA|Latvia
MAC|Macao
MAF|Saint-Martin
MAR|Marokko
MCO|Monaco
MDA|Moldova
MDG|Madagaskar
MDV|Maldivene
MEX|Mexico
MHL|Marshalløyene
MKD|Nord-Makedonia
MLI|Mali
MLT|Malta
MMR|Myanmar
MNE|Montenegro
MNG|Mongolia
MNP|Nord-Marianene
MOZ|Mosambik
MRT|Mauritania
MSR|Montserrat
MUS|Mauritius
MWI|Malawi
MYS|Malaysia
NAM|Namibia
NCL|Ny-Caledonia
NER|Niger
NFK|Norfolkøya
NGA|Nigeria
NIC|Nicaragua
NIU|Niue
NLD|Nederland
NOR|Norge
NPL|Nepal
NRU|Nauru
NZL|New Zealand
OMN|Oman
PAK|Pakistan
PAN|Panama
PCN|Pitcairnøyene
PER|Peru
PHL|Filippinene
PLW|Palau
PNG|Papua Ny-Guinea
POL|Polen
PRI|Puerto Rico
PRK|Nord-Korea
PRT|Portugal
PRY|Paraguay
PYF|Fransk Polynesia
QAT|Qatar
ROU|Romania
RUS|Russland
RWA|Rwanda
SAU|Saudi-Arabia
SDN|Sudan
SDS|Sør-Sudan
SEN|Senegal
SGP|Singapore
SGS|Sør-Georgia og Sør-Sandwichøyene
SHN|St. Helena
SLB|Salomonøyene
SLE|Sierra Leone
SLV|El Salvador
SMR|San Marino
SOM|Somalia
SPM|Saint-Pierre og Miquelon
SRB|Serbia
STP|São Tomé og Príncipe
SUR|Surinam
SVK|Slovakia
SVN|Slovenia
SWE|Sverige
SWZ|Eswatini
SXM|Sint Maarten
SYC|Seychellene
SYR|Syria
TCA|Turks- og Caicosøyene
TCD|Tsjad
TGO|Togo
THA|Thailand
TJK|Tadsjikistan
TKM|Turkmenistan
TLS|Øst-Timor
TON|Tonga
TTO|Trinidad og Tobago
TUN|Tunisia
TUR|Tyrkia
TUV|Tuvalu
TZA|Tanzania
UGA|Uganda
UKR|Ukraina
URY|Uruguay
USA|USA
UZB|Usbekistan
VAT|Vatikanstaten
VCT|Saint Vincent og Grenadinene
VEN|Venezuela
VGB|De britiske Jomfruøyene
VIR|De amerikanske Jomfruøyene
VNM|Vietnam
VUT|Vanuatu
WLF|Wallis og Futuna
WSM|Samoa
YEM|Jemen
ZAF|Sør-Afrika
ZMB|Zambia
ZWE|Zimbabwe
CLP|Clippertonøya
CSI|Korallhavsøyene
ESB|Dhekelia
GIB|Gibraltar
UMI|USAs ytre småøyer
USG|Guantánamo
WSB|Akrotiri
""")

# Brazilian Portuguese where it differs from Natural Earth's NAME_PT (European)
PT_BR = table("""
ARM|Armênia
BEN|Benin
BHR|Bahrein
BLM|São Bartolomeu
CYM|Ilhas Cayman
CZE|Tchéquia
EST|Estônia
IRL|Irlanda
IRN|Irã
KEN|Quênia
LVA|Letônia
MCO|Mônaco
MDG|Madagascar
MKD|Macedônia do Norte
MUS|Maurício
NCL|Nova Caledônia
POL|Polônia
ROU|Romênia
SVN|Eslovênia
VNM|Vietnã
YEM|Iêmen
ZWE|Zimbábue
SMR|San Marino
GRL|Groenlândia
""")

# European Portuguese: Natural Earth's, with these corrections
PT_PT = table("""
BLM|São Bartolomeu
IRL|Irlanda
ZWE|Zimbabué
EGY|Egito
""")

# the everyday short form where Natural Earth gives an official or an old one, by language
FIX = {
    "en": table("""
CHN|China
USA|United States
BHS|Bahamas
GMB|Gambia
FSM|Micronesia
CZE|Czechia
TLS|Timor-Leste
CIV|Côte d'Ivoire
COD|DR Congo
COG|Republic of the Congo
VAT|Vatican City
SWZ|Eswatini
"""),
    "de": table("""
CHN|China
CYP|Zypern
MDA|Moldau
FSM|Mikronesien
USA|Vereinigte Staaten
COD|Demokratische Republik Kongo
"""),
    "el": table("""
CHN|Κίνα
IRL|Ιρλανδία
USA|Ηνωμένες Πολιτείες
FSM|Μικρονησία
"""),
    "es": table("""
FSM|Micronesia
SWZ|Esuatini
"""),
    "fr": table("""
CHN|Chine
FSM|Micronésie
"""),
    "id": table("""
CHN|Tiongkok
MKD|Makedonia Utara
IRL|Irlandia
BRN|Brunei
CZE|Ceko
"""),
    "it": table("""
USA|Stati Uniti
MCO|Monaco
VAT|Vaticano
FSM|Micronesia
JEY|Jersey
"""),
    "ja": table("""
CHN|中国
PRK|北朝鮮
KOR|韓国
USA|アメリカ合衆国
ZAF|南アフリカ
THA|タイ
MNG|モンゴル
MLI|マリ
"""),
    "ko": table("""
CHN|중국
PRK|북한
KOR|대한민국
"""),
    "nl": table("""
USA|Verenigde Staten
CHN|China
SWZ|Eswatini
"""),
    "pl": table("""
CHN|Chiny
"""),
    "ru": table("""
CHN|Китай
PRK|КНДР
KOR|Южная Корея
HTI|Гаити
"""),
    "sv": table("""
SWZ|Eswatini
"""),
    "tr": table("""
CHN|Çin
CYP|Kıbrıs
CZE|Çekya
ZAF|Güney Afrika
"""),
    "uk": table("""
CHN|Китай
PRK|Північна Корея
ZAF|Південна Африка
USA|США
"""),
    "zh_CN": table("""
CHN|中国
PRK|朝鲜
KOR|韩国
"""),
}

# the app's names of the maps (the random tab's cards, the chapters): world, the continents
MAP_NAMES = {
    "world": {"en": "World", "cs": "Svět", "da": "Verden", "de": "Welt", "el": "Κόσμος", "es": "Mundo",
              "fi": "Maailma", "fr": "Monde", "id": "Dunia", "it": "Mondo", "ja": "世界", "ko": "세계",
              "nb": "Verden", "nl": "Wereld", "pl": "Świat", "pt_BR": "Mundo", "pt_PT": "Mundo", "ru": "Мир",
              "sv": "Världen", "tr": "Dünya", "uk": "Світ", "zh_CN": "世界"},
    "europe": {"en": "Europe", "cs": "Evropa", "da": "Europa", "de": "Europa", "el": "Ευρώπη", "es": "Europa",
               "fi": "Eurooppa", "fr": "Europe", "id": "Eropa", "it": "Europa", "ja": "ヨーロッパ", "ko": "유럽",
               "nb": "Europa", "nl": "Europa", "pl": "Europa", "pt_BR": "Europa", "pt_PT": "Europa",
               "ru": "Европа", "sv": "Europa", "tr": "Avrupa", "uk": "Європа", "zh_CN": "欧洲"},
    "asia": {"en": "Asia", "cs": "Asie", "da": "Asien", "de": "Asien", "el": "Ασία", "es": "Asia", "fi": "Aasia",
             "fr": "Asie", "id": "Asia", "it": "Asia", "ja": "アジア", "ko": "아시아", "nb": "Asia", "nl": "Azië",
             "pl": "Azja", "pt_BR": "Ásia", "pt_PT": "Ásia", "ru": "Азия", "sv": "Asien", "tr": "Asya",
             "uk": "Азія", "zh_CN": "亚洲"},
    "africa": {"en": "Africa", "cs": "Afrika", "da": "Afrika", "de": "Afrika", "el": "Αφρική", "es": "África",
               "fi": "Afrikka", "fr": "Afrique", "id": "Afrika", "it": "Africa", "ja": "アフリカ", "ko": "아프리카",
               "nb": "Afrika", "nl": "Afrika", "pl": "Afryka", "pt_BR": "África", "pt_PT": "África",
               "ru": "Африка", "sv": "Afrika", "tr": "Afrika", "uk": "Африка", "zh_CN": "非洲"},
    "americas": {"en": "Americas", "cs": "Amerika", "da": "Amerika", "de": "Amerika", "el": "Αμερική",
                 "es": "América", "fi": "Amerikka", "fr": "Amériques", "id": "Amerika", "it": "Americhe",
                 "ja": "南北アメリカ", "ko": "아메리카", "nb": "Amerika", "nl": "Amerika", "pl": "Ameryka",
                 "pt_BR": "Américas", "pt_PT": "Américas", "ru": "Америка", "sv": "Amerika", "tr": "Amerika",
                 "uk": "Америка", "zh_CN": "美洲"},
    "oceania": {"en": "Oceania", "cs": "Oceánie", "da": "Oceanien", "de": "Ozeanien", "el": "Ωκεανία",
                "es": "Oceanía", "fi": "Oseania", "fr": "Océanie", "id": "Oseania", "it": "Oceania",
                "ja": "オセアニア", "ko": "오세아니아", "nb": "Oseania", "nl": "Oceanië", "pl": "Oceania",
                "pt_BR": "Oceania", "pt_PT": "Oceânia", "ru": "Океания", "sv": "Oceanien", "tr": "Okyanusya",
                "uk": "Океанія", "zh_CN": "大洋洲"},
}

# "Find: %s" in each language (the prompt; %s the place's name)
FIND = {"en": "Find: %s", "cs": "Najdi: %s", "da": "Find: %s", "de": "Finde: %s", "el": "Βρες: %s",
        "es": "Encuentra: %s", "fi": "Etsi: %s", "fr": "Trouve : %s", "id": "Temukan: %s", "it": "Trova: %s",
        "ja": "%s はどこ？", "ko": "%s 찾기", "nb": "Finn: %s", "nl": "Zoek: %s", "pl": "Znajdź: %s",
        "pt_BR": "Encontre: %s", "pt_PT": "Encontra: %s", "ru": "Найди: %s", "sv": "Hitta: %s", "tr": "Bul: %s",
        "uk": "Знайди: %s", "zh_CN": "找出：%s"}
