// jogador.h - o cangaceiro: andar, mirar, atirar e perder vida
#ifndef JOGADOR_H
#define JOGADOR_H
#include "entity.h"

void      jogador_iniciar(int x, int y);
void      jogador_atualizar(void);
void      jogador_dano(void);
void      jogador_curar(void);                  // +1 vida, até o máximo (VIDAS_MAX)
void      jogador_dar_escudo(void);
int       jogador_tem_escudo(void);
int       jogador_cx(void);                     // centro do sprite em x
int       jogador_cy(void);                     // centro do sprite em y
Rectangle jogador_area(void);                   // hitbox de dano, menor que o sprite para o jogo ser mais justo
int       jogador_direcao_mov(void);            // -1 se está parado, senão a Direction em que anda
int       jogador_morto_fim(void);              // vira 1 quando a animação de morte termina
#endif
