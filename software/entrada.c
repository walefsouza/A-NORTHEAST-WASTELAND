/* entrada.c - teclado e mouse via evdev, com uma thread por dispositivo.
 *
 * Os caminhos dos dispositivos sao fornecidos pelo main (preferencialmente
 * por /dev/input/by-id). Cada thread le apenas o seu dispositivo. O estado
 * compartilhado e protegido por 'trava'; nenhuma leitura bloqueante ocorre
 * enquanto o mutex esta travado.
 *
 * Se um dispositivo for removido, seu estado e limpo e a thread tenta
 * reabri-lo periodicamente pelo mesmo caminho. Isso permite reconexao quando
 * o caminho e um symlink estavel de /dev/input/by-id.
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "entrada.h"

/* Fallback caso o header do kernel nao defina o ioctl de selecao de clock */
#ifndef EVIOCSCLOCKID
#define EVIOCSCLOCKID _IOW('E', 0xa0, int)
#endif

/* Quantidade total de codigos de tecla possiveis no subsistema input */
#ifndef KEY_CNT
#define KEY_CNT (KEY_MAX + 1)
#endif

#define LARGURA             320  /* limite horizontal do cursor (tela VGA) */
#define ALTURA              240  /* limite vertical do cursor (tela VGA) */
#define TEMPO_POLL_MS       100  /* timeout do poll() para checar 'rodando' periodicamente */
#define INTERVALO_RETRY_MS  500  /* tempo de espera entre tentativas de reconexao */
#define BITS_POR_LONG       ((int)(sizeof(unsigned long) * CHAR_BIT)) /* bits por palavra no bitmask */
#define QTD_LONGS_TECLAS    ((KEY_CNT + BITS_POR_LONG - 1) / BITS_POR_LONG) /* tamanho do array para EVIOCGKEY */

/* Identifica a natureza do periférico gerenciado pela estrutura */
enum tipo_dispositivo {
    DISPOSITIVO_TECLADO,
    DISPOSITIVO_MOUSE
};

/* Contexto de controle e estado de execucao de cada periférico evdev */
struct dispositivo {
    const char *nome;           /* rotulo legivel para logs ("teclado" / "mouse") */
    char *caminho;              /* caminho no filesystem (ex: /dev/input/by-id/...) */
    enum tipo_dispositivo tipo; /* categoria do dispositivo */
    int fd;                     /* descritor de arquivo; thread assume a propriedade depois de iniciar */
    int thread_criada;          /* flag indicando se a pthread deste dispositivo esta ativa */
    int timestamp_monotonic;    /* 1 se EVIOCSCLOCKID foi configurado para CLOCK_MONOTONIC */
    pthread_t thread;           /* identificador da thread leitora */
};

/* Instancias estaticas dos dois dispositivos suportados */
static struct dispositivo teclado = {
    .nome = "teclado", .tipo = DISPOSITIVO_TECLADO, .fd = -1
};
static struct dispositivo mouse = {
    .nome = "mouse", .tipo = DISPOSITIVO_MOUSE, .fd = -1
};

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER; /* protege todo o estado compartilhado abaixo */
static int rodando = 0;                                   /* flag global de controle do loop das threads */

/* Evento mais antigo ainda nao consumido pelo medidor de desempenho. */
static long long ev_pendente_us = 0; /* timestamp em microssegundos (CLOCK_MONOTONIC) */
static int ev_pendente_tipo = -1;    /* qual dispositivo gerou o evento pendente */

/* Estados separados evitam que a desconexao de um dispositivo apague o
 * estado do outro. */
static unsigned char teclas[KEY_CNT];       /* mapa de teclas pressionadas (1 = ativa, 0 = solta) */
static unsigned char botoes_mouse[KEY_CNT]; /* mapa de botoes do mouse (1 = ativo, 0 = solto) */
static int pos_x = LARGURA / 2;             /* coordenada X acumulada da mira (inicia no centro) */
static int pos_y = ALTURA / 2;              /* coordenada Y acumulada da mira (inicia no centro) */

/* Restringe 'valor' ao intervalo fechado [minimo, maximo]. */
static int limita(int valor, int minimo, int maximo)
{
    return valor < minimo ? minimo : (valor > maximo ? maximo : valor);
}

/* Consulta de forma thread-safe se o subsistema de entrada continua ativo. */
static int ainda_rodando(void)
{
    int resultado;
    pthread_mutex_lock(&trava);
    resultado = rodando;
    pthread_mutex_unlock(&trava);
    return resultado;
}

