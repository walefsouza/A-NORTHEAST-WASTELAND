// powerups.h - itens do chão: quais existem, como caem dos inimigos e o que cada um faz
#ifndef POWERUPS_H
#define POWERUPS_H

typedef enum {
    PU_NENHUM = 0,
    PU_BACAMARTE,     // três tiros em leque, 10 s
    PU_CARTUCHEIRA,   // atira duas vezes mais rápido, 10 s
    PU_ESTRELA,       // tiro nas 8 direções, 8 s
    PU_RAPADURA,      // anda 50% mais rápido, 10 s
    PU_CANTIL,        // escudo que segura 1 dano
    PU_FOGUEIRA,      // limpa a tela, menos o chefe
    PU_CORACAO,       // +1 vida, até 3
    PU_MOEDA1,        // +50 pontos
    PU_MOEDA5,        // +250 pontos
    PU_N
} PowerUp;

#define DROP_CHANCE_PCT   10          // chance, em %, de um inimigo derrubar item ao morrer
#define MAX_ITENS_TELA     2          // itens no chão ao mesmo tempo
#define ITEM_VIDA_FRAMES 480          // o item some depois de 8 s
#define ITEM_PISCA_FRAMES 120         // e pisca nos 2 s finais

void powerups_reset(void);
void powerups_tick(void);                       // uma vez por quadro, desconta os tempos
void powerups_coletar(PowerUp p);               // aplica o efeito do item
void powerups_dropar(int cx, int cy);           // sorteia se o inimigo que morreu em (cx, cy) derruba item
void powerups_dropar_garantido(void);           // um item garantido por onda, em ponto livre

PowerUp powerups_arma(void);                    // arma ativa: PU_NENHUM, PU_BACAMARTE ou PU_ESTRELA
int     powerups_rapido(void);
int     powerups_veloz(void);
// ícone do último power-up com tempo que ainda vale. frac_q8 é o tempo que sobra, de 0 a 256.
// Devolve 0 se não há nenhum ativo
int     powerups_hud(int *padrao, int *frac_q8);
#endif
