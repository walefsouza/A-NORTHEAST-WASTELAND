// inimigos.c - os dados de cada inimigo e uma função de comportamento por tipo
#include <string.h>
#include "config.h"
#include "sprites.h"
#include "entity.h"
#include "collision.h"
#include "mat.h"
#include "jogo.h"
#include "jogador.h"
#include "projeteis.h"
#include "powerups.h"
#include "gfx.h"
#include "inimigos.h"

// O inimigo sempre olha para o jogador, não para onde anda. Como o hardware só espelha (H e V) e não
// gira o sprite, cada tipo resolve de um jeito:
// - calango: a arte é vista de cima, com a cabeça para cima. Olhando para cima fica normal, para baixo
// espelha em V, para os lados usa o padrão girado (PAT_MANHA_CALANGO_LADO) e, para a direita,
// espelha esse padrão em H.
// - escorpião e cascavel: arte de lado, virada para a esquerda. Jogador à direita = espelho H.
// - os outros têm arte simétrica e não espelham.
enum { FACE_CIMA, FACE_BAIXO, FACE_ESQ, FACE_DIR };
#define ZONA_MORTA   6   // px: se o jogador está mais perto que isso, conta como em cima e o inimigo não vira
#define HISTERESE    8   // px de folga: o outro eixo só vence se passar o atual por essa margem, senão o calango treme

// velocidades do urubu em q4 (16 = 1 px por quadro). Dentro do bando alternam lento e rápido
#define URUBU_VEL_LENTO  26
#define URUBU_VEL_RAPIDO 46
#define URUBU_ESPACO     24    // px entre um urubu e o próximo da fileira

// ficha de cada tipo: nome, vida, velocidade em q4, pontos ao morrer, primeiro padrão, quantos frames
// de animação tem e se voa (quem voa bate asa mesmo parado)
typedef struct {
    const char *nome;
    short vida, vel_q4, pontos, pat;
    unsigned char frames, voa;
} Def;

// vel_q4: 16 = 1 px por quadro. Para comparar, o jogador anda 2 px (32) e com a rapadura 3 px (48)
static const Def DEF[INI_N] = {
    [INI_CALANGO]    = { "calango",    1, 14,  10, PAT_MANHA_CALANGO,       1, 0 },
    [INI_MARIMBONDO] = { "marimbondo", 1, 30,  20, PAT_MANHA_MARIMBONDO_0,  2, 1 },
    [INI_CACTO]      = { "cacto",      2,  8,  30, PAT_MANHA_CACTO,         1, 0 },
    [INI_CASCAVEL]   = { "cascavel",   2, 16,  25, PAT_TARDE_CASCAVEL_0,    2, 0 },
    [INI_URUBU]      = { "urubu",      1, 36,  15, PAT_TARDE_URUBU_A_0,     2, 1 },
    [INI_MORCEGO]    = { "morcego",    1, 32,  15, PAT_NOITE_MORCEGO_0,     2, 1 },
    [INI_CORPO_SECO] = { "corpo_seco", 4, 10,  50, PAT_NOITE_CORPO_SECO_0,  1, 0 },
    [INI_ESCORPIAO]  = { "escorpiao",  2, 20,  30, PAT_NOITE_ESCORPIAO_0,   1, 0 },
    [INI_LOBISOMEN]  = { "lobisomen",  5, 28, 100, PAT_NOITE_LOBISOMEN_0,   1, 0 },
    [INI_BOSS]       = { "chefe",     55, 26, 500, PAT_NOITE_BOSS_0,        1, 0 },
};

