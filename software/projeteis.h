// projeteis.h - balas do jogador e espinhos dos inimigos
#ifndef PROJETEIS_H
#define PROJETEIS_H
#include "entity.h"

#define DONO_JOGADOR 0           // o dono decide quem o tiro machuca
#define DONO_INIMIGO 1
#define MAX_TIROS_JOGADOR 10     // tiros do jogador vivos ao mesmo tempo
#define MAX_TIROS_INIMIGO  6     // idem para os inimigos
#define VEL_TIRO_Q4       64     // 4 px por quadro

// cria um tiro saindo do centro (cx, cy) com velocidade em q4 (1/16 px por quadro).
// Devolve NULL se o dono já está no limite ou o pool está cheio
Entity *proj_disparar(int dono, int cx, int cy, int vx_q4, int vy_q4);
int     proj_contar(int dono);     // quantos tiros desse dono estão na tela
void    proj_limpar(int dono);     // apaga todos os tiros desse dono
#endif
