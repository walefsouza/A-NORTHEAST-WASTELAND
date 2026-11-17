/* gfx.h - camada sobre o driver; retorna status e mantem cache apos ACK.
 *
 * Gerencia o shadow state de sprites (0..63), poligonos (0..7) e tilemap (40x30),
 * enviando comandos para a FIFO da FPGA apenas quando os atributos mudam.
 * Todas as funcoes que enviam comandos retornam DRIVER_OK (0) ou codigo negativo.
 */
#ifndef GFX_H
#define GFX_H
#include "sprites.h"

/* O primeiro erro do quadro fica retido (latched) ate gfx_limpar_erro(); main() consulta o status. */
int  gfx_ultimo_erro(void);  /* retorna DRIVER_OK (0) ou o primeiro erro ocorrido desde a ultima limpeza */
void gfx_limpar_erro(void);  /* reseta o estado de erro acumulado para DRIVER_OK */

/* Reinicia o estado visual na FPGA (oculta 64 sprites e 8 poligonos, zera scroll e invalida caches) */
int gfx_reset(void);

/* Carrega um vetor 't' de 'n' padroes 16x16 na memoria de sprites da FPGA */
int gfx_carregar(const CargaPadrao *t, int n);

/* Atualiza os 1200 tiles (40x30) do background, enviando apenas as posicoes que mudaram */
int gfx_fundo(const unsigned char *mapa1200);

/* Atualiza uma sub-regiao retangular (w x h) do tilemap a partir de (col, row) via cache diferencial */
int gfx_fundo_regiao(int col, int row, int w, int h, const unsigned char *ids);

/* Sincroniza um sprite (0..63): ativa/oculta, muda padrao, prioridade e posicao se alterados */
int gfx_sprite(int id, int padrao, int x, int y, int prio, int visivel);

/* Desativa o sprite 'id' (0..63) na FPGA (se estiver visivel) e limpa seu cache local */
int gfx_esconder(int id);

/* Agenda o espelhamento horizontal (h) e/ou vertical (v) do sprite 'id'; aplicado no proximo gfx_sprite() */
int gfx_inverter(int id, int h, int v);

/* Varre o pool global de entidades (0..46) e atualiza seus sprites com prioridade baseada em Y (Y-sorting) */
int gfx_entidades(void);

/* Sincroniza um poligono (0..7), respeitando a ordem de hardware (TAMPOL antes de INSPOL) */
int gfx_poligono(int id, int forma, int cor, int x, int y, int w, int h);

/* Desativa o poligono 'id' (0..7) na FPGA e limpa seu registro no cache local */
int gfx_poligono_off(int id);

#endif