// ondas.h - diretor de ondas (estilo Journey of the Prairie King)
#ifndef ONDAS_H
#define ONDAS_H
#include "fase.h"

#define FRAMES_ONDA   1800     // ~30 s a 60 Hz

extern int debug_boss_direto;  // DEBUG: 1 = proxima partida comeca direto no chefe

void ondas_iniciar(const FaseCfg *f);   // comeca a fase pela onda 0 (ou pelo chefe, se debug)
void ondas_atualizar(void);             // 1x por quadro: spawns, drops, troca de onda, chefe
int  ondas_fase_concluida(void);        // 1 = fase terminou (chefe morto ou ultima onda acabou)
#endif
