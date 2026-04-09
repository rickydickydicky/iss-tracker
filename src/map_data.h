#pragma once
#include <Arduino.h>

// ─── World Coastline Data for ISS Tracker ───────────────
// Detailed continent outlines for 800x480 equirectangular projection
//
// Format: arrays of int16_t pairs (latitude×10, longitude×10)
//   e.g. Melbourne -37.8°, 144.9° → { -378, 1449 }
//
// MAP_BREAK, 0 = pen-up (start new polyline segment)
// ────────────────────────────────────────────────────────

#define MAP_BREAK 9999

struct ContinentData {
    const int16_t* points;
    uint16_t totalValues;  // total int16_t values in the array
};

// ════════════════════════════════════════════════════════
// AFRICA — ~85 pts, clockwise from Morocco
// ════════════════════════════════════════════════════════
static const int16_t africa_pts[] PROGMEM = {
    // ── Northwest coast: Morocco → Senegal ──
    358,  -58,     // Tangier
    335,  -72,     // Casablanca
    310,  -96,     // Agadir
    275, -130,     // Western Sahara coast
    240, -155,     // WS south
    210, -170,     // Mauritania (Nouakchott)
    180, -163,     // Senegal north
    147, -175,     // Dakar
    133, -165,     // Gambia
    120, -150,     // Guinea-Bissau

    // ── West coast: Guinea → Ghana ──
     95, -138,     // Conakry
     85, -132,     // Sierra Leone
     63, -108,     // Monrovia, Liberia
     53,  -70,     // Côte d'Ivoire west
     52,  -40,     // Abidjan
     56,   -2,     // Accra, Ghana

    // ── Gulf of Guinea indent ──
     65,   15,     // Togo/Benin
     65,   34,     // Lagos
     55,   55,     // Niger Delta west
     45,   70,     // Niger Delta east
     40,   87,     // Cameroon coast
     36,   97,     // Douala

    // ── Bight: Equatorial Guinea → Congo ──
      4,   95,     // Libreville, Gabon
    -10,   90,     // Gabon south
    -42,  120,     // Congo (Pointe-Noire)
    -60,  122,     // Cabinda
    -88,  132,     // Luanda, Angola
   -155,  120,     // Benguela, Angola
   -178,  117,     // Namibia (Namibe)

    // ── Southwest coast ──
   -229,  145,     // Walvis Bay
   -268,  150,     // Lüderitz
   -290,  165,     // Orange River mouth
   -335,  176,     // West of Cape Town
   -340,  183,     // Cape of Good Hope
   -345,  197,     // False Bay

    // ── South coast ──
   -338,  256,     // Port Elizabeth
   -332,  272,     // Algoa Bay
   -310,  300,     // East London
   -299,  310,     // Durban

    // ── East coast: Mozambique → Somalia ──
   -262,  327,     // Maputo
   -236,  352,     // Mozambique south
   -175,  370,     // Beira
   -150,  405,     // Mozambique Channel north
   -105,  400,     // Tanzania south
    -68,  393,     // Dar es Salaam
    -40,  397,     // Mombasa
    -10,  420,     // Lamu
     20,  453,     // Mogadishu
     50,  458,     // central Somalia

    // ── Horn of Africa ──
     80,  490,     // Hafun (tip of Horn)
    116,  510,     // Cape Guardafui
    118,  500,     // Tip
    116,  431,     // Djibouti
    130,  435,     // Eritrea south

    // ── Red Sea coast ──
    155,  429,     // Eritrea coast
    180,  388,     // Eritrea north
    200,  380,     // Sudan coast
    220,  370,     // Sudan north
   MAP_BREAK, 0,

    // ── North coast: Egypt → Morocco ──
    312,  299,     // Alexandria
    320,  250,     // Sidi Barrani
    330,  230,     // Libya (Tobruk)
    329,  200,     // Benghazi
    320,  150,     // Sirte
    329,  132,     // Tripoli
    340,  110,     // Tunisia east
    370,  102,     // Tunis
    368,   80,     // Tunisia north
    367,   30,     // Algiers
    358,   -5,     // Oran
    358,  -58,     // Tangier (close loop)
   MAP_BREAK, 0,

    // ── Madagascar ──
   -123,  493,     // North tip (Diego Suarez)
   -156,  500,     // NE coast
   -180,  498,     // East coast
   -210,  475,     // SE coast
   -256,  452,     // South tip (Fort Dauphin)
   -230,  437,     // SW coast
   -185,  440,     // West coast
   -160,  445,     // NW coast
   -123,  493,     // Close
};

