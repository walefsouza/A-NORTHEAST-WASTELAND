/* entrada.h - teclado e mouse (evdev) lidos por threads POSIX
 *
 * O jogo so usa estas funcoes. Nada de read()/pthread aparece fora de entrada.c.
 * Compilar com -pthread.
 */
#ifndef ENTRADA_H
#define ENTRADA_H

#include <linux/input.h>   /* KEY_*, BTN_* usados pelo jogo */

/* Abre os dispositivos e cria as threads de leitura.
 * Caminhos tipo "/dev/input/event3" ou "/dev/input/by-id/...". NULL = nao usar aquele dispositivo.
 * Retorna 0 ok, -1 falha (a mensagem sai no stderr). Precisa de permissao de leitura em /dev/input (ex: root). */
int  entrada_iniciar(const char *caminho_teclado, const char *caminho_mouse);

/* Para as threads, espera elas terminarem (pthread_join), fecha os arquivos e limpa os estados. */
void entrada_parar(void);

/* Consultas (rapidas, thread-safe via mutex, nunca bloqueiam esperando evento) */
int  tecla_apertada(int codigo);   /* KEY_A, KEY_SPACE...: 1 apertada, 0 solta */
int  mouse_x(void);                /* coordenada X acumulada da mira (0..319) */
int  mouse_y(void);                /* coordenada Y acumulada da mira (0..239) */
int  mouse_botao(int botao);       /* BTN_LEFT, BTN_RIGHT, BTN_MIDDLE: 1 apertado, 0 solto */

/* Medicao de latencia: timestamp (em microssegundos, CLOCK_MONOTONIC do kernel) do evento
 * mais antigo ainda nao consumido, ou 0 se nenhum. Zera ao ler. Usado por perf.c. */
long long entrada_pegar_evento_us(void);

#endif