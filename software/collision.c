// collision.c - colisão por retângulo, por tile e por entidade (a conta é a mesma do Java)
#include <stddef.h>
#include "collision.h"

static const CollisionMap *current_map = NULL;   // mapa da fase atual, definido em collision_set_map
#define WALL_THICKNESS 10                        // espessura das paredes invisíveis fora da tela

// dois retângulos se cruzam quando se sobrepõem nos dois eixos. Largura ou altura 0 nunca colide
int collision_intersects(Rectangle a, Rectangle b) {
    if (a.width <= 0 || a.height <= 0 || b.width <= 0 || b.height <= 0) return 0;
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

void collision_set_map(const CollisionMap *m) {
    current_map = m;
}

// monta quatro paredes invisíveis em volta da tela e vê se o próximo passo bate em alguma
void checkBounds(Entity *e) {
    Rectangle top    = rectangle_new(0, -WALL_THICKNESS, SCREEN_WIDTH, WALL_THICKNESS);
    Rectangle bottom = rectangle_new(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, WALL_THICKNESS);
    Rectangle left   = rectangle_new(-WALL_THICKNESS, 0, WALL_THICKNESS, SCREEN_HEIGHT);
    Rectangle right  = rectangle_new(SCREEN_WIDTH - 1, 0, WALL_THICKNESS, SCREEN_HEIGHT);
    
    Rectangle predicted = entity_get_predicted_area(e);

    if (collision_intersects(predicted, top) ||
        collision_intersects(predicted, bottom) ||
        collision_intersects(predicted, left) ||
        collision_intersects(predicted, right)) {
        e->collision = 1;
    }
}

// 1 se o tile daquela célula é sólido. Fora do mapa conta como livre, quem segura nas bordas é o checkBounds
static int tile_has_collision(int col, int row) {
    if (!current_map) return 0;
    if (col < 0 || col >= current_map->maxCol || row < 0 || row >= current_map->maxRow) return 0;
    int id = current_map->tileNum[row * current_map->maxCol + col];
    if (id >= current_map->numTiles) return 0;
    return current_map->collision[id] != 0;
}

// olha os dois cantos da hitbox que vão na frente, conforme a direção. Pixel vira coluna ou linha
// dividindo pelo tamanho do tile
void checkTile(Entity *e) {
    if (current_map == NULL || current_map->tileSize <= 0) return;

    int tile = current_map->tileSize;
    int left_x = e->x + e->solidArea.x;
    int right_x = left_x + e->solidArea.width;
    int top_y = e->y + e->solidArea.y;
    int bottom_y = top_y + e->solidArea.height;

    int col_left = left_x / tile;
    int col_right = right_x / tile;
    int row_top = top_y / tile;
    int row_bottom = bottom_y / tile;

    switch (e->direction) {
        case DIR_UP:
            row_top = (top_y - e->speed) / tile;
            if (tile_has_collision(col_left, row_top) || tile_has_collision(col_right, row_top)) e->collision = 1;
            break;
        case DIR_DOWN:
            row_bottom = (bottom_y + e->speed) / tile;
            if (tile_has_collision(col_left, row_bottom) || tile_has_collision(col_right, row_bottom)) e->collision = 1;
            break;
        case DIR_LEFT:
            col_left = (left_x - e->speed) / tile;
            if (tile_has_collision(col_left, row_top) || tile_has_collision(col_left, row_bottom)) e->collision = 1;
            break;
        case DIR_RIGHT:
            col_right = (right_x + e->speed) / tile;
            if (tile_has_collision(col_right, row_top) || tile_has_collision(col_right, row_bottom)) e->collision = 1;
            break;
    }
}

// confere o próximo passo contra todas as outras entidades. Só bloqueia se a outra for sólida.
// Devolve o índice da última que encostou, ou -1
int checkEntity(Entity *e) {
    Rectangle prev = entity_get_predicted_area(e);
    int hit_index = -1;

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *target = entity_get(i);
        if (!target->active || target == e) continue;
        
        if (collision_intersects(prev, entity_get_area(target))) {
            if (target->solid) {
                e->collision = 1;
            }
            hit_index = i;
        }
    }
    return hit_index;
}

// devolve o índice da primeira entidade que encosta no próximo passo, sólida ou não, ou -1
int checkObjectInteraction(const Entity *e) {
    Rectangle prev = entity_get_predicted_area(e);

    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *target = entity_get(i);
        if (!target->active || target == e) continue;
        
        if (collision_intersects(prev, entity_get_area(target))) {
            return i;
        }
    }
    return -1;
}

// tenta dar um passo. Se nenhum teste marcar colisão, anda e devolve 1. Senão fica parado e devolve 0
int collision_try_move(Entity *e) {
    e->collision = 0;
    checkBounds(e);
    checkTile(e);
    checkEntity(e);

    if (e->collision) return 0;
    
    entity_move(e);
    return 1;
}

// 1 se o retângulo não toca tile sólido nem obstáculo de cenário (TYPE_NPC).
// Primeiro varre os tiles que o retângulo cobre, depois os obstáculos
int collision_area_livre(Rectangle r) {
    if (current_map && current_map->tileSize > 0 && r.width > 0 && r.height > 0) {
        int t = current_map->tileSize;
        for (int row = r.y / t; row <= (r.y + r.height - 1) / t; row++)
            for (int col = r.x / t; col <= (r.x + r.width - 1) / t; col++)
                if (tile_has_collision(col, row)) return 0;
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity *e = &entities[i];
        if (e->active && e->type == TYPE_NPC && collision_intersects(r, entity_get_area(e))) return 0;
    }
    return 1;
}
