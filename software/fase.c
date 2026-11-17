// fase.c - dados de cada fase: ondas, inimigos por onda, obstaculos e fundo
#include "config.h"
#include "sprites.h"
#include "menu_mapas.h"
#include "mapas.h"
#include "entity.h"
#include "collision.h"
#include "mat.h"
#include "jogador.h"
#include "fase.h"

// Obstaculos (16x16, solidos): {x, y, tipo} com tipo 0 = pedra, 1 = mandacaru.
// Posicoes de ANW_mapas_40x30_v4 (obstaculos_sprites_*.csv). Centro livre para o jogador.
// manha: cactos nas bordas, pedras em pares
static const ObstCfg OBST_MANHA[] = {
    {  32,   8, 1 },
    { 152,   8, 1 },
    { 272,   8, 1 },
    {  32, 216, 1 },
    { 152, 216, 1 },
    { 272, 216, 1 },
    {   8, 104, 1 },
    { 296, 104, 1 },
    {  64,  64, 0 },
    {  80,  64, 0 },
    { 224, 160, 0 },
    { 240, 160, 0 },
};
// tarde: mistura de cactos e pedras espalhadas
static const ObstCfg OBST_TARDE[] = {
    {  40,   8, 1 },
    { 160,   8, 1 },
    { 264, 216, 1 },
    {  40, 216, 1 },
    {   8,  72, 1 },
    { 296, 152, 1 },
    {  88,  64, 0 },
    { 104,  64, 0 },
    { 208, 160, 0 },
    { 224, 160, 0 },
    {  72, 176, 0 },
    { 240,  56, 0 },
};
// noite: pedras nas bordas, cactos em pares (inverso da manha)
static const ObstCfg OBST_NOITE[] = {
    {  32,   8, 0 },
    { 152,   8, 0 },
    { 272,   8, 0 },
    {  32, 216, 0 },
    { 152, 216, 0 },
    { 272, 216, 0 },
    {   8, 104, 0 },
    { 296, 104, 0 },
    {  72,  64, 1 },
    {  88,  64, 1 },
    { 216, 160, 1 },
    { 232, 160, 1 },
};
// numero de elementos de um vetor
#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

// ordem dos campos: nome, mapa, mapa da transicao, carga de sprites, n_carga,
// n_ondas, tem_boss, elite, pool por onda, n_pool por onda, obstaculos, n_obst
static const FaseCfg FASES[NUM_FASES_CFG] = {
    // ---- 1: MANHA - 3 ondas. Onda 1 so calango; depois marimbondo e cacto ----
    {
        "MANHA", mapas_fase[0], MAPA_TRANSICAO_MANHA, CARGA_MANHA, &N_CARGA_MANHA,
        3, 0, INI_NENHUM,
        { { INI_CALANGO },
          { INI_CALANGO, INI_CALANGO, INI_MARIMBONDO },
          { INI_CALANGO, INI_MARIMBONDO, INI_CACTO } },
        { 1, 3, 3 },
        OBST_MANHA, N(OBST_MANHA)
    },
    // ---- 2: TARDE - 3 ondas. Cascavel sozinha, depois bandos de urubu ----
    {
        "TARDE", mapas_fase[1], MAPA_TRANSICAO_TARDE, CARGA_TARDE, &N_CARGA_TARDE,
        3, 0, INI_NENHUM,
        { { INI_CASCAVEL },
          { INI_CASCAVEL, INI_URUBU },
          { INI_CASCAVEL, INI_URUBU, INI_URUBU } },
        { 1, 2, 3 },
        OBST_TARDE, N(OBST_TARDE)
    },
    // ---- 3: NOITE - 2 ondas e o chefe. Lobisomem (elite) a partir da onda 2 ----
    {
        "NOITE", mapas_fase[2], MAPA_TRANSICAO_NOITE, CARGA_NOITE, &N_CARGA_NOITE,
        2, 1, INI_LOBISOMEN,
        { { INI_MORCEGO, INI_ESCORPIAO },
          { INI_MORCEGO, INI_ESCORPIAO, INI_CORPO_SECO, INI_CORPO_SECO } },
        { 2, 4, 0 },
        OBST_NOITE, N(OBST_NOITE)
    },
};

// devolve a configuracao da fase, limitando o indice a 0..NUM_FASES_CFG-1
const FaseCfg *fase_cfg(int fase) {
    if (fase < 0) fase = 0;
    if (fase >= NUM_FASES_CFG) fase = NUM_FASES_CFG - 1;
    return &FASES[fase];
}

// cria uma entidade solida (NPC) para cada obstaculo da fase
void fase_montar_obstaculos(const FaseCfg *f) {
    for (int i = 0; i < f->n_obst; i++) {
        const ObstCfg *o = &f->obst[i];
        Entity *e = entity_spawn(TYPE_NPC, o->tipo ? "mandacaru" : "pedra", o->x, o->y);
        if (!e) continue;
        e->solid = 1;
        e->width = e->height = 16;
        e->solidArea = rectangle_new(2, 2, 12, 12);   // hitbox menor que o sprite (margem de 2 px)
        e->pattern = o->tipo ? PAT_MANDACARU : PAT_PEDRA;
    }
}

// 1 se o retangulo (x,y,w,h) nao toca nenhum obstaculo (entidade NPC ativa)
int fase_area_livre(int x, int y, int w, int h) {
    Rectangle r = rectangle_new(x, y, w, h);
    for (int i = 0; i < MAX_ENTITIES; i++) {
        const Entity *e = &entities[i];
        if (e->active && e->type == TYPE_NPC && collision_intersects(r, entity_get_area(e))) return 0;
    }
    return 1;
}

// sorteia um ponto sem obstaculo e a pelo menos dist_min do jogador (para itens garantidos)
// devolve 1 e preenche x,y; 0 se nao achou em 40 tentativas
int fase_ponto_livre(int *x, int *y, int dist_min) {
    for (int t = 0; t < 40; t++) {
        int px = rng_faixa(8, SCREEN_WIDTH - 24), py = rng_faixa(24, SCREEN_HEIGHT - 24);
        int dx = px + 8 - jogador_cx(), dy = py + 8 - jogador_cy();
        if (isqrt_i(dx * dx + dy * dy) < dist_min) continue;
        if (fase_area_livre(px, py, 16, 16)) { *x = px; *y = py; return 1; }
    }
    return 0;
}
