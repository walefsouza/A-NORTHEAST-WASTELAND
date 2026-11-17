/*
 * perf.c
 *
 * Implementacao do modulo de medicao de desempenho. Coleta metricas do jogo
 * e do coprocessador sem espalhar a instrumentacao pela logica das cenas.
 *
 * O arquivo trabalha em conjunto com main.c: cada funcao perf_* e chamada em
 * um ponto especifico do ciclo de vida de um quadro. Assim, os tempos de
 * espera, logica e conclusao dos comandos podem ser analisados separadamente.
 *
 * Metricas registradas:
 *   - taxa de atualizacao da logica e taxa de quadros do hardware;
 *   - tempo de cada etapa do quadro e latencia de entrada;
 *   - uso de CPU do processo e da thread principal;
 *   - comandos concluidos e coerencia de leituras do STATUS;
 *   - estatisticas por cena, no terminal e em arquivo CSV.
 *
 * O modulo tambem oferece um benchmark opcional para comparar ACK e DONE.
 *
 * --------------------------------------------------------------------------
 * Medicao e interpretacao
 *
 * O periodo de um quadro e decomposto aproximadamente em:
 *   espera_quadro + logica + dreno + outros.
 * "Outros" inclui tarefas como leitura das teclas, verificacoes e atualizacao
 * do painel. Os valores sao aproximacoes alinhadas por quadro.
 *
 * CPU bruta mede o tempo efetivamente consumido pelo processo dividido pelo
 * tempo real decorrido. Como o driver consulta registradores em polling
 * (espera ocupada), esse percentual pode ficar proximo de 100%.
 *
 * CPU util e uma estimativa do tempo de logica em relacao ao periodo do quadro.
 * A parcela de espera ocupada considera espera_quadro + dreno.
 *
 * O relogio monotônico e usado para que ajustes no relogio do sistema nao
 * afetem a duracao das amostras.
 *
 * O relogio e obtido por CLOCK_MONOTONIC via syscall(), evitando a necessidade
 * de adicionar -lrt a linha de compilacao usada pelo projeto.
 */
#define _GNU_SOURCE             /* RUSAGE_THREAD */
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

/* Frequencia de atualizacao visual/CSV: 500.000 us = 0,5 s. */
#define INTERVALO_PAINEL_US 500000LL
/* Numero maximo de amostras recentes mantidas para percentis do painel. */
#define N_AMOSTRAS          512
/* Limites usados apenas para indicar OK/FALHA no painel. */
#define LIM_UPDATES_MIN     30.0
#define LIM_ENTRADA_MS      100.0         /* PBL03: <= 100 ms       */
/* Habilita a verificacao de leituras consecutivas do registrador STATUS. */
#define TESTE_STATUS        1
/* Quantidade de linhas reescritas pelo painel no terminal. */
#define N_LINHAS_PAINEL     12
/* Largura de cada faixa do histograma usado nos percentis do resumo (ms). */
#define HIST_BIN_MS         0.01
/* 20.000 faixas cobrem de 0 a 200 ms; valores maiores vao para a ultima.
 * O maximo real continua guardado separadamente em max_t. */
#define HIST_N              20000

/* ------------------------------------------------------------------ tempo */
/* Retorna o tempo monotônico em microssegundos.
 * CLOCK_MONOTONIC e apropriado para medir intervalos, pois nao retrocede
 * quando o horario civil da maquina e ajustado. */
static long long mono_us(void) {
    struct timespec ts;
    syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}
/* Tempo de CPU consumido pelo processo inteiro, incluindo as threads de
 * entrada. O resultado e convertido para microssegundos. */
static long long cpu_us(void) {
    struct rusage r;
    getrusage(RUSAGE_SELF, &r);            /* inclui as threads de entrada */
    return ((long long)r.ru_utime.tv_sec + r.ru_stime.tv_sec) * 1000000LL + r.ru_utime.tv_usec + r.ru_stime.tv_usec;
}

/* Tempo de CPU apenas da thread que executa a medicao (a principal).
 * A diferenca entre cpu_us() e esta funcao aproxima o consumo das outras
 * threads, principalmente as responsaveis pela entrada. */
static long long cpu_thread_us(void) {
    struct rusage r;
    getrusage(RUSAGE_THREAD, &r);
    return ((long long)r.ru_utime.tv_sec + r.ru_stime.tv_sec) * 1000000LL + r.ru_utime.tv_usec + r.ru_stime.tv_usec;
}

