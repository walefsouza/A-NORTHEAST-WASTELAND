// mat.h - aleatório e contas de vetor só com inteiros, sem math.h
#ifndef MAT_H
#define MAT_H
void     rng_semente(unsigned v);
unsigned rng_prox(void);
int      rng_faixa(int lo, int hi);          // sorteia entre lo e hi, os dois valores podem sair
int      isqrt_i(int v);
int      abs_i(int v);
int      sinal(int v);
// transforma (dx, dy) num vetor de tamanho vel_q4. Em q4, 16 equivale a 1 px por quadro
void     vetor_q4(int dx, int dy, int vel_q4, int *vx, int *vy);
#endif
