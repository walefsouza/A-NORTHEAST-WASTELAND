/* perf_status.h - invariantes observaveis do PIO_STATUS do ANW.
 *
 * Esta verificacao e diagnostica: nao prova ausencia de metastabilidade nem
 * substitui simulacao CDC/STA. Ela detecta saltos impossiveis do contador de
 * quadros entre leituras proximas e FIFO simultaneamente vazia/cheia.
 */
#ifndef PERF_STATUS_H
#define PERF_STATUS_H

/* Mascaras e deslocamentos dos campos do registrador MMIO PIO_STATUS (32 bits) */
#define PERF_STATUS_FIFO_VAZIA   (1u << 0) /* bit 0: indica que a FIFO de comandos esta vazia */
#define PERF_STATUS_FIFO_CHEIA   (1u << 1) /* bit 1: indica que a FIFO de comandos esta cheia */
#define PERF_STATUS_QUADRO_SHIFT 8u        /* posicao inicial do contador de quadros (bits [23:8]) */
#define PERF_STATUS_QUADRO_MASK  0xFFFFu   /* mascara para extrair o contador de quadros de 16 bits */

/* Verifica se duas leituras consecutivas e imediatas do STATUS violam invariantes fisicos do hardware.
 * Retorna 1 se detectar leitura rasgada/incoerente (erro de CDC) ou 0 se a transicao for valida. */
static inline int perf_status_amostra_incoerente(unsigned anterior, unsigned atual)
{
    /* Extrai o valor do contador de quadros (0..65535) das duas amostras */
    unsigned quadro_anterior = (anterior >> PERF_STATUS_QUADRO_SHIFT) & PERF_STATUS_QUADRO_MASK;
    unsigned quadro_atual = (atual >> PERF_STATUS_QUADRO_SHIFT) & PERF_STATUS_QUADRO_MASK;

    /* Calcula o avanco de quadros com aritmetica modular de 16 bits */
    unsigned delta = (quadro_atual - quadro_anterior) & PERF_STATUS_QUADRO_MASK;

    /* Invariante 1: a FIFO nao pode estar fisicamente vazia e cheia ao mesmo tempo */
    if ((atual & (PERF_STATUS_FIFO_VAZIA | PERF_STATUS_FIFO_CHEIA)) ==
        (PERF_STATUS_FIFO_VAZIA | PERF_STATUS_FIFO_CHEIA))
        return 1;

    /* Invariante 2: em leituras separadas por microssegundos, o quadro de video (60 Hz)
     * so pode permanecer igual (delta == 0) ou avancar no maximo 1 unidade (delta == 1).
     * Delta modular: 0xFFFF -> 0x0000 representa um unico quadro e e valido. */
    return delta > 1u;
}

#endif