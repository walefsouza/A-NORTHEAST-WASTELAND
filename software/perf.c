/* perf.c - medicao de desempenho: updates/s, video do hardware, latencia de entrada,
 * tempo de logica/dreno de comandos, CPU e coerencia de leitura do STATUS.
 *
 * Decomposicao do periodo de um quadro (tudo medido na thread principal):
 *   periodo ~= espera_quadro (polling em esperar_quadro) + logica + dreno (polling em
 *              esperar_fim) + outros (teclas_quadro, checagens, impressao do painel)
 * Duas visoes de CPU:
 *   CPU bruta  = tempo de CPU do processo (getrusage) / tempo real. Fica perto de 100%
 *                por construcao: o driver faz espera ocupada lendo o STATUS por MMIO.
 *   CPU util   = logica / periodo. Limite superior do trabalho real do jogo (a logica
 *                inclui os enviar_inst, que esperam ACK).
 *   espera ocupada = (espera_quadro + dreno) / periodo.
 * A bruta e separada em thread principal (RUSAGE_THREAD) e demais (threads de entrada).
 *
 * Relogio: CLOCK_MONOTONIC via syscall(), assim NAO precisa de -lrt na compilacao
 * (gcc -std=gnu99 -pthread *.c driver.S -o jogo continua valendo). */
#define _GNU_SOURCE             /* habilita RUSAGE_THREAD em sys/resource.h */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include "config.h"
#include "entity.h"
#include "estado.h"
#include "entrada.h"
#include "driver.h"
#include "perf.h"
#include "perf_status.h"

#define INTERVALO_PAINEL_US 500000LL      /* intervalo de atualizacao do painel no terminal (0.5 s = 2 Hz) */
#define N_AMOSTRAS          512           /* tamanho do buffer circular para calculo de percentis (p95/p99) */
#define LIM_UPDATES_MIN     30.0          /* meta minima de taxa de atualizacao do requisito (PBL03: >= 30 upd/s) */
#define LIM_ENTRADA_MS      100.0         /* meta maxima de latencia de entrada do requisito (PBL03: <= 100 ms) */
#define TESTE_STATUS        1             /* 1 = ativa rajada de leituras do registrador STATUS por quadro */
#define N_LINHAS_PAINEL     12            /* total de linhas impressas pelo painel ao vivo (para recuo ANSI) */

/* ------------------------------------------------------------------ tempo */

/* Retorna o tempo atual do relogio monotonico em microssegundos via chamada de sistema direta. */
static long long mono_us(void) {
    struct timespec ts;
    syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

/* Retorna o tempo total de CPU (usuario + sistema) consumido pelo processo inteiro (inclui threads evdev). */
static long long cpu_us(void) {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);            /* inclui as threads de entrada */
    return ((long long)r.ru_utime.tv_sec + r.ru_stime.tv_sec) * 1000000LL + r.ru_utime.tv_usec + r.ru_stime.tv_usec;
}

/* Retorna o tempo de CPU (usuario + sistema) consumido exclusivamente pela thread chamadora (principal). */
static long long cpu_thread_us(void) {  /* so a thread que chama (a principal) */
    struct rusage r;
    getrusage(RUSAGE_THREAD, &r);
    return ((long long)r.ru_utime.tv_sec + r.ru_stime.tv_sec) * 1000000LL + r.ru_utime.tv_usec + r.ru_stime.tv_usec;
}

/* ----------------------------------------------------------------- series */

/* Acumulador estatistico com buffer circular (anel) e metricas de janela/totais */
typedef struct {
    float  v[N_AMOSTRAS];                  /* buffer circular com as ultimas amostras (em ms) */
    int    n, pos;                         /* quantidade de itens validos no anel e indice de escrita */
    double soma_j, max_j; long long n_j;   /* acumuladores da janela atual do painel (0.5 s) */
    double soma_t, max_t; long long n_t;   /* acumuladores globais desde o inicio da execucao */
} Serie;

