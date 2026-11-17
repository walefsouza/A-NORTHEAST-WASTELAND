// jogador.c - controle do cangaceiro: anda em 8 direções, mira com mouse ou setas, atira e toma dano
#include "config.h"
#include "sprites.h"
#include "entity.h"
#include "collision.h"
#include "entrada.h"
#include "mat.h"
#include "jogo.h"
#include "projeteis.h"
#include "powerups.h"
#include "inimigos.h"
#include "jogador.h"

#define VEL_NORMAL        2               // pixels por quadro
#define VEL_RAPADURA      3               // com a rapadura anda 50% mais rápido
#define CADENCIA_NORMAL  14               // quadros de espera entre um tiro e outro
#define CADENCIA_RAPIDA   7               // com a cartucheira, metade da espera
#define INVULN_FRAMES    90               // 1,5 s sem tomar dano depois de ser atingido
#define MORTE_FRAMES     70               // duração da animação de morte

// estado interno do jogador. idx é o slot dele no pool, invuln conta os quadros de invulnerabilidade,
// escudo vale 1 se tem o cantil e cd é a espera até poder atirar de novo. O resto controla a
// animação de morte (t_morte conta quadros, morto_fim vira 1 no final) e se está andando
static struct {
    int idx;
    int invuln, escudo, cd;
    int morrendo, t_morte, morto_fim;
    int movendo;
} J;

void jogador_iniciar(int x, int y) {
    Entity *e = entity_spawn(TYPE_PLAYER, "Cangaceiro", x, y);
    J.idx = e ? entity_get_index(e) : 0;
    // nasce com 1 s de invulnerabilidade e o resto zerado
    J.invuln = 60; J.escudo = 0; J.cd = 0; J.morrendo = 0; J.t_morte = 0; J.morto_fim = 0; J.movendo = 0;
    if (!e) return;
    e->speed = VEL_NORMAL;
    e->width = e->height = 16;
    e->solidArea = rectangle_new(3, 3, 10, 12);       // hitbox contra cenário e obstáculos
    e->pattern = PAT_PLAYER_INITIAL;
}

// atalho para a entidade do jogador no pool
static Entity *eu(void) { return &entities[J.idx]; }

// centro do sprite 16x16
int jogador_cx(void) { return eu()->x + 8; }
int jogador_cy(void) { return eu()->y + 8; }
int jogador_direcao_mov(void) { return J.movendo ? (int)eu()->direction : -1; }
int jogador_morto_fim(void) { return J.morto_fim; }
int jogador_tem_escudo(void) { return J.escudo; }
void jogador_dar_escudo(void) { J.escudo = 1; }
void jogador_curar(void) { if (jogo.vidas < VIDAS_MAX) jogo.vidas++; }

Rectangle jogador_area(void) {                         // hitbox de dano: 8x9 dentro do sprite, tiro que passa raspando não pega
    return rectangle_new(eu()->x + 4, eu()->y + 4, 8, 9);
}

// ordem das defesas: invulnerável ignora tudo, depois o cantil segura o golpe, e só então perde vida
void jogador_dano(void) {
    if (J.invuln > 0 || J.morrendo) return;
    if (J.escudo) { J.escudo = 0; J.invuln = 60; return; }       // o cantil segura esse golpe e some
    if (--jogo.vidas <= 0) { jogo.vidas = 0; J.morrendo = 1; J.t_morte = 0; return; }
    J.invuln = INVULN_FRAMES;
}

// vira para a direção e tenta andar um passo, a colisão decide se anda de fato
static void passo(Entity *p, Direction d) { p->direction = d; collision_try_move(p); }

// lê a direção do tiro. As setas têm prioridade. Sem elas, o botão esquerdo do mouse mira do jogador
// até o cursor. Devolve 0 se não há para onde atirar
static int ler_mira(int *dx, int *dy) {
    *dx = (tecla_apertada(KEY_RIGHT) ? 1 : 0) - (tecla_apertada(KEY_LEFT) ? 1 : 0);
    *dy = (tecla_apertada(KEY_DOWN)  ? 1 : 0) - (tecla_apertada(KEY_UP)   ? 1 : 0);
    if (*dx || *dy) return 1;
    if (mouse_botao(BTN_LEFT)) {
        *dx = mouse_x() - jogador_cx(); *dy = mouse_y() - jogador_cy();
        return *dx || *dy;
    }
    return 0;
}

