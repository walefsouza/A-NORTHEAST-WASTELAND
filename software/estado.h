// estado.h - maquina de estados modular
// cada tela do jogo (menu, partida, pausa...) e um Estado com 3 funcoes
#ifndef ESTADO_H
#define ESTADO_H

#include "entity.h"

typedef struct {
    void (*entrar)(void);                  // roda 1x ao entrar no estado
    void (*atualizar)(void);               // so logica (entrada, fisica, regras)
    void (*desenhar)(void);                // so hardware (comandos para o coprocessador grafico)
} Estado;

extern const Estado estado_menu;
extern const Estado estado_partida;
extern const Estado estado_transicao;
extern const Estado estado_pausa;
extern const Estado estado_gameover;       // tambem serve de tela de vitoria (jogo.venceu)

void estado_ir_para(const Estado *e);      // agenda troca (vale no inicio do proximo quadro)
void estado_rodar_quadro(void);            // chamada 1x por quadro pelo main
const Estado *estado_atual(void);          // para o painel de desempenho

// a pausa devolve o controle para a partida SEM reiniciar (ver estado_partida.c)
void partida_marcar_retomada(void);        // proxima entrada na partida e uma retomada
void partida_cancelar_retomada(void);      // proxima entrada na partida e uma partida nova

#endif
