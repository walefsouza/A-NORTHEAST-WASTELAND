/* teclas.c - Deteccao de borda de subida (clique unico) para teclado e mouse.
 *
 * Amostra o estado instantaneo de entrada.c uma vez por quadro e compara com
 * o quadro anterior, permitindo distinguir "tecla mantida pressionada" de
 * "tecla acabou de ser pressionada neste quadro".
 */
#include <string.h>
#include "entrada.h"
#include "teclas.h"

#define N_TECLAS 256                /* limite de codigos monitorados (KEY_* usados ficam abaixo de 256) */

/* Vetores de estado das teclas: 'ant' guarda o quadro anterior e 'atu' o quadro atual */
static unsigned char ant[N_TECLAS], atu[N_TECLAS];

/* Estado do botao esquerdo do mouse no quadro anterior ('mouse_ant') e no atual ('mouse_atu') */
static int mouse_ant = 0, mouse_atu = 0;

/* Atualiza o historico de entradas; deve ser chamada exatamente uma vez no inicio de cada quadro. */
void teclas_quadro(void) {
    /* Copia o estado do quadro passado para o buffer anterior */
    memcpy(ant, atu, sizeof atu);

    /* Captura o estado atual de todas as teclas monitoradas */
    for (int k = 0; k < N_TECLAS; k++)
        atu[k] = (unsigned char)(tecla_apertada(k) ? 1 : 0);

    /* Atualiza o historico e captura o estado atual do botao esquerdo do mouse */
    mouse_ant = mouse_atu;
    mouse_atu = mouse_botao(BTN_LEFT) ? 1 : 0;
}

/* Retorna 1 apenas no quadro exato em que a tecla 'c' transita de solta (0) para pressionada (1). */
int tecla_pressionou(int c) {
    return c >= 0 && c < N_TECLAS && atu[c] && !ant[c];
}

/* Retorna 1 apenas no quadro exato em que o botao esquerdo do mouse acaba de ser clicado. */
int mouse_clicou(void) {
    return mouse_atu && !mouse_ant;
}