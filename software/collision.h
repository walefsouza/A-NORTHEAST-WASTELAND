// collision.h - testes de colisão (era o CollisionChecker do Java)
#ifndef COLLISION_H
#define COLLISION_H

#include "entity.h"

typedef struct {
    const unsigned char *tileNum;   // id do tile de cada célula do mapa
    int maxCol, maxRow;             // tamanho do mapa em tiles
    int tileSize;                   // lado do tile em pixels
    const unsigned char *collision; // por id de tile: 1 = bloqueia
    int numTiles;                   // quantos ids existem na tabela collision
} CollisionMap;

// funções que vieram do CollisionChecker
int collision_intersects(Rectangle a, Rectangle b);   // 1 se os retângulos se cruzam
void collision_set_map(const CollisionMap *m);        // mapa da fase atual

void checkBounds(Entity *e);                // bordas da tela
void checkTile(Entity *e);                  // tiles sólidos do mapa
int checkEntity(Entity *e);                 // outras entidades, devolve o índice da que encostou ou -1
int checkObjectInteraction(const Entity *e);// índice da primeira entidade no caminho, sólida ou não
int collision_try_move(Entity *e);          // roda os testes e anda se estiver livre
int collision_area_livre(Rectangle r);   // 1 se não há tile sólido nem obstáculo de cenário, ou seja, o jogador conseguiria ficar ali

#endif