/* Insere uma nova amostra 'x' no buffer circular e atualiza as somas e maximos da janela e do total. */
static void s_add(Serie *s, double x) {
    s->v[s->pos] = (float)x;
    s->pos = (s->pos + 1) % N_AMOSTRAS;
    if (s->n < N_AMOSTRAS) s->n++;
    s->soma_j += x; s->n_j++; if (x > s->max_j) s->max_j = x;
    s->soma_t += x; s->n_t++; if (x > s->max_t) s->max_t = x;
}

/* Reinicia os acumuladores estatisticos da janela de exibicao atual. */
static void s_nova_janela(Serie *s) { s->soma_j = 0; s->max_j = 0; s->n_j = 0; }

/* Calcula a media aritmetica das amostras colhidas na janela atual. */
static double s_media_j(const Serie *s) { return s->n_j ? s->soma_j / s->n_j : 0.0; }

/* Calcula a media aritmetica global desde o inicio do programa. */
static double s_media_t(const Serie *s) { return s->n_t ? s->soma_t / s->n_t : 0.0; }

/* Funcao de comparacao de floats em ordem crescente para uso no qsort(). */
static int cmp_f(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

/* Calcula o percentil 'p' (ex: 0.95 ou 0.99) ordenando uma copia das amostras presentes no anel. */
static double s_pct(const Serie *s, double p) {          /* percentil sobre o anel */
    float tmp[N_AMOSTRAS];
    if (s->n == 0) return 0.0;
    memcpy(tmp, s->v, sizeof(float) * (size_t)s->n);
    qsort(tmp, (size_t)s->n, sizeof(float), cmp_f);
    int k = (int)(p * (s->n - 1) + 0.5);
    return tmp[k];
}

/* ------------------------------------------------------------------ estado */

/* Categorias de cena monitoradas separadamente para o relatorio final */
enum { CENA_MENU = 0, CENA_PARTIDA, CENA_OUTRAS, N_CENAS };
static const char *NOME_CENA[N_CENAS] = { "MENU", "PARTIDA", "OUTRAS (pausa/trans/fim)" };

/* Metricas acumuladas por categoria de cena para impressao no resumo de encerramento */
typedef struct {                           /* acumulado por cena, para o resumo final */
    long long quadros;                     /* total de quadros processados nesta cena */
    double soma_periodo, max_periodo;      /* soma e pico do tempo entre quadros (ms) */
    double soma_ocup, max_ocup;            /* soma e pico do tempo de trabalho (logica + dreno, em ms) */
    double soma_logica, soma_dreno, soma_espera;   /* somas parciais (ms) para calculo de CPU util e espera ocupada */
    int    max_entidades;                  /* pico de entidades simultaneamente ativas nesta cena */
} Cena;
static Cena cena_t[N_CENAS];

/* Series temporais das diferentes etapas do quadro e latencia de input */
static Serie s_periodo, s_logica, s_dreno, s_ocup, s_entrada, s_espera;

static int       ativo, painel_ligado;                   /* flags de ativacao do modulo e da impressao ANSI */
static long long t_inicio, t_ult_painel, t_prev_quadro, t_q0, t_l0, t_l1; /* marcos temporais em microssegundos */
static long long cpu_ult, wall_ult;                      /* marcos anteriores para calculo de % de CPU da janela */
static long long cpu_ini, cpu_main_ini, cpu_main_ult;    /* marcos de CPU total e exclusiva da thread principal */
static long long t_w0;                                   /* instante imediatamente anterior a esperar_quadro() */
static double    espera_ult;                             /* duracao (ms) gasta aguardando o quadro atual */
static double    cpu_main_pct_j, cpu_outras_pct_j;       /* % de CPU da thread principal e das threads evdev na janela */
static int       hw_prev = -1;                           /* ultimo valor lido do contador de quadros de 16 bits da FPGA */
static long long hw_quadros_j, hw_perdidos_j, hw_perdidos_t, quadros_j; /* contadores de quadros de video e drops */
static long long ev_pendente_us;                         /* timestamp do input capturado antes da logica do quadro */
static double    cpu_pico;                               /* maior taxa de uso de CPU bruta registrada */
static unsigned  done_prev; static int done_ok;          /* historico do contador circular done_cnt (7 bits) da FPGA */
static long long cmds_j;                                 /* total de comandos concluidos pela FPGA na janela atual */
static long long st_leituras, st_incoerentes;            /* total de leituras de teste do STATUS e falhas de CDC */
static FILE     *csv;                                    /* ponteiro para o arquivo de log CSV (se ANW_PERF_LOG definido) */
static int       painel_desenhado;                       /* indica se o painel ja foi impresso ao menos uma vez */

/* Identifica a qual categoria de telemetria pertence o estado atual da maquina de estados. */
static int cena_atual(void) {
    const Estado *e = estado_atual();
    if (e == &estado_menu) return CENA_MENU;
    if (e == &estado_partida) return CENA_PARTIDA;
    return CENA_OUTRAS;
}

/* Conta quantas posicoes do pool global de entidades estao marcadas como ativas no momento. */
static int entidades_ativas(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) if (entities[i].active) n++;
    return n;
}

