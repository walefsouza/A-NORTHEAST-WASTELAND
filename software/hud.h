// hud.h - pontos, vidas, icones, barra de tempo da onda, faixas de texto
// todas as funcoes que desenham retornam 0 (DRIVER_OK) ou o codigo de erro do driver
#ifndef HUD_H
#define HUD_H
void hud_iniciar(void);                          // zera banner e barra (inicio da partida)
void hud_banner(const char *texto, int frames);  // mostra texto central por N quadros
void hud_barra_onda(int frac_q8);                // tempo restante da onda, ponto fixo Q8 (0..256)
int hud_desenhar(void);                          // desenha o HUD inteiro 1x por quadro
int hud_pausa_abrir(void);                       // painel "PAUSA"
int hud_pausa_fechar(void);                      // remove o painel
#endif
