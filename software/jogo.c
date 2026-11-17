// jogo.c - estado global da campanha
#include "jogo.h"

Jogo jogo;

// reinicia a campanha: fase 0, onda 0, zero pontos, vidas cheias
// (nao mexe em jogo.sair: so o menu decide sair)
void jogo_novo(void) {
    jogo.fase = 0;
    jogo.onda = 0;
    jogo.pontos = 0;
    jogo.vidas = VIDAS_MAX;
    jogo.venceu = 0;
}

// soma pontos com saturacao em 999999 (HUD tem 6 digitos)
void jogo_pontuar(int p) {
    jogo.pontos += (unsigned long)p;
    if (jogo.pontos > 999999UL) jogo.pontos = 999999UL;
}
