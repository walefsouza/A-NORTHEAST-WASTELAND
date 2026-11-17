// main.c - ponto de entrada: abre o driver, inicia entrada e roda o loop de quadros
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
    // caminhos dos dispositivos: variaveis de ambiente (padrao) ou argumentos
    const char *teclado = getenv("ANW_TECLADO");
    const char *mouse   = getenv("ANW_MOUSE");
    int falha_driver = 0;                  // 1 = alguma falha de driver/FPGA (retorno 2)

    if (argc > 3) {
        fprintf(stderr,
                "Uso: %s <caminho_teclado> <caminho_mouse>\n"
                "Ou configure ANW_TECLADO e ANW_MOUSE.\n", argv[0]);
        return 1;
    }
    // argumentos tem prioridade sobre as variaveis de ambiente
    if (argc > 1) teclado = argv[1];
    if (argc > 2) mouse   = argv[2];

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

    // ---- inicializacao ----
    if (abrir() != DRIVER_OK) {            // mapeia /dev/mem (acesso ao coprocessador)
        fprintf(stderr, "Erro: nao foi possivel abrir/mapear /dev/mem para o coprocessador.\n");
        return 1;
    }
    if (entrada_iniciar(teclado, mouse) != 0) {
        fprintf(stderr, "Erro: falha ao inicializar teclado/mouse.\n");
        fechar();
        return 1;
    }

    perf_iniciar();
    if (getenv("ANW_BENCH")) perf_bench_comandos();   // benchmark opcional dos comandos

    gfx_limpar_erro();
    jogo_novo();
    estado_ir_para(&estado_menu);          // primeiro estado: menu

    // ---- loop principal: 1 iteracao = 1 quadro (sincronizado com o hardware) ----
    while (!tecla_apertada(KEY_ESC) && !jogo.sair) {
        int quadro = esperar_quadro();     // bloqueia ate o proximo quadro (vsync)
        if (quadro < 0) {
            fprintf(stderr, "Erro: timeout esperando o proximo quadro (%d).\n", quadro);
            falha_driver = 1;
            break;
        }

        perf_inicio_quadro(quadro);
        teclas_quadro();                   // atualiza o estado das teclas deste quadro
        perf_antes_logica();
        estado_rodar_quadro();             // logica + comandos graficos do estado atual
        perf_depois_logica();

        // erro acumulado pela camada grafica durante o quadro
        int erro_gfx = gfx_ultimo_erro();
        if (erro_gfx != DRIVER_OK) {
            fprintf(stderr, "Erro na camada grafica/driver (codigo %d); quadro interrompido.\n", erro_gfx);
            falha_driver = 1;
            break;
        }

        // espera a FIFO drenar e a unidade de controle ficar livre
        int resultado = esperar_fim();
        perf_fim_quadro();
        int erros_fpga = erro_inst();      // flags de erro do registrador de status

        // Registrar os flags antes de limpa-los no registrador de status.
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

    // ---- encerramento (sempre executado, mesmo apos erro) ----
    perf_fim();
    entrada_parar();
    fechar();
    return falha_driver ? 2 : 0;          // 0 ok, 2 falha de driver (1 = erro de uso)
}
