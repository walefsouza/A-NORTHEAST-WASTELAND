/* gfx.c - camada grafica com cache atualizado somente apos ACK do driver.
 *
 * Mantem uma copia local (shadow state) do estado dos 64 sprites, 8 poligonos
 * e 1200 tiles do background. Evita saturar a FIFO da FPGA com comandos
 * redundantes quando posicao, padrao ou cor nao mudaram entre quadros.
 */
#include <string.h>
#include "config.h"
#include "driver.h"
#include "entity.h"
#include "gfx.h"

/* Estado em cache de um sprite de hardware (ID 0..63).
 * Cada campo tem validade propria: uma falha no meio de uma operacao
 * composta nao faz o cache afirmar que os comandos seguintes foram aceitos. */
typedef struct {
    unsigned char on;                      /* 1 se o sprite esta inserido/ativo na FPGA, 0 se oculto */
    unsigned char pat, prio;               /* padrao grafico (0..63) e prioridade de camada (0..63) */
    unsigned char pat_ok, prio_ok, pos_ok; /* flags indicando se o hardware ja confirmou cada atributo */
    short x, y;                            /* coordenadas atuais na tela confirmadas por ACK */
} SpSh;

/* Espelhamento: 'flip_want' e o desejado pelo jogo (bit0 = H, bit1 = V);
 * 'flip_hw' e o ultimo valor aceito pelo hardware para aquele ID e sobrevive
 * ao delspr/insspr. 4 = desconhecido (forca reenvio). */
static unsigned char flip_want[64];
static unsigned char flip_hw[64];

/* Estado em cache de um poligono de hardware (ID 0..7) */
typedef struct {
    unsigned char on;                                  /* 1 se o poligono esta ativo na FPGA */
    unsigned char forma, cor;                          /* tipo geometrico (0..4) e cor RRRGGGBB (0..255) */
    unsigned char forma_ok, cor_ok, tamanho_ok, pos_ok;/* flags de sincronizacao com o hardware */
    short x, y, w, h;                                  /* posicao (x, y) e dimensoes (largura w, altura h) */
} PolSh;

static SpSh sp[64];                                    /* cache dos 64 registradores de sprite */
static PolSh pol[8];                                   /* cache dos 8 registradores de poligono */
static unsigned char bg[MAP_COLS * MAP_ROWS];          /* cache dos indices de tile do background (40x30) */
static unsigned char bg_ok[MAP_COLS * MAP_ROWS];       /* 1 se a posicao correspondente em bg[] ja foi gravada */

/* Guarda o primeiro erro grafico do quadro. Assim, mesmo funcoes antigas
 * cujo chamador ainda nao consome o retorno nao escondem uma falha: main()
 * consulta gfx_ultimo_erro() antes de continuar o jogo. */
static int erro_gfx = DRIVER_OK;

/* Armazena o primeiro codigo de erro ocorrido (latch) e retorna o estado de erro atual. */
static int registrar_resultado(int rc) {
    if (rc != DRIVER_OK && erro_gfx == DRIVER_OK)
        erro_gfx = rc;
    return (erro_gfx != DRIVER_OK) ? erro_gfx : rc;
}

/* Retorna DRIVER_OK se nao houve falhas pendentes, ou o codigo do erro travado. */
static int pronto(void) {
    return erro_gfx;
}

/* Consulta publica do ultimo erro retornado pelo driver na camada grafica. */
int gfx_ultimo_erro(void) {
    return erro_gfx;
}

/* Reseta a flag de erro acumulada na camada grafica. */
void gfx_limpar_erro(void) {
    erro_gfx = DRIVER_OK;
}

/* Reinicia o estado visual na FPGA (desativa sprites/poligonos, zera scroll e invalida caches). */
int gfx_reset(void) {
    int rc;

    if (pronto() != DRIVER_OK) return pronto();

    /* Invalida o cache do fundo: o conteudo da RAM de tiles pode ser legado. */
    memset(bg_ok, 0, sizeof bg_ok);

    /* Limpa todos os IDs, inclusive os que o cache local desconhece. */
    for (int i = 0; i < 64; i++) {
        rc = delspr(i);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        memset(&sp[i], 0, sizeof sp[i]);
        flip_want[i] = 0;
        flip_hw[i] = 4; /* 4 = estado de espelhamento desconhecido no hardware */
    }

    /* Desativa todos os 8 poligonos na FPGA e limpa o cache local */
    for (int i = 0; i < 8; i++) {
        rc = delpol(i);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        memset(&pol[i], 0, sizeof pol[i]);
    }

    /* Habilita exibicao do tilemap, zera deslocamentos de scroll e reseta prioridades do compositor */
    rc = delbg(1);
    if (rc != DRIVER_OK) return registrar_resultado(rc);
    rc = movbgx(0);
    if (rc != DRIVER_OK) return registrar_resultado(rc);
    rc = movbgy(0);
    if (rc != DRIVER_OK) return registrar_resultado(rc);
    rc = pricomp(0, 0, 0);
    if (rc != DRIVER_OK) return registrar_resultado(rc);

    return DRIVER_OK;
}