/* Zera o vetor de teclas/botoes de um dispositivo desconectado para evitar teclas "presas". */
static void limpar_estado(enum tipo_dispositivo tipo)
{
    pthread_mutex_lock(&trava);
    if (tipo == DISPOSITIVO_TECLADO)
        memset(teclas, 0, sizeof teclas);
    else
        memset(botoes_mouse, 0, sizeof botoes_mouse);
    if (ev_pendente_tipo == (int)tipo) {
        ev_pendente_us = 0;
        ev_pendente_tipo = -1;
    }
    pthread_mutex_unlock(&trava);
}

/* Abre o no evdev em modo nao-bloqueante, configura o clock monotonico e tenta acesso exclusivo. */
static int abrir_dispositivo(struct dispositivo *dev)
{
    int fd = open(dev->caminho, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    clockid_t clock_id = CLOCK_MONOTONIC;
    if (fd < 0) {
        return -1;
    }

    /* Alinha os timestamps do kernel ao relogio usado por perf.c. Se o ioctl
     * nao existir/falhar, o dispositivo continua funcionando, mas seus eventos
     * nao entram na estatistica de latencia (evita subtrair relogios diferentes). */
    dev->timestamp_monotonic = (ioctl(fd, EVIOCSCLOCKID, &clock_id) == 0);
    if (!dev->timestamp_monotonic) {
        fprintf(stderr,
                "entrada: aviso: EVIOCSCLOCKID falhou para %s (%s): %s; "
                "latencia desse dispositivo nao sera medida.\n",
                dev->nome, dev->caminho, strerror(errno));
    }

    /* EVIOCGRAB e opcional para o funcionamento do jogo. Se falhar, avisamos
     * que o dispositivo pode continuar entregando eventos a outros processos. */
    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        fprintf(stderr,
                "entrada: aviso: EVIOCGRAB falhou para %s (%s): %s; "
                "seguindo sem exclusividade.\n",
                dev->nome, dev->caminho, strerror(errno));
    }

    return fd;
}

/* Reconstroi o estado das teclas/botoes depois de SYN_DROPPED ou reconexao. */
static void sincronizar_estado(int fd, enum tipo_dispositivo tipo)
{
    unsigned long bits[QTD_LONGS_TECLAS];
    unsigned char *destino;
    int i;

    memset(bits, 0, sizeof bits);
    if (ioctl(fd, EVIOCGKEY(sizeof bits), bits) < 0) {
        limpar_estado(tipo);
        fprintf(stderr, "entrada: nao foi possivel consultar estado atual das teclas: %s\n",
                strerror(errno));
        return;
    }

    pthread_mutex_lock(&trava);
    destino = (tipo == DISPOSITIVO_TECLADO) ? teclas : botoes_mouse;
    for (i = 0; i < KEY_CNT; ++i) {
        destino[i] = (unsigned char)((bits[i / BITS_POR_LONG] >>
                                     (i % BITS_POR_LONG)) & 1UL);
    }
    pthread_mutex_unlock(&trava);
}

/* Aguarda e le um unico evento do descritor evdev.
 * Retorno: 1 = evento lido; 0 = timeout/sem evento; -1 = dispositivo caiu.
 * O fd e nao bloqueante. EINTR e EAGAIN nao derrubam a thread.
 */