// graus: 0, +15 ou -15. Não usamos seno e cosseno, a rotação é feita com valores fixos sobre 256
static void tiro(int dx, int dy, int graus) {
    int vx, vy; vetor_q4(dx, dy, VEL_TIRO_Q4, &vx, &vy);
    if (graus) {                                              // cos 15° = 247/256 e sen 15° = 66/256
        int s = graus > 0 ? 66 : -66, c = 247;
        int nx = (vx * c - vy * s) / 256, ny = (vx * s + vy * c) / 256;
        vx = nx; vy = ny;
    }
    proj_disparar(DONO_JOGADOR, jogador_cx(), jogador_cy(), vx, vy);
}

// o que sai depende da arma: bacamarte dá três tiros em leque, estrela dá oito em volta, sem nada é
// um tiro só. No fim reinicia a espera
static void atirar(int dx, int dy) {
    switch (powerups_arma()) {
    case PU_BACAMARTE: tiro(dx, dy, 0); tiro(dx, dy, 15); tiro(dx, dy, -15); break;
    case PU_ESTRELA: {
        static const int d[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
        for (int k = 0; k < 8; k++) tiro(d[k][0], d[k][1], 0);
        break; }
    default: tiro(dx, dy, 0);
    }
    J.cd = powerups_rapido() ? CADENCIA_RAPIDA : CADENCIA_NORMAL;
}

// escolhe o padrão do sprite pela direção e pelo frame da animação. Parado fica sempre no inicial
static void escolher_quadro(Entity *p) {
    if (!J.movendo) { p->pattern = PAT_PLAYER_INITIAL; return; }
    int f = p->spriteNum;
    switch (p->direction) {
    case DIR_UP:    p->pattern = PAT_PLAYER_UP_0 + (f % 3); break;
    case DIR_DOWN:  p->pattern = (f & 1) ? PAT_PLAYER_INITIAL : PAT_PLAYER_DOWN_0; break;
    case DIR_LEFT:  p->pattern = PAT_PLAYER_LEFT_0 + (f & 1); break;
    default:        p->pattern = PAT_PLAYER_RIGHT_0 + (f & 1); break;
    }
}

void jogador_atualizar(void) {
    Entity *p = eu();
    if (J.morrendo) {                                  // morte em três fases: caveira, fumaça e brilho
        J.t_morte++;
        p->pattern = J.t_morte < 30 ? PAT_PLAYER_DEATH_0 : (J.t_morte < 50 ? PAT_PLAYER_DEATH_1 : PAT_PLAYER_DEATH_2);
        if (J.t_morte >= MORTE_FRAMES) J.morto_fim = 1;
        return;
    }
    p->speed = powerups_veloz() ? VEL_RAPADURA : VEL_NORMAL;

    int mv = 0;                                        // na diagonal é um passo em cada eixo. A ganha de D e W ganha de S
    if (tecla_apertada(KEY_A))      { passo(p, DIR_LEFT);  mv = 1; }
    else if (tecla_apertada(KEY_D)) { passo(p, DIR_RIGHT); mv = 1; }
    if (tecla_apertada(KEY_W))      { passo(p, DIR_UP);    mv = 1; }
    else if (tecla_apertada(KEY_S)) { passo(p, DIR_DOWN);  mv = 1; }
    J.movendo = mv;
    if (mv) entity_animate(p, 4, 8); else p->spriteNum = 0;
    escolher_quadro(p);

    if (J.cd > 0) J.cd--;
    int dx, dy;
    if (J.cd == 0 && ler_mira(&dx, &dy)) atirar(dx, dy);

    if (J.invuln > 0) J.invuln--;
    // invulnerável pisca, trocando a cada 4 quadros
    p->visible = (J.invuln == 0) || ((J.invuln / 4) & 1);

    {                                                  // contato com inimigo (só vale depois do pisca de nascimento)
        Rectangle h = jogador_area();
        for (int i = 0; i < MAX_ENTITIES; i++) {
            const Entity *e = &entities[i];
            if (e->active && e->type == TYPE_ENEMY && inimigo_perigoso(e) &&
                collision_intersects(h, entity_get_area(e))) { jogador_dano(); break; }
        }
        // o mandacaru é sólido e também machuca. Como o jogador para colado nele, a hitbox de contato
        // cresce 3 px para cada lado (o passo máximo é 3 px)
        Rectangle ps = rectangle_new(p->x + p->solidArea.x - 3, p->y + p->solidArea.y - 3,
                                     p->solidArea.width + 6, p->solidArea.height + 6);
        for (int i = 0; i < MAX_ENTITIES; i++) {
            const Entity *e = &entities[i];
            if (e->active && e->type == TYPE_NPC && e->pattern == PAT_MANDACARU &&
                collision_intersects(ps, entity_get_area(e))) { jogador_dano(); break; }
        }
    }
}