/* Inicializa os acumuladores de telemetria, le variaveis de ambiente e abre o arquivo CSV opcional. */
void perf_iniciar(void) {
    const char *p = getenv("ANW_PERF");
    const char *log = getenv("ANW_PERF_LOG");
    painel_ligado = !(p && p[0] == '0');
    memset(&s_periodo, 0, sizeof s_periodo); memset(&s_logica, 0, sizeof s_logica);
    memset(&s_dreno, 0, sizeof s_dreno);     memset(&s_ocup, 0, sizeof s_ocup);
    memset(&s_entrada, 0, sizeof s_entrada); memset(&s_espera, 0, sizeof s_espera);
    t_inicio = t_ult_painel = mono_us();
    wall_ult = t_inicio; cpu_ult = cpu_ini = cpu_us();
    cpu_main_ult = cpu_main_ini = cpu_thread_us();
    if (log && log[0]) {
        csv = fopen(log, "a");
        if (csv && ftell(csv) == 0)
            fputs("t_s,cena,updates_s,video_fps,perdidos,logica_ms_med,logica_ms_max,dreno_ms_med,"
                  "dreno_ms_max,ocup_pct,entrada_ms_med,entrada_ms_max,cpu_pct,cmds_quadro,"
                  "status_leituras,status_incoerentes,entidades,espera_ms_med,espera_ms_max,outros_ms_med,"
                  "cpu_util_pct,espera_ocupada_pct,cpu_main_pct,cpu_outras_pct\n", csv);
    }
    ativo = 1;
}

/* ------------------------------------------------------- por quadro */

/* Marca o instante imediatamente anterior ao bloqueio/polling em esperar_quadro(). */
void perf_antes_espera_quadro(void) {
    if (!ativo) return;
    t_w0 = mono_us();
}

/* Registra o inicio de um novo quadro logico, contabiliza o tempo de espera e detecta perda de quadros de video. */
void perf_inicio_quadro(int quadro_hw) {
    if (!ativo) return;
    long long agora = mono_us();
    t_q0 = agora;
    if (t_w0) {                                          /* tempo preso em esperar_quadro() */
        espera_ult = (agora - t_w0) / 1000.0;
        s_add(&s_espera, espera_ult);
        t_w0 = 0;
    } else {
        espera_ult = 0.0;                                /* hook nao chamado: nao inventa dado */
    }
    if (t_prev_quadro) s_add(&s_periodo, (agora - t_prev_quadro) / 1000.0);
    t_prev_quadro = agora;

    if (hw_prev >= 0) {                                  /* contador de 16 bits do hardware */
        int d = (quadro_hw - hw_prev) & 0xFFFF;
        hw_quadros_j += d;
        if (d > 1) { hw_perdidos_j += d - 1; hw_perdidos_t += d - 1; }   /* video andou, logica nao */
    }
    hw_prev = quadro_hw;
}

/* Captura o timestamp do evento de entrada mais antigo pendente e marca o inicio da logica do jogo. */
void perf_antes_logica(void) {
    if (!ativo) return;
    ev_pendente_us = entrada_pegar_evento_us();          /* evento mais antigo ainda nao tratado */
    t_l0 = mono_us();
}

