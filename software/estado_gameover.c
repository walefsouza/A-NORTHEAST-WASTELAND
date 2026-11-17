// estado_gameover.c - FIM DE JOGO (jogo.venceu = 0) e VITORIA (jogo.venceu = 1)
#include <stdio.h>
#include "estado.h"
#include "teclas.h"
#include "entrada.h"
#include "gfx.h"
#include "texto.h"
#include "jogo.h"
#include "menu_mapas.h"
#include "sprites.h"

// ---- botoes: ENTER JOGAR (cima) e R MENU (baixo), cada um um retangulo de poligonos ----
// cada botao usa 2 poligonos: borda (maior) e fundo (menor, por cima)
#define POL_BTN0_BORDA 4
#define POL_BTN0_FUNDO 5
#define POL_BTN1_BORDA 6
#define POL_BTN1_FUNDO 7
#define RET_X   74                  // x do retangulo (px)
#define RET_LARG 172                // largura
#define RET_ALT  28                 // altura
#define RET0_Y  110                 // y do botao JOGAR
#define RET1_Y  148                 // y do botao MENU
#define PISCA_QUADROS 12            // troca de cor a cada 0,2 s: lento, nao frenetico

// pares {borda clara, fundo escuro} (RRRGGGBB) percorridos ao piscar
static const unsigned char PISCA[][2] = {
    { 0xFC, 0x68 },   // amarelo
    { 0xF0, 0x64 },   // laranja
    { 0xE0, 0x60 },   // vermelho
    { 0xE3, 0x63 },   // magenta
    { 0x1F, 0x0B },   // ciano
    { 0x1C, 0x08 },   // verde
};
#define N_PISCA ((int)(sizeof PISCA / sizeof PISCA[0]))   // quantidade de pares
#define COR_BORDA_NORMAL 0x92       // cores com o mouse fora do botao
#define COR_FUNDO_NORMAL 0x25

static int quadros;                 // contador de quadros (anima o pisca)

// 1 se o mouse (mx,my) esta dentro do botao que comeca em y = by
static int sobre(int mx, int my, int by) {
    return mx >= RET_X && mx < RET_X + RET_LARG && my >= by && my < by + RET_ALT;
}

// desenha o botao (borda + fundo); pisca de cor se o mouse esta em cima
// retorna 0 ou erro do driver
static int pintar_botao(int pol_borda, int pol_fundo, int by, int em_cima) {
    int borda = COR_BORDA_NORMAL, fundo = COR_FUNDO_NORMAL;
    if (em_cima) {
        int k = (quadros / PISCA_QUADROS) % N_PISCA;   // indice do par de cores atual
        borda = PISCA[k][0]; fundo = PISCA[k][1];
    }
    // borda: retangulo 3 px maior em cada lado
    int rc = gfx_poligono(pol_borda, 1, borda, RET_X - 3, by - 3, RET_LARG + 6, RET_ALT + 6);
    if (rc != 0) return rc;
    return gfx_poligono(pol_fundo, 1, fundo, RET_X, by, RET_LARG, RET_ALT);
}

// carrega fonte e fundo; o fundo depende de venceu (vitoria x game over)
static void entrar(void) {
    quadros = 0;
    if (gfx_reset() != 0) return;
    if (gfx_carregar(CARGA_MENU, N_CARGA_MENU) != 0) return;
    if (gfx_fundo(jogo.venceu ? MAPA_VITORIA : MAPA_GAMEOVER) != 0) return;
}

// reinicia a campanha e volta pra transicao da fase 1
static void jogar_de_novo(void) { jogo_novo(); estado_ir_para(&estado_transicao); }

// R / clique em MENU -> menu; ENTER / clique em JOGAR -> nova partida
static void atualizar(void) {
    quadros++;
    int mx = mouse_x(), my = mouse_y();
    int clicou = mouse_clicou();

    if (tecla_pressionou(KEY_R) || (clicou && sobre(mx, my, RET1_Y))) estado_ir_para(&estado_menu);
    else if (tecla_pressionou(KEY_ENTER) || (clicou && sobre(mx, my, RET0_Y))) jogar_de_novo();
}

// botoes, textos e mira; cada texto devolve o proximo id livre de sprite
static void desenhar(void) {
    char b[48];
    int id = 0, fim = 63;                 // id 63 = mira do mouse (reservado)
    int mx = mouse_x(), my = mouse_y();

    if (pintar_botao(POL_BTN0_BORDA, POL_BTN0_FUNDO, RET0_Y, sobre(mx, my, RET0_Y)) != 0) return;
    if (pintar_botao(POL_BTN1_BORDA, POL_BTN1_FUNDO, RET1_Y, sobre(mx, my, RET1_Y)) != 0) return;

    id = texto_centro(id, fim, 28, jogo.venceu ? "VITORIA!" : "FIM DE JOGO", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    snprintf(b, sizeof b, "PONTOS %06lu", jogo.pontos);
    id = texto_centro(id, fim, 56, b, TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    if (jogo.venceu) id = texto_centro(id, fim, 76, "O SERTAO E SEU", TXT_MENU);
    else { snprintf(b, sizeof b, "FASE %d  ONDA %d", jogo.fase + 1, jogo.onda + 1); id = texto_centro(id, fim, 76, b, TXT_MENU); }
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, RET0_Y + 6, "JOGAR", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    id = texto_centro(id, fim, RET1_Y + 6, "MENU", TXT_MENU);
    if (gfx_ultimo_erro() != 0) return;
    // esconde os sprites que sobraram da partida (cowboy, inimigos, tiros)
    texto_limpar(id, fim);
    if (gfx_ultimo_erro() != 0) return;
    // mira do mouse, limitada ao canto da tela
    (void)gfx_sprite(63, PAT_MENU_MIRA, mx - 8 < 0 ? 0 : mx - 8, my - 8 < 0 ? 0 : my - 8, 63, 1);
}

const Estado estado_gameover = { entrar, atualizar, desenhar };