// ════════════════════════════════════════════════════════
// EUROPE — ~130 pts, mainland + islands
// ════════════════════════════════════════════════════════
static const int16_t europe_pts[] PROGMEM = {
    // ── Iberian Peninsula: Portugal → Med ──
    387,  -91,     // Lisbon
    394,  -88,     // Cabo da Roca
    410,  -87,     // Porto
    432,  -85,     // NW Spain (La Coruña)
    435,  -58,     // Santander
    433,  -29,     // Bilbao
    433,  -18,     // San Sebastián
    463,  -15,     // Biarritz / SW France

    // ── France: Atlantic → Med ──
    475,  -18,     // Bordeaux area
    485,  -30,     // Brittany south
    484,  -45,     // Brittany west
    490,  -35,     // Brittany north
    495,  -15,     // Normandy
    510,  -10,     // Cherbourg
    507,   15,     // Le Havre
    492,   -4,     // Brest area
   MAP_BREAK, 0,

    // ── France Med coast → Italy ──
    433,   54,     // Marseille
    437,   63,     // Toulon
    437,   72,     // Nice
    440,   88,     // Genoa
    444,   89,     // La Spezia

    // ── Italy boot ──
    443,  100,     // Livorno
    430,  109,     // Rome coast (Ostia)
    413,  133,     // Naples
    403,  152,     // Calabria west
    385,  162,     // Calabria toe
    382,  167,     // Reggio Calabria
    395,  172,     // Calabria east (Crotone)
    407,  167,     // Taranto
    413,  173,     // Brindisi
    420,  162,     // Bari
    437,  142,     // Ancona
    445,  124,     // Rimini
    452,  132,     // Venice
   MAP_BREAK, 0,

    // ── Sicily ──
    383,  136,     // Palermo
    376,  143,     // SW Sicily
    370,  150,     // South Sicily
    373,  152,     // Syracuse
    382,  155,     // Catania
    383,  150,     // NE tip (Messina)
    383,  136,     // Close
   MAP_BREAK, 0,

    // ── Sardinia ──
    410,   85,     // North
    400,   96,     // NE
    390,   95,     // SE
    390,   84,     // SW
    405,   82,     // NW
   MAP_BREAK, 0,

    // ── Corsica ──
    425,   90,     // North (Bastia)
    418,   92,     // East
    414,   90,     // SE
    415,   86,     // SW
    425,   87,     // NW
   MAP_BREAK, 0,

    // ── Greece + Aegean ──
    394,  206,     // Peloponnese SW
    379,  215,     // Peloponnese S tip
    373,  228,     // Peloponnese SE
    380,  237,     // Athens (Attica)
    388,  240,     // Evia
    397,  230,     // Thessaloniki area
    407,  247,     // Thrace
    414,  258,     // Turkey border
   MAP_BREAK, 0,

    // ── Crete ──
    353,  240,     // West Crete
    350,  250,     // South Crete
    352,  262,     // East Crete
    355,  255,     // NE Crete
    353,  240,     // Close
   MAP_BREAK, 0,

    // ── Turkey: Aegean → Black Sea ──
    414,  258,     // Thrace
    410,  290,     // Istanbul
    420,  300,     // Bosporus north
    418,  328,     // Sinop (Black Sea)
    410,  370,     // Trabzon

    // ── Black Sea north coast ──
    442,  287,     // Romania (Constanța)
    464,  306,     // Odesa
    450,  335,     // Crimea west
    448,  340,     // Crimea south
    452,  360,     // Crimea east
    470,  388,     // Azov / Rostov
   MAP_BREAK, 0,

    // ── North Europe: Netherlands → Baltics ──
    524,   49,     // Amsterdam
    535,   70,     // Frisian Islands
    538,   89,     // Hamburg
    543,  100,     // Kiel
    555,  105,     // Denmark east
    557,  126,     // Copenhagen
    565,  130,     // Sweden SW
    582,  160,     // Gothenburg
    593,  181,     // Stockholm area
    597,  195,     // Åland
    603,  248,     // Helsinki
    593,  247,     // Estonia
    580,  240,     // Riga
    560,  210,     // Lithuania
    545,  195,     // Kaliningrad
    543,  186,     // Gdańsk
   MAP_BREAK, 0,

    // ── British Isles: England + Wales ──
    501,  -57,     // Cornwall (Land's End)
    510,  -53,     // Devon
    513,  -35,     // Bristol Channel
    518,  -46,     // Pembroke (Wales)
    530,  -37,     // Cardigan
    534,  -30,     // Liverpool area
    540,  -30,     // Lancashire
    548,  -32,     // Cumbria
    550,  -16,     // Newcastle
    537,   -4,     // Hull
    526,   17,     // East Anglia (Norwich)
    518,   14,     // Ipswich
    515,    8,     // Thames Estuary
    511,   13,     // Dover
    508,    0,     // Brighton
    506,  -15,     // Bournemouth
    504,  -34,     // Exeter
    501,  -57,     // Cornwall (close)
   MAP_BREAK, 0,

    // ── Scotland ──
    558,  -35,     // Borders
    566,  -55,     // Glasgow
    575,  -52,     // Highlands SW
    584,  -50,     // Cape Wrath
    582,  -33,     // Highlands NE
    577,  -18,     // Aberdeen
    568,  -24,     // Dundee
    558,  -35,     // Close
   MAP_BREAK, 0,

    // ── Ireland ──
    518,  -96,     // Dublin
    524,  -83,     // NE coast
    530, -100,     // Donegal
    528, -103,     // Mayo
    524, -100,     // Galway
    517,  -97,     // Clare
    514, -101,     // Kerry
    519,  -91,     // Cork
    518,  -96,     // Close
   MAP_BREAK, 0,

    // ── Iceland ──
    660, -225,     // NW
    663, -185,     // North
    660, -135,     // NE
    641, -140,     // SE
    637, -200,     // South coast
    640, -230,     // SW
    660, -225,     // Close
   MAP_BREAK, 0,

    // ── Scandinavia: Norway coast ──
    590,   57,     // Stavanger
    602,   53,     // Bergen
    625,   65,     // Ålesund
    634,  104,     // Trondheim
    665,  130,     // Bodø
    685,  155,     // Lofoten
    695,  170,     // Tromsø
    700,  200,     // Hammerfest
    710,  258,     // Nordkapp
    702,  300,     // Varanger
    689,  331,     // Murmansk
};