// estado de cada inimigo, no mesmo índice do slot no pool. tipo diz qual é, est é a etapa do
// comportamento (aproximando, avisando, atacando) e var é uma variação (a fileira do urubu, a fúria
// do chefe)
typedef struct {
    unsigned char tipo, var, est;
    unsigned char fh, fv;  // espelho horizontal e vertical pedidos ao sprite
    unsigned char face;    // lado para onde olha agora (só o calango usa os quatro)
    short vida;
    int fx, fy;            // posição em q4, mais fina que o pixel
    int vx, vy;            // velocidade guardada para quem anda reto, em q4
    int t;                 // contador da etapa atual
    int tick;              // quadros desde que nasceu de verdade
    int aviso;             // quadros que faltam de pisca antes de valer
} Ini;
static Ini ini[MAX_ENTITIES];

// limites da tela para um sprite 16x16, já em q4
#define LIM_X ((SCREEN_WIDTH  - 16) << 4)
#define LIM_Y ((SCREEN_HEIGHT - 16) << 4)

void inimigos_reset(void) { memset(ini, 0, sizeof ini); }

// remove o inimigo e antes desfaz o espelho. Senão quem ocupar esse slot depois herda o sprite virado
static void despawn(Entity *e) {
    int i = entity_get_index(e);
    if (i >= 0) {
        gfx_inverter(i, 0, 0);
        ini[i].fh = ini[i].fv = 0;
    }
    entity_despawn(e);
}

// funções de apoio

// vetor do centro do inimigo até o centro do jogador
static void alvo(const Entity *e, int *dx, int *dy) {
    *dx = jogador_cx() - (e->x + 8);
    *dy = jogador_cy() - (e->y + 8);
}

// 1 se a hitbox do inimigo, colocada em (x, y), bate em pedra ou cacto
static int bate_obstaculo(int x, int y, const Entity *e) {
    Rectangle r = rectangle_new(x + e->solidArea.x, y + e->solidArea.y, e->solidArea.width, e->solidArea.height);
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity *o = &entities[i];
        if (o->active && o->type == TYPE_NPC && collision_intersects(r, entity_get_area(o))) return 1;
    }
    return 0;
}

// anda (vx, vy) testando um eixo de cada vez, assim o inimigo desliza na lateral do obstáculo em vez de
// travar. terreno = 1 respeita pedra e cacto, preso = 1 não deixa sair da tela
static void mover(Entity *e, Ini *n, int vx, int vy, int terreno, int preso) {
    int nx = n->fx + vx;
    if (!terreno || !bate_obstaculo(nx >> 4, n->fy >> 4, e)) n->fx = nx;
    int ny = n->fy + vy;
    if (!terreno || !bate_obstaculo(n->fx >> 4, ny >> 4, e)) n->fy = ny;
    if (preso) {
        if (n->fx < 0) n->fx = 0; else if (n->fx > LIM_X) n->fx = LIM_X;
        if (n->fy < 0) n->fy = 0; else if (n->fy > LIM_Y) n->fy = LIM_Y;
    }
}

// 1 se já saiu bem para fora da tela
static int fora_da_tela(const Ini *n) {
    int x = n->fx >> 4, y = n->fy >> 4;
    return x < -24 || x > SCREEN_WIDTH + 8 || y < -24 || y > SCREEN_HEIGHT + 8;
}

// solta até 6 espinhos: os 4 pontos cardeais e 2 diagonais viradas para o lado do jogador. O limite de
// tiros de inimigo também é 6, então todos cabem
static void radial(const Entity *e, int vel) {
    int dx, dy; alvo(e, &dx, &dy);
    int sx = (dx >= 0) ? 1 : -1;
    int sy = (dy >= 0) ? 1 : -1;
    const int d[6][2] = {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { sx, sy }, { sx, -sy }
    };
    for (int k = 0; k < 6; k++) {
        int vx, vy; vetor_q4(d[k][0], d[k][1], vel, &vx, &vy);
        proj_disparar(DONO_INIMIGO, e->x + 8, e->y + 8, vx, vy);
    }
}

// comportamentos, um por tipo

// vai direto no jogador, respeitando os obstáculos
static void ia_perseguir(Entity *e, Ini *n, const Def *d) {
    int dx, dy, vx, vy; alvo(e, &dx, &dy);
    vetor_q4(dx, dy, d->vel_q4, &vx, &vy);
    mover(e, n, vx, vy, 1, 1);
}