/* Marca o instante em que a logica e o envio de comandos terminaram (antes de esperar_fim). */
void perf_depois_logica(void) {
    if (!ativo) return;
    t_l1 = mono_us();
}

/* Rajada de leituras: o contador de quadros so pode ficar igual ou subir 1 dentro de
 * poucos microssegundos. Qualquer outra coisa = leitura rasgada (CDC 25 -> 50 MHz). */
static void testar_status(void) {
#if TESTE_STATUS
    unsigned anterior = ler_status();
    for (int i = 0; i < 32; i++) {
        unsigned atual = ler_status();
        st_leituras++;
        if (perf_status_amostra_incoerente(anterior, atual))
            st_incoerentes++;
        anterior = atual;
    }
#endif
}

/* Prototipo da funcao interna de renderizacao do painel e gravacao de linha CSV */
static void painel(long long agora, double upd_s, double video_fps, double cpu_pct);

/* Finaliza a medicao do quadro (apos esperar_fim), calcula latencia de entrada, contadores da FPGA e atualiza o painel. */
void perf_fim_quadro(void) {
    if (!ativo) return;
    long long t_fim = mono_us();
    double logica = (t_l1 - t_l0) / 1000.0;
    double dreno  = (t_fim - t_l1) / 1000.0;
    double ocup   = (t_fim - t_q0) / 1000.0;
    s_add(&s_logica, logica); s_add(&s_dreno, dreno); s_add(&s_ocup, ocup);
    quadros_j++;

    if (ev_pendente_us > 0) {                            /* evento -> comandos executados na FPGA */
        double lat = (mono_us() - ev_pendente_us) / 1000.0;
        if (lat >= 0 && lat < 5000) s_add(&s_entrada, lat);
        ev_pendente_us = 0;
    }

    unsigned st = ler_status();
    unsigned done = (st >> 25) & 0x7F;                   /* done_cnt: contador circular de 7 bits */
    if (done_ok) cmds_j += (done - done_prev) & 0x7F;
    done_prev = done; done_ok = 1;
    testar_status();

    /* Acumula as estatisticas na estrutura correspondente a cena atual */
    int c = cena_atual();
    double periodo = s_periodo.n ? s_periodo.v[(s_periodo.pos + N_AMOSTRAS - 1) % N_AMOSTRAS] : 0.0;
    cena_t[c].quadros++;
    cena_t[c].soma_periodo += periodo; if (periodo > cena_t[c].max_periodo) cena_t[c].max_periodo = periodo;
    cena_t[c].soma_ocup += ocup;       if (ocup > cena_t[c].max_ocup) cena_t[c].max_ocup = ocup;
    cena_t[c].soma_logica += logica; cena_t[c].soma_dreno += dreno; cena_t[c].soma_espera += s_periodo.n ? espera_ult : 0.0;   /* 1o quadro nao tem periodo */
    int ne = entidades_ativas(); if (ne > cena_t[c].max_entidades) cena_t[c].max_entidades = ne;

    /* Se completou o intervalo de 0.5 s, calcula taxas de CPU/FPS, desenha o painel e abre nova janela */
    if (t_fim - t_ult_painel >= INTERVALO_PAINEL_US) {
        double dt = (t_fim - t_ult_painel) / 1e6;
        double upd_s = quadros_j / dt;
        double video_fps = hw_quadros_j / dt;
        long long cpu_agora = cpu_us();
        double cpu_pct = 100.0 * (cpu_agora - cpu_ult) / (double)(t_fim - wall_ult);
        long long cpu_main_agora = cpu_thread_us();
        cpu_main_pct_j = 100.0 * (cpu_main_agora - cpu_main_ult) / (double)(t_fim - wall_ult);
        cpu_outras_pct_j = cpu_pct - cpu_main_pct_j;
        if (cpu_outras_pct_j < 0.0) cpu_outras_pct_j = 0.0;
        cpu_main_ult = cpu_main_agora;
        cpu_ult = cpu_agora; wall_ult = t_fim;
        if (cpu_pct > cpu_pico) cpu_pico = cpu_pct;

        painel(t_fim, upd_s, video_fps, cpu_pct);

        quadros_j = hw_quadros_j = hw_perdidos_j = cmds_j = 0;
        s_nova_janela(&s_periodo); s_nova_janela(&s_logica); s_nova_janela(&s_dreno);
        s_nova_janela(&s_ocup);    s_nova_janela(&s_entrada);   s_nova_janela(&s_espera);
        t_ult_painel = t_fim;
    }
}

