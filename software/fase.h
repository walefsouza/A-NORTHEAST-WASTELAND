// fase.h - configuracao de cada fase: fundo, sprites, obstaculos e inimigos por onda
#ifndef FASE_H
#define FASE_H
#include "sprites.h"
#include "inimigos.h"
#include "jogo.h"

#define MAX_POOL    8              // maximo de entradas no sorteio de uma onda
#define MAX_ONDAS   3              // maximo de ondas por fase
#define NUM_FASES_CFG 3

// obstaculo fixo: posicao em pixels e tipo
typedef struct { short x, y; unsigned char tipo; } ObstCfg;      // tipo: 0 pedra, 1 mandacaru

typedef struct {
    const char          *nome;                 // mostrado na transicao
    const unsigned char *mapa;                 // tilemap da partida
    const unsigned char *mapa_transicao;       // tilemap da tela "FASE n"
    const CargaPadrao   *carga;                // padroes (sprites) especificos da fase
    const int           *n_carga;              // quantidade de itens em carga
    int                  n_ondas;
    int                  tem_boss;             // 1 = depois da ultima onda vem o chefe
    TipoInimigo          elite;                // INI_NENHUM = sem elite; 1 por onda a partir da onda 2
    TipoInimigo          pool[MAX_ONDAS][MAX_POOL];   // sorteio ponderado (repita = mais comum)
    int                  n_pool[MAX_ONDAS];    // quantos itens validos em cada pool
    const ObstCfg       *obst;                 // lista de obstaculos fixos
    int                  n_obst;
} FaseCfg;

const FaseCfg *fase_cfg(int fase);                         // configuracao da fase (limita o indice)
void           fase_montar_obstaculos(const FaseCfg *f);   // spawna pedras/mandacarus
int            fase_area_livre(int x, int y, int w, int h);          // 1 = sem obstaculo
int            fase_ponto_livre(int *x, int *y, int dist_min_jogador);  // 1 = achou
#endif