// ════════════════════════════════════════════════════════
// ASIA — ~170 pts, Middle East → Pacific
// ════════════════════════════════════════════════════════
static const int16_t asia_pts[] PROGMEM = {
    // ── Arabian Peninsula ──
    300,  325,     // Suez / Sinai NE
    295,  330,     // Sinai south
    280,  335,     // Saudi Red Sea
    240,  370,     // Jeddah
    200,  395,     // Jizan
    155,  429,     // Eritrea/Yemen border (Bab el-Mandeb)
    130,  450,     // Aden
    145,  520,     // Oman south coast
    175,  550,     // Oman mid-coast
    223,  595,     // Muscat / Ras al Hadd
    240,  575,     // Oman NE tip
    253,  565,     // UAE (Fujairah)
    264,  556,     // UAE (Dubai)
    260,  540,     // Qatar tip
    267,  510,     // Qatar west
    292,  505,     // Bahrain area
    300,  490,     // Kuwait
    305,  480,     // Iraq (Basra)
    310,  470,     // Shatt al-Arab

    // ── Levant coast ──
    337,  358,     // Levant (Turkey border)
    335,  355,     // Latakia
    330,  350,     // Lebanon (Beirut)
    325,  347,     // Haifa
    318,  343,     // Tel Aviv
    313,  323,     // Port Said
    312,  299,     // Alexandria (connect to Africa)
   MAP_BREAK, 0,

    // ── Iran / Caspian ──
    310,  470,     // Shatt al-Arab / Basra
    275,  510,     // Iran south (Bushehr)
    265,  540,     // Bandar Abbas
    255,  580,     // Iran SE coast (Jask)
    254,  625,     // Pakistan border (Gwadar)
   MAP_BREAK, 0,

    // ── South Asia: Pakistan → India ──
    254,  625,     // Karachi
    230,  687,     // Mumbai
    172,  732,     // Goa
    150,  738,     // Karnataka coast
    120,  750,     // Mangalore
     83,  769,     // Kerala (Cochin)
     68,  766,     // Trivandrum
     68,  797,     // Kanyakumari (S tip)
     80,  800,     // SE coast start
     93,  802,     // Rameswaram
    110,  800,     // Tamil Nadu
    130,  803,     // Pondicherry
    158,  815,     // Andhra coast
    173,  833,     // Visakhapatnam
    200,  860,     // Odisha
    215,  870,     // Kolkata / Bengal
    224,  888,     // Bangladesh
    220,  900,     // Chittagong
    210,  927,     // Myanmar coast
    185,  945,     // Myanmar mid
    165,  978,     // Myanmar south (Irrawaddy delta)
   MAP_BREAK, 0,

    // ── Sri Lanka ──
     70,  798,     // NW (Jaffna)
     80,  820,     // NE
     65,  817,     // East coast
     60,  806,     // South
     68,  797,     // West coast
     70,  798,     // Close
   MAP_BREAK, 0,

    // ── SE Asia: Thailand → Singapore ──
    145,  985,     // Thailand south (Isthmus of Kra)
    137,  998,     // Thailand south
    100, 1003,     // Malay Peninsula north
     70, 1013,     // Penang
     40, 1030,     // Kuala Lumpur coast
     14, 1039,     // Singapore
   MAP_BREAK, 0,

    // ── Vietnam / Cambodia ──
    100, 1065,     // Vietnam south (Ca Mau)
    105, 1078,     // Ho Chi Minh City area
    122, 1092,     // Central Vietnam south
    163, 1084,     // Đà Nẵng
    183, 1063,     // Hue
    210, 1060,     // Hanoi area
    214, 1064,     // Haiphong
    220, 1080,     // China border
   MAP_BREAK, 0,

    // ── Indonesia: Sumatra ──
     55, 1045,     // NE Sumatra (Medan)
     20, 1040,     // Equator
    -10, 1044,     // Padang area
    -30, 1050,     // Bengkulu
    -55, 1058,     // Lampung
    -60, 1054,     // Sunda Strait
   MAP_BREAK, 0,

    // ── Java ──
    -65, 1065,     // West Java
    -70, 1095,     // Jakarta
    -73, 1105,     // Central Java
    -78, 1125,     // East Java
    -84, 1141,     // Bali Strait
   MAP_BREAK, 0,

    // ── Borneo ──
     40, 1100,     // Brunei / North
     20, 1100,     // NW coast
    -10, 1095,     // West coast
    -30, 1105,     // SW coast
    -20, 1160,     // SE coast
      0, 1180,     // NE coast
     40, 1170,     // Sabah
     70, 1165,     // NE tip
     40, 1100,     // Close
   MAP_BREAK, 0,

    // ── Philippines: Luzon ──
    185, 1210,     // North Luzon
    170, 1220,     // East coast
    145, 1210,     // South
    147, 1200,     // West coast
    165, 1200,     // Manila area
    185, 1210,     // Close
   MAP_BREAK, 0,

    // ── Philippines: Mindanao ──
     80, 1260,     // NE
     60, 1265,     // East
     55, 1255,     // South
     65, 1240,     // West
     80, 1240,     // NW
     80, 1260,     // Close
   MAP_BREAK, 0,

    // ── China coast ──
    220, 1080,     // Vietnam border
    226, 1086,     // Guangxi
    230, 1100,     // Guangdong west
    233, 1132,     // Hong Kong / Shenzhen
    235, 1140,     // Guangdong east
    240, 1160,     // Fujian south
    250, 1185,     // Xiamen
    265, 1200,     // Fuzhou
    290, 1218,     // Wenzhou
    301, 1214,     // Shanghai
    320, 1215,     // Jiangsu coast
    350, 1195,     // Shandong south
    365, 1210,     // Shandong east tip
    375, 1195,     // Shandong north
    390, 1215,     // Liaodong tip
    395, 1235,     // Dalian
    410, 1240,     // Liaoning coast
   MAP_BREAK, 0,

    // ── Taiwan ──
    252, 1207,     // South tip
    235, 1210,     // SW
    230, 1205,     // West coast
    250, 1215,     // NW
    252, 1218,     // North tip
    250, 1220,     // NE
    235, 1213,     // East coast
    252, 1207,     // Close
   MAP_BREAK, 0,

    // ── Korean Peninsula ──
    380, 1268,     // Busan
    355, 1294,     // Ulsan / East coast
    378, 1290,     // Gangneung
    385, 1280,     // DMZ east
    395, 1275,     // North Korea east
    415, 1310,     // Rason / Russia border
   MAP_BREAK, 0,
    389, 1260,     // West coast mid
    378, 1265,     // South coast (Mokpo)
    370, 1269,     // Jeju area
   MAP_BREAK, 0,

    // ── Japan: Honshū ──
    368, 1399,     // Tokyo
    370, 1410,     // Chiba
    382, 1415,     // Ibaraki
    395, 1405,     // Sendai
    406, 1398,     // Northern Honshū
    415, 1400,     // Aomori
    400, 1378,     // Niigata
    380, 1370,     // Kanazawa
    360, 1370,     // Kyoto area
    350, 1358,     // Osaka
    340, 1350,     // Wakayama
    350, 1370,     // Nagoya
   MAP_BREAK, 0,

    // ── Japan: Hokkaidō ──
    430, 1410,     // South (Hakodate)
    435, 1430,     // East coast
    445, 1445,     // NE (Shiretoko)
    438, 1440,     // SE
    430, 1410,     // Close
   MAP_BREAK, 0,

    // ── Japan: Kyūshū ──
    334, 1304,     // South tip (Kagoshima)
    335, 1315,     // East coast
    340, 1320,     // NE
    338, 1300,     // West coast
    334, 1304,     // Close
   MAP_BREAK, 0,

    // ── Japan: Shikoku ──
    340, 1330,     // North
    335, 1340,     // East
    330, 1330,     // South
    335, 1320,     // West
    340, 1330,     // Close
   MAP_BREAK, 0,

    // ── Russia: Pacific coast ──
    430, 1310,     // Vladivostok
    450, 1340,     // Khabarovsk area
    485, 1370,     // Sakhalin south
    510, 1420,     // Sakhalin north
    530, 1430,     // Tatar Strait
    545, 1440,     // Amur area
    565, 1480,     // Kamchatka south
    585, 1500,     // Kamchatka west
    600, 1580,     // Kamchatka east tip
    605, 1598,     // Kamchatka NE
    625, 1620,     // Kamchatka north
    650, 1700,     // Chukotka
    660, 1780,     // Chukotka east (near Dateline)
   MAP_BREAK, 0,

    // ── Russia: Arctic coast (west → east) ──
    689,  331,     // Murmansk (connect to Europe)
    690,  380,     // White Sea
    645,  410,     // Arkhangelsk
    675,  500,     // Novaya Zemlya area
    700,  600,     // Yamal west
    710,  680,     // Yamal east
    720,  760,     // Ob Gulf
    730,  850,     // Taimyr west
    750,  870,     // Taimyr north
    740, 1000,     // Taimyr east
    730, 1050,     // Laptev Sea
    720, 1200,     // New Siberian Islands area
    715, 1300,     // East Siberia
    690, 1500,     // Chukchi coast
    660, 1700,     // Connect to Chukotka
};

