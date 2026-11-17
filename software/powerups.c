// powerups.c - os 9 itens, sorteio de drops, coleta e efeito de cada um
#include <string.h>
#include "config.h"
#include "sprites.h"
#include "entity.h"
#include "collision.h"
#include "mat.h"
#include "jogo.h"
#include "jogador.h"
#include "projeteis.h"
#include "inimigos.h"
#include "fase.h"
#include "powerups.h"

typedef struct {
    const char *nome;
    short pat;
    short duracao;       // em quadros (60 = 1 s). 0 = efeito na hora
    short peso;          // quanto maior, mais comum no sorteio
} Def;

// nome, padrão do sprite, duração e peso no sorteio
static const Def DEF[PU_N] = {
    [PU_BACAMARTE]   = { "bacamarte",   PAT_ITEM_BACAMARTE,   600, 12 },
    [PU_CARTUCHEIRA] = { "cartucheira", PAT_ITEM_CARTUCHEIRA, 600, 12 },
    [PU_ESTRELA]     = { "estrela",     PAT_ITEM_ESTRELA,     480,  8 },
    [PU_RAPADURA]    = { "rapadura",    PAT_ITEM_RAPADURA,    600, 10 },
    [PU_CANTIL]      = { "cantil",      PAT_ITEM_CANTIL,        0, 12 },
    [PU_FOGUEIRA]    = { "fogueira",    PAT_ITEM_FOGUEIRA,      0,  6 },
    [PU_CORACAO]     = { "coracao",     PAT_ITEM_CORACAO,       0,  8 },
    [PU_MOEDA1]      = { "moeda1",      PAT_ITEM_MOEDA1,        0, 30 },
    [PU_MOEDA5]      = { "moeda5",      PAT_ITEM_MOEDA5,        0,  8 },
};

// dados do item caído: qual é e quantos quadros faltam para sumir. No mesmo índice do slot no pool
typedef struct { unsigned char tipo; int vida; } Item;
static Item item[MAX_ENTITIES];

// efeitos ativos: só uma arma por vez e um tempo restante para cada efeito
static struct {
    PowerUp arma;  int arma_t;
    int rapido_t, veloz_t;
    PowerUp ultimo;                 // último com tempo que foi coletado, para o ícone do HUD
} P;

void powerups_reset(void) { memset(&P, 0, sizeof P); memset(item, 0, sizeof item); }

// desconta um quadro de cada efeito. Quando a arma acaba, volta o tiro normal
void powerups_tick(void) {
    if (P.arma_t > 0 && --P.arma_t == 0) P.arma = PU_NENHUM;
    if (P.rapido_t > 0) P.rapido_t--;
    if (P.veloz_t > 0) P.veloz_t--;
}

PowerUp powerups_arma(void)   { return P.arma_t > 0 ? P.arma : PU_NENHUM; }
int     powerups_rapido(void) { return P.rapido_t > 0; }
int     powerups_veloz(void)  { return P.veloz_t > 0; }

// quadros que ainda faltam de um efeito
static int restante(PowerUp p) {
    switch (p) {
    case PU_BACAMARTE: case PU_ESTRELA: return (P.arma == p) ? P.arma_t : 0;
    case PU_CARTUCHEIRA: return P.rapido_t;
    case PU_RAPADURA:    return P.veloz_t;
    default: return 0;
    }
}

// mostra no HUD o último power-up coletado. padrao recebe o sprite e frac_q8 a fração de tempo que sobra
// (256 = cheio)
int powerups_hud(int *padrao, int *frac_q8) {
    PowerUp p = P.ultimo;
    if (p == PU_NENHUM || restante(p) <= 0) {                 // o último já acabou, pega outro que ainda esteja ativo
        p = PU_NENHUM;
        if (P.arma_t > 0) p = P.arma; else if (P.rapido_t > 0) p = PU_CARTUCHEIRA; else if (P.veloz_t > 0) p = PU_RAPADURA;
    }
    if (p == PU_NENHUM) return 0;
    *padrao = DEF[p].pat;
    *frac_q8 = restante(p) * 256 / DEF[p].duracao;
    return 1;
}

