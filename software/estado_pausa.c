// estado_pausa.c - painel de poligonos sobre a partida congelada
// a partida nao e atualizada nem reiniciada enquanto este estado roda
#include <stdio.h>
#include "estado.h"
#include "teclas.h"
#include "hud.h"

// desenha o painel por cima dos sprites da partida
static void entrar(void) { if (hud_pausa_abrir() != 0) return; }

static void atualizar(void) {
    if (tecla_pressionou(KEY_ENTER) || tecla_pressionou(KEY_P)) {
        // volta para a partida sem reiniciar (entrar() da partida faz so hud_pausa_fechar)
        partida_marcar_retomada();
        estado_ir_para(&estado_partida);
    } else if (tecla_pressionou(KEY_R)) {
        // abandona a partida e vai pro menu
        partida_cancelar_retomada();
        estado_ir_para(&estado_menu);
    }
}

// nada a redesenhar: a tela fica congelada
static void desenhar(void) { }

const Estado estado_pausa = { entrar, atualizar, desenhar };
