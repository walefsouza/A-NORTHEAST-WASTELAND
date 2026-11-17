module registrador_status (
    input  wire        clk,
    input  wire        reset,
    input  wire        fifo_vazia,
    input  wire        fifo_cheia,
    input  wire        instr_accepted_pulse, // push efetivamente aceito pela FIFO
    input  wire        uc_ocupada,
    input  wire        esperando_vblank,
    input  wire        opcode_invalido,     // pulso de 1 ciclo vindo da unidade de controle
    input  wire        limpar_erro,         // limpa erro_opcode e overflow (vem do HPS ou de um botao)
    input  wire        em_vblank,
    input  wire        front_sel,           // pagina do frame buffer que ta sendo exibida
    input  wire        vblank_start_pulse,  // incrementa o contador de quadros
    input  wire        overflow_pulse,      // tentativa de escrita com a fila cheia
    input  wire        instr_done_pulse,    // uma instrucao foi concluida
    output reg  [31:0] status
);

    // erro_opcode e overflow ficam em 1 ate alguem pedir pra limpar
    reg        erro_opcode;
    reg        overflow;
    reg        ack_toggle = 1'b0;     // alterna a cada instrucao aceita pela FIFO
    reg [6:0]  done_cnt;           // instrucoes concluidas, volta a 0 depois de 127
    reg [15:0] contador_quadros;   // quadros desde o reset, volta a 0 depois de 65535

    always @(posedge clk) begin
        if (reset) begin
            erro_opcode      <= 1'b0;
            overflow         <= 1'b0;
            ack_toggle       <= 1'b0;
            done_cnt         <= 7'd0;
            contador_quadros <= 16'd0;
            status           <= 32'd0;
        end else begin
            // erro novo tem prioridade sobre o pedido de limpar
            if (opcode_invalido)      erro_opcode <= 1'b1;
            else if (limpar_erro)     erro_opcode <= 1'b0;

            if (overflow_pulse)       overflow <= 1'b1;
            else if (limpar_erro)     overflow <= 1'b0;

            if (instr_accepted_pulse) ack_toggle <= ~ack_toggle;
            if (instr_done_pulse)     done_cnt <= done_cnt + 7'd1;

            if (vblank_start_pulse)
                contador_quadros <= contador_quadros + 16'd1;

            // layout do status (do bit 31 pro 0):
            // [31:25] done_cnt | [24] overflow | [23:8] contador_quadros | [7] front_sel
            // [6] em_vblank | [5] erro_opcode | [4] esperando_vblank | [3] uc_ocupada
            // [2] ack_toggle | [1] fifo_cheia | [0] fifo_vazia
            // O ACK alterna somente quando o push e aceito pela FIFO.
            // O status e registrado; usar ack_toggle ^ instr_accepted_pulse
            // faz o bit 2 refletir a alternancia neste mesmo ciclo.
            status <= {done_cnt,
                       overflow | overflow_pulse,
                       contador_quadros,
                       front_sel,
                       em_vblank,
                       erro_opcode | opcode_invalido,
                       esperando_vblank,
                       uc_ocupada,
                       (ack_toggle ^ instr_accepted_pulse),
                       fifo_cheia,
                       fifo_vazia};
        end
    end

endmodule