// marimbondo: se aproxima, para a uns 80 px piscando por 0,5 s e depois dispara o corpo em linha reta
// contra onde o jogador estava. Etapas: 0 aproxima, 1 pisca, 2 investe
static void ia_marimbondo(Entity *e, Ini *n, const Def *d) {
    int dx, dy, vx, vy; alvo(e, &dx, &dy);
    int dist = isqrt_i(dx * dx + dy * dy);
    switch (n->est) {
    case 0:
        vetor_q4(dx, dy, d->vel_q4, &vx, &vy);
        mover(e, n, vx, vy, 0, 1);
        if (dist < 80) { n->est = 1; n->t = 30; }
        break;
    case 1:
        e->visible = ((n->t / 3) & 1) ? 0 : 1;
        if (--n->t <= 0) {
            vetor_q4(dx, dy, 56, &n->vx, &n->vy);
            e->visible = 1; n->est = 2; n->t = 0;
        }
        break;
    default:
        mover(e, n, n->vx, n->vy, 0, 0);
        if (++n->t > 120 || fora_da_tela(n)) despawn(e);
    }
}

// cacto: lento, para a uns 100 px e atira um espinho a cada 2 s
static void ia_cacto(Entity *e, Ini *n, const Def *d) {
    int dx, dy, vx, vy; alvo(e, &dx, &dy);
    int dist = isqrt_i(dx * dx + dy * dy);
    if (dist > 100) {
        vetor_q4(dx, dy, d->vel_q4, &vx, &vy);
        mover(e, n, vx, vy, 1, 1);
        n->t = 60;                                  // já começa com metade do tempo, o primeiro tiro sai 1 s depois de parar
    } else if (++n->t >= 120) {
        n->t = 0;
        vetor_q4(dx, dy, 36, &vx, &vy);
        proj_disparar(DONO_INIMIGO, e->x + 8, e->y + 8, vx, vy);
    }
}

// cascavel: persegue em zigue-zague. A cada 20 quadros troca o lado do desvio, que é a velocidade
// girada 90° e multiplicada por 12/16
static void ia_cascavel(Entity *e, Ini *n, const Def *d) {
    int dx, dy, vx, vy; alvo(e, &dx, &dy);
    vetor_q4(dx, dy, d->vel_q4, &vx, &vy);
    int k = ((n->tick / 20) & 1) ? 12 : -12;
    mover(e, n, vx - (vy * k) / 16, vy + (vx * k) / 16, 1, 1);
}

// urubu: as fileiras a e b cruzam a tela de lado a lado com um balanço leve na vertical. As c e d
// entram por cima ou por baixo, em diagonal. A velocidade já vem pronta em vx e vy
static void ia_urubu(Entity *e, Ini *n) {
    int vy = n->vy;
    if (n->var < 2) vy += ((n->tick / 12) & 1) ? 5 : -5;   // só a e b balançam
    mover(e, n, n->vx, vy, 0, 0);
    int x = n->fx >> 4, y = n->fy >> 4;
    if (x < -20 || x > SCREEN_WIDTH || y < -20 || y > SCREEN_HEIGHT) despawn(e);
}

// morcego: voo irregular. A cada 24 quadros escolhe um rumo novo na direção do jogador, mas com um
// sorteio de erro na direção e na velocidade
static void ia_morcego(Entity *e, Ini *n, const Def *d) {
    if (n->tick % 24 == 1) {
        int dx, dy; alvo(e, &dx, &dy);
        vetor_q4(dx + rng_faixa(-70, 70), dy + rng_faixa(-70, 70), d->vel_q4 + rng_faixa(-6, 6), &n->vx, &n->vy);
    }
    mover(e, n, n->vx, n->vy, 0, 1);
}

