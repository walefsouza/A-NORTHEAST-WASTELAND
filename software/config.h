/* config.h - Constantes globais do jogo */
#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>      /* NULL */

/* Resolucao e tamanho de tela */
#define SCREEN_WIDTH  320  /* largura da area visivel do monitor VGA em pixels */
#define SCREEN_HEIGHT 240  /* altura da area visivel do monitor VGA em pixels */
#define TILE_SIZE     8    /* dimensao lateral (8x8 pixels) de cada bloco do background */
#define SPRITE_SIZE   16   /* dimensao lateral (16x16 pixels) de cada sprite de hardware */

/* A FPGA tem 64 registradores de sprite (ids 0..63). Divisao:
 *   0..46  = pool de entidades (jogador, ate 12 obstaculos, inimigos, tiros, itens)
 *   47..63 = interface (mira, vidas, pontos, icones, texto)
 * O id do sprite de uma entidade e o seu indice no pool. */
#define MAX_ENTITIES  47   /* capacidade maxima do pool dinamico de entidades (ids 0..46) */
#define MAX_NAME      32   /* tamanho maximo do buffer de string para identificadores/nomes */

#define SPR_MIRA        47     /* id reservado para o cursor de mira do jogador */
#define SPR_HUD_VIDA0   48     /* 48..50: coracoes (indicadores de vida no HUD) */
#define SPR_HUD_PONTOS  51     /* 51..56: 6 digitos do placar de pontuacao */
#define SPR_HUD_ICONE   57     /* indicador do power-up ativo no momento */
#define SPR_HUD_ESCUDO  58     /* indicador do cantil (escudo de protecao ativo) */
#define SPR_HUD_TEXTO   59     /* 59..63: faixas de ate 5 letras (ONDA 2, PAUSA, CHEFE) */
#define SPR_HUD_TEXTO_N 5      /* quantidade de sprites dedicados ao letreiro de texto */

/* Tipos de Entidade (para usar na logica e no switch de comportamentos) */
typedef enum {
    TYPE_PLAYER,       /* personagem controlado pelo jogador */
    TYPE_NPC,          /* obstaculos estaticos de cenario (pedra, mandacaru) */
    TYPE_ENEMY,        /* inimigos comuns e chefes (controlados por IA) */
    TYPE_PROJECTILE,   /* disparos ativos no cenario (do jogador ou inimigos) */
    TYPE_ITEM          /* power-ups e itens colecionaveis largados na arena */
} EntityType;

/* Dimensoes do Mapa */
#define MAP_COLS 40    /* numero de colunas do tilemap (320 px / 8 px por tile) */
#define MAP_ROWS 30    /* numero de linhas do tilemap (240 px / 8 px por tile) */

#endif