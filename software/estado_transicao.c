// estado_transicao.c - tela "FASE n / NOME" antes de cada fase
#include <stdio.h>
#include "estado.h"
#include "teclas.h"
#include "gfx.h"
#include "texto.h"
#include "jogo.h"
#include "fase.h"

#define DURACAO 150                        // 2,5 s a 60 Hz (ENTER pula)
static int t;                              // quadros desde a entrada

// limpa o coprocessador e carrega o fundo da fase atual
static void entrar(void) {
    const FaseCfg *f = fase_cfg(jogo.fase);
    if (gfx_reset() != 0) return;
    if (gfx_carregar(CARGA_MENU, N_CARGA_MENU) != 0) return;   // fonte do menu
    if (gfx_fundo(f->mapa_transicao) != 0) return;
    t = 0;
}

// passa pra partida quando o tempo acaba ou o jogador aperta ENTER
static void atualizar(void) {
    if (++t >= DURACAO || tecla_pressionou(KEY_ENTER)) estado_ir_para(&estado_partida);
}

static void desenhar(void) {
    char b[32];
    int id = 0, fim = 64;                  // sprites 0..63 reservados pro texto
    snprintf(b, sizeof b, "FASE %d", jogo.fase + 1);
    id = texto_centro(id, fim, 72, b, TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, 96, fase_cfg(jogo.fase)->nome, TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, 126, "ATENCAO!", TXT_MENU);
    (void)id;
}

const Estado estado_transicao = { entrar, atualizar, desenhar };