// escorpião: anda por um eixo até ficar alinhado com o jogador no outro (6 px de tolerância), pisca
// erguendo a cauda e dá o bote em linha reta. Etapas: 0 anda, 1 pisca, 2 bote
static void ia_escorpiao(Entity *e, Ini *n, const Def *d) {
    int dx, dy; alvo(e, &dx, &dy);
    int ax = abs_i(dx), ay = abs_i(dy);
    switch (n->est) {
    case 0:
        if (ax <= 6 || ay <= 6) {
            n->est = 1; n->t = 24;
            if (ax <= 6) { n->vx = 0; n->vy = sinal(dy) * 56; } else { n->vx = sinal(dx) * 56; n->vy = 0; }
        } else if (ax < ay) mover(e, n, sinal(dx) * d->vel_q4, 0, 1, 1);
        else                mover(e, n, 0, sinal(dy) * d->vel_q4, 1, 1);
        break;
    case 1:
        e->visible = ((n->t / 3) & 1) ? 0 : 1;
        if (--n->t <= 0) { e->visible = 1; n->est = 2; n->t = 22; }
        break;
    default:
        mover(e, n, n->vx, n->vy, 1, 1);
        if (--n->t <= 0) n->est = 0;
    }
}

// chefe da fase 3, o lobisomem. Ele repete este ciclo:
// 1. persegue o jogador e, na metade da perseguição, faz o ataque de garras: um leque de 3 espinhos
// mirado no jogador (sprites PAT_BOSS_ATAQUE_0, 1 e 2)
// 2. ergue as garras (BOSS_AVISO) e investe rasgando (BOSS_INVESTE)
// 3. cospe espinhos para todos os lados e ruge. Se não sobrou ninguém na tela além dele, o uivo
// chama morcegos de apoio
// Quando a vida chega na metade ele enfurece (n->var = 1): fica mais rápido, o leque sai duas vezes
// e a investida é dupla.
#define BOSS_FURIA          1      // 1 liga a fúria, 0 desliga
#define BOSS_FURIA_VEL_EXTRA 8     // q4: a velocidade vai de 26 para 34
#define BOSS_PERSEGUE_N     130    // quadros perseguindo antes de investir, uns 2,1 s
#define BOSS_PERSEGUE_FURIA 96     // o mesmo enfurecido, uns 1,6 s
#define BOSS_RUGIDO_CUSPE   24     // quadros de rugido depois de cuspir
#define BOSS_RUGIDO_FURIA   45     // quadros de rugido na hora que enfurece

// declarada aqui porque o uivo do chefe chama criar antes da definição
static Entity *criar(TipoInimigo t, int x, int y, int aviso);

// etapas do chefe
enum {
    BOSS_PERSEGUE = 0,
    BOSS_AVISO,
    BOSS_INVESTE,
    BOSS_CUSPE,
    BOSS_RUGE,
    BOSS_LEQUE,         // patada com leque de 3 espinhos
    BOSS_INVESTE2       // segundo bote seguido, só enfurecido
};

// 3 espinhos em leque: -20°, 0 e +20° em relação à direção (dx, dy)
static void boss_leque_tiros(const Entity *e, int dx, int dy, int vel) {
    int vx, vy;
    vetor_q4(dx, dy, vel, &vx, &vy);
    proj_disparar(DONO_INIMIGO, e->x + 8, e->y + 8, vx, vy);
    // mesma rotação do tiro do jogador, agora com 20°: cos ≈ 240/256 e sen ≈ 88/256
    int c = 240, s = 88;
    int vx1 = (vx * c - vy * s) / 256, vy1 = (vx * s + vy * c) / 256;
    int vx2 = (vx * c + vy * s) / 256, vy2 = (-vx * s + vy * c) / 256;
    proj_disparar(DONO_INIMIGO, e->x + 8, e->y + 8, vx1, vy1);
    proj_disparar(DONO_INIMIGO, e->x + 8, e->y + 8, vx2, vy2);
}