/* ------------------------------------------------------------------ painel */

/* Grava os dados da janela no arquivo CSV (se aberto) e imprime o painel interativo no terminal via codigos ANSI. */
static void painel(long long agora, double upd_s, double video_fps, double cpu_pct) {
    double ocup_pct = s_media_j(&s_periodo) > 0 ? 100.0 * s_media_j(&s_ocup) / s_media_j(&s_periodo) : 0.0;
    double cmds_q = quadros_j ? (double)cmds_j / quadros_j : 0.0;
    double t_s = (agora - t_inicio) / 1e6;
    int ne = entidades_ativas();

    /* decomposicao do periodo (medias da janela) */
    double per = s_media_j(&s_periodo), esp = s_media_j(&s_espera);
    double lg = s_media_j(&s_logica),   dr  = s_media_j(&s_dreno);
    double outros = per - (esp + lg + dr);               /* teclas, checagens, painel */
    if (outros < 0.0) outros = 0.0;                      /* janelas desalinhadas por 1 quadro */
    double util_pct   = per > 0 ? 100.0 * lg / per : 0.0;
    double espocup_pct = per > 0 ? 100.0 * (esp + dr) / per : 0.0;

    if (csv) {
        fprintf(csv, "%.1f,%s,%.2f,%.2f,%lld,%.3f,%.3f,%.3f,%.3f,%.1f,%.2f,%.2f,%.1f,%.1f,%lld,%lld,%d,"
                "%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f\n",
                t_s, NOME_CENA[cena_atual()], upd_s, video_fps, hw_perdidos_j,
                s_media_j(&s_logica), s_logica.max_j, s_media_j(&s_dreno), s_dreno.max_j, ocup_pct,
                s_media_j(&s_entrada), s_entrada.max_j, cpu_pct, cmds_q, st_leituras, st_incoerentes, ne,
                esp, s_espera.max_j, outros, util_pct, espocup_pct, cpu_main_pct_j, cpu_outras_pct_j);
        fflush(csv);
    }
    if (!painel_ligado) return;

    if (painel_desenhado) printf("\033[%dA", N_LINHAS_PAINEL);   /* volta ao topo do painel */
    painel_desenhado = 1;
    printf("\033[K== ANW desempenho (ao vivo) ======================================\n");
    printf("\033[K cena: %-24s entidades: %2d/%d        t = %6.1f s\n",
           NOME_CENA[cena_atual()], ne, MAX_ENTITIES, t_s);
    printf("\033[K logica   : %5.1f upd/s  periodo med %5.2f max %5.2f p99 %5.2f ms  [%s]\n",
           upd_s, s_media_j(&s_periodo), s_periodo.max_j, s_pct(&s_periodo, 0.99),
           upd_s >= LIM_UPDATES_MIN ? "OK >=30" : "FALHA <30");
    printf("\033[K video HW : %5.1f fps     quadros perdidos: %lld agora, %lld no total\n",
           video_fps, hw_perdidos_j, hw_perdidos_t);
    printf("\033[K quadro   : logica %5.2f/%5.2f ms  dreno(DONE) %5.2f/%5.2f ms  ocupado %3.0f%%\n",
           s_media_j(&s_logica), s_logica.max_j, s_media_j(&s_dreno), s_dreno.max_j, ocup_pct);
    if (s_entrada.n_j)
        printf("\033[K entrada  : med %6.2f max %6.2f p95 %6.2f ms (n=%lld)  [%s]\n",
               s_media_j(&s_entrada), s_entrada.max_j, s_pct(&s_entrada, 0.95), s_entrada.n_j,
               s_entrada.max_j <= LIM_ENTRADA_MS ? "OK <=100" : "FALHA >100");
    else
        printf("\033[K entrada  : (sem eventos nesta janela - mexa/aperte algo)\n");
    printf("\033[K comandos  : done_cnt +%.1f por quadro (ver ANW_BENCH p/ ACK x DONE)\n", cmds_q);
    printf("\033[K espera    : quadro med %5.2f max %5.2f ms  outros %5.2f ms (polling)\n",
           esp, s_espera.max_j, outros);
    printf("\033[K CPU util  : %5.1f %% (logica/periodo)   espera ocupada %5.1f %%\n",
           util_pct, espocup_pct);
    printf("\033[K CPU bruta : %5.1f %% (main %5.1f, outras %4.1f)  pico %5.1f %%\n",
           cpu_pct, cpu_main_pct_j, cpu_outras_pct_j, cpu_pico);
    printf("\033[K STATUS    : %lld leituras, %lld incoerentes %s\n", st_leituras, st_incoerentes,
           st_incoerentes ? "<-- leitura rasgada: revisar CDC do STATUS" : "");
    printf("\033[K (Esc sai)  log: %s\n", csv ? "ANW_PERF_LOG ativo" : "defina ANW_PERF_LOG=arq.csv");
    fflush(stdout);
}

/* -------------------------------------------------------------- resumo final */

/* Imprime o relatorio consolidado de todas as cenas no encerramento do jogo e fecha o arquivo CSV. */
void perf_fim(void) {
    if (!ativo) return;
    if (painel_desenhado) printf("\n");
    printf("\n== RESUMO (desde o inicio) ==\n");
    double tot_per = 0, tot_log = 0, tot_dre = 0, tot_esp = 0;
    for (int c = 0; c < N_CENAS; c++) {
        const Cena *k = &cena_t[c];
        if (!k->quadros) continue;
        double per = k->soma_periodo / k->quadros;
        printf(" %-26s %7lld quadros  %5.1f upd/s  periodo max %6.2f ms  ocupacao med %5.2f max %6.2f ms  entidades max %d\n",
               NOME_CENA[c], k->quadros, per > 0 ? 1000.0 / per : 0.0, k->max_periodo,
               k->soma_ocup / k->quadros, k->max_ocup, k->max_entidades);
        printf("   -> CPU util %5.1f %%  espera ocupada %5.1f %%  espera de quadro med %.2f ms\n",
               k->soma_periodo > 0 ? 100.0 * k->soma_logica / k->soma_periodo : 0.0,
               k->soma_periodo > 0 ? 100.0 * (k->soma_espera + k->soma_dreno) / k->soma_periodo : 0.0,
               k->soma_espera / k->quadros);
        tot_per += k->soma_periodo; tot_log += k->soma_logica;
        tot_dre += k->soma_dreno;   tot_esp += k->soma_espera;
    }
    printf(" periodo  : med %.2f ms  p95 %.2f  p99 %.2f  max %.2f\n",
           s_media_t(&s_periodo), s_pct(&s_periodo, 0.95), s_pct(&s_periodo, 0.99), s_periodo.max_t);
    printf(" entrada  : med %.2f ms  p95 %.2f  p99 %.2f  max %.2f  (n=%lld)  limite %.0f ms\n",
           s_media_t(&s_entrada), s_pct(&s_entrada, 0.95), s_pct(&s_entrada, 0.99), s_entrada.max_t,
           s_entrada.n_t, LIM_ENTRADA_MS);
    printf(" dreno    : med %.2f ms  max %.2f   CPU pico %.1f %%   quadros perdidos %lld\n",
           s_media_t(&s_dreno), s_dreno.max_t, cpu_pico, hw_perdidos_t);
    printf(" espera   : quadro med %.2f ms  p95 %.2f  p99 %.2f  max %.2f\n",
           s_media_t(&s_espera), s_pct(&s_espera, 0.95), s_pct(&s_espera, 0.99), s_espera.max_t);
    {
        double wall_us = (double)(mono_us() - t_inicio);
        double bruta = wall_us > 0 ? 100.0 * (cpu_us() - cpu_ini) / wall_us : 0.0;
        double main_ = wall_us > 0 ? 100.0 * (cpu_thread_us() - cpu_main_ini) / wall_us : 0.0;
        double outras = bruta - main_; if (outras < 0.0) outras = 0.0;
        printf(" CPU      : bruta %.1f %% (main %.1f, outras %.1f)  util %.1f %%  espera ocupada %.1f %%\n",
               bruta, main_, outras,
               tot_per > 0 ? 100.0 * tot_log / tot_per : 0.0,
               tot_per > 0 ? 100.0 * (tot_esp + tot_dre) / tot_per : 0.0);
        printf("            (bruta ~100%% = espera ocupada no driver; util = logica/periodo)\n");
    }
    printf(" STATUS   : %lld leituras, %lld incoerentes\n", st_leituras, st_incoerentes);
    if (csv) { fclose(csv); csv = NULL; }
}

