// estado_partida.c - orquestra a fase: jogador, ondas, entidades, HUD
#include <stdio.h>
#include "estado.h"
#include "entrada.h"
#include "teclas.h"
#include "driver.h"
#include "collision.h"
#include "gfx.h"
#include "mat.h"
#include "jogo.h"
#include "fase.h"
#include "ondas.h"
#include "jogador.h"
#include "inimigos.h"
#include "powerups.h"
#include "hud.h"

extern int debug_boss_direto;

static int retomando;                       // 1 = voltando da pausa (nao reinicia a fase)

void partida_marcar_retomada(void) { retomando = 1; }
void partida_cancelar_retomada(void) { retomando = 0; }

static void entrar(void) {
    // retomada: o mundo ja esta montado, so remove o painel de pausa
    if (retomando) {
        retomando = 0;
        if (hud_pausa_fechar() != 0) return;
        return;
    }

    // partida nova: monta tudo do zero
    const FaseCfg *f = fase_cfg(jogo.fase);
    rng_semente(0x9E3779B9u ^ (unsigned)(quadro_atual() * 2654435761u));   // semente varia com o quadro

    if (gfx_reset() != 0) return;
    if (gfx_carregar(CARGA_COMUM, N_CARGA_COMUM) != 0) return; // jogador, itens, digitos, letras
    if (gfx_carregar(f->carga, *f->n_carga) != 0) return;      // cenario e inimigos
    if (gfx_fundo(f->mapa) != 0) return;
    collision_set_map(&mapa_colisao[jogo.fase]);        // tiles solidos (bordas) valem para o jogador

    entity_init_pool();                                // jogador PRECISA ser o indice 0
    inimigos_reset();
    powerups_reset();
    jogador_iniciar(152, 112);                         // centro da tela
    fase_montar_obstaculos(f);
    hud_iniciar();
    ondas_iniciar(f);
}

static void atualizar(void) {
    // P = pausa (marca retomada para a partida nao reiniciar ao voltar)
    if (tecla_pressionou(KEY_P)) {
        partida_marcar_retomada();
        estado_ir_para(&estado_pausa);
        return;
    }
    // ---- atalhos de debug ----
    // B = pula para o chefe da fase 3
    if (tecla_pressionou(KEY_B)) {
        partida_cancelar_retomada();
        jogo.fase = 2;
        jogo.onda = 0;
        debug_boss_direto = 1;
        estado_ir_para(&estado_transicao);
        return;
    }
    // N = pula para a onda 1 da fase 3 (noite)
    if (tecla_pressionou(KEY_N)) {
        partida_cancelar_retomada();
        jogo.fase = 2;
        jogo.onda = 0;
        debug_boss_direto = 0;
        estado_ir_para(&estado_transicao);
        return;
    }
    // ---- logica normal do quadro ----
    jogador_atualizar();
    ondas_atualizar();
    entity_update_all();                               // inimigos, tiros e itens (campo update)
    powerups_tick();

    // acabaram as vidas: fim de jogo
    if (jogador_morto_fim()) { jogo.venceu = 0; estado_ir_para(&estado_gameover); return; }
    // fase concluida: proxima fase, ou vitoria se era a ultima
    if (ondas_fase_concluida()) {
        jogo.fase++;
        jogo.onda = 0;
        if (jogo.fase >= NUM_FASES) { jogo.fase = NUM_FASES - 1; jogo.venceu = 1; estado_ir_para(&estado_gameover); }
        else estado_ir_para(&estado_transicao);
    }
}

static void desenhar(void) {
    if (gfx_entidades() != 0) return;                  // manda todas as entidades pro coprocessador
    if (hud_desenhar() != 0) return;
    // mira do mouse (sprite 16x16 centralizado no cursor)
    int mx = mouse_x() - 8, my = mouse_y() - 8;
    (void)gfx_sprite(SPR_MIRA, PAT_MIRA, mx < 0 ? 0 : mx, my < 0 ? 0 : my, 63, 1);
}

const Estado estado_partida = { entrar, atualizar, desenhar };
