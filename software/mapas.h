/* mapas.h - tilemaps das fases (visual) e mapa de colisao (logica).
 *
 * Declara os dados de cada fase: o tilemap que e desenhado na tela, a tabela que diz
 * quais tiles sao solidos e as estruturas CollisionMap que ligam as duas coisas
 * ao modulo de colisao. Os valores ficam definidos em mapas.c. */
#ifndef MAPAS_H
#define MAPAS_H

#include "config.h"      /* MAP_COLS, MAP_ROWS, TILE_SIZE */
#include "collision.h"   /* tipo CollisionMap */

#define NUM_FASES  3     /* 0 manha, 1 tarde, 2 noite */
#define NUM_TILES  33    /* ids de tile 0..32 = hex/Sprites/Tiles/tile_00..tile_32 */

/* Tilemap visual de cada fase: cada celula guarda o id do tile a desenhar.
 * Celula = linha * MAP_COLS + coluna. */
extern const unsigned char mapas_fase[NUM_FASES][MAP_COLS * MAP_ROWS];

/* Tabela de solidez, indexada pelo id do tile (igual para todas as fases).
 * tiles_solidos[id] = 1 se o tile bloqueia passagem. */
extern const unsigned char tiles_solidos[NUM_TILES];

/* Mapa de colisao de cada fase (aponta para mapas_fase + tiles_solidos).
 * Pronto para collision_set_map(&mapa_colisao[fase]). */
extern const CollisionMap mapa_colisao[NUM_FASES];

#endif
