// ondas.c - diretor de ondas: ritmo de spawn, spawn seguro, drops e chefe
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "entity.h"
#include "mat.h"
#include "jogo.h"
#include "jogador.h"
#include "powerups.h"
#include "inimigos.h"
#include "hud.h"
#include "ondas.h"

#define SPAWN_INTERVALO_INI 90      // quadros entre spawns: 1,5 s na primeira onda
#define SPAWN_INTERVALO_FIM 48      // 0,8 s na ultima
#define MAX_TELA_INI        12      // maximo de inimigos vivos na primeira onda
#define MAX_TELA_FIM        16      // maximo na ultima (tambem limita bandos em spawnar)
#define DIST_MIN_SPAWN      96      // distancia minima do jogador ao nascer (px)

// DEBUG: 1 = inicia direto no boss (zerado ao usar)
int debug_boss_direto = 0;

// prototipo necessario para chamar entrar_boss em ondas_iniciar
static void entrar_boss(void);

// estado do diretor (uma instancia so)
static struct {
    const FaseCfg *f;               // fase em andamento
    int onda, t;                    // onda atual e quadros desde que ela comecou
    int prox_spawn, intervalo, maximo;   // contagem pro proximo spawn, intervalo e teto de inimigos
    int drop_dado, elite_dado;      // flags: power-up garantido e elite ja saiu nesta onda
    int boss, concluida;            // fase do chefe ativa / fase terminada
} O;

// interpola linearmente de a (primeira onda) ate b (ultima)
static int lerp_onda(int a, int b) {
    int n = O.f->n_ondas;
    return n <= 1 ? a : a + (b - a) * O.onda / (n - 1);
}

// comeca a onda n: zera contadores, ajusta dificuldade e mostra o banner
static void entrar_onda(int n) {
    char b[32];
    O.onda = n; jogo.onda = n; O.t = 0;
    O.drop_dado = 0; O.elite_dado = 0;
    O.intervalo = lerp_onda(SPAWN_INTERVALO_INI, SPAWN_INTERVALO_FIM);
    O.maximo    = lerp_onda(MAX_TELA_INI, MAX_TELA_FIM);
    O.prox_spawn = 60;              // primeiro spawn 1 s depois do inicio
    snprintf(b, sizeof b, "ONDA %d", n + 1);
    hud_banner(b, 120);
}

// prepara o diretor para a fase f; comeca na onda 0 ou, em debug, direto no chefe
void ondas_iniciar(const FaseCfg *f) {
    memset(&O, 0, sizeof O);
    O.f = f;

    if (debug_boss_direto) {
        debug_boss_direto = 0;
        entrar_boss();
    } else {
        entrar_onda(0);
    }
}

int ondas_fase_concluida(void) { return O.concluida; }

// Spawn seguro: nasce numa borda, nunca a menos de DIST_MIN_SPAWN do jogador
// e nunca na borda para a qual ele esta andando.
// lado_fixo >= 0 forca a borda (usado pelo urubu); -1 = sorteia
// devolve 1 e preenche x, y, lado se achou ponto valido; 0 se desistiu
static int ponto_spawn(int lado_fixo, int *x, int *y, int *lado) {
    int proibido = jogador_direcao_mov();          // Direction: DOWN=0 UP=1 LEFT=2 RIGHT=3
    int lado_proib = -1;                           // borda vetada (a que o jogador esta indo)
    // converte direcao do jogador na borda correspondente
    switch (proibido) {
        case DIR_UP: lado_proib = LADO_CIMA; break;
        case DIR_DOWN: lado_proib = LADO_BAIXO; break;
        case DIR_LEFT: lado_proib = LADO_ESQ; break;
        case DIR_RIGHT: lado_proib = LADO_DIR; break;
    }
    // ate 24 tentativas de achar um ponto valido
    for (int t = 0; t < 24; t++) {
        int l = lado_fixo >= 0 ? lado_fixo : rng_faixa(0, 3);
        if (l == lado_proib && lado_fixo < 0) continue;
        int px, py;
        // sorteia a posicao ao longo da borda escolhida
        switch (l) {
            case LADO_CIMA:  px = rng_faixa(0, SCREEN_WIDTH - 16);  py = 16; break;
            case LADO_BAIXO: px = rng_faixa(0, SCREEN_WIDTH - 16);  py = SCREEN_HEIGHT - 16; break;
            case LADO_ESQ:   px = 0;                  py = rng_faixa(24, SCREEN_HEIGHT - 32); break;
            default:         px = SCREEN_WIDTH - 16;  py = rng_faixa(24, SCREEN_HEIGHT - 32); break;
        }
        // distancia entre o centro do inimigo (+8) e o centro do jogador
        int dx = px + 8 - jogador_cx(), dy = py + 8 - jogador_cy();
        if (isqrt_i(dx * dx + dy * dy) < DIST_MIN_SPAWN) continue;
        if (!fase_area_livre(px, py, 16, 16)) continue;       // nao nasce dentro de pedra/cacto
        *x = px; *y = py; *lado = l;
        return 1;
    }
    return 0;
}