// aplica o efeito. Os com tempo só reiniciam o contador (pegar de novo renova), os outros agem na hora
void powerups_coletar(PowerUp p) {
    switch (p) {
    case PU_BACAMARTE: case PU_ESTRELA: P.arma = p; P.arma_t = DEF[p].duracao; P.ultimo = p; break;
    case PU_CARTUCHEIRA: P.rapido_t = DEF[p].duracao; P.ultimo = p; break;
    case PU_RAPADURA:    P.veloz_t  = DEF[p].duracao; P.ultimo = p; break;
    case PU_CANTIL:      jogador_dar_escudo(); break;
    case PU_FOGUEIRA:    inimigos_limpar_tela(1); proj_limpar(DONO_INIMIGO); break;   // o chefe não é afetado
    case PU_CORACAO:     jogador_curar(); break;
    case PU_MOEDA1:      jogo_pontuar(50); break;
    case PU_MOEDA5:      jogo_pontuar(250); break;
    default: break;
    }
}

// itens que ficam no chão
static void item_update(Entity *e) {
    int i = entity_get_index(e);
    Item *it = &item[i];
    if (--it->vida <= 0) { entity_despawn(e); return; }   // acabou o tempo, some
    // nos últimos 2 s fica piscando
    e->visible = (it->vida > ITEM_PISCA_FRAMES) || ((it->vida / 4) & 1);
    // o jogador encostou: tira do chão e aplica o efeito
    if (collision_intersects(entity_get_area(e), jogador_area())) {
        PowerUp p = (PowerUp)it->tipo;
        entity_despawn(e);
        powerups_coletar(p);
    }
}

// quantos itens estão no chão agora
static int itens_na_tela(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) if (entities[i].active && entities[i].type == TYPE_ITEM) n++;
    return n;
}

// sorteio por peso: soma todos os pesos, sorteia um número até esse total e vai descontando o peso de
// cada item até achar onde ele caiu
static PowerUp sortear(void) {
    int total = 0;
    for (int p = 1; p < PU_N; p++) total += DEF[p].peso;
    int r = rng_faixa(0, total - 1);
    for (int p = 1; p < PU_N; p++) { if (r < DEF[p].peso) return (PowerUp)p; r -= DEF[p].peso; }
    return PU_MOEDA1;
}

// testa se o jogador conseguiria pisar no item, usando a hitbox dele (10x12, começando em +3,+3)
static int alcancavel(int x, int y) {
    return collision_area_livre(rectangle_new(x + 3, y + 3, 10, 12));
}

// cria o item em (x, y), corrigindo a posição se ficar fora da tela ou num lugar que o jogador não alcança
static void criar(PowerUp p, int x, int y) {
    if (x < 0) x = 0;
    if (x > SCREEN_WIDTH - 16) x = SCREEN_WIDTH - 16;
    if (y < 16) y = 16;
    if (y > SCREEN_HEIGHT - 16) y = SCREEN_HEIGHT - 16;
    if (!alcancavel(x, y)) {                    // caiu dentro de pedra, cacto ou parede: procura o ponto livre mais perto
        int ok = 0;
        for (int r = 4; r <= 64 && !ok; r += 4)
            for (int dy = -r; dy <= r && !ok; dy += 4)
                for (int dx = -r; dx <= r && !ok; dx += 4) {
                    if (abs_i(dx) != r && abs_i(dy) != r) continue;        // só interessa a borda do quadrado, o miolo já foi testado nos anéis menores
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx > SCREEN_WIDTH - 16 || ny < 16 || ny > SCREEN_HEIGHT - 16) continue;
                    if (alcancavel(nx, ny)) { x = nx; y = ny; ok = 1; }
                }
        if (!ok && !fase_ponto_livre(&x, &y, 0)) return;
    }
    Entity *e = entity_spawn(TYPE_ITEM, DEF[p].nome, x, y);
    if (!e) return;
    int i = entity_get_index(e);
    e->solid = 0;
    e->width = e->height = 16;
    e->solidArea = rectangle_new(2, 2, 12, 12);
    e->pattern = DEF[p].pat;
    e->update = item_update;
    item[i].tipo = (unsigned char)p;
    item[i].vida = ITEM_VIDA_FRAMES;
}

// sorteia de 1 a 100 e só cria o item se cair dentro da chance. Também respeita o limite de itens na tela
void powerups_dropar(int cx, int cy) {
    if (rng_faixa(1, 100) > DROP_CHANCE_PCT || itens_na_tela() >= MAX_ITENS_TELA) return;
    criar(sortear(), cx - 8, cy - 8);
}

// item que a onda garante. Nasce em ponto livre e a pelo menos 64 px do jogador
void powerups_dropar_garantido(void) {
    int x, y;
    if (itens_na_tela() >= MAX_ITENS_TELA) return;
    if (fase_ponto_livre(&x, &y, 64)) criar(sortear(), x, y);
}
