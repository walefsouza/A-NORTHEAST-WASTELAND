// hud.c - HUD da partida (pontos, vidas, icone de power-up, escudo, barra da onda, banner) e painel de pausa
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "driver.h"
#include "sprites.h"
#include "gfx.h"
#include "texto.h"
#include "jogo.h"
#include "jogador.h"
#include "powerups.h"
#include "hud.h"
#include "entity.h"

// cores RRRGGGBB
#define COR_BARRA_ONDA   0xFC       // amarelo
#define COR_BARRA_PERIGO 0xE0       // vermelho (onda perto do fim)
#define COR_BARRA_ITEM   0x1F       // ciano
// ids de poligono usados pelo HUD (os botoes do gameover usam 4..7)
#define POL_ONDA 0                  // barra de tempo da onda
#define POL_ITEM 1                  // barra de duracao do power-up
#define POL_PAUSA_BORDA 2
#define POL_PAUSA_FUNDO 3

static char banner[16];             // texto central atual
static int banner_t;                // quadros restantes do banner (0 = sem banner)
static int barra_onda;              // tempo restante da onda, Q8 (0..256)

// zera estado do HUD ao comecar a partida
void hud_iniciar(void) {
    banner[0] = 0;
    banner_t = 0;
    barra_onda = 0;
}

// agenda texto central por 'frames' quadros (copia e trunca o texto)
void hud_banner(const char *s, int frames) {
    if (s == NULL) return;
    strncpy(banner, s, sizeof banner - 1);
    banner[sizeof banner - 1] = 0;
    banner_t = frames;
}

// atualiza a fracao da barra de onda, limitada a 0..256
void hud_barra_onda(int q8) {
    barra_onda = q8 < 0 ? 0 : (q8 > 256 ? 256 : q8);
}

// desenha o HUD completo; para no primeiro erro e devolve o codigo
int hud_desenhar(void) {
    char b[12];
    int rc;

    // nao empilha comandos se ja houve erro no quadro
    if (gfx_ultimo_erro() != DRIVER_OK) return gfx_ultimo_erro();

    // pontos: 6 digitos no topo
    snprintf(b, sizeof b, "%06lu", jogo.pontos);
    texto_desenhar(SPR_HUD_PONTOS, SPR_HUD_PONTOS + 6, 64, 8, b, TXT_JOGO);
    if (gfx_ultimo_erro() != DRIVER_OK) return gfx_ultimo_erro();

    // vidas: 1 coracao por vida maxima; visivel so se k < vidas atuais
    for (int k = 0; k < VIDAS_MAX; k++) {
        rc = gfx_sprite(SPR_HUD_VIDA0 + k, PAT_ITEM_CORACAO,
                        184 + 16 * k, 6, 63, k < jogo.vidas);
        if (rc != DRIVER_OK) return rc;
    }

    // power-up ativo: icone + barra de duracao (fr em Q8, barra de 16 px no maximo)
    int pat, fr;
    if (powerups_hud(&pat, &fr)) {
        rc = gfx_sprite(SPR_HUD_ICONE, pat, 4, 26, 63, 1);
        if (rc != DRIVER_OK) return rc;
        rc = gfx_poligono(POL_ITEM, 1, COR_BARRA_ITEM,
                          4, 44, (fr * 16) >> 8, 3);
        if (rc != DRIVER_OK) return rc;
    } else {                                     // sem power-up: esconde icone e barra
        rc = gfx_esconder(SPR_HUD_ICONE);
        if (rc != DRIVER_OK) return rc;
        rc = gfx_poligono_off(POL_ITEM);
        if (rc != DRIVER_OK) return rc;
    }

    // icone do escudo (cantil), visivel so se o jogador tem escudo
    rc = gfx_sprite(SPR_HUD_ESCUDO, PAT_ITEM_CANTIL,
                    24, 26, 63, jogador_tem_escudo());
    if (rc != DRIVER_OK) return rc;

    // barra de tempo da onda no topo: fica vermelha nos ultimos ~20% (52/256)
    rc = gfx_poligono(POL_ONDA, 1,
                      barra_onda < 52 ? COR_BARRA_PERIGO : COR_BARRA_ONDA,
                      0, 0, (SCREEN_WIDTH * barra_onda) >> 8, 3);
    if (rc != DRIVER_OK) return rc;

    // banner central; quando o tempo acaba, apaga os sprites do texto
    if (banner_t > 0) {
        texto_centro(SPR_HUD_TEXTO, SPR_HUD_TEXTO + SPR_HUD_TEXTO_N,
                     104, banner, TXT_JOGO);
        if (gfx_ultimo_erro() != DRIVER_OK) return gfx_ultimo_erro();
        if (--banner_t == 0) {
            rc = texto_limpar(SPR_HUD_TEXTO, SPR_HUD_TEXTO + SPR_HUD_TEXTO_N);
            if (rc != DRIVER_OK) return rc;
        }
    }

    return DRIVER_OK;
}

// ---- pausa: painel de poligonos (borda dourada + fundo escuro) ----
int hud_pausa_abrir(void) {
    int rc;

    // painel: borda dourada (76,92 168x56) e fundo escuro 4 px para dentro
    rc = gfx_poligono(POL_PAUSA_BORDA, 1, 0xFC, 76, 92, 168, 56);
    if (rc != DRIVER_OK) return rc;
    rc = gfx_poligono(POL_PAUSA_FUNDO, 1, 0x25, 80, 96, 160, 48);
    if (rc != DRIVER_OK) return rc;

    // esconde sprites que ficariam por cima do painel (poligonos ficam atras dos sprites)
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity *e = &entities[i];
        if (e->active && e->x + 16 > 76 && e->x < 244 &&
            e->y + 16 > 92 && e->y < 148) {
            rc = gfx_esconder(i);
            if (rc != DRIVER_OK) return rc;
        }
    }

    // troca qualquer banner pelo texto PAUSA
    rc = texto_limpar(SPR_HUD_TEXTO, SPR_HUD_TEXTO + SPR_HUD_TEXTO_N);
    if (rc != DRIVER_OK) return rc;
    texto_centro(SPR_HUD_TEXTO, SPR_HUD_TEXTO + SPR_HUD_TEXTO_N,
                 112, "PAUSA", TXT_JOGO);
    return gfx_ultimo_erro();
}

// remove o painel e o texto; as entidades escondidas voltam no proximo gfx_entidades()
int hud_pausa_fechar(void) {
    int rc = gfx_poligono_off(POL_PAUSA_BORDA);
    if (rc != DRIVER_OK) return rc;
    rc = gfx_poligono_off(POL_PAUSA_FUNDO);
    if (rc != DRIVER_OK) return rc;
    rc = texto_limpar(SPR_HUD_TEXTO, SPR_HUD_TEXTO + SPR_HUD_TEXTO_N);
    if (rc != DRIVER_OK) return rc;
    banner_t = 0;
    return DRIVER_OK;
}
