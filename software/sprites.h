/* sprites.h - Dados dos sprites e tiles (convertidos dos .hex). Edite so se souber o que faz.
 *
 * Declara o mapa de padroes (indices dos 64 slots da memoria de padroes da FPGA)
 * e as tabelas de lote (CargaPadrao) usadas para carregar os graficos conforme a fase.
 * Os pixels em si ficam definidos em sprites.c. */
#ifndef SPRITES_H
#define SPRITES_H

#define SPR_PIXELS 256   /* 16x16, 1 byte RRRGGGBB, 0 = transparente */

/* Um item de carga: qual slot da memoria de padroes preencher e de onde vem
 * a matriz de 256 pixels (usado por gfx_carregar()). */
typedef struct { unsigned char padrao; const unsigned char *pixels; } CargaPadrao;

/* ---- mapa de padroes em jogo (posicoes fixas) ---- */
/* Os padroes 0..46 sao comuns a todas as fases e ficam sempre carregados.
 * A partir do 47 os slots sao reaproveitados: cada fase carrega seus proprios
 * inimigos nas mesmas posicoes (por isso os valores se repetem entre MANHA, TARDE e NOITE). */
enum {
    /* jogador: um padrao por quadro de animacao em cada direcao + morte */
    PAT_PLAYER_INITIAL = 0,
    PAT_PLAYER_UP_0 = 1,
    PAT_PLAYER_UP_1 = 2,
    PAT_PLAYER_UP_2 = 3,
    PAT_PLAYER_DOWN_0 = 4,
    PAT_PLAYER_LEFT_0 = 5,
    PAT_PLAYER_LEFT_1 = 6,
    PAT_PLAYER_RIGHT_0 = 7,
    PAT_PLAYER_RIGHT_1 = 8,
    PAT_PLAYER_DEATH_0 = 9,
    PAT_PLAYER_DEATH_1 = 10,
    PAT_PLAYER_DEATH_2 = 11,

    /* projeteis e mira */
    PAT_BALA = 12,
    PAT_MIRA = 13,
    PAT_TIRO_INIMIGO = 14,

    /* itens coletaveis */
    PAT_ITEM_BACAMARTE = 15,
    PAT_ITEM_CARTUCHEIRA = 16,
    PAT_ITEM_ESTRELA = 17,
    PAT_ITEM_RAPADURA = 18,
    PAT_ITEM_CANTIL = 19,
    PAT_ITEM_FOGUEIRA = 20,
    PAT_ITEM_CORACAO = 21,
    PAT_ITEM_MOEDA1 = 22,
    PAT_ITEM_MOEDA5 = 23,

    /* obstaculos do cenario */
    PAT_PEDRA = 24,
    PAT_MANDACARU = 25,

    /* digitos 0-9 para o HUD (pontuacao, vidas, etc.) */
    PAT_DIG_0 = 26,
    PAT_DIG_1 = 27,
    PAT_DIG_2 = 28,
    PAT_DIG_3 = 29,
    PAT_DIG_4 = 30,
    PAT_DIG_5 = 31,
    PAT_DIG_6 = 32,
    PAT_DIG_7 = 33,
    PAT_DIG_8 = 34,
    PAT_DIG_9 = 35,

    /* letras usadas no HUD / mensagens em jogo (so as necessarias) */
    PAT_LET_O = 36,
    PAT_LET_N = 37,
    PAT_LET_D = 38,
    PAT_LET_A = 39,
    PAT_LET_F = 40,
    PAT_LET_S = 41,
    PAT_LET_E = 42,
    PAT_LET_P = 43,
    PAT_LET_U = 44,
    PAT_LET_C = 45,
    PAT_LET_H = 46,

    /* ---- fase 1 (manha): inimigos, slots 47..51 ---- */
    PAT_MANHA_CALANGO = 47,
    PAT_MANHA_CACTO = 48,
    PAT_MANHA_MARIMBONDO_0 = 49,
    PAT_MANHA_MARIMBONDO_1 = 50,
    PAT_MANHA_CALANGO_LADO = 51,   /* calango girado: cabeca para a esquerda */

    /* ---- fase 2 (tarde): inimigos, slots 47..56 (urubus A-D em 2 quadros cada) ---- */
    PAT_TARDE_CASCAVEL_0 = 47,
    PAT_TARDE_CASCAVEL_1 = 48,
    PAT_TARDE_URUBU_A_0 = 49,
    PAT_TARDE_URUBU_A_1 = 50,
    PAT_TARDE_URUBU_B_0 = 51,
    PAT_TARDE_URUBU_B_1 = 52,
    PAT_TARDE_URUBU_C_0 = 53,
    PAT_TARDE_URUBU_C_1 = 54,
    PAT_TARDE_URUBU_D_0 = 55,
    PAT_TARDE_URUBU_D_1 = 56,

    /* ---- fase 3 (noite): inimigos comuns e chefe, slots 47..62 ---- */
    PAT_NOITE_MORCEGO_0 = 47,
    PAT_NOITE_MORCEGO_1 = 48,
    PAT_NOITE_CORPO_SECO_0 = 49,
    PAT_NOITE_ESCORPIAO_0 = 50,
    PAT_NOITE_LOBISOMEN_0 = 51,
    PAT_NOITE_BOSS_0 = 52,         /* chefe da fase 3: parado, quadro 1 */
    PAT_BOSS_IDLE_1 = 53,          /* chefe: parado, quadro 2 */
    PAT_BOSS_ANDA_0 = 54,          /* chefe: andando (ciclo 0-1-2-1) */
    PAT_BOSS_ANDA_1 = 55,
    PAT_BOSS_ANDA_2 = 56,
    PAT_BOSS_ATAQUE_0 = 57,        /* chefe: garras erguidas (aviso da investida) */
    PAT_BOSS_ATAQUE_1 = 58,        /* chefe: golpe esquerdo (durante a investida) */
    PAT_BOSS_ATAQUE_2 = 59,        /* chefe: golpe direito (durante a investida) */
    PAT_BOSS_FURIA_0 = 60,         /* chefe enfurecido: andando, quadro 1 */
    PAT_BOSS_RUGIDO = 61,          /* chefe: boca aberta (rugido / cuspe dos espinhos) */
    PAT_BOSS_FURIA_1 = 62,         /* chefe enfurecido: andando, quadro 2 */
    PAT_TOTAL_JOGO                 /* quantidade total de padroes usados em jogo */
};

/* ---- menus: fonte completa nos padroes 0..36 (0-9, A-Z, !) + mira ---- */
/* Nos menus a memoria de padroes e recarregada com a fonte completa,
 * por isso os indices abaixo nao coincidem com os do enum acima. */
#define PAT_MENU_FONTE 0
#define PAT_MENU_MIRA  37

/* Tabelas de lote e seus tamanhos (definidas em sprites.c):
 *   CARGA_COMUM - padroes 0..46, sempre presentes em qualquer fase
 *   CARGA_MANHA / CARGA_TARDE / CARGA_NOITE - padroes exclusivos de cada fase
 *   CARGA_MENU  - fonte completa + mira para as telas de menu */
extern const CargaPadrao CARGA_COMUM[];   extern const int N_CARGA_COMUM;
extern const CargaPadrao CARGA_MANHA[];   extern const int N_CARGA_MANHA;
extern const CargaPadrao CARGA_TARDE[];   extern const int N_CARGA_TARDE;
extern const CargaPadrao CARGA_NOITE[];   extern const int N_CARGA_NOITE;
extern const CargaPadrao CARGA_MENU[];   extern const int N_CARGA_MENU;

#endif
