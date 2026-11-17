// inimigos.h - tipos de inimigo e funções para criar, contar e machucar
#ifndef INIMIGOS_H
#define INIMIGOS_H
#include "entity.h"

typedef enum {
    INI_NENHUM = 0,
    INI_CALANGO, INI_MARIMBONDO, INI_CACTO,            // fase 1, manhã
    INI_CASCAVEL, INI_URUBU,                           // fase 2, tarde
    INI_MORCEGO, INI_CORPO_SECO, INI_ESCORPIAO,        // fase 3, noite
    INI_LOBISOMEN,                                     // elite da noite, entra a partir da onda 2
    INI_BOSS,                                          // chefe da fase 3
    INI_N
} TipoInimigo;

// por qual borda da tela o inimigo entra
enum { LADO_CIMA = 0, LADO_BAIXO, LADO_ESQ, LADO_DIR };

#define AVISO_FRAMES 30          // pisca por 0,5 s antes de valer (nesse tempo não machuca nem leva dano)

void    inimigos_reset(void);
// cria o inimigo em (x, y), na borda indicada por lado. Urubu e morcego vêm em bando de 3 ou 4.
// Devolve quantos foram criados
int     inimigo_spawn_em(TipoInimigo t, int x, int y, int lado);
int     inimigos_contar(void);
int     inimigo_tamanho_grupo(TipoInimigo t);       // quantos podem nascer de uma vez, para ver se cabe na tela
int     inimigo_perigoso(const Entity *e);          // 0 enquanto está piscando ao nascer
// tira vida do inimigo. Devolve 1 se ele morreu e 0 se sobreviveu ou foi ignorado (ainda piscando)
int     inimigo_dano(Entity *e, int dano);
void    inimigos_limpar_tela(int pontuar);          // some com todos menos o chefe, pontuando se pontuar for 1 (fogueira)
int     inimigos_boss_vivo(void);                   // 1 enquanto o chefe estiver na tela
#endif