static int proximo_evento(int fd, struct input_event *ev)
{
    struct pollfd pfd;
    int pronto;

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    do {
        pronto = poll(&pfd, 1, TEMPO_POLL_MS);
    } while (pronto < 0 && errno == EINTR && ainda_rodando());

    if (pronto == 0)
        return 0;
    if (pronto < 0)
        return -1;
    if (pfd.revents & POLLNVAL)
        return -1;

    if (pfd.revents & POLLIN) {
        ssize_t lidos;
        do {
            lidos = read(fd, ev, sizeof *ev);
        } while (lidos < 0 && errno == EINTR && ainda_rodando());

        if (lidos == (ssize_t)sizeof *ev)
            return 1;
        if (lidos < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0;
        if (lidos < 0 && errno == EINTR)
            return 0;
        return -1;
    }

    if (pfd.revents & (POLLERR | POLLHUP))
        return -1;

    return 0;
}

/* Pausa a thread em fatias de 100 ms antes de tentar reabrir um dispositivo desconectado. */
static void esperar_reconexao(void)
{
    const struct timespec intervalo = { .tv_sec = 0, .tv_nsec = 100000000L };
    int fatia;

    for (fatia = 0; fatia < INTERVALO_RETRY_MS / 100; ++fatia) {
        if (!ainda_rodando())
            break;
        (void)nanosleep(&intervalo, NULL);
    }
}

/* Registra o timestamp do evento para calculo de latencia input-to-photon em perf.c. */
static void marcar_evento(const struct dispositivo *dev,
                          const struct input_event *ev)
{
    long long timestamp;

    if (!dev->timestamp_monotonic || ev->time.tv_sec == 0)
        return;

    timestamp = (long long)ev->time.tv_sec * 1000000LL + ev->time.tv_usec;
    /* As duas threads podem chegar fora de ordem; conserva o menor timestamp. */
    if (ev_pendente_us == 0 || timestamp < ev_pendente_us) {
        ev_pendente_us = timestamp;
        ev_pendente_tipo = (int)dev->tipo;
    }
}

/* Atualiza o mapa de teclas/botoes ou acumula o deslocamento relativo do mouse. */
static void processar_evento(const struct dispositivo *dev,
                             const struct input_event *ev)
{
    if (ev->type == EV_KEY && ev->code < KEY_CNT) {
        pthread_mutex_lock(&trava);
        marcar_evento(dev, ev); /* press, release e repeticao */
        if (dev->tipo == DISPOSITIVO_TECLADO)
            teclas[ev->code] = (unsigned char)(ev->value != 0);
        else
            botoes_mouse[ev->code] = (unsigned char)(ev->value != 0);
        pthread_mutex_unlock(&trava);
    } else if (dev->tipo == DISPOSITIVO_MOUSE && ev->type == EV_REL &&
               (ev->code == REL_X || ev->code == REL_Y)) {
        pthread_mutex_lock(&trava);
        marcar_evento(dev, ev);
        if (ev->code == REL_X)
            pos_x = limita(pos_x + ev->value, 0, LARGURA - 1);
        else if (ev->code == REL_Y)
            pos_y = limita(pos_y + ev->value, 0, ALTURA - 1);
        pthread_mutex_unlock(&trava);
    }
}

/* Rotina principal da thread de cada periférico: le eventos, trata perdas de buffer e reconecta. */
static void *thread_dispositivo(void *arg)
{
    struct dispositivo *dev = (struct dispositivo *)arg;
    struct input_event ev;
    int fd = dev->fd;
    int desconectou = 0;
    int descartando_ate_syn_report = 0;

    /* A partir daqui, somente esta thread fecha/atualiza o fd do dispositivo. */
    dev->fd = -1;

    while (ainda_rodando()) {
        int resultado;

        /* Se o dispositivo caiu, tenta reabri-lo periodicamente */
        if (fd < 0) {
            esperar_reconexao();
            if (!ainda_rodando())
                break;

            fd = abrir_dispositivo(dev);
            if (fd < 0) {
                continue;
            }

            /* O descritor pode ter sido reenumerado; recupera teclas que ja
             * estavam pressionadas no momento da reconexao. */
            sincronizar_estado(fd, dev->tipo);
            descartando_ate_syn_report = 0;
            if (desconectou) {
                fprintf(stderr, "entrada: %s reconectado em %s.\n",
                        dev->nome, dev->caminho);
                desconectou = 0;
            }
        }

        resultado = proximo_evento(fd, &ev);
        if (resultado == 0)
            continue;
        if (resultado < 0) {
            fprintf(stderr, "entrada: %s desconectado/indisponivel em %s; "
                            "tentando reconectar.\n",
                    dev->nome, dev->caminho);
            close(fd);                 /* tambem libera EVIOCGRAB, se obtido */
            fd = -1;
            desconectou = 1;
            descartando_ate_syn_report = 0;
            limpar_estado(dev->tipo);
            continue;
        }

        /* Se o buffer do kernel transbordou (SYN_DROPPED), ignora eventos parciais ate SYN_REPORT */
        if (ev.type == EV_SYN && ev.code == SYN_DROPPED) {
            fprintf(stderr,
                    "entrada: eventos perdidos em %s; aguardando SYN_REPORT para ressincronizar.\n",
                    dev->nome);
            descartando_ate_syn_report = 1;
            continue;
        }
        if (descartando_ate_syn_report) {
            if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                sincronizar_estado(fd, dev->tipo);
                descartando_ate_syn_report = 0;
            }
            continue;
        }
        processar_evento(dev, &ev);
    }

    if (fd >= 0)
        close(fd);
    limpar_estado(dev->tipo);
    return NULL;
}

/* Duplica o caminho informado e realiza a abertura inicial do descritor antes de criar a thread. */
static int preparar_dispositivo(struct dispositivo *dev, const char *caminho)
{
    if (caminho == NULL || caminho[0] == '\0')
        return 0; /* NULL/ausente desativa esse dispositivo. */

    dev->caminho = strdup(caminho);
    if (dev->caminho == NULL) {
        fprintf(stderr, "entrada: sem memoria para guardar caminho de %s.\n", dev->nome);
        return -1;
    }

    dev->fd = abrir_dispositivo(dev);
    if (dev->fd < 0) {
        fprintf(stderr, "entrada: nao foi possivel abrir %s (%s): %s\n",
                dev->nome, dev->caminho, strerror(errno));
        return -1;
    }

    return 0;
}

/* Fecha o descritor residual (se houver) e libera a string de caminho alocada. */
static void liberar_dispositivo(struct dispositivo *dev)
{
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
    free(dev->caminho);
    dev->caminho = NULL;
    dev->thread_criada = 0;
}