// ════════════════════════════════════════════════════════
// NORTH AMERICA — ~120 pts
// ════════════════════════════════════════════════════════
static const int16_t north_america_pts[] PROGMEM = {
    // ── East coast: Newfoundland → Florida ──
    475, -528,     // Newfoundland SE
    487, -555,     // Newfoundland south
    470, -580,     // Nova Scotia NE
    447, -635,     // Nova Scotia SW
    458, -660,     // Bay of Fundy
    445, -670,     // Maine
    433, -700,     // Boston
    420, -720,     // Cape Cod
    409, -740,     // New York
    396, -743,     // New Jersey
    389, -753,     // Delaware Bay
    380, -760,     // Chesapeake Bay entrance
    375, -762,     // Virginia Beach
    358, -758,     // Outer Banks (NC)
    340, -778,     // Cape Hatteras
    326, -797,     // Charleston
    320, -808,     // Savannah
    305, -813,     // Jacksonville
    288, -806,     // Cape Canaveral

    // ── Florida ──
    275, -802,     // Fort Pierce
    265, -800,     // Palm Beach
    258, -801,     // Miami
    252, -812,     // Florida Keys
    250, -818,     // Key West
    260, -826,     // Naples
    268, -826,     // Fort Myers
    275, -828,     // Tampa area
    280, -830,     // Tampa Bay
    295, -833,     // Cedar Key

    // ── Gulf coast: Florida → Texas ──
    300, -845,     // Apalachee Bay
    304, -860,     // Apalachicola
    305, -873,     // Pensacola
    303, -884,     // Mobile Bay
    299, -895,     // Biloxi
    295, -900,     // Mississippi
    296, -905,     // New Orleans area
    293, -912,     // Louisiana
    293, -920,     // Vermilion Bay
    296, -935,     // Louisiana west
    290, -950,     // Texas east
    284, -965,     // Corpus Christi
    267, -973,     // South Texas
    262, -972,     // Mexican border (Brownsville)
   MAP_BREAK, 0,

    // ── Mexico Gulf coast → Yucatan ──
    232, -978,     // Tampico
    195, -963,     // Veracruz
    185, -955,     // Coatzacoalcos
    180, -935,     // Tabasco
    186, -910,     // Campeche
    195, -900,     // Yucatan west
    210, -870,     // Yucatan north
    212, -868,     // Cancun
    200, -875,     // Yucatan east coast
    190, -880,     // Belize
    160, -890,     // Honduras coast
   MAP_BREAK, 0,

    // ── Central America Pacific ──
    167, -952,     // Guatemala Pacific
    153, -932,     // El Salvador
    135, -900,     // Nicaragua
    120, -870,     // Costa Rica
    100, -840,     // Panama west
     85, -800,     // Panama Canal area
     97, -795,     // Panama east
   MAP_BREAK, 0,

    // ── Mexico Pacific → California ──
    167, -952,     // Guatemala (connect)
    165, -960,     // Mexico south Pacific
    175, -985,     // Acapulco
    190,-1010,     // Zihuatanejo
    200,-1030,     // Manzanillo
    215,-1050,     // Mazatlan
    230,-1080,     // Los Cabos (Baja tip)
   MAP_BREAK, 0,

    // ── Baja California ──
    230,-1080,     // Cabo San Lucas
    248,-1100,     // La Paz
    270,-1120,     // Baja mid-east
    284,-1133,     // Tijuana
   MAP_BREAK, 0,

    // ── US West coast ──
    284,-1133,     // Tijuana/San Diego
    327,-1172,     // Los Angeles
    347,-1195,     // Point Conception
    368,-1218,     // Monterey
    378,-1224,     // San Francisco
    395,-1237,     // Cape Mendocino
    424,-1240,     // Oregon coast
    440,-1241,     // Astoria
    467,-1238,     // Washington coast
    485,-1238,     // Olympic Peninsula
    490,-1264,     // Vancouver Island west
    492,-1235,     // Vancouver
   MAP_BREAK, 0,

    // ── Alaska ──
    548,-1340,     // Panhandle south
    560,-1360,     // Juneau area
    575,-1450,     // Yakutat
    583,-1490,     // Seward
    585,-1496,     // Anchorage area
    605,-1520,     // Kenai
    575,-1580,     // Kodiak
    570,-1620,     // Alaska Peninsula
    555,-1640,     // Unimak
    540,-1690,     // Aleutians start
    528,-1720,     // Central Aleutians
    520,-1760,     // Western Aleutians
   MAP_BREAK, 0,

    // ── Alaska North Slope → Arctic Canada ──
    650,-1620,     // Barrow area
    690,-1550,     // Prudhoe Bay area
    700,-1450,     // Mackenzie delta
    705,-1380,     // Banks Island area
    695,-1250,     // Victoria Island area
    720,-1150,     // King William
    720, -950,     // Baffin west
    700, -850,     // Hudson Bay NW
    625, -780,     // Hudson Bay SW
    570, -790,     // James Bay south
    575, -790,     // Hudson Bay east shore
    600, -780,     // Ungava west
    620, -710,     // Ungava Bay
    595, -660,     // Labrador
    530, -595,     // Labrador south
    475, -528,     // Newfoundland (close to east coast)
   MAP_BREAK, 0,

    // ── Greenland ──
    830, -300,     // North tip
    800, -200,     // NE coast
    770, -180,     // East coast
    680, -370,     // SE coast
    600, -440,     // South tip (Cape Farewell)
    630, -510,     // SW coast
    660, -530,     // West coast
    700, -550,     // Disko Bay
    770, -600,     // NW coast
    830, -300,     // Close
};