/* ----------------------------------------------------------------- series */
/* Estrutura reutilizada para armazenar uma metrica ao longo do tempo.
 *
 * v[] guarda somente as N_AMOSTRAS mais recentes e serve para os percentis
 * do painel. Os campos *_j acumulam a janela visual atual (aprox. 0,5 s),
 * enquanto *_t acumulam todo o periodo do resumo.
 *
 * h aponta para um histograma opcional. Quando existe, permite calcular
 * percentis do resumo sem guardar individualmente todas as amostras.
 */
typedef struct {
    float  v[N_AMOSTRAS];                  /* ultimas amostras (ms) */
    int    n, pos;
    double soma_j, max_j; long long n_j;   /* janela do painel */
    double soma_t, max_t; long long n_t;   /* desde o inicio (ou do inicio da janela do resumo) */
    unsigned *h;                           /* histograma (so nas series do resumo); NULL = sem */
} Serie;

/* Adiciona uma amostra a todas as visoes da serie: anel recente, janela
 * atual, acumulado total e histograma (quando alocado). */
static void s_add(Serie *s, double x) {
    s->v[s->pos] = (float)x;
    s->pos = (s->pos + 1) % N_AMOSTRAS;
    if (s->n < N_AMOSTRAS) s->n++;
    s->soma_j += x; s->n_j++; if (x > s->max_j) s->max_j = x;
    s->soma_t += x; s->n_t++; if (x > s->max_t) s->max_t = x;
    if (s->h) {
        int b = (int)(x / HIST_BIN_MS);
        if (b < 0) b = 0;
        if (b >= HIST_N) b = HIST_N - 1;
        s->h[b]++;
    }
}
/* Zera apenas os acumuladores da janela do painel; preserva o total e o
 * historico recente usados pelo resumo e pelos percentis do painel. */
static void s_nova_janela(Serie *s) { s->soma_j = 0; s->max_j = 0; s->n_j = 0; }
/* Media das amostras da janela visual atual. */
static double s_media_j(const Serie *s) { return s->n_j ? s->soma_j / s->n_j : 0.0; }
/* Media de todas as amostras acumuladas para o resumo. */
static double s_media_t(const Serie *s) { return s->n_t ? s->soma_t / s->n_t : 0.0; }
/* Reinicia os acumuladores do resumo e o histograma, sem desalocar memoria.
 * Usado quando a janela configurada comeca no primeiro quadro da partida. */
static void s_reset_t(Serie *s) {
    s->soma_t = 0; s->max_t = 0; s->n_t = 0;
    if (s->h) memset(s->h, 0, sizeof(unsigned) * HIST_N);
}
/* percentil sobre TODAS as amostras do resumo (nao so as ultimas N_AMOSTRAS) */
/* Percentil aproximado sobre todas as amostras do resumo. A busca percorre
 * o histograma ate encontrar a faixa que contem a posicao desejada. */
static double s_pct_t(const Serie *s, double p) {
    if (!s->h || s->n_t == 0) return 0.0;
    long long k = (long long)(p * (double)(s->n_t - 1) + 0.5), cum = 0;
    for (int b = 0; b < HIST_N; b++) {
        cum += s->h[b];
        if (cum > k) return b * HIST_BIN_MS;
    }
    return s->max_t;
}

/* Comparador usado por qsort() para ordenar as amostras recentes. */
static int cmp_f(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}
/* Percentil calculado exatamente sobre as amostras ainda presentes no anel.
 * O conjunto e copiado antes da ordenacao para nao alterar a ordem circular. */
static double s_pct(const Serie *s, double p) {
    float tmp[N_AMOSTRAS];
    if (s->n == 0) return 0.0;
    memcpy(tmp, s->v, sizeof(float) * (size_t)s->n);
    qsort(tmp, (size_t)s->n, sizeof(float), cmp_f);
    int k = (int)(p * (s->n - 1) + 0.5);
    return tmp[k];
}

/* ------------------------------------------------------------------ estado */
/* Categorias usadas para separar as estatisticas por estado do jogo.
 * Estados que nao sejam menu nem partida entram em CENA_OUTRAS. */