/* Inicializa o subsistema de entrada, abrindo os dispositivos e disparando as threads leitoras. */
int entrada_iniciar(const char *caminho_teclado, const char *caminho_mouse)
{
    if (teclado.thread_criada || mouse.thread_criada || rodando) {
        fprintf(stderr, "entrada: inicializacao solicitada mais de uma vez.\n");
        return -1;
    }
    if ((!caminho_teclado || !caminho_teclado[0]) &&
        (!caminho_mouse || !caminho_mouse[0])) {
        fprintf(stderr, "entrada: informe ao menos um dispositivo.\n");
        return -1;
    }

    limpar_estado(DISPOSITIVO_TECLADO);
    limpar_estado(DISPOSITIVO_MOUSE);

    if (preparar_dispositivo(&teclado, caminho_teclado) < 0)
        goto falha;
    if (preparar_dispositivo(&mouse, caminho_mouse) < 0)
        goto falha;

    pthread_mutex_lock(&trava);
    rodando = 1;
    pthread_mutex_unlock(&trava);

    if (teclado.fd >= 0) {
        if (pthread_create(&teclado.thread, NULL, thread_dispositivo, &teclado) != 0) {
            fprintf(stderr, "entrada: falhou criar thread do teclado.\n");
            goto falha_threads;
        }
        teclado.thread_criada = 1;
    }

    if (mouse.fd >= 0) {
        if (pthread_create(&mouse.thread, NULL, thread_dispositivo, &mouse) != 0) {
            fprintf(stderr, "entrada: falhou criar thread do mouse.\n");
            goto falha_threads;
        }
        mouse.thread_criada = 1;
    }

    return 0;

falha_threads:
    pthread_mutex_lock(&trava);
    rodando = 0;
    pthread_mutex_unlock(&trava);
    if (teclado.thread_criada) {
        pthread_join(teclado.thread, NULL);
        teclado.thread_criada = 0;
    }
    if (mouse.thread_criada) {
        pthread_join(mouse.thread, NULL);
        mouse.thread_criada = 0;
    }
falha:
    liberar_dispositivo(&teclado);
    liberar_dispositivo(&mouse);
    limpar_estado(DISPOSITIVO_TECLADO);
    limpar_estado(DISPOSITIVO_MOUSE);
    return -1;
}

/* Sinaliza encerramento, aguarda o termino das threads (join) e libera recursos. */
void entrada_parar(void)
{
    pthread_mutex_lock(&trava);
    rodando = 0;
    pthread_mutex_unlock(&trava);

    if (teclado.thread_criada) {
        pthread_join(teclado.thread, NULL);
        teclado.thread_criada = 0;
    }
    if (mouse.thread_criada) {
        pthread_join(mouse.thread, NULL);
        mouse.thread_criada = 0;
    }

    liberar_dispositivo(&teclado);
    liberar_dispositivo(&mouse);
    limpar_estado(DISPOSITIVO_TECLADO);
    limpar_estado(DISPOSITIVO_MOUSE);
}

/* Retorna e consome o timestamp monotonic do evento mais antigo ainda pendente. */
long long entrada_pegar_evento_us(void)
{
    long long valor;
    pthread_mutex_lock(&trava);
    valor = ev_pendente_us;
    ev_pendente_us = 0;
    ev_pendente_tipo = -1;
    pthread_mutex_unlock(&trava);
    return valor;
}

/* Retorna 1 se a tecla correspondente ao codigo evdev estiver pressionada, ou 0 caso contrario. */
int tecla_apertada(int codigo)
{
    int valor;
    if (codigo < 0 || codigo >= KEY_CNT)
        return 0;

    pthread_mutex_lock(&trava);
    valor = teclas[codigo];
    pthread_mutex_unlock(&trava);
    return valor;
}

/* Retorna a coordenada X atual do cursor do mouse (0..319). */
int mouse_x(void)
{
    int valor;
    pthread_mutex_lock(&trava);
    valor = pos_x;
    pthread_mutex_unlock(&trava);
    return valor;
}

/* Retorna a coordenada Y atual do cursor do mouse (0..239). */
int mouse_y(void)
{
    int valor;
    pthread_mutex_lock(&trava);
    valor = pos_y;
    pthread_mutex_unlock(&trava);
    return valor;
}

/* Retorna 1 se o botao do mouse informado (ex: BTN_LEFT) estiver pressionado, ou 0 caso contrario. */
int mouse_botao(int botao)
{
    int valor;
    if (botao < 0 || botao >= KEY_CNT)
        return 0;

    pthread_mutex_lock(&trava);
    valor = botoes_mouse[botao];
    pthread_mutex_unlock(&trava);
    return valor;
}