// texto.h - texto com sprites 16x16 (uma letra = um sprite)
#ifndef TEXTO_H
#define TEXTO_H

// escolhe a fonte: menu (letras e numeros completos) ou jogo (so as letras usadas em HUD)
typedef enum {
    TXT_MENU,
    TXT_JOGO
} TxtModo;

int texto_largura(const char *s);   // largura em pixels (para centralizar)
// id..fim = faixa de sprites reservada para o texto
// Retorna o proximo ID livre. Em falha grafica, consultar gfx_ultimo_erro().
int texto_desenhar(int id, int fim, int x, int y, const char *s, TxtModo m);
int texto_centro(int id, int fim, int y, const char *s, TxtModo m);   // idem, centralizado na tela
int texto_limpar(int id, int fim);   // esconde os sprites id..fim-1
#endif