enum { CENA_MENU = 0, CENA_PARTIDA, CENA_OUTRAS, N_CENAS };
static const char *NOME_CENA[N_CENAS] = { "MENU", "PARTIDA", "OUTRAS (pausa/trans/fim)" };

typedef struct {                           /* acumulado por cena, para o resumo final */
    long long quadros;
    double soma_periodo, max_periodo;      /* ms entre quadros */
    double soma_ocup, max_ocup;            /* ms de logica + dreno */
    double soma_logica, soma_dreno, soma_espera;   /* ms, para CPU util e espera ocupada */
    int    max_entidades;
} Cena;
static Cena cena_t[N_CENAS];

static Serie s_periodo, s_logica, s_dreno, s_ocup, s_entrada, s_espera;

static int       ativo, painel_ligado;
static long long t_inicio, t_ult_painel, t_prev_quadro, t_q0, t_l0, t_l1;
static long long cpu_ult, wall_ult;
static long long cpu_ini, cpu_main_ini, cpu_main_ult;   /* CPU total / da thread principal */
static long long t_w0;                   /* instante antes de esperar_quadro() */
static double    espera_ult;             /* ms esperando o quadro atual */
static double    cpu_main_pct_j, cpu_outras_pct_j;   /* janela do painel */
static int       hw_prev = -1;
static long long hw_quadros_j, hw_perdidos_j, hw_perdidos_t, quadros_j;
static long long ev_pendente_us;
static double    cpu_pico;
static unsigned  done_prev; static int done_ok;
static long long cmds_j;
static long long st_leituras, st_incoerentes;
static FILE     *csv;
static int       painel_desenhado;
/* resumo / janela */
static double    janela_s;               /* ANW_PERF_JANELA: 0 = desde o inicio do programa */
static int       res_aguarda, res_ativa, res_fechado;
static long long t_res0;                 /* inicio da janela do resumo */
static long long hw_quadros_t, hw_repetidos_t;   /* video: soma dos deltas do PIO_STATUS na janela */
static double    painel_max_ms;          /* maior tempo gasto imprimindo o painel */
static const char *resumo_arq;

/* Converte o estado ativo do jogo em uma das categorias de medicao acima. */
static int cena_atual(void) {
    const Estado *e = estado_atual();
    if (e == &estado_menu) return CENA_MENU;
    if (e == &estado_partida) return CENA_PARTIDA;
    return CENA_OUTRAS;
}
/* Conta as entidades atualmente marcadas como ativas no mundo do jogo. */
static int entidades_ativas(void) {
    int n = 0;
    for (int i = 0; i < MAX_ENTITIES; i++) if (entities[i].active) n++;
    return n;
}

/* Reinicia todos os acumuladores do resumo no instante de inicio da janela.
 * Quando ANW_PERF_JANELA esta configurada, isso ocorre ao detectar PARTIDA.
 * inicio_us e usado como referencia temporal da janela. */
static void res_reset(long long inicio_us) {
    memset(cena_t, 0, sizeof cena_t);
    s_reset_t(&s_periodo); s_reset_t(&s_logica); s_reset_t(&s_dreno);
    s_reset_t(&s_ocup);    s_reset_t(&s_entrada); s_reset_t(&s_espera);
    hw_perdidos_t = hw_quadros_t = hw_repetidos_t = 0;
    st_leituras = st_incoerentes = 0;
    cpu_pico = 0.0; painel_max_ms = 0.0;
    t_res0 = inicio_us;
    cpu_ini = cpu_us(); cpu_main_ini = cpu_thread_us();
    res_ativa = 1;
}

/* Prepara o sistema de medicao:
 *   - le as variaveis de ambiente;
 *   - inicializa as series e os histogramas;
 *   - define se o resumo comeca imediatamente ou aguarda a PARTIDA;
 *   - abre o CSV em modo de acrescimo e escreve o cabecalho se estiver vazio.
 */
