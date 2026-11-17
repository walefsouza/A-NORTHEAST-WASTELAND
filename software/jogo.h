// jogo.h - estado global da campanha (persiste entre fases)
#ifndef JOGO_H
#define JOGO_H

#include "mapas.h"      // NUM_FASES

#define VIDAS_MAX    3

typedef struct {
    int           fase;      // 0 manha, 1 tarde, 2 noite
    int           onda;      // onda atual (0..)
    unsigned long pontos;
    int           vidas;
    int           venceu;    // 1 = zerou o jogo
    int           sair;      // 1 = pediu para fechar
} Jogo;

extern Jogo jogo;            // instancia unica, definida em jogo.c

void jogo_novo(void);                 // zera a campanha
void jogo_pontuar(int p);             // soma pontos (limitado a 999999)

#endif
