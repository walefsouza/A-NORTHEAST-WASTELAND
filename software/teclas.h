/* teclas.h - Deteccao de borda de subida (clique unico) sobre entrada.h
 *
 * Permite identificar o exato quadro em que uma tecla ou botao do mouse
 * foi pressionado, evitando repeticoes continuas em menus, pausa e acoes
 * discretas enquanto o botao permanece segurado.
 */
#ifndef TECLAS_H
#define TECLAS_H

#include "entrada.h"      /* importa codigos KEY_* e BTN_* do subsistema evdev */

/* Amostra o estado atual de entrada.h e atualiza o historico do quadro anterior.
 * Deve ser chamada exatamente 1x por quadro no loop principal, antes da logica de estados. */
void teclas_quadro(void);

/* Retorna 1 apenas no quadro em que a tecla 'codigo' (0..255) transita de solta para pressionada. */
int  tecla_pressionou(int codigo);

/* Retorna 1 apenas no quadro em que o botao esquerdo do mouse (BTN_LEFT) transita para pressionado. */
int  mouse_clicou(void);

#endif