/* Transfere um lote de 'n' matrizes de pixels 16x16 da tabela 't' para a memoria de padroes da FPGA. */
int gfx_carregar(const CargaPadrao *t, int n) {
    if (pronto() != DRIVER_OK) return pronto();
    if (n < 0 || (n > 0 && t == NULL))
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);

    for (int i = 0; i < n; i++) {
        int rc = carregar_padrao(t[i].padrao, t[i].pixels);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
    }
    return DRIVER_OK;
}

/* Atualiza a tela inteira de background (1200 tiles), enviando apenas os blocos que mudaram. */
int gfx_fundo(const unsigned char *m) {
    if (pronto() != DRIVER_OK) return pronto();
    if (m == NULL) return registrar_resultado(DRIVER_ERRO_PARAMETRO);

    for (int i = 0; i < MAP_COLS * MAP_ROWS; i++) {
        if (!bg_ok[i] || bg[i] != m[i]) {
            int rc = pdrbg(i, m[i]);
            if (rc != DRIVER_OK) return registrar_resultado(rc);
            bg[i] = m[i];
            bg_ok[i] = 1;
        }
    }
    return DRIVER_OK;
}

/* Atualiza uma sub-regiao retangular (w x h) do tilemap a partir de (col, row) usando cache diferencial. */
int gfx_fundo_regiao(int col, int row, int w, int h,
                     const unsigned char *ids) {
    if (pronto() != DRIVER_OK) return pronto();
    if (col < 0 || row < 0 || w < 0 || h < 0 ||
        col > MAP_COLS || row > MAP_ROWS ||
        w > MAP_COLS - col || h > MAP_ROWS - row ||
        (w > 0 && h > 0 && ids == NULL))
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);

    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            int a = (row + r) * MAP_COLS + col + c;
            unsigned char tile = ids[r * w + c];
            if (!bg_ok[a] || bg[a] != tile) {
                int rc = pdrbg(a, tile);
                if (rc != DRIVER_OK) return registrar_resultado(rc);
                bg[a] = tile;
                bg_ok[a] = 1;
            }
        }
    }
    return DRIVER_OK;
}

/* Agenda o espelhamento horizontal (h) e/ou vertical (v) do sprite 'id' para o proximo gfx_sprite(). */
int gfx_inverter(int id, int h, int v) {
    if (id < 0 || id >= 64) return registrar_resultado(DRIVER_ERRO_PARAMETRO);
    /* So registra o desejado; o envio acontece em gfx_sprite(). */
    flip_want[id] = (unsigned char)((h ? 1 : 0) | (v ? 2 : 0));
    return DRIVER_OK;
}

/* Desativa o sprite 'id' no hardware (caso esteja visivel) e limpa sua entrada no cache. */
int gfx_esconder(int id) {
    int rc;
    if (pronto() != DRIVER_OK) return pronto();
    if (id < 0 || id >= 64)
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);

    if (!sp[id].on) return DRIVER_OK;

    rc = delspr(id);
    if (rc != DRIVER_OK) return registrar_resultado(rc);
    memset(&sp[id], 0, sizeof sp[id]);
    return DRIVER_OK;
}

/* Sincroniza um sprite (insercao, padrao, prioridade, posicao e espelhamento) enviando so o que mudou. */
int gfx_sprite(int id, int pat, int x, int y, int prio, int vis) {
    int rc;
    SpSh *s;

    if (pronto() != DRIVER_OK) return pronto();
    if (id < 0 || id >= 64)
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);

    /* Coordenadas fora da faixa do hardware significam sprite invisivel,
     * preservando a convencao usada pelo codigo original. */
    if (x < 0 || x > 511 || y < 0 || y > 255) vis = 0;
    if (!vis) return gfx_esconder(id);

    if (pat < 0 || pat >= 64)
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);
    if (prio < 0) prio = 0;
    else if (prio > 63) prio = 63;

    s = &sp[id];
    int recem_criado = 0;
    if (!s->on) {
        recem_criado = 1;
        rc = insspr(id, pat);
        if (rc != DRIVER_OK) return registrar_resultado(rc);

        /* INSSPR ja aceitou o padrao; os outros campos ainda nao foram enviados. */
        s->on = 1;
        s->pat = (unsigned char)pat;
        s->pat_ok = 1;
        s->prio_ok = 0;
        s->pos_ok = 0;
    }

    /* Atualiza o padrao grafico se ainda nao sincronizado ou se foi alterado */
    if (!s->pat_ok || s->pat != pat) {
        rc = pdrspr(id, pat);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        s->pat = (unsigned char)pat;
        s->pat_ok = 1;
    }

    /* Atualiza a prioridade de camada (depth sorting) se necessario */
    if (!s->prio_ok || s->prio != prio) {
        rc = prispr(id, prio);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        s->prio = (unsigned char)prio;
        s->prio_ok = 1;
    }

    /* Atualiza as coordenadas (x, y) na tela se mudaram */
    if (!s->pos_ok || s->x != x || s->y != y) {
        rc = movspr(id, x, y);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        s->x = (short)x;
        s->y = (short)y;
        s->pos_ok = 1;
    }

    /* Espelhamento. Ao recriar o sprite o estado do hardware e incerto, entao
     * reenvia se houver (ou tiver havido) espelho; ja existente, so se mudou. */
    {
        unsigned char want = flip_want[id];
        int enviar = recem_criado ? (want != 0 || flip_hw[id] != 0)
                                  : (flip_hw[id] != want);
        if (enviar) {
            rc = invspr(id, want & 1, (want >> 1) & 1);
            if (rc != DRIVER_OK) return registrar_resultado(rc);
            flip_hw[id] = want;
        }
    }

    return DRIVER_OK;
}

