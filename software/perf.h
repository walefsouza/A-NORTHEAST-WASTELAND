/*
 * perf.h
 *
 * Interface publica do modulo de instrumentacao de desempenho. O arquivo
 * declara os hooks chamados pelo laco principal e documenta as opcoes
 * configuraveis por variaveis de ambiente.
 *
 * O modulo mede tempo de quadro, logica, espera/dreno, entrada, uso de CPU e
 * consistencia de leituras do registrador STATUS.
 *
 * O painel se redesenha SEMPRE no mesmo lugar (sobe o cursor com ANSI), entao nao
 * acumula linhas: parece um monitor rodando junto com o jogo.
 *
 * Variaveis de ambiente (todas opcionais):
 *   Os nomes abaixo sao lidos por perf_iniciar(). Valores nao definidos usam
 *   o comportamento padrao descrito em cada item.
 *   ANW_PERF=0            desliga o painel (o resumo final continua)
 *   ANW_PERF_LOG=arq.csv  grava uma linha por atualizacao do painel (evidencia p/ relatorio)
 *   ANW_BENCH=1           roda, antes do jogo, o benchmark de latencia ACK x DONE (1/10/32 cmds)
 *   ANW_PERF_JANELA=30    limita o resumo a uma janela de 30 s, iniciada no
 *                         primeiro quadro identificado como PARTIDA. Sem essa
 *                         variavel, o resumo acumula desde a inicializacao ate
 *                         o encerramento do programa.
 *   ANW_PERF_RESUMO=arq   alem do terminal, grava o resumo final nesse arquivo
 *
 * Observacao: nesta implementacao, ANW_PERF_JANELA controla a janela de coleta
 * do resumo; atingir esse tempo emite o resumo, mas nao encerra por si so o
 * laco principal do jogo.
 *
 * Ordem dos hooks no laco principal:
 *   Os hooks devem permanecer nesta ordem para que cada intervalo seja medido
 *   no trecho correto do quadro.
 *   perf_antes_espera_quadro(); esperar_quadro(); perf_inicio_quadro(q);
 *   perf_antes_logica(); logica; perf_depois_logica(); esperar_fim(); perf_fim_quadro();
 */
#ifndef PERF_H
#define PERF_H

/* Inicializa variaveis, histogramas, CSV e parametros de medicao.
 * Deve ser chamada depois de abrir() e entrada_iniciar(). */
void perf_iniciar(void);
/* Executa o benchmark opcional de latencia ACK/DONE. Deve ser chamado
 * antes de o laco principal comecar a enviar comandos do jogo. */
void perf_bench_comandos(void);

/* Marca o instante imediatamente anterior a esperar_quadro(). */
void perf_antes_espera_quadro(void);
/* Recebe o contador de quadros do hardware logo apos esperar_quadro()
 * retornar com sucesso. O contador do hardware tem 16 bits. */
void perf_inicio_quadro(int quadro_hw);
/* Marca o inicio da logica e captura o timestamp do evento de entrada. */
void perf_antes_logica(void);
/* Marca o fim da logica do estado atual. */
void perf_depois_logica(void);
/* Fecha a medicao do quadro, atualiza as estatisticas e redesenha o painel.
 * Deve ser chamada logo depois de esperar_fim(). */
void perf_fim_quadro(void);

/* Emite o resumo final, quando aplicavel, e fecha o arquivo CSV. */
void perf_fim(void);
#endif
