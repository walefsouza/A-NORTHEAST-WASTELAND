// entity.h - struct única que serve de base para tudo que aparece na tela
#ifndef ENTITY_H
#define ENTITY_H

#include "config.h"

// direção em que a entidade anda ou olha (no Java isso era uma String)
typedef enum {
    DIR_DOWN = 0,
    DIR_UP,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

// retângulo usado em hitbox e colisão, igual ao Rectangle do Java
typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rectangle;

struct Entity;
typedef void (*EntityFunc)(struct Entity *e);

// no Java eram quatro classes (Entity, Player, Npc e Objects). Aqui ficaram numa struct só,
// e o campo type diz qual é qual
typedef struct Entity {
    int active;           // 0 = vaga livre no pool, 1 = em uso
    EntityType type;      // jogador, npc, inimigo, tiro ou item
    char name[MAX_NAME];  // nome, ex: "bala" ou "pedra"

    // posição do canto superior esquerdo e tamanho do sprite, em pixels
    int x, y;
    int width, height;
    
    // colisão
    Rectangle solidArea;  // hitbox, com x e y contados a partir do canto da entidade
    int collision;        // vira 1 se o próximo passo bateria em algo (zera a cada tentativa de mover)
    int solid;            // 1 = bloqueia quem bate nele (pedra, cacto), 0 = dá para atravessar (inimigo, tiro, item)

    // movimento
    int speed;            // pixels por quadro
    Direction direction;  // para onde anda e olha

    // animação
    int spriteCounter;    // conta quadros até trocar de frame
    int spriteNum;        // frame atual
    int pattern;          // qual padrão 16x16 da FPGA desenha a entidade (0 a 63)
    int visible;          // 0 esconde o sprite sem tirar a entidade do pool (usado nos piscados)

    // cada tipo de entidade preenche com o próprio comportamento
    EntityFunc update;    // roda uma vez por quadro, é a IA do tipo
    EntityFunc interact;
    void *data;           // espaço livre para dados extras de um tipo específico
} Entity;

// pool com todas as entidades. O índice do slot é o mesmo ID do sprite na FPGA
extern Entity entities[MAX_ENTITIES];

// funções que trabalham em cima das entidades
Rectangle rectangle_new(int x, int y, int width, int height);

void entity_init_pool(void);
Entity* entity_spawn(EntityType type, const char *name, int x, int y);
void entity_despawn(Entity *e);
Entity* entity_get(int index);
int entity_get_index(const Entity *e);

void entity_update_all(void);
void entity_move(Entity *e);
void entity_animate(Entity *e, int total_frames, int interval);

Rectangle entity_get_area(const Entity *e);
Rectangle entity_get_predicted_area(const Entity *e);
int entity_get_bottom_y(const Entity *e);

#endif
