// entity.c - pool de entidades, movimento e animação básicos
#include <stdint.h>
#include <string.h>
#include "entity.h"

Entity entities[MAX_ENTITIES];

Rectangle rectangle_new(int x, int y, int width, int height) {
    Rectangle r;
    r.x = x;
    r.y = y;
    r.width = width;
    r.height = height;
    return r;
}

void entity_init_pool(void) {
    memset(entities, 0, sizeof(entities));
}

// ocupa o primeiro slot livre. O memset limpa o que a entidade anterior deixou ali
Entity* entity_spawn(EntityType type, const char *name, int x, int y) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!entities[i].active) {
            Entity *e = &entities[i];
            memset(e, 0, sizeof(*e));
            e->active = 1;
            e->type = type;
            e->x = x;
            e->y = y;
            e->direction = DIR_DOWN;
            e->solid = 1; // sólido por padrão, tiro, item e inimigo desligam depois
            e->visible = 1;
            if (name) {
                strncpy(e->name, name, MAX_NAME - 1);
                e->name[MAX_NAME - 1] = '\0';
            }
            return e;
        }
    }
    return NULL; // pool cheio
}

void entity_despawn(Entity *e) {
    if (e) {
        memset(e, 0, sizeof(*e));
    }
}

Entity* entity_get(int index) {
    if (index < 0 || index >= MAX_ENTITIES) return NULL;
    return &entities[index];
}

// devolve o índice do slot (que também é o ID do sprite), ou -1 se o ponteiro não aponta para um slot do pool
int entity_get_index(const Entity *e) {
    uintptr_t a = (uintptr_t)e;
    uintptr_t b = (uintptr_t)entities;
    if (a < b || a >= b + sizeof(entities) || (a - b) % sizeof(Entity) != 0) return -1;
    return (int)((a - b) / sizeof(Entity));
}

// chama o update de toda entidade ativa que tenha um
void entity_update_all(void) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (e->active && e->update) {
            e->update(e);
        }
    }
}

// anda um passo (speed pixels) na direção atual, sem checar colisão
void entity_move(Entity *e) {
    switch (e->direction) {
        case DIR_UP:    e->y -= e->speed; break;
        case DIR_DOWN:  e->y += e->speed; break;
        case DIR_LEFT:  e->x -= e->speed; break;
        case DIR_RIGHT: e->x += e->speed; break;
    }
}

// troca de frame a cada interval + 1 quadros, voltando ao 0 depois de total_frames
void entity_animate(Entity *e, int total_frames, int interval) {
    if (total_frames <= 0) return;
    e->spriteCounter++;
    if (e->spriteCounter > interval) {
        e->spriteNum = (e->spriteNum + 1) % total_frames;
        e->spriteCounter = 0;
    }
}

// hitbox em coordenadas da tela (posição da entidade + deslocamento da hitbox)
Rectangle entity_get_area(const Entity *e) {
    return rectangle_new(e->x + e->solidArea.x, e->y + e->solidArea.y,
                         e->solidArea.width, e->solidArea.height);
}

// a mesma hitbox, só que um passo à frente na direção atual. Serve para testar antes de mover
Rectangle entity_get_predicted_area(const Entity *e) {
    Rectangle r = entity_get_area(e);
    switch (e->direction) {
        case DIR_UP:    r.y -= e->speed; break;
        case DIR_DOWN:  r.y += e->speed; break;
        case DIR_LEFT:  r.x -= e->speed; break;
        case DIR_RIGHT: r.x += e->speed; break;
    }
    return r;
}

int entity_get_bottom_y(const Entity *e) {
    return e->y + e->height;
}