// ════════════════════════════════════════════════════════
// SOUTH AMERICA — ~65 pts, clockwise from Venezuela
// ════════════════════════════════════════════════════════
static const int16_t south_america_pts[] PROGMEM = {
    // ── North coast ──
    108, -720,     // Venezuela (Caracas coast)
    107, -680,     // Venezuela east
     84, -620,     // Trinidad area
     78, -600,     // Guyana
     52, -520,     // French Guiana
     25, -500,     // Amapá

    // ── Brazil NE bulge ──
     10, -480,     // Amazon mouth (Belém)
    -10, -475,     // Maranhão
    -25, -441,     // Piauí coast
    -47, -410,     // Fortaleza
    -70, -375,     // Paraíba
    -81, -349,     // Natal (easternmost point)
    -85, -350,     // Recife
   -100, -362,     // Alagoas
   -130, -387,     // Salvador

    // ── Brazil east coast ──
   -180, -398,     // Bahia south
   -200, -402,     // Espírito Santo
   -228, -413,     // Rio de Janeiro
   -240, -435,     // Paraty
   -252, -463,     // Florianópolis
   -300, -497,     // Porto Alegre

    // ── Río de la Plata → Patagonia ──
   -349, -567,     // Montevideo
   -350, -575,     // Colonia
   -385, -583,     // Buenos Aires
   -390, -620,     // Bahía Blanca
   -413, -625,     // Patagonia north
   -430, -650,     // Patagonia mid
   -462, -668,     // Santa Cruz
   -500, -685,     // Patagonia south
   -520, -694,     // Río Gallegos
   -540, -700,     // Strait of Magellan
   -548, -680,     // Tierra del Fuego south
   -540, -704,     // Cape Horn
   MAP_BREAK, 0,

    // ── West coast (south to north) ──
   -540, -704,     // Cape Horn
   -510, -745,     // Chile far south
   -465, -758,     // Puerto Montt
   -420, -740,     // Valdivia
   -395, -735,     // Valparaíso
   -355, -720,     // La Serena
   -300, -710,     // Copiapó
   -240, -705,     // Arica (Chile/Peru border)
   -178, -763,     // Lima
   -140, -770,     // Peru mid-coast
   -120, -772,     // Northern Peru
    -50, -812,     // Ecuador coast
    -10, -802,     // Esmeraldas (Ecuador)
     20, -790,     // Colombia Pacific (Tumaco)
     38, -777,     // Buenaventura
     55, -769,     // Colombia NW
     70, -770,     // Cartagena area
     92, -763,     // Colombia north (Santa Marta)
    108, -720,     // Venezuela (close)
};