static void ia_boss(Entity *e, Ini *n, const Def *d) {
    int dx, dy; alvo(e, &dx, &dy);
    switch (n->est) {
    case BOSS_PERSEGUE: {
        int vx, vy;
        if (BOSS_FURIA && !n->var && n->vida <= d->vida / 2) {
            n->var = 1; n->est = BOSS_RUGE; n->t = BOSS_RUGIDO_FURIA;
            break;
        }
        int dur = n->var ? BOSS_PERSEGUE_FURIA : BOSS_PERSEGUE_N;
        vetor_q4(dx, dy, d->vel_q4 + (n->var ? BOSS_FURIA_VEL_EXTRA : 0), &vx, &vy);
        mover(e, n, vx, vy, 1, 1);
        n->t++;
        // na metade da perseguição para e faz o ataque de leque
        if (n->t == dur / 2) {
            n->est = BOSS_LEQUE;
            n->t = 36;
            break;
        }
        if (n->t >= dur) {
            n->est = BOSS_AVISO;
            n->t = 26;
        }
        break; }
    case BOSS_LEQUE: {
        // n->t vai de 36 até 1 e define a pose. De 36 a 25 ele ergue as garras (ATAQUE_0) e dá um passo
        // curto para frente. De 24 a 13 é o golpe esquerdo (ATAQUE_1) e o primeiro leque sai em 24.
        // De 12 a 1 é o golpe direito (ATAQUE_2) e o segundo leque, só enfurecido, sai em 12.
        if (n->t > 24) {
            int vx, vy;
            vetor_q4(dx, dy, 12, &vx, &vy);
            mover(e, n, vx, vy, 1, 1);
        }
        if (n->t == 24) {
            boss_leque_tiros(e, dx, dy, n->var ? 46 : 40);
        } else if (n->t == 12 && n->var) {
            boss_leque_tiros(e, dx, dy, 48);
        }
        if (--n->t <= 0) {
            n->est = BOSS_PERSEGUE;
            // volta para a perseguição já na segunda metade, rumo à investida
            n->t = ((n->var ? BOSS_PERSEGUE_FURIA : BOSS_PERSEGUE_N) / 2) + 1;
        }
        break; }
    case BOSS_AVISO:
        // visível o tempo todo, para o jogador ver as garras erguidas antes da investida
        e->visible = 1;
        if (--n->t <= 0) {
            vetor_q4(dx, dy, n->var ? 72 : 64, &n->vx, &n->vy);
            n->est = BOSS_INVESTE;
            n->t = 32;
        }
        break;
    case BOSS_INVESTE:
        mover(e, n, n->vx, n->vy, 1, 1);
        if (--n->t <= 0) {
            if (n->var) {
                // enfurecido: acerta a mira de novo e dá um segundo bote
                vetor_q4(dx, dy, 74, &n->vx, &n->vy);
                n->est = BOSS_INVESTE2;
                n->t = 24;
            } else {
                n->est = BOSS_CUSPE;
            }
        }
        break;
    case BOSS_INVESTE2:
        mover(e, n, n->vx, n->vy, 1, 1);
        if (--n->t <= 0) n->est = BOSS_CUSPE;
        break;
    case BOSS_CUSPE:
        radial(e, n->var ? 42 : 36);
        // uivo: se o chefe é o único inimigo na tela, chama morcego de apoio no lado mais longe do jogador
        // (dois, se estiver enfurecido)
        if (inimigos_contar() <= 1) {
            int l1 = (jogador_cx() < SCREEN_WIDTH / 2) ? LADO_DIR : LADO_ESQ;
            int x1 = (l1 == LADO_ESQ) ? 0 : SCREEN_WIDTH - 16;
            Entity *m1 = criar(INI_MORCEGO, x1, 40, AVISO_FRAMES);
            if (m1) (void)m1;
            if (n->var) {
                Entity *m2 = criar(INI_MORCEGO, x1, SCREEN_HEIGHT - 48, AVISO_FRAMES);
                if (m2) (void)m2;
            }
        }
        n->est = BOSS_RUGE; n->t = BOSS_RUGIDO_CUSPE;
        break;
    default:                                    // BOSS_RUGE: parado de boca aberta até o tempo acabar
        if (--n->t <= 0) { n->est = BOSS_PERSEGUE; n->t = 0; }
    }
}

