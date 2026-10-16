/*
 * Jigsaw's pictures: one line a picture. The tarot's 1898 faces (cards.h's cards_tarot_art: 0 the excuse, 1..21 the
 * trumps, 22..37 the courts), then the public-domain paintings (art/jigsaw/<file>.jpg, about 1024 px, 150-200 KB:
 * tools/jigsaw_prep.py; their record: tools/jigsaw_paintings.txt).
 * Adding a painting: its file in art/jigsaw (tools/jigsaw_prep.py makes it), its line here at the end of the list
 * ({-1, file, width, height, set, the artist's death year}), then tools/jigsaw_gen.c run again for the levels (a new
 * picture's levels go at the end of their chapter's list: the levels after it move on by one, so add pictures before
 * a release, not after it).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#ifndef JIGSAW_PICTURES_H
#define JIGSAW_PICTURES_H

#include "games/jigsaw.h"

static const jig_picture JIG_PICS[] = {
    /* the trumps 1..21, then the excuse */
    {1, 0, 400, 640, JS_TRUMPS, 0},
    {2, 0, 400, 640, JS_TRUMPS, 0},
    {3, 0, 400, 640, JS_TRUMPS, 0},
    {4, 0, 400, 640, JS_TRUMPS, 0},
    {5, 0, 400, 640, JS_TRUMPS, 0},
    {6, 0, 400, 640, JS_TRUMPS, 0},
    {7, 0, 400, 640, JS_TRUMPS, 0},
    {8, 0, 400, 640, JS_TRUMPS, 0},
    {9, 0, 400, 640, JS_TRUMPS, 0},
    {10, 0, 400, 640, JS_TRUMPS, 0},
    {11, 0, 400, 640, JS_TRUMPS, 0},
    {12, 0, 400, 640, JS_TRUMPS, 0},
    {13, 0, 400, 640, JS_TRUMPS, 0},
    {14, 0, 400, 640, JS_TRUMPS, 0},
    {15, 0, 400, 640, JS_TRUMPS, 0},
    {16, 0, 400, 640, JS_TRUMPS, 0},
    {17, 0, 400, 640, JS_TRUMPS, 0},
    {18, 0, 400, 640, JS_TRUMPS, 0},
    {19, 0, 400, 640, JS_TRUMPS, 0},
    {20, 0, 400, 640, JS_TRUMPS, 0},
    {21, 0, 400, 640, JS_TRUMPS, 0},
    {0, 0, 400, 640, JS_TRUMPS, 0},
    /* the courts: jack, knight, queen, king of spades, hearts, diamonds, clubs */
    {22, 0, 400, 640, JS_COURTS, 0},
    {23, 0, 400, 640, JS_COURTS, 0},
    {24, 0, 400, 640, JS_COURTS, 0},
    {25, 0, 400, 640, JS_COURTS, 0},
    {26, 0, 400, 640, JS_COURTS, 0},
    {27, 0, 400, 640, JS_COURTS, 0},
    {28, 0, 400, 640, JS_COURTS, 0},
    {29, 0, 400, 640, JS_COURTS, 0},
    {30, 0, 400, 640, JS_COURTS, 0},
    {31, 0, 400, 640, JS_COURTS, 0},
    {32, 0, 400, 640, JS_COURTS, 0},
    {33, 0, 400, 640, JS_COURTS, 0},
    {34, 0, 400, 640, JS_COURTS, 0},
    {35, 0, 400, 640, JS_COURTS, 0},
    {36, 0, 400, 640, JS_COURTS, 0},
    {37, 0, 400, 640, JS_COURTS, 0},
    /* the paintings */
    {-1, "hokusai-great-wave", 1024, 706, JS_JAPAN, 1849},  /* The Great Wave off Kanagawa, Katsushika Hokusai */
    {-1, "hokusai-red-fuji", 1024, 691, JS_JAPAN, 1849},  /* Fine Wind, Clear Morning (Red Fuji), Katsushika Hokusai */
    {-1, "hokusai-kirifuri", 694, 1024, JS_JAPAN, 1849},  /* Kirifuri Waterfall at Mount Kurokami, Katsushika Hokusai */
    {-1, "hiroshige-shower", 676, 1024, JS_JAPAN, 1858},  /* Sudden Shower over Shin-Ōhashi Bridge, Hiroshige */
    {-1, "hiroshige-plum", 704, 1024, JS_JAPAN, 1858},  /* Plum Park in Kameido, Utagawa Hiroshige */
    {-1, "monet-water-lilies", 820, 1024, JS_IMPRESSION, 1926},  /* Bridge over a Pond of Water Lilies, Claude Monet */
    {-1, "monet-impression-sunrise", 1024, 794, JS_IMPRESSION, 1926},  /* Impression, Sunrise, Claude Monet */
    {-1, "monet-parasol", 824, 1024, JS_IMPRESSION, 1926},  /* Woman with a Parasol, Claude Monet */
    {-1, "monet-poppies", 1024, 781, JS_IMPRESSION, 1926},  /* Poppy Field, Claude Monet */
    {-1, "renoir-moulin", 1024, 761, JS_IMPRESSION, 1919},  /* Dance at Le Moulin de la Galette, Renoir */
    {-1, "renoir-luncheon", 1024, 758, JS_IMPRESSION, 1919},  /* Luncheon of the Boating Party, Pierre-Auguste Renoir */
    {-1, "degas-ballet-class", 892, 1024, JS_IMPRESSION, 1917},  /* The Ballet Class, Edgar Degas */
    {-1, "caillebotte-rainy-day", 1024, 776, JS_IMPRESSION, 1894},  /* Paris Street; Rainy Day, Gustave Caillebotte */
    {-1, "cassatt-childs-bath", 671, 1024, JS_IMPRESSION, 1926},  /* The Child's Bath, Mary Cassatt */
    {-1, "seurat-grande-jatte", 1024, 682, JS_IMPRESSION, 1891},  /* A Sunday on La Grande Jatte, Georges Seurat */
    {-1, "vangogh-starry-night", 1024, 791, JS_POST, 1890},  /* The Starry Night, Vincent van Gogh */
    {-1, "vangogh-sunflowers", 806, 1024, JS_POST, 1890},  /* Sunflowers, Vincent van Gogh */
    {-1, "vangogh-cafe-terrace", 780, 1024, JS_POST, 1890},  /* Café Terrace at Night, Vincent van Gogh */
    {-1, "vangogh-irises", 1024, 782, JS_POST, 1890},  /* Irises, Vincent van Gogh */
    {-1, "vangogh-wheat-cypresses", 1024, 806, JS_POST, 1890},  /* Wheat Field with Cypresses, Vincent van Gogh */
    {-1, "vangogh-bedroom", 1024, 794, JS_POST, 1890},  /* The Bedroom, Vincent van Gogh */
    {-1, "vangogh-almond", 1024, 809, JS_POST, 1890},  /* Almond Blossom, Vincent van Gogh */
    {-1, "cezanne-sainte-victoire", 1024, 830, JS_POST, 1906},  /* Mont Sainte-Victoire, Paul Cézanne */
    {-1, "rousseau-surprised", 1024, 819, JS_POST, 1910},  /* Surprised! (Tiger in a Tropical Storm), Henri Rousseau */
    {-1, "klimt-kiss", 1021, 1024, JS_POST, 1918},  /* The Kiss, Gustav Klimt */
    {-1, "mucha-seasons", 1024, 701, JS_POST, 1939},  /* The Seasons, Alphonse Mucha */
    {-1, "vermeer-pearl", 875, 1024, JS_MASTERS, 1675},  /* Girl with a Pearl Earring, Johannes Vermeer */
    {-1, "vermeer-milkmaid", 913, 1024, JS_MASTERS, 1675},  /* The Milkmaid, Johannes Vermeer */
    {-1, "vermeer-delft", 1024, 854, JS_MASTERS, 1675},  /* View of Delft, Johannes Vermeer */
    {-1, "leonardo-mona-lisa", 687, 1024, JS_MASTERS, 1519},  /* Mona Lisa, Leonardo da Vinci */
    {-1, "bruegel-hunters", 1024, 729, JS_MASTERS, 1569},  /* Hunters in the Snow, Pieter Bruegel the Elder */
    {-1, "bruegel-babel", 1024, 750, JS_MASTERS, 1569},  /* The Tower of Babel, Pieter Bruegel the Elder */
    {-1, "rembrandt-night-watch", 1024, 833, JS_MASTERS, 1669},  /* The Night Watch, Rembrandt van Rijn */
    {-1, "vaneyck-arnolfini", 749, 1024, JS_MASTERS, 1441},  /* The Arnolfini Portrait, Jan van Eyck */
    {-1, "velazquez-meninas", 890, 1024, JS_MASTERS, 1660},  /* Las Meninas, Diego Velázquez */
    {-1, "arcimboldo-vertumnus", 832, 1024, JS_MASTERS, 1593},  /* Vertumnus, Giuseppe Arcimboldo */
    {-1, "canaletto-regatta", 1024, 643, JS_MASTERS, 1768},  /* A Regatta on the Grand Canal, Canaletto */
    {-1, "turner-temeraire", 1024, 761, JS_ROMANTIC, 1851},  /* The Fighting Temeraire, J. M. W. Turner */
    {-1, "constable-hay-wain", 1024, 706, JS_ROMANTIC, 1837},  /* The Hay Wain, John Constable */
    {-1, "friedrich-wanderer", 798, 1024, JS_ROMANTIC, 1840},  /* Wanderer above the Sea of Fog, C. D. Friedrich */
    {-1, "aivazovsky-ninth-wave", 1024, 688, JS_ROMANTIC, 1900},  /* The Ninth Wave, Ivan Aivazovsky */
    {-1, "bierstadt-sierra", 1024, 612, JS_ROMANTIC, 1902},  /* Among the Sierra Nevada, California, Albert Bierstadt */
};

#endif
