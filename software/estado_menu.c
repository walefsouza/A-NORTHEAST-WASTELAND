// estado_menu.c - menu principal: fundo em tiles + botoes (tiles trocam ao selecionar)
#include <stdio.h>
#include "estado.h"
#include "entrada.h"
#include "teclas.h"
#include "driver.h"
#include "gfx.h"
#include "texto.h"
#include "jogo.h"
#include "menu_mapas.h"
#include "ondas.h"

extern int debug_boss_direto;

static int sel;                               // botao selecionado: 0 = JOGAR, 1 = SAIR

// redesenha os 2 botoes (tiles de BTN_SELEC no selecionado, BTN_NORMAL no outro)
// retorna 0 ou erro do driver
static int pintar_botoes(void) {
    int rc = gfx_fundo_regiao(BTN_COL, BTN_LINHA_0, BTN_W, BTN_H,
                              sel == 0 ? BTN_SELEC : BTN_NORMAL);
    if (rc != 0) return rc;
    return gfx_fundo_regiao(BTN_COL, BTN_LINHA_1, BTN_W, BTN_H,
                            sel == 1 ? BTN_SELEC : BTN_NORMAL);
}

// monta a tela do menu: carrega fonte, fundo de titulo e botoes
static void entrar(void) {
    if (gfx_reset() != 0) return;
    if (gfx_carregar(CARGA_MENU, N_CARGA_MENU) != 0) return;
    if (gfx_fundo(MAPA_TITULO) != 0) return;
    sel = 0;
    if (pintar_botoes() != 0) return;
}

// 1 se o mouse esta sobre o botao que comeca em 'linha' (coords em tiles x 8 = pixels)
static int sobre_botao(int mx, int my, int linha) {
    return mx >= BTN_COL * 8 && mx < (BTN_COL + BTN_W) * 8 && my >= linha * 8 && my < (linha + BTN_H) * 8;
}

static void atualizar(void) {
    int mx = mouse_x(), my = mouse_y();
    static int mx_ant = -1, my_ant = -1;   // posicao anterior do mouse
    // so troca a selecao pelo mouse se ele mexeu (senao o teclado nunca ganharia)
    int mexeu = (mx != mx_ant || my != my_ant);
    mx_ant = mx; my_ant = my;

    // teclado: W/cima seleciona JOGAR, S/baixo seleciona SAIR
    if (tecla_pressionou(KEY_W) || tecla_pressionou(KEY_UP))   sel = 0;
    if (tecla_pressionou(KEY_S) || tecla_pressionou(KEY_DOWN)) sel = 1;
    if (mexeu) {
        if (sobre_botao(mx, my, BTN_LINHA_0)) sel = 0;
        else if (sobre_botao(mx, my, BTN_LINHA_1)) sel = 1;
    }
    if (pintar_botoes() != 0) return;

    // ---- atalhos de debug: N/ESPACO = fase 3 na onda 1, B/A/D = fase 3 direto no chefe ----
    int ir_noite = tecla_pressionou(KEY_N) || tecla_apertada(KEY_N) ||
                   tecla_pressionou(KEY_SPACE);
    int ir_boss  = tecla_pressionou(KEY_B) || tecla_apertada(KEY_B) ||
                   tecla_pressionou(KEY_A) || tecla_pressionou(KEY_D);

    if (ir_noite || ir_boss) {
        jogo_novo();
        jogo.fase = 2; // Fase 3 = indice 2 (NOITE)
        if (ir_boss) {
            debug_boss_direto = 1; // Pula direto para o Chefe da Fase 3
        } else {
            debug_boss_direto = 0; // Inicia na Onda 1 da Fase 3 (Noite)
        }
        estado_ir_para(&estado_transicao);
        return;
    }

    // ENTER ou clique em qualquer botao confirma a selecao atual
    int clicou_botao = mouse_clicou() && (sobre_botao(mx, my, BTN_LINHA_0) || sobre_botao(mx, my, BTN_LINHA_1));
    if (tecla_pressionou(KEY_ENTER) || clicou_botao) {
        if (sel == 0) {                  // JOGAR: campanha nova desde a fase 1
            debug_boss_direto = 0;
            jogo_novo();
            estado_ir_para(&estado_transicao);
        } else {                         // SAIR: main encerra o loop
            jogo.sair = 1;
        }
    }
}

// textos sobre os botoes + mira do mouse
static void desenhar(void) {
    int id = 0, fim = 62;                // ids 62 e 63 ficam fora da faixa do texto
    id = texto_centro(id, fim, 68, "WILD TE AMO", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, BTN_LINHA_0 * 8 + 4, "JOGAR", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, BTN_LINHA_1 * 8 + 4, "SAIR", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, 200, "ENTER OU CLIQUE", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    (void)id;
    int mx = mouse_x() - 8, my = mouse_y() - 8;   // centraliza o sprite 16x16 no cursor
    if (gfx_sprite(63, PAT_MENU_MIRA, mx < 0 ? 0 : mx, my < 0 ? 0 : my, 63, 1) != 0) return;
}

const Estado estado_menu = { entrar, atualizar, desenhar };