// escolhe o sprite do chefe pela etapa. Andando usa um ciclo de 4 passos (0, 1, 2, 1) e parado alterna
// entre dois quadros
static int boss_padrao(const Ini *n, int andou) {
    static const unsigned char ANDAR[4] = { PAT_BOSS_ANDA_0, PAT_BOSS_ANDA_1, PAT_BOSS_ANDA_2, PAT_BOSS_ANDA_1 };
    switch (n->est) {
    case BOSS_AVISO:
        return PAT_BOSS_ATAQUE_0;                                               // garras erguidas
    case BOSS_LEQUE:
        if (n->t > 24) return PAT_BOSS_ATAQUE_0;                                // prepara garras
        if (n->t > 12) return PAT_BOSS_ATAQUE_1;                                // rasgo esquerdo
        return PAT_BOSS_ATAQUE_2;                                               // rasgo direito
    case BOSS_INVESTE:
    case BOSS_INVESTE2:
        return ((n->t / 5) & 1) ? PAT_BOSS_ATAQUE_2 : PAT_BOSS_ATAQUE_1;        // alterna golpes
    case BOSS_CUSPE:
    case BOSS_RUGE:
        return PAT_BOSS_RUGIDO;
    }
    if (n->var)  return ((n->tick / 6) & 1) ? PAT_BOSS_FURIA_1 : PAT_BOSS_FURIA_0; // enfurecido
    if (andou)   return ANDAR[(n->tick / 6) & 3];
    return ((n->tick / 24) & 1) ? PAT_BOSS_IDLE_1 : PAT_NOITE_BOSS_0;
}

// orientação do sprite

// vira o inimigo para o jogador e registra o espelho no gfx, que só manda para o hardware quando algo
// muda. Também roda enquanto ele pisca, assim já nasce olhando para o jogador
static void atualizar_orientacao(Entity *e, Ini *n) {
    int i = entity_get_index(e);
    int dx, dy; alvo(e, &dx, &dy);
    int ax = abs_i(dx), ay = abs_i(dy);
    unsigned char fh = 0, fv = 0;

    switch (n->tipo) {
    case INI_CALANGO: {
        int horiz = (n->face == FACE_ESQ || n->face == FACE_DIR);
        if (ax > ZONA_MORTA || ay > ZONA_MORTA) {          // se está perto demais, mantém o lado em que estava
            if (horiz) { if (ay > ax + HISTERESE) horiz = 0; }
            else       { if (ax > ay + HISTERESE) horiz = 1; }
            if (horiz) n->face = (dx < 0) ? FACE_ESQ : FACE_DIR;
            else       n->face = (dy < 0) ? FACE_CIMA : FACE_BAIXO;
        }
        fh = (n->face == FACE_DIR);
        fv = (n->face == FACE_BAIXO);
        e->pattern = (n->face == FACE_ESQ || n->face == FACE_DIR) ? PAT_MANHA_CALANGO_LADO : PAT_MANHA_CALANGO;
        break; }
    case INI_ESCORPIAO: case INI_CASCAVEL:
        if (ax > ZONA_MORTA) n->face = (dx > 0) ? FACE_DIR : FACE_ESQ;   // só vira se estiver fora da zona morta
        fh = (n->face == FACE_DIR);
        break;
    default:
        break;
    }
    n->fh = fh; n->fv = fv;
    if (i >= 0) gfx_inverter(i, fh, fv);   // só registra o pedido, quem envia é o gfx_sprite() quando mudou
}

// ciclo de vida