// ════════════════════════════════════════════════════════
// AUSTRALIA — ~55 pts + New Zealand
// ════════════════════════════════════════════════════════
static const int16_t australia_pts[] PROGMEM = {
    // ── Clockwise from Darwin ──
   -124, 1309,     // Darwin
   -118, 1322,     // Arnhem Land west
   -120, 1360,     // Arnhem Land east
   -130, 1368,     // Gulf of Carpentaria west
   -160, 1380,     // Gulf south
   -172, 1406,     // Gulf bottom
   -145, 1415,     // Gulf east
   -120, 1423,     // Cape York west
   -107, 1424,     // Cape York tip

    // ── East coast: Queensland → Victoria ──
   -140, 1440,     // Cairns
   -165, 1460,     // Townsville
   -195, 1464,     // Mackay
   -218, 1490,     // Gladstone
   -233, 1504,     // Bundaberg
   -260, 1530,     // Brisbane area
   -272, 1533,     // Gold Coast
   -295, 1534,     // Coffs Harbour
   -313, 1523,     // Newcastle
   -330, 1515,     // Sydney
   -345, 1510,     // Wollongong
   -370, 1500,     // Batemans Bay
   -378, 1499,     // Melbourne area
   -385, 1497,     // Bass Strait

    // ── Tasmania ──
   MAP_BREAK, 0,
   -410, 1462,     // Devonport
   -418, 1475,     // NE Tasmania
   -435, 1480,     // East coast
   -435, 1473,     // South
   -423, 1455,     // West coast
   -410, 1462,     // Close
   MAP_BREAK, 0,

    // ── South coast: SA → WA ──
   -370, 1399,     // Adelaide
   -365, 1380,     // Spencer Gulf
   -340, 1370,     // Eyre Peninsula
   -330, 1340,     // Ceduna
   -320, 1296,     // Nullarbor east
   -318, 1250,     // Nullarbor west
   -320, 1220,     // Esperance
   -310, 1180,     // Albany
   -280, 1150,     // SW corner
   -235, 1136,     // Perth
   -220, 1140,     // Geraldton area
   -200, 1166,     // Shark Bay
   -185, 1200,     // Exmouth
   -178, 1219,     // Broome area
   -160, 1250,     // Kimberley
   -145, 1280,     // Darwin area (close)
   MAP_BREAK, 0,

    // ── New Zealand: North Island ──
   -347, 1727,     // Auckland
   -353, 1745,     // Coromandel
   -370, 1781,     // East Cape
   -395, 1775,     // Hawke's Bay
   -412, 1749,     // Wellington
   -395, 1740,     // West coast
   -378, 1735,     // Taranaki
   -365, 1740,     // Waikato
   -347, 1727,     // Close
   MAP_BREAK, 0,

    // ── New Zealand: South Island ──
   -412, 1740,     // Marlborough
   -420, 1735,     // West coast north
   -437, 1700,     // West coast south (Glaciers)
   -455, 1680,     // Fiordland
   -465, 1682,     // Invercargill
   -460, 1700,     // Dunedin
   -440, 1718,     // Christchurch
   -425, 1730,     // Canterbury
   -412, 1740,     // Close
};

