/*
 * main.c
 *
 * Ponto de entrada do jogo. Este arquivo coordena a inicializacao dos
 * dispositivos, do driver de hardware, da entrada do usuario e do sistema
 * de medicao de desempenho.
 *
 * O laco principal executa um quadro por iteracao:
 *   1. aguarda o proximo quadro do hardware;
 *   2. processa teclado/mouse e atualiza a logica do estado atual;
 *   3. espera os comandos enviados ao coprocessador terminarem;
 *   4. registra as metricas daquele quadro.
 *
 * A implementacao das regras do jogo e das operacoes de hardware fica nos
 * respectivos modulos; aqui apenas organizamos a ordem das chamadas.
 */
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "estado.h"
#include "entrada.h"
#include "teclas.h"
#include "jogo.h"
#include "driver.h"
#include "gfx.h"
#include "perf.h"

int main(int argc, char **argv) {
    /* Os dispositivos podem ser informados por variaveis de ambiente.
     * Argumentos da linha de comando, quando presentes, tem prioridade. */
    const char *teclado = getenv("ANW_TECLADO");
    const char *mouse   = getenv("ANW_MOUSE");
    int falha_driver = 0;

    /* Sao aceitos no maximo dois argumentos: caminho do teclado e do mouse.
     * O argc inclui tambem o nome do proprio executavel. */
    if (argc > 3) {
        fprintf(stderr,
                "Uso: %s <caminho_teclado> <caminho_mouse>\n"
                "Ou configure ANW_TECLADO e ANW_MOUSE.\n", argv[0]);
        return 1;
    }
    /* Permite executar como: ./jogo <teclado> <mouse>.
     * Se os argumentos nao forem passados, mantem os valores do ambiente. */
    if (argc > 1) teclado = argv[1];
    if (argc > 2) mouse   = argv[2];

    /* Antes de abrir o hardware, garante que os dois dispositivos de entrada
     * foram configurados. Caminhos comuns no Linux ficam em /dev/input/. */
    if (teclado == NULL || teclado[0] == '\0' ||
        mouse == NULL || mouse[0] == '\0') {
        fprintf(stderr,
                "Dispositivos de entrada nao configurados.\n"
                "Uso: %s <caminho_teclado> <caminho_mouse>\n"
                "Exemplo: %s /dev/input/by-id/TECLADO-event-kbd "
                "/dev/input/by-id/MOUSE-event-mouse\n"
                "Alternativa: exporte ANW_TECLADO e ANW_MOUSE.\n",
                argv[0], argv[0]);
        return 1;
    }

    /* Inicializa o acesso ao coprocessador. O driver configura/mapeia os
     * registradores de hardware (incluindo o acesso via /dev/mem). */
    if (abrir() != DRIVER_OK) {
        fprintf(stderr, "Erro: nao foi possivel abrir/mapear /dev/mem para o coprocessador.\n");
        return 1;
    }
    /* Inicia a leitura dos dispositivos de entrada. Se falhar, fecha o
     * driver ja aberto para nao deixar o acesso ao hardware pendente. */
    if (entrada_iniciar(teclado, mouse) != 0) {
        fprintf(stderr, "Erro: falha ao inicializar teclado/mouse.\n");
        fechar();
        return 1;
    }

    /* Inicializa os acumuladores de desempenho e, se solicitado por
     * ANW_BENCH, executa o benchmark de envio/execucao de comandos. */
    perf_iniciar();
    if (getenv("ANW_BENCH")) perf_bench_comandos();

    /* Prepara o estado grafico e inicia o jogo diretamente no menu. */
    gfx_limpar_erro();
    jogo_novo();
    estado_ir_para(&estado_menu);

    /* Laco principal: continua enquanto o usuario nao pedir para sair e
     * enquanto a logica do jogo nao sinalizar encerramento.
     *
     * Os hooks perf_* delimitam as etapas do quadro. A ordem deles e
     * importante para que espera, logica e dreno sejam medidos separadamente. */
    while (!tecla_apertada(KEY_ESC) && !jogo.sair) {
        /* Mede o intervalo gasto aguardando o proximo quadro do hardware. */
        perf_antes_espera_quadro();
        int quadro = esperar_quadro();
        if (quadro < 0) {
            fprintf(stderr, "Erro: timeout esperando o proximo quadro (%d).\n", quadro);
            falha_driver = 1;
            break;
        }

        /* Registra o quadro recebido e fecha a medicao da espera anterior. */
        perf_inicio_quadro(quadro);
        /* Atualiza o estado das teclas/mouse para este quadro antes da logica. */
        teclas_quadro();
        /* Marca o inicio da logica e captura o instante do evento de entrada
         * mais antigo que ainda aguarda processamento. */
        perf_antes_logica();
        /* Executa a atualizacao do estado atual (menu, partida, pausa etc.). */
        estado_rodar_quadro();
        /* Marca o fim da logica; o tempo ate esperar_fim() sera o dreno. */
        perf_depois_logica();

        /* Erros graficos indicam falha na comunicacao/execucao dos comandos
         * enviados ao hardware. Interrompe o laco para evitar novas operacoes. */
        int erro_gfx = gfx_ultimo_erro();
        if (erro_gfx != DRIVER_OK) {
            fprintf(stderr, "Erro na camada grafica/driver (codigo %d); quadro interrompido.\n", erro_gfx);
            falha_driver = 1;
            break;
        }

        /* Espera a FIFO esvaziar e a unidade de controle ficar livre.
         * O tempo desta chamada e contabilizado como dreno de comandos. */
        int resultado = esperar_fim();
        perf_fim_quadro();
        /* Consulta os flags de erro da FPGA depois de finalizar o quadro.
         * Os flags sao copiados antes de limpar o registrador de status. */
        int erros_fpga = erro_inst();

        /* Registrar os flags antes de limpa-los no registrador de status. */
        if (erros_fpga != 0) {
            if (erros_fpga & 0x1)
                fprintf(stderr, "Erro FPGA: opcode invalido.\n");
            if (erros_fpga & 0x2)
                fprintf(stderr, "Erro FPGA: overflow da FIFO.\n");
            limpar_erro();
            falha_driver = 1;
            break;
        }

        if (resultado == DRIVER_TIMEOUT_FIM) {
            fprintf(stderr, "Erro: timeout esperando a FIFO drenar e a UC ficar livre.\n");
            falha_driver = 1;
            break;
        }
        if (resultado == DRIVER_ERRO_FPGA) {
            fprintf(stderr, "Erro FPGA reportado, mas sem flags identificaveis em erro_inst().\n");
            limpar_erro();
            falha_driver = 1;
            break;
        }
        if (resultado != DRIVER_OK) {
            fprintf(stderr, "Erro inesperado ao esperar conclusao dos comandos: %d.\n", resultado);
            falha_driver = 1;
            break;
        }
    }

    /* Finaliza a medicao (imprime/grava o resumo quando aplicavel), para a
     * thread de entrada e fecha o acesso ao hardware, mesmo apos erro. */
    perf_fim();
    /* Encerra a captura de teclado/mouse antes de liberar o driver. */
    entrada_parar();
    fechar();
    return falha_driver ? 2 : 0;
}