static void inimigo_update(Entity *e) {
    int i = entity_get_index(e);
    Ini *n = &ini[i];
    const Def *d = &DEF[n->tipo];

    if (n->aviso > 0) {                         // ainda nascendo: pisca e não machuca nem leva dano
        atualizar_orientacao(e, n);             // já nasce olhando para o jogador
        n->aviso--;
        e->visible = ((n->aviso / 3) & 1) ? 0 : 1;
        if (n->aviso == 0) e->visible = 1;
        return;
    }
    n->tick++;

    int ox = n->fx, oy = n->fy;                 // posição antes da IA, para saber depois se ele andou

    switch (n->tipo) {
    case INI_CALANGO: case INI_CORPO_SECO: case INI_LOBISOMEN: ia_perseguir(e, n, d); break;
    case INI_MARIMBONDO: ia_marimbondo(e, n, d); break;
    case INI_CACTO:      ia_cacto(e, n, d);      break;
    case INI_CASCAVEL:   ia_cascavel(e, n, d);   break;
    case INI_URUBU:      ia_urubu(e, n);         break;
    case INI_MORCEGO:    ia_morcego(e, n, d);    break;
    case INI_ESCORPIAO:  ia_escorpiao(e, n, d);  break;
    case INI_BOSS:       ia_boss(e, n, d);       break;
    }
    if (!e->active) return;                     // a própria IA pode ter removido o inimigo

    int mx = n->fx - ox, my = n->fy - oy;
    int andou = (mx != 0 || my != 0);

    e->x = n->fx >> 4; e->y = n->fy >> 4;

    // quem voa bate asa o tempo todo, os outros só animam andando e parados ficam no frame 0
    if (d->frames > 1 && (andou || d->voa)) entity_animate(e, d->frames, 8);
    else if (!andou && !d->voa) e->spriteNum = 0;
    else e->spriteNum = 0;
    e->pattern = d->pat + (n->tipo == INI_URUBU ? 2 * n->var : 0) + e->spriteNum;
    if (n->tipo == INI_BOSS) e->pattern = boss_padrao(n, andou);

    atualizar_orientacao(e, n);   // tem que vir depois de definir o pattern, porque o calango troca para o padrão girado lá dentro
}

static Entity *criar(TipoInimigo t, int x, int y, int aviso) {
    const Def *d = &DEF[t];
    Entity *e = entity_spawn(TYPE_ENEMY, d->nome, x, y);
    if (!e) return NULL;
    int i = entity_get_index(e);
    e->solid = 0;                               // inimigo machuca, mas não bloqueia o movimento de ninguém
    e->width = e->height = 16;
    e->solidArea = rectangle_new(2, 2, 12, 12);
    e->pattern = d->pat;
    e->update = inimigo_update;
    memset(&ini[i], 0, sizeof ini[i]);
    ini[i].tipo = (unsigned char)t;
    ini[i].vida = d->vida;
    ini[i].fx = x * 16; ini[i].fy = y * 16;
    ini[i].aviso = aviso;
    ini[i].face = FACE_CIMA;
    if (t == INI_ESCORPIAO || t == INI_CASCAVEL) ini[i].face = FACE_ESQ;
    e->visible = 0;                             // começa escondido, quem liga o sprite é o pisca de nascimento
    return e;
}

int inimigo_tamanho_grupo(TipoInimigo t) { return (t == INI_URUBU || t == INI_MORCEGO) ? 4 : 1; }