// tenta criar um inimigo (ou bando) do tipo t; se nao couber ou nao achar ponto, nao cria
static void spawnar(TipoInimigo t) {
    int x, y, lado;
    int fixo = -1;                                   // -1 = borda sorteada
    if (t == INI_URUBU) {
        if (rng_faixa(0, 1))                         // fileira a/b: cruza a tela de lado a lado
            fixo = (jogador_cx() < SCREEN_WIDTH / 2) ? LADO_DIR : LADO_ESQ;
        else                                         // fileira c/d: vem de cima ou de baixo, em diagonal
            fixo = (jogador_cy() < SCREEN_HEIGHT / 2) ? LADO_BAIXO : LADO_CIMA;
    }
    // nao spawna se estourar o limite de inimigos na tela
    if (inimigos_contar() + inimigo_tamanho_grupo(t) > MAX_TELA_FIM) return;
    if (!ponto_spawn(fixo, &x, &y, &lado)) return;
    inimigo_spawn_em(t, x, y, lado);                    // urubu: bando nasce nesta borda e cruza a tela
}

// fase do chefe: limpa a tela, mostra o banner e cria o boss no topo
static void entrar_boss(void) {
    O.boss = 1;
    inimigos_limpar_tela(0);
    hud_banner("CHEFE", 150);
    inimigo_spawn_em(INI_BOSS, SCREEN_WIDTH / 2 - 8, 24, LADO_CIMA);
}

// 1x por quadro: avanca o relogio da onda, spawna inimigos e troca de onda
void ondas_atualizar(void) {
    if (O.concluida) return;
    if (O.boss) {                                          // so o chefe; fase acaba quando ele cai
        hud_barra_onda(0);
        if (!inimigos_boss_vivo()) O.concluida = 1;
        return;
    }
    O.t++;
    hud_barra_onda(256 - O.t * 256 / FRAMES_ONDA);         // barra de tempo restante em Q8 (256 = cheia)

    if (!O.drop_dado && O.t >= FRAMES_ONDA / 3) {          // 1 power-up garantido por onda (apos 1/3 do tempo)
        powerups_dropar_garantido();
        O.drop_dado = 1;
    }
    if (O.f->elite != INI_NENHUM && O.onda >= 1 && !O.elite_dado && O.t >= 300) {
        spawnar(O.f->elite);                               // elite: 1 por onda a partir da onda 2 (aos 5 s)
        O.elite_dado = 1;
    }
    // spawn periodico: sorteia um tipo do pool da onda (tipo repetido = mais comum)
    if (O.prox_spawn > 0) O.prox_spawn--;
    if (O.prox_spawn <= 0 && inimigos_contar() < O.maximo) {
        const TipoInimigo *pool = O.f->pool[O.onda];
        int n = O.f->n_pool[O.onda];
        if (n > 0) spawnar(pool[rng_faixa(0, n - 1)]);
        O.prox_spawn = O.intervalo;
    }
    // fim do tempo da onda: proxima onda, chefe ou fim da fase
    if (O.t >= FRAMES_ONDA) {
        if (O.onda + 1 < O.f->n_ondas) entrar_onda(O.onda + 1);
        else if (O.f->tem_boss) entrar_boss();
        else { inimigos_limpar_tela(0); O.concluida = 1; }
    }
}
