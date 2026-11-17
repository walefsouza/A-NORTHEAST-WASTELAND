module fifo_instrucoes (
    input wire clock,
    input wire reset,
    input wire hab_escrita,                // pede a escrita de uma instrucao na fila
    input wire [31:0] instrucao_entrada,   // instrucao a escrever
    input wire hab_leitura,                // pop: descarta a instrucao que ta na saida
    output wire [31:0] instrucao_saida,    // instrucao mais antiga da fila (a proxima a sair)
    output wire cheia,
    output wire quase_cheia,               // 31 elementos
    output wire vazia,
    output wire quase_vazia                // 1 elemento
);

    // fila circular de 32 posicoes de 32 bits
    reg [31:0] memoria_fila [0:31];

    reg [4:0] ponteiro_escrita = 0;     // proxima posicao livre pra escrever
    reg [4:0] ponteiro_leitura = 0;     // posicao da instrucao mais antiga
    reg [5:0] contador_elementos = 0;   // 6 bits porque precisa contar ate 32

    // flags tiradas direto do contador
    assign cheia = (contador_elementos == 32);
    assign quase_cheia = (contador_elementos == 31);
    assign vazia = (contador_elementos == 0);
    assign quase_vazia = (contador_elementos == 1);

    // leitura combinacional: a saida ja mostra a instrucao mais antiga,
    // o hab_leitura so avanca o ponteiro
    assign instrucao_saida = memoria_fila[ponteiro_leitura];

    always @(posedge clock) begin
        if (reset) begin
            // so zera ponteiros e contador, a memoria nao precisa ser limpa
            ponteiro_leitura <= 0;
            ponteiro_escrita <= 0;
            contador_elementos <= 0;
        end else begin
            // escrita e leitura no mesmo ciclo se compensam, o contador fica parado
            // escrita com a fila vazia conta mesmo com hab_leitura em 1 (nao tem o que ler)
            // leitura com a fila cheia conta mesmo com hab_escrita em 1 (a escrita e ignorada)
            if (hab_escrita && !cheia && (!hab_leitura || vazia))
                contador_elementos <= contador_elementos + 1;
            else if (hab_leitura && !vazia && (!hab_escrita || cheia))
                contador_elementos <= contador_elementos - 1;

            // escreve e avanca o ponteiro, volta pra 0 depois da posicao 31
            if (hab_escrita && !cheia) begin
                memoria_fila[ponteiro_escrita] <= instrucao_entrada;

                if (ponteiro_escrita == 31)
                    ponteiro_escrita <= 0;
                else
                    ponteiro_escrita <= ponteiro_escrita + 1;
            end

            // leitura so avanca o ponteiro, o dado ja ta na saida
            if (hab_leitura && !vazia) begin
                if (ponteiro_leitura == 31)
                    ponteiro_leitura <= 0;
                else
                    ponteiro_leitura <= ponteiro_leitura + 1;
            end
        end
    end

endmodule