/* ------------------------------------------------------ benchmark ACK x DONE */
/* Usa movbgx(0): scroll zero, inofensivo. Mede, por lote de 1/10/32 comandos:
 *  ACK  = tempo ate TODOS os comandos serem aceitos pela FIFO (enviar_inst retornou)
 *  DONE = tempo ate FIFO vazia e UC livre (esperar_fim retornou) */
void perf_bench_comandos(void) {
    static const int lotes[3] = { 1, 10, 32 };
    const int REP = 100;
    Serie um; memset(&um, 0, sizeof um);

    printf("== BENCH de comandos (movbgx(0), %d repeticoes) ==\n", REP);
    if (esperar_fim() != DRIVER_OK) { printf(" falha ao drenar antes do bench\n"); return; }

    /* Mede a latencia individual de aceitacao (ACK) na FIFO para comandos isolados */
    for (int r = 0; r < REP * 5; r++) {                  /* 1 comando isolado: latencia de ACK */
        long long a = mono_us();
        int rc = movbgx(0);
        long long b = mono_us();
        if (rc != DRIVER_OK) { printf(" erro %d no envio\n", rc); return; }
        s_add(&um, (b - a) / 1000.0);
        if (esperar_fim() != DRIVER_OK) { printf(" erro/timeout no esperar_fim\n"); return; }
    }
    printf(" 1 cmd isolado  ACK: med %.4f ms  p95 %.4f  p99 %.4f  max %.4f\n",
           s_media_t(&um), s_pct(&um, 0.95), s_pct(&um, 0.99), um.max_t);

    /* Compara o tempo de enfileiramento (ACK) com o tempo de conclusao total na UC (DONE) em rajadas */
    for (int li = 0; li < 3; li++) {
        Serie ack, done; memset(&ack, 0, sizeof ack); memset(&done, 0, sizeof done);
        for (int r = 0; r < REP; r++) {
            long long t0 = mono_us();
            for (int k = 0; k < lotes[li]; k++)
                if (movbgx(0) != DRIVER_OK) { printf(" erro no envio do lote %d\n", lotes[li]); return; }
            long long t1 = mono_us();
            if (esperar_fim() != DRIVER_OK) { printf(" erro/timeout no lote %d\n", lotes[li]); return; }
            long long t2 = mono_us();
            s_add(&ack, (t1 - t0) / 1000.0);
            s_add(&done, (t2 - t0) / 1000.0);
        }
        printf(" lote %2d cmds  ACK(todos): med %.3f max %.3f ms | DONE: med %.3f max %.3f ms | DONE-ACK med %.3f ms\n",
               lotes[li], s_media_t(&ack), ack.max_t, s_media_t(&done), done.max_t,
               s_media_t(&done) - s_media_t(&ack));
    }
    int e = erro_inst();
    printf(" erro_inst() apos o bench: 0x%x %s\n", e, e ? "(limpando)" : "(ok)");
    if (e) limpar_erro();
    printf("\n");
}