// mapas.h - tilemaps das fases (visual) e mapa de colisao (logica)
#ifndef MAPAS_H
#define MAPAS_H

#include "config.h"
#include "collision.h"

#define NUM_FASES  3     // 0 manha, 1 tarde, 2 noite
#define NUM_TILES  33    // ids de tile 0..32 = hex/Sprites/Tiles/tile_00..tile_32

// Celula = linha * MAP_COLS + coluna.
extern const unsigned char mapas_fase[NUM_FASES][MAP_COLS * MAP_ROWS];

// tiles_solidos[id] = 1 se o tile bloqueia passagem.
extern const unsigned char tiles_solidos[NUM_TILES];

// Pronto para collision_set_map(&mapa_colisao[fase]).
extern const CollisionMap mapa_colisao[NUM_FASES];

#endif