int inimigo_spawn_em(TipoInimigo t, int x, int y, int lado) {
    int criados = 0;
    if (t == INI_URUBU && (lado == LADO_CIMA || lado == LADO_BAIXO)) {
        // fileira c e d, entrando por cima ou por baixo em diagonal. Alterna c lento e d rápido
        int n = rng_faixa(3, 4);
        int diry = (lado == LADO_CIMA) ? 1 : -1;
        int dirx = (x < SCREEN_WIDTH / 2) ? 1 : -1;        // vai na diagonal para o lado oposto de onde nasceu
        int y0 = (lado == LADO_CIMA) ? 0 : SCREEN_HEIGHT - 16;
        for (int k = 0; k < n; k++) {
            int xx = x + (k - n / 2) * URUBU_ESPACO;
            if (xx < 0) xx = 0;
            if (xx > SCREEN_WIDTH - 16) xx = SCREEN_WIDTH - 16;
            Entity *e = criar(t, xx, y0, AVISO_FRAMES + k * 6);
            if (!e) break;
            int i = entity_get_index(e);
            ini[i].var = (unsigned char)(2 + (k & 1));     // 2 = c, 3 = d
            vetor_q4(dirx, diry, (k & 1) ? URUBU_VEL_RAPIDO : URUBU_VEL_LENTO, &ini[i].vx, &ini[i].vy);
            criados++;
        }
    } else if (t == INI_URUBU) {                 // fileira a e b, nasce na borda lateral e cruza a tela para o outro lado
        int n = rng_faixa(3, 4), dir = (lado == LADO_ESQ) ? 1 : -1;
        int x0 = (dir > 0) ? 0 : SCREEN_WIDTH - 16;
        for (int k = 0; k < n; k++) {
            int yy = y + (k - n / 2) * 24;
            if (yy < 16) yy = 16;
            if (yy > SCREEN_HEIGHT - 32) yy = SCREEN_HEIGHT - 32;
            Entity *e = criar(t, x0, yy, AVISO_FRAMES + k * 6);
            if (!e) break;
            int i = entity_get_index(e);
            ini[i].var = (unsigned char)(k & 1);           // 0 = a (lento), 1 = b (rápido)
            ini[i].vx = dir * ((k & 1) ? URUBU_VEL_RAPIDO : URUBU_VEL_LENTO);
            criados++;
        }
    } else if (t == INI_MORCEGO) {               // bando junto, espalhado ao longo da borda
        int n = rng_faixa(3, 4);
        for (int k = 0; k < n; k++) {
            int xx = x, yy = y;
            if (lado == LADO_CIMA || lado == LADO_BAIXO) xx = x + (k - n / 2) * 14; else yy = y + (k - n / 2) * 14;
            if (xx < 0) xx = 0;
            if (xx > SCREEN_WIDTH - 16) xx = SCREEN_WIDTH - 16;
            if (yy < 0) yy = 0;
            if (yy > SCREEN_HEIGHT - 16) yy = SCREEN_HEIGHT - 16;
            if (!criar(t, xx, yy, AVISO_FRAMES)) break;
            criados++;
        }
    } else if (criar(t, x, y, AVISO_FRAMES)) criados = 1;
    return criados;
}

int inimigos_contar(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) if (entities[i].active && entities[i].type == TYPE_ENEMY) n++;
    return n;
}

// só vira alvo e só machuca depois que o pisca de nascimento acabou
int inimigo_perigoso(const Entity *e) {
    int i = entity_get_index(e);
    return i >= 0 && e->active && e->type == TYPE_ENEMY && ini[i].aviso == 0;
}

// desconta o dano da vida. Se zerou, dá os pontos, tenta derrubar item e remove o inimigo
int inimigo_dano(Entity *e, int dano) {
    int i = entity_get_index(e);
    if (i < 0 || !inimigo_perigoso(e)) return 0;
    ini[i].vida -= dano;
    if (ini[i].vida > 0) return 0;
    jogo_pontuar(DEF[ini[i].tipo].pontos);
    powerups_dropar(e->x + 8, e->y + 8);
    despawn(e);
    return 1;
}

void inimigos_limpar_tela(int pontuar) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &entities[i];
        if (!e->active || e->type != TYPE_ENEMY || ini[i].tipo == INI_BOSS) continue;
        if (pontuar) jogo_pontuar(DEF[ini[i].tipo].pontos);
        despawn(e);
    }
}

int inimigos_boss_vivo(void) {
    for (int i = 0; i < MAX_ENTITIES; i++)
        if (entities[i].active && entities[i].type == TYPE_ENEMY && ini[i].tipo == INI_BOSS) return 1;
    return 0;
}
