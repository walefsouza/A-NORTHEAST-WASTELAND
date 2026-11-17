// texto.c - desenha texto usando 1 sprite 16x16 por letra
#include "config.h"
#include "sprites.h"
#include "gfx.h"
#include "driver.h"
#include "texto.h"

#define AVANCO_LETRA  14   // px que o cursor anda apos uma letra
#define AVANCO_ESPACO  8   // idem para espaco e '!' (mais estreitos)

// quanto o cursor avanca depois do caractere c
static int avanco(char c) { return (c == ' ' || c == '!') ? AVANCO_ESPACO : AVANCO_LETRA; }

// padrao (indice do sprite) do caractere na fonte do menu; -1 = sem glifo
// fonte: 0-9, depois A-Z, depois '!'
static int padrao_menu(char c) {
    if (c >= '0' && c <= '9') return PAT_MENU_FONTE + (c - '0');
    if (c >= 'A' && c <= 'Z') return PAT_MENU_FONTE + 10 + (c - 'A');
    if (c == '!') return PAT_MENU_FONTE + 36;
    return -1;
}

// idem para a fonte do jogo (HUD): digitos + so as letras que o jogo usa
// (PONTOS, ONDA, CHEFE, PAUSA...)
static int padrao_jogo(char c) {
    if (c >= '0' && c <= '9') return PAT_DIG_0 + (c - '0');
    switch (c) {
        case 'O': return PAT_LET_O; case 'N': return PAT_LET_N; case 'D': return PAT_LET_D;
        case 'A': return PAT_LET_A; case 'F': return PAT_LET_F; case 'S': return PAT_LET_S;
        case 'E': return PAT_LET_E; case 'P': return PAT_LET_P; case 'U': return PAT_LET_U;
        case 'C': return PAT_LET_C; case 'H': return PAT_LET_H;
    }
    return -1;
}

// largura total em pixels, somando o avanco de cada caractere
int texto_largura(const char *s) {
    if (s == NULL) return 0;
    int w = 0;
    for (; *s; s++) w += avanco(*s);
    return w;
}

// desenha s em (x,y) usando sprites id..fim-1; devolve o proximo id livre
// caractere sem glifo (ex: acento) so avanca o cursor; para se acabarem os ids ou houver erro
int texto_desenhar(int id, int fim, int x, int y, const char *s, TxtModo m) {
    if (s == NULL || gfx_ultimo_erro() != DRIVER_OK) return id;
    for (; *s; s++) {
        int pat = (m == TXT_MENU) ? padrao_menu(*s) : padrao_jogo(*s);
        if (pat >= 0 && id < fim) {
            // x - 1 alinha o glifo; '!' e o unico sem esse ajuste
            if (gfx_sprite(id, pat, x - (*s == '!' ? 0 : 1), y, 63, 1) != DRIVER_OK)
                return id;
            id++;
        }
        x += avanco(*s);
    }
    return id;
}

// texto horizontalmente centralizado na tela
int texto_centro(int id, int fim, int y, const char *s, TxtModo m) {
    return texto_desenhar(id, fim, (SCREEN_WIDTH - texto_largura(s)) / 2, y, s, m);
}

// esconde os sprites id..fim-1 (usar para apagar um texto)
int texto_limpar(int id, int fim) {
    if (id < 0 || fim < id) return DRIVER_ERRO_PARAMETRO;
    for (int i = id; i < fim; i++) {
        int rc = gfx_esconder(i);
        if (rc != DRIVER_OK) return rc;
    }
    return DRIVER_OK;
}