/* Varre o pool de entidades (0..46) e sincroniza seus sprites (calculando prioridade Y-sorting). */
int gfx_entidades(void) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity *e = &entities[i];
        int rc;
        if (!(e->active && e->type == TYPE_ENEMY))
            flip_want[i] = 0;          /* slot reaproveitado: nao herda espelho de inimigo */
        if (e->active && e->visible)
            rc = gfx_sprite(i, e->pattern, e->x, e->y,
                            (e->y + 16) >> 2, 1); /* Y-sorting: entidades mais abaixo sobrepoem as de cima */
        else
            rc = gfx_esconder(i);
        if (rc != DRIVER_OK) return rc;
    }
    return DRIVER_OK;
}

/* Sincroniza um poligono de hardware (0..7) garantindo a ordem TAMPOL -> INSPOL exigida pela FPGA. */
int gfx_poligono(int id, int forma, int cor,
                 int x, int y, int w, int h) {
    int rc;
    PolSh *p;

    if (pronto() != DRIVER_OK) return pronto();
    if (id < 0 || id >= 8 || forma < 0 || forma > 4 || cor < 0 || cor > 255)
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);

    /* Dimensoes nulas ou coordenadas fora da faixa desativam o poligono */
    if (w <= 0 || h <= 0 || w > 511 || h > 255 ||
        x < 0 || x > 511 || y < 0 || y > 255)
        return gfx_poligono_off(id);

    p = &pol[id];

    /* A ordem exigida pelo hardware e TAMPOL antes de INSPOL. */
    if (!p->on || !p->forma_ok || p->forma != forma) {
        rc = tampol(id, w, h);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        rc = inspol(id, forma, cor);
        if (rc != DRIVER_OK) return registrar_resultado(rc);

        p->on = 1;
        p->forma = (unsigned char)forma;
        p->cor = (unsigned char)cor;
        p->w = (short)w;
        p->h = (short)h;
        p->forma_ok = 1;
        p->cor_ok = 1;
        p->tamanho_ok = 1;
        p->pos_ok = 0; /* forca MOVPOL apos criar/trocar a forma */
    }

    /* Atualiza a cor se mudou desde o ultimo envio */
    if (!p->cor_ok || p->cor != cor) {
        rc = colpol(id, cor);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        p->cor = (unsigned char)cor;
        p->cor_ok = 1;
    }

    /* Atualiza largura e altura se mudaram */
    if (!p->tamanho_ok || p->w != w || p->h != h) {
        rc = tampol(id, w, h);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        p->w = (short)w;
        p->h = (short)h;
        p->tamanho_ok = 1;
    }

    /* Atualiza posicao (x, y) se mudou ou se o poligono acabou de ser (re)criado */
    if (!p->pos_ok || p->x != x || p->y != y) {
        rc = movpol(id, x, y);
        if (rc != DRIVER_OK) return registrar_resultado(rc);
        p->x = (short)x;
        p->y = (short)y;
        p->pos_ok = 1;
    }

    return DRIVER_OK;
}

/* Desativa o poligono 'id' (0..7) na FPGA e limpa seu estado no cache local. */
int gfx_poligono_off(int id) {
    int rc;
    if (pronto() != DRIVER_OK) return pronto();
    if (id < 0 || id >= 8)
        return registrar_resultado(DRIVER_ERRO_PARAMETRO);
    if (!pol[id].on) return DRIVER_OK;

    rc = delpol(id);
    if (rc != DRIVER_OK) return registrar_resultado(rc);
    memset(&pol[id], 0, sizeof pol[id]);
    return DRIVER_OK;
}