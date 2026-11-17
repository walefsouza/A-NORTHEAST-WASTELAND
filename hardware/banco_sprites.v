// =============================================================================
// banco_sprites.v - BANCO DE REGISTRADORES DOS SPRITES
// =============================================================================
// Guarda os ATRIBUTOS dos 64 sprites. Os PIXELS dos desenhos ficam na RAM de
// padroes (ram_sprites.v / padroes_sprites.mif); aqui ficam apenas os atributos.
//
// Organizacao: 64 registradores x 32 bits (um registrador por sprite, id 0..63).
//
// Mapeamento de bits de cada registrador:
//
//   bit  31      | 30..25  | 24 | 23 | 22..14 | 13..6 | 5..0
//   -------------+---------+----+----+--------+-------+------------
//   ATIVO        | PADRAO  | H  | V  |   X    |   Y   | PRIORIDADE
//   (1=desenha)  | (0..63) |flip|flip| 0..511 | 0..255| 0..63
//
// ID, padrao e prioridade comportam 64 valores independentes (0..63).
//
// Duas formas de LEITURA, ao mesmo tempo:
//   1) read_addr / read_data (6 / 32 bits): leitura de um sprite pela UC.
//   2) read_data_all (2048 bits): os 64 registradores lado a lado. O sprite i
//      fica em read_data_all[i*32 +: 32]. Usada pelo motor, se necessario.
// =============================================================================

module banco_sprites (
    input clk,                       // clock do sistema (25 MHz)
    input reset,                     // reset sincrono: zera todos os sprites
    input we,                         // write enable: pulso vindo da UC
    input [5:0] write_addr,           // endereco do sprite escrito (0..63)
    input [31:0] write_data,          // atributos do sprite
    input [5:0] read_addr,            // endereco do sprite lido (0..63)
    output [31:0] read_data,          // atributos do sprite selecionado
    output [2047:0] read_data_all     // atributos dos 64 sprites
);

    // Banco com 64 posicoes (IDs 0..63), cada uma com 32 bits. A UC e o motor
    // interpretam os campos conforme o mapa documentado acima.
    reg [31:0] registradores [0:63];
    integer i;

    always @(posedge clk) begin
        // Reset sincrono: todos os sprites ficam inativos.
        if (reset) begin
            for (i = 0; i < 64; i = i + 1)
                registradores[i] <= 32'd0;

        // Escreve em um unico registrador; os demais mantem seus dados.
        end else if (we) begin
            registradores[write_addr] <= write_data;
        end
    end

    // Leitura assincrona de um sprite para o read-modify-write da UC.
    assign read_data = registradores[read_addr];

    // Concatena os 64 registradores: sprite 0 em [31:0], sprite 63 em [2047:2016].
    // A geracao parametrizada evita uma concatenacao manual muito extensa.
    genvar g;
    generate
        for (g = 0; g < 64; g = g + 1) begin : GERAR_BARRAMENTO_SPRITES
            assign read_data_all[g*32 +: 32] = registradores[g];
        end
    endgenerate

endmodule
