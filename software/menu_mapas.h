// menu_mapas.h - dados dos tilemaps de menu e botoes (convertidos dos .hex). Edite so se souber o que faz.
#ifndef MENU_MAPAS_H
#define MENU_MAPAS_H

#define N_TILES_ROM 250   // ids 0..249 usados; ROM comporta 256

// posicao e tamanho dos botoes do menu, em tiles (1 tile = 8 px)
#define BTN_COL 14        // coluna do canto esquerdo
#define BTN_W 12          // largura
#define BTN_H 3           // altura
#define BTN_LINHA_0 14    // linha do botao JOGAR
#define BTN_LINHA_1 18    // linha do botao SAIR

extern const unsigned char MAPA_TITULO[1200];
extern const unsigned char MAPA_GAMEOVER[1200];
extern const unsigned char MAPA_VITORIA[1200];
extern const unsigned char MAPA_TRANSICAO_MANHA[1200];
extern const unsigned char MAPA_TRANSICAO_TARDE[1200];
extern const unsigned char MAPA_TRANSICAO_NOITE[1200];
extern const unsigned char BTN_NORMAL[BTN_W * BTN_H];
extern const unsigned char BTN_SELEC[BTN_W * BTN_H];

#endif
