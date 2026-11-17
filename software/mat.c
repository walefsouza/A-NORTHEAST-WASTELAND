// mat.c - gerador xorshift e funções de inteiro usadas pela IA e pelos tiros
#include "mat.h"

static unsigned estado = 2463534242u;   // semente padrão, vale até alguém chamar rng_semente

// a semente 0 travaria o xorshift em 0 para sempre, por isso vira 1
void rng_semente(unsigned v) { estado = v ? v : 1u; }
// xorshift32, com deslocamentos 13, 17 e 5
unsigned rng_prox(void) {
    estado ^= estado << 13; estado ^= estado >> 17; estado ^= estado << 5;
    return estado;
}
// o módulo favorece um pouco os primeiros valores, mas para o jogo isso não importa
int rng_faixa(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rng_prox() % (unsigned)(hi - lo + 1));
}
int abs_i(int v) { return v < 0 ? -v : v; }
// -1, 0 ou +1 conforme o sinal de v
int sinal(int v) { return (v > 0) - (v < 0); }
// raiz quadrada inteira pelo método de Newton. Começa em v e vai descendo até parar de diminuir
int isqrt_i(int v) {
    if (v <= 0) return 0;
    int x = v, y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + v / x) / 2; }
    return x;
}
// divide (dx, dy) pela distância para ficar com tamanho 1 e multiplica por vel_q4. Se d = 0 devolve 0 nos dois
void vetor_q4(int dx, int dy, int vel_q4, int *vx, int *vy) {
    int d = isqrt_i(dx * dx + dy * dy);
    if (d == 0) { *vx = 0; *vy = 0; return; }
    *vx = dx * vel_q4 / d;
    *vy = dy * vel_q4 / d;
}
