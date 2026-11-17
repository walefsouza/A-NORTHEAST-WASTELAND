/* perf.h - painel de desempenho ao vivo no terminal (metricas do PBL03, secao 4.1)
 *
 * O painel se redesenha SEMPRE no mesmo lugar (sobe o cursor com ANSI), entao nao
 * acumula linhas: parece um monitor rodando junto com o jogo.
 *
 * Variaveis de ambiente (opcionais):
 *   ANW_PERF=0            desliga o painel (o resumo final continua)
 *   ANW_PERF_LOG=arq.csv  grava uma linha por atualizacao do painel (evidencia p/ relatorio)
 *   ANW_BENCH=1           roda, antes do jogo, o benchmark de latencia ACK x DONE (1/10/32 cmds)
 *
 * Ordem dos hooks no laco principal:
 *   perf_antes_espera_quadro(); esperar_quadro(); perf_inicio_quadro(q);
 *   perf_antes_logica(); logica; perf_depois_logica(); esperar_fim(); perf_fim_quadro();
 */
#ifndef PERF_H
#define PERF_H

/* Inicializa acumuladores, le variaveis de ambiente e abre o CSV opcional.
 * Chamar depois de abrir() e entrada_iniciar(). */
void perf_iniciar(void);

/* Executa bateria de testes medindo latencia ACK vs DONE por lote (1, 10 e 32 comandos).
 * Chamar antes do 1o quadro quando ANW_BENCH=1. */
void perf_bench_comandos(void);

/* Marca o timestamp imediatamente ANTES de esperar_quadro() para medir o tempo de espera de VBlank. */
void perf_antes_espera_quadro(void);

/* Registra o inicio do quadro logico logo apos esperar_quadro() retornar ok,
 * calculando o periodo total e checando saltos no contador de 16 bits ('quadro_hw'). */
void perf_inicio_quadro(int quadro_hw);

/* Coleta o timestamp do evento evdev pendente mais antigo e marca o inicio de estado_rodar_quadro(). */
void perf_antes_logica(void);

/* Marca o fim de estado_rodar_quadro() (antes de esperar_fim()), isolando o tempo de CPU util/logica. */
void perf_depois_logica(void);

/* Chamado logo apos esperar_fim(): calcula o tempo de dreno da FIFO, latencia input-to-photon,
 * contadores da FPGA (done_cnt e coerencia de STATUS) e atualiza o painel/CSV a cada 0.5 s. */
void perf_fim_quadro(void);

/* Imprime o relatorio estatistico consolidado (medias, maximos e percentis por cena) e fecha o CSV. */
void perf_fim(void);

#endif