// ════════════════════════════════════════════════════════
// ANTARCTICA — simplified outline
// ════════════════════════════════════════════════════════
static const int16_t antarctica_pts[] PROGMEM = {
   -630, -600,     // Antarctic Peninsula tip
   -650, -500,     // Peninsula mid
   -700, -300,     // Weddell Sea
   -700,    0,     // Queen Maud Land
   -680,  300,     // Enderby Land
   -680,  500,     // Mac Robertson Land
   -665,  800,     // Wilkes Land west
   -665,  900,     // Wilkes Land east
   -680, 1100,     // Adélie Land
   -710, 1500,     // Ross Sea area
   -720, 1700,     // Marie Byrd Land
   -700,-1600,     // Pacific side
   -680,-1200,     // Amundsen Sea
   -680, -900,     // Bellingshausen Sea
   -660, -700,     // Alexander Island
   -630, -600,     // Close at Peninsula
};

// ════════════════════════════════════════════════════════
// MASTER INDEX
// ════════════════════════════════════════════════════════

static const ContinentData continents[] = {
    { africa_pts,         sizeof(africa_pts)         / sizeof(int16_t) },
    { europe_pts,         sizeof(europe_pts)         / sizeof(int16_t) },
    { asia_pts,           sizeof(asia_pts)           / sizeof(int16_t) },
    { north_america_pts,  sizeof(north_america_pts)  / sizeof(int16_t) },
    { south_america_pts,  sizeof(south_america_pts)  / sizeof(int16_t) },
    { australia_pts,      sizeof(australia_pts)       / sizeof(int16_t) },
    { antarctica_pts,     sizeof(antarctica_pts)      / sizeof(int16_t) },
};

static const int NUM_CONTINENTS = sizeof(continents) / sizeof(ContinentData);