void perf_iniciar(void) {
    const char *p = getenv("ANW_PERF");
    const char *log = getenv("ANW_PERF_LOG");
    /* O painel e ligado por padrao; ANW_PERF=0 oculta somente a exibicao,
     * sem desativar a coleta nem o resumo final. */
    painel_ligado = !(p && p[0] == '0');
    memset(&s_periodo, 0, sizeof s_periodo); memset(&s_logica, 0, sizeof s_logica);
    memset(&s_dreno, 0, sizeof s_dreno);     memset(&s_ocup, 0, sizeof s_ocup);
    memset(&s_entrada, 0, sizeof s_entrada); memset(&s_espera, 0, sizeof s_espera);
    /* Histogramas usados nos percentis do resumo. As demais series usam
     * apenas seus acumuladores e/ou o anel das amostras recentes. */
    s_periodo.h = calloc(HIST_N, sizeof(unsigned)); s_dreno.h  = calloc(HIST_N, sizeof(unsigned));
    s_entrada.h = calloc(HIST_N, sizeof(unsigned)); s_espera.h = calloc(HIST_N, sizeof(unsigned));
    {
        /* Sem janela positiva, a medicao do resumo comeca na inicializacao.
         * Com ANW_PERF_JANELA, aguarda-se o primeiro quadro da PARTIDA. */
        const char *j = getenv("ANW_PERF_JANELA");
        janela_s = (j && j[0]) ? atof(j) : 0.0;
        if (janela_s < 0.0) janela_s = 0.0;
        resumo_arq = getenv("ANW_PERF_RESUMO");
        if (resumo_arq && !resumo_arq[0]) resumo_arq = NULL;
    }
    t_inicio = t_ult_painel = t_res0 = mono_us();
    /* res_aguarda/res_ativa controlam o inicio efetivo do resumo. O relogio
     * t_inicio continua servindo ao tempo global exibido no painel. */
    res_aguarda = janela_s > 0.0;          /* com janela, so mede a partir do 1o quadro da partida */
    res_ativa = !res_aguarda;
    wall_ult = t_inicio; cpu_ult = cpu_ini = cpu_us();
    cpu_main_ult = cpu_main_ini = cpu_thread_us();
    /* Abre o CSV em modo append para preservar execucoes anteriores.
     * O cabecalho so e escrito quando o arquivo esta vazio. */
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
/* Hook chamado imediatamente antes de esperar_quadro(). Marca o inicio da
 * espera pelo proximo quadro do hardware. */
void perf_antes_espera_quadro(void) {
    if (!ativo) return;
    t_w0 = mono_us();
}

/* Hook chamado depois de receber um quadro valido.
 *
 * Alem de finalizar a medicao da espera, calcula o periodo entre quadros e
 * acompanha o contador de 16 bits do hardware. A subtracao com mascara
 * 0xFFFF trata corretamente o retorno do contador de 65535 para 0.
 *
 * Se uma janela foi solicitada, a primeira deteccao de CENA_PARTIDA chama
 * res_reset() para iniciar os acumuladores naquele ponto. */
void perf_inicio_quadro(int quadro_hw) {
    if (!ativo) return;
    long long agora = mono_us();
    t_q0 = agora;
    /* A janela configurada nao inclui o tempo passado no menu. */
    if (res_aguarda && !res_ativa && cena_atual() == CENA_PARTIDA)
        res_reset(t_prev_quadro ? t_prev_quadro : agora);     /* janela comeca no quadro anterior */
    if (t_w0) {                                          /* tempo preso em esperar_quadro() */
        espera_ult = (agora - t_w0) / 1000.0;
        s_add(&s_espera, espera_ult);
        t_w0 = 0;
    } else {
        espera_ult = 0.0;                                /* hook nao chamado: nao inventa dado */
    }
    if (t_prev_quadro) s_add(&s_periodo, (agora - t_prev_quadro) / 1000.0);
    t_prev_quadro = agora;

    /* Compara o contador atual com o anterior. Delta 0 indica repeticao;
     * delta maior que 1 indica que o video avancou mais de um quadro entre
     * duas atualizacoes da logica. */
    if (hw_prev >= 0) {                                  /* contador de 16 bits do hardware */
        int d = (quadro_hw - hw_prev) & 0xFFFF;
        hw_quadros_j += d; hw_quadros_t += d;
        if (d == 0) hw_repetidos_t++;
        if (d > 1) { hw_perdidos_j += d - 1; hw_perdidos_t += d - 1; }   /* video andou, logica nao */
    } else {
        hw_quadros_t += 1;                               /* 1o quadro do programa: conta como 1 de video */
    }
    hw_prev = quadro_hw;
}

/* Marca o inicio da atualizacao da cena e captura o timestamp do evento
 * de entrada mais antigo que ainda nao foi consumido pela logica. */
void perf_antes_logica(void) {
    if (!ativo) return;
    ev_pendente_us = entrada_pegar_evento_us();          /* evento mais antigo ainda nao tratado */
    t_l0 = mono_us();
}
/* Marca o fim da atualizacao da cena. O intervalo posterior, ate t_fim,
 * sera contabilizado como tempo de dreno/conclusao dos comandos. */
void perf_depois_logica(void) {
    if (!ativo) return;
    t_l1 = mono_us();
}

/* Verifica a coerencia de leituras consecutivas do STATUS.
 *
 * Em uma rajada curta, o contador de quadros deve permanecer igual ou avancar
 * no maximo uma unidade. Uma variacao impossivel pode indicar leitura
 * inconsistente durante a passagem do sinal entre dominios de clock (CDC).
 * A funcao de comparacao esta isolada em perf_status.c. */
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

/* Funcoes internas: atualizam o painel/CSV e montam o resumo final. */
static void painel(long long agora, double upd_s, double video_fps, double cpu_pct);
static void resumo_emitir(long long agora);

/* Fecha a coleta do quadro atual.
 *
 * Calcula:
 *   logica = tempo entre perf_antes_logica() e perf_depois_logica();
 *   dreno  = tempo entre perf_depois_logica() e o fim deste hook;
 *   ocup   = tempo entre a chegada do quadro e o fim da conclusao.
 *
 * Tambem atualiza latencia de entrada, contadores do coprocessador, estatisticas
 * por cena e, a cada INTERVALO_PAINEL_US, redesenha o painel e grava o CSV.
 */
void perf_fim_quadro(void) {
    if (!ativo) return;
    long long t_fim = mono_us();
    double logica = (t_l1 - t_l0) / 1000.0;
    double dreno  = (t_fim - t_l1) / 1000.0;
    double ocup   = (t_fim - t_q0) / 1000.0;
    s_add(&s_logica, logica); s_add(&s_dreno, dreno); s_add(&s_ocup, ocup);
    quadros_j++;

    /* Latencia de entrada: do timestamp coletado pela thread de entrada ate
     * o momento em que a logica ja processou o evento e os comandos associados
     * foram encaminhados. Amostras absurdamente grandes sao descartadas. */
    if (ev_pendente_us > 0) {                            /* evento -> comandos executados na FPGA */
        double lat = (mono_us() - ev_pendente_us) / 1000.0;
        if (lat >= 0 && lat < 5000) s_add(&s_entrada, lat);
        ev_pendente_us = 0;
    }

    /* done_cnt e um contador circular de 7 bits exposto no registrador STATUS.
     * A diferenca modular estima quantos comandos foram concluidos desde a
     * leitura anterior. */
    unsigned st = ler_status();
    unsigned done = (st >> 25) & 0x7F;                   /* done_cnt: contador circular de 7 bits */
    if (done_ok) cmds_j += (done - done_prev) & 0x7F;
    done_prev = done; done_ok = 1;
    testar_status();

    /* Associa as estatisticas deste quadro a cena que estava ativa ao final
     * da atualizacao. */
    int c = cena_atual();
    double periodo = s_periodo.n ? s_periodo.v[(s_periodo.pos + N_AMOSTRAS - 1) % N_AMOSTRAS] : 0.0;
    cena_t[c].quadros++;
    cena_t[c].soma_periodo += periodo; if (periodo > cena_t[c].max_periodo) cena_t[c].max_periodo = periodo;
    cena_t[c].soma_ocup += ocup;       if (ocup > cena_t[c].max_ocup) cena_t[c].max_ocup = ocup;
    cena_t[c].soma_logica += logica; cena_t[c].soma_dreno += dreno; cena_t[c].soma_espera += s_periodo.n ? espera_ult : 0.0;   /* 1o quadro nao tem periodo */
    int ne = entidades_ativas(); if (ne > cena_t[c].max_entidades) cena_t[c].max_entidades = ne;

    /* O painel e atualizado em intervalos de aproximadamente meio segundo.
     * As taxas sao calculadas dividindo os contadores pelo tempo real do
     * intervalo, nao pelo numero de iteracoes do laco. */
    if (t_fim - t_ult_painel >= INTERVALO_PAINEL_US) {
        double dt = (t_fim - t_ult_painel) / 1e6;
        double upd_s = quadros_j / dt;
        double video_fps = hw_quadros_j / dt;
        long long cpu_agora = cpu_us();
        /* Percentual bruto: microssegundos de CPU consumidos / microssegundos
         * reais decorridos. Pode ultrapassar 100% em processos multithread. */
        double cpu_pct = 100.0 * (cpu_agora - cpu_ult) / (double)(t_fim - wall_ult);
        long long cpu_main_agora = cpu_thread_us();
        cpu_main_pct_j = 100.0 * (cpu_main_agora - cpu_main_ult) / (double)(t_fim - wall_ult);
        cpu_outras_pct_j = cpu_pct - cpu_main_pct_j;
        if (cpu_outras_pct_j < 0.0) cpu_outras_pct_j = 0.0;
        cpu_main_ult = cpu_main_agora;
        cpu_ult = cpu_agora; wall_ult = t_fim;
        if (cpu_pct > cpu_pico) cpu_pico = cpu_pct;

        {
            long long tp0 = mono_us();
            painel(t_fim, upd_s, video_fps, cpu_pct);
            double dp = (mono_us() - tp0) / 1000.0;
            if (dp > painel_max_ms) painel_max_ms = dp;
        }

        /* Reinicia somente as estatisticas da proxima janela do painel.
         * Os acumuladores do resumo permanecem preservados. */
        quadros_j = hw_quadros_j = hw_perdidos_j = cmds_j = 0;
        s_nova_janela(&s_periodo); s_nova_janela(&s_logica); s_nova_janela(&s_dreno);
        s_nova_janela(&s_ocup);    s_nova_janela(&s_entrada);   s_nova_janela(&s_espera);
        t_ult_painel = t_fim;
    }

    /* Ao atingir a duracao configurada, emite o resumo uma unica vez.
     * Este trecho encerra a janela estatistica; o encerramento do jogo continua
     * sendo responsabilidade do laco principal em main.c. */
    if (janela_s > 0.0 && res_ativa && !res_fechado && (t_fim - t_res0) >= (long long)(janela_s * 1e6)) {
        resumo_emitir(t_fim);
        res_fechado = 1;
    }
}

/* ------------------------------------------------------------------ painel */
/* Monta uma fotografia das metricas da janela atual.
 *
 * O CSV e atualizado mesmo quando ANW_PERF=0; essa variavel desliga somente
 * a exibicao no terminal. Os codigos ANSI limpam e reposicionam o cursor para
 * redesenhar o painel no mesmo bloco de linhas. */
static void painel(long long agora, double upd_s, double video_fps, double cpu_pct) {
    double ocup_pct = s_media_j(&s_periodo) > 0 ? 100.0 * s_media_j(&s_ocup) / s_media_j(&s_periodo) : 0.0;
    double cmds_q = quadros_j ? (double)cmds_j / quadros_j : 0.0;
    double t_s = (agora - t_inicio) / 1e6;
    int ne = entidades_ativas();

    /* Decomposicao aproximada do periodo medio:
     *   outros = periodo - (espera + logica + dreno).
     * Os percentuais usam o periodo como denominador. */
    /* decomposicao do periodo (medias da janela) */
    double per = s_media_j(&s_periodo), esp = s_media_j(&s_espera);
    double lg = s_media_j(&s_logica),   dr  = s_media_j(&s_dreno);
    double outros = per - (esp + lg + dr);               /* teclas, checagens, painel */
    if (outros < 0.0) outros = 0.0;                      /* janelas desalinhadas por 1 quadro */
    double util_pct   = per > 0 ? 100.0 * lg / per : 0.0;
    double espocup_pct = per > 0 ? 100.0 * (esp + dr) / per : 0.0;

    /* Cada chamada do painel grava uma linha no CSV: os valores representam
     * as medias/maximos da janela visual atual e os contadores associados. */
    if (csv) {
        fprintf(csv, "%.1f,%s,%.2f,%.2f,%lld,%.3f,%.3f,%.3f,%.3f,%.1f,%.2f,%.2f,%.1f,%.1f,%lld,%lld,%d,"
                "%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f\n",
                t_s, NOME_CENA[cena_atual()], upd_s, video_fps, hw_perdidos_j,
                s_media_j(&s_logica), s_logica.max_j, s_media_j(&s_dreno), s_dreno.max_j, ocup_pct,
                s_media_j(&s_entrada), s_entrada.max_j, cpu_pct, cmds_q, st_leituras, st_incoerentes, ne,
                esp, s_espera.max_j, outros, util_pct, espocup_pct, cpu_main_pct_j, cpu_outras_pct_j);
        fflush(csv);
    }
    /* A coleta e a gravacao ja foram feitas. A partir daqui so existe trabalho
     * quando a exibicao interativa estiver habilitada. */
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
/* Escreve as metricas consolidadas em um fluxo FILE (terminal ou arquivo).
 * As estatisticas por cena permitem separar menu, partida e demais estados. */
static void resumo_escrever(FILE *f, long long agora) {
    double dur = (agora - t_res0) / 1e6;
    long long q_logica = 0;
    for (int c = 0; c < N_CENAS; c++) q_logica += cena_t[c].quadros;

    if (janela_s > 0.0)
        fprintf(f, "\n== RESUMO (janela de %.0f s a partir do inicio da partida) ==\n", janela_s);
    else
        fprintf(f, "\n== RESUMO (desde o inicio) ==\n");
    fprintf(f, " duracao  : %.1f s medidos%s\n", dur,
            (janela_s > 0.0 && dur + 0.05 < janela_s) ? "  (janela incompleta: o jogo terminou antes)" : "");
    fprintf(f, " quadros  : logica %lld  video(PIO_STATUS) %lld  perdidos %lld  repetidos %lld  [logica+perdidos %s video]\n",
            q_logica, hw_quadros_t, hw_perdidos_t, hw_repetidos_t,
            (q_logica + hw_perdidos_t == hw_quadros_t) ? "==" : "!=");
    fprintf(f, " taxa     : logica %.1f upd/s  video %.1f fps (medias da janela)\n",
            dur > 0 ? q_logica / dur : 0.0, dur > 0 ? hw_quadros_t / dur : 0.0);

    /* Acumuladores globais usados para calcular os percentuais finais de CPU. */
    double tot_per = 0, tot_log = 0, tot_dre = 0, tot_esp = 0;
    for (int c = 0; c < N_CENAS; c++) {
        const Cena *k = &cena_t[c];
        if (!k->quadros) continue;
        double per = k->soma_periodo / k->quadros;
        fprintf(f, " %-26s %7lld quadros  %5.1f upd/s  periodo max %6.2f ms  ocupacao med %5.2f max %6.2f ms  entidades max %d\n",
                NOME_CENA[c], k->quadros, per > 0 ? 1000.0 / per : 0.0, k->max_periodo,
                k->soma_ocup / k->quadros, k->max_ocup, k->max_entidades);
        fprintf(f, "   -> CPU util %5.1f %%  espera ocupada %5.1f %%  espera de quadro med %.2f ms\n",
                k->soma_periodo > 0 ? 100.0 * k->soma_logica / k->soma_periodo : 0.0,
                k->soma_periodo > 0 ? 100.0 * (k->soma_espera + k->soma_dreno) / k->soma_periodo : 0.0,
                k->soma_espera / k->quadros);
        tot_per += k->soma_periodo; tot_log += k->soma_logica;
        tot_dre += k->soma_dreno;   tot_esp += k->soma_espera;
    }
    fprintf(f, " periodo  : med %.2f ms  p95 %.2f  p99 %.2f  max %.2f\n",
            s_media_t(&s_periodo), s_pct_t(&s_periodo, 0.95), s_pct_t(&s_periodo, 0.99), s_periodo.max_t);
    fprintf(f, " entrada  : med %.2f ms  p95 %.2f  p99 %.2f  max %.2f  (n=%lld)  limite %.0f ms\n",
            s_media_t(&s_entrada), s_pct_t(&s_entrada, 0.95), s_pct_t(&s_entrada, 0.99), s_entrada.max_t,
            s_entrada.n_t, LIM_ENTRADA_MS);
    fprintf(f, " dreno    : med %.2f ms  max %.2f   CPU pico %.1f %%   quadros perdidos %lld\n",
            s_media_t(&s_dreno), s_dreno.max_t, cpu_pico, hw_perdidos_t);
    fprintf(f, " espera   : quadro med %.2f ms  p95 %.2f  p99 %.2f  max %.2f\n",
            s_media_t(&s_espera), s_pct_t(&s_espera, 0.95), s_pct_t(&s_espera, 0.99), s_espera.max_t);
    fprintf(f, " painel   : impressao max %.2f ms (fica fora da ocupacao; entra no periodo do quadro seguinte)\n",
            painel_max_ms);
    /* Separa o consumo bruto do processo entre a thread principal e as
     * demais threads. CPU util e espera ocupada sao estimativas baseadas nos
     * tempos das etapas do quadro, nao no consumo bruto do sistema operacional. */
    {
        double wall_us = (double)(agora - t_res0);
        double bruta = wall_us > 0 ? 100.0 * (cpu_us() - cpu_ini) / wall_us : 0.0;
        double main_ = wall_us > 0 ? 100.0 * (cpu_thread_us() - cpu_main_ini) / wall_us : 0.0;
        double outras = bruta - main_; if (outras < 0.0) outras = 0.0;
        fprintf(f, " CPU      : bruta %.1f %% (main %.1f, outras %.1f)  util %.1f %%  espera ocupada %.1f %%\n",
                bruta, main_, outras,
                tot_per > 0 ? 100.0 * tot_log / tot_per : 0.0,
                tot_per > 0 ? 100.0 * (tot_esp + tot_dre) / tot_per : 0.0);
        fprintf(f, "            (bruta ~100%% = espera ocupada no driver; util = logica/periodo)\n");
    }
    fprintf(f, " STATUS   : %lld leituras, %lld incoerentes\n", st_leituras, st_incoerentes);
}

/* Emite o mesmo resumo no terminal e, se ANW_PERF_RESUMO foi configurada,
 * grava uma copia em arquivo. O arquivo de resumo e sobrescrito a cada execucao. */
static void resumo_emitir(long long agora) {
    if (painel_desenhado) { printf("\n"); painel_desenhado = 0; }   /* o painel recomeca abaixo do resumo */
    resumo_escrever(stdout, agora);
    fflush(stdout);
    if (resumo_arq) {
        FILE *f = fopen(resumo_arq, "w");
        if (f) { resumo_escrever(f, agora); fclose(f); }
    }
}

/* Finalizacao do modulo. Se a janela ainda nao foi iniciada, informa que
 * nenhum dado foi medido; se ja houve coleta, emite o resumo pendente.
 * Por ultimo, fecha o CSV para garantir que os dados sejam descarregados. */
void perf_fim(void) {
    if (!ativo) return;
    if (!res_fechado) {
        if (res_aguarda && !res_ativa) {
            if (painel_desenhado) printf("\n");
            printf("\n== RESUMO == a partida nao chegou a comecar: nada medido na janela.\n");
        } else {
            resumo_emitir(mono_us());
        }
    }
    if (csv) { fclose(csv); csv = NULL; }
}

/* ------------------------------------------------------ benchmark ACK x DONE */
/* Benchmark auxiliar para separar duas latencias:
 *   ACK  - tempo ate o driver aceitar todos os comandos enviados;
 *   DONE - tempo ate todos os comandos terminarem na unidade de controle.
 *
 * movbgx(0) usa deslocamento zero, portanto nao deve alterar visualmente
 * a posicao do fundo. O benchmark e opcional e roda antes do laco do jogo. */
/* Usa movbgx(0): scroll zero, inofensivo. Mede, por lote de 1/10/32 comandos:
 *  ACK  = tempo ate TODOS os comandos serem aceitos pela FIFO (enviar_inst retornou)
 *  DONE = tempo ate FIFO vazia e UC livre (esperar_fim retornou) */
void perf_bench_comandos(void) {
    static const int lotes[3] = { 1, 10, 32 };
    const int REP = 100;
    Serie um; memset(&um, 0, sizeof um);

    printf("== BENCH de comandos (movbgx(0), %d repeticoes) ==\n", REP);
    if (esperar_fim() != DRIVER_OK) { printf(" falha ao drenar antes do bench\n"); return; }

    /* Mede 500 envios isolados (5 * REP) para caracterizar a latencia de ACK
     * de um unico comando, aguardando a conclusao antes da proxima repeticao. */
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

    /* Para cada tamanho de lote (1, 10 e 32 comandos), mede o tempo de envio
     * de todos os comandos (ACK) e o tempo total ate o processamento terminar
     * (DONE), repetindo REP vezes. */
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
