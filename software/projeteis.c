// projeteis.c - movimento e colisão dos tiros do jogador e dos inimigos
#include "config.h"
#include "sprites.h"
#include "entity.h"
#include "collision.h"
#include "projeteis.h"
#include "inimigos.h"
#include "jogador.h"

// fx e fy são a posição em q4 (1/16 px), vx e vy a velocidade também em q4. Fica em q4 para os tiros
// em diagonal não perderem a parte fracionária do passo
typedef struct { int fx, fy, vx, vy; unsigned char dono; } Proj;
static Proj pr[MAX_ENTITIES];   // no mesmo índice do slot do tiro no pool

static void proj_update(Entity *e) {
    int i = entity_get_index(e);
    Proj *p = &pr[i];
    // x e y da entidade saem da posição em q4 dividida por 16
    p->fx += p->vx; p->fy += p->vy;
    e->x = p->fx >> 4; e->y = p->fy >> 4;
    // saiu da tela, libera o slot
    if (e->x < -8 || e->x > SCREEN_WIDTH - 8 || e->y < -8 || e->y > SCREEN_HEIGHT - 8) {
        entity_despawn(e); return;
    }
    Rectangle a = entity_get_area(e);
    for (int j = 0; j < MAX_ENTITIES; j++) {
        Entity *t = &entities[j];
        if (!t->active || t == e) continue;
        if (t->type == TYPE_NPC && collision_intersects(a, entity_get_area(t))) {
            entity_despawn(e); return;                      // pedra e mandacaru seguram qualquer tiro
        }
        // bala do jogador tira 1 de vida do inimigo e some. Inimigo ainda piscando não conta, a bala passa
        if (p->dono == DONO_JOGADOR && t->type == TYPE_ENEMY && inimigo_perigoso(t) &&
            collision_intersects(a, entity_get_area(t))) {
            inimigo_dano(t, 1);
            entity_despawn(e); return;
        }
    }
    // espinho de inimigo machuca o jogador e some
    if (p->dono == DONO_INIMIGO && collision_intersects(a, jogador_area())) {
        jogador_dano();
        entity_despawn(e);
    }
}

// respeita o limite de cada dono. O centro vira canto subtraindo 8, já que o sprite é 16x16
Entity *proj_disparar(int dono, int cx, int cy, int vx, int vy) {
    if (proj_contar(dono) >= (dono == DONO_JOGADOR ? MAX_TIROS_JOGADOR : MAX_TIROS_INIMIGO)) return NULL;
    Entity *e = entity_spawn(TYPE_PROJECTILE, dono == DONO_JOGADOR ? "bala" : "espinho", cx - 8, cy - 8);
    if (!e) return NULL;
    int i = entity_get_index(e);
    e->solid = 0;                               // tiro não bloqueia ninguém
    e->width = e->height = 16;
    e->solidArea = rectangle_new(5, 5, 6, 6);   // hitbox pequena no meio do sprite
    e->pattern = (dono == DONO_JOGADOR) ? PAT_BALA : PAT_TIRO_INIMIGO;
    e->update = proj_update;
    // posição inicial em q4 (vezes 16)
    pr[i].fx = (cx - 8) * 16; pr[i].fy = (cy - 8) * 16;
    pr[i].vx = vx; pr[i].vy = vy; pr[i].dono = (unsigned char)dono;
    return e;
}

// conta os tiros ativos desse dono
int proj_contar(int dono) {
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (entities[i].active && entities[i].type == TYPE_PROJECTILE && pr[i].dono == dono) n++;
    return n;
}

// some com todos os tiros desse dono (a fogueira usa isso para limpar os espinhos)
void proj_limpar(int dono) {
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (entities[i].active && entities[i].type == TYPE_PROJECTILE && pr[i].dono == dono)
            entity_despawn(&entities[i]);
}
