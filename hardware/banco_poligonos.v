// =============================================================================
// banco_poligonos.v - BANCO DE REGISTRADORES DOS POLIGONOS
// =============================================================================
// Guarda o estado dos 8 poligonos (ids 0..7) em DOIS bancos separados:
//
//   1) ATRIBUTOS: 8 registradores x 32 bits
//
//      bit  31    | 30..28 | 27..20 | 19 | 18 | 17..9  | 8..1  | 0
//      -----------+--------+--------+----+----+--------+-------+------
//      ATIVO      | FORMA  |  COR   | H  | V  |   X    |   Y   | livre
//                 | 0..4   | 0..255 |flip|flip| 0..511 | 0..255|
//
//      FORMA: 0 quadrado | 1 retangulo | 2 triangulo | 3 triangulo equilatero
//             4 triangulo com vertice deslocado (ver motor_poligonos.v)
//      COR  : indice de cor RRRGGGBB (0 = transparente)
//
//   2) TAMANHOS: 8 registradores x 17 bits
//
//      bits 16..8 | 7..0
//      -----------+------
//          W      |  H        (largura 0..511, altura 0..255)
//
// POR QUE DOIS BANCOS SEPARADOS?
//   - A instrucao TAMPOL escreve so no banco de tamanhos, e INSPOL escreve so no
//     de atributos. Assim INSPOL troca forma e cor SEM perder o tamanho ja
//     definido. DELPOL zera os dois (isso e feito pela UC, com dois "we").
//   - 32 + 17 = 49 bits nao caberiam numa unica palavra de 32 bits.
//
// O BANCO E "DINAMICO" (read-modify-write):
//   - read_addr/read_data devolve o registro atual de UM poligono. A UC le esse
//     valor, troca so os campos que a instrucao enviou e grava de volta:
//       MOVPOL -> troca so X e Y        COLPOL -> troca so COR
//       INVPOL -> troca so H e V        (o resto do registro e preservado)
//
// SAIDA PARALELA para o rasterizador:
//   - read_data_all (256 bits = 8 x 32) e tam_all (136 bits = 8 x 17) entregam
//     os 8 poligonos ao mesmo tempo. O rasterizador testa os 8 em paralelo a
//     cada pixel, sem precisar escolher qual ler.
// =============================================================================
module banco_poligonos (
    input          clk,            // clock do sistema (25 MHz)
    input          reset,          // reset sincrono
    // ---- porta de atributos (32 bits) ----
    input          we,             // escreve atributos (pulso de 1 ciclo da UC)
    input  [2:0]   write_addr,     // id do poligono a escrever (0..7)
    input  [31:0]  write_data,     // novo registro de atributos
    input  [2:0]   read_addr,      // id do poligono a ler (para o RMW da UC)
    output [31:0]  read_data,      // registro atual do poligono read_addr
    output [255:0] read_data_all,  // os 8 registros lado a lado (rasterizador)
    // ---- porta de tamanhos (17 bits) ----
    input          tam_we,         // escreve tamanho (TAMPOL / DELPOL)
    input  [2:0]   tam_addr,       // id do poligono
    input  [16:0]  tam_data,       // {W[8:0], H[7:0]}
    output [135:0] tam_all         // os 8 tamanhos lado a lado (rasterizador)
);

    // tamanho inicial apos o reset: 50 x 50 pixels
    localparam [16:0] TAM_PADRAO = {9'd50, 8'd50};

    reg [31:0] registradores [0:7];   // banco de atributos
    reg [16:0] tamanhos      [0:7];   // banco de tamanhos
    integer i;

    always @(posedge clk) begin
        if (reset) begin
            for (i = 0; i < 8; i = i + 1) begin
                registradores[i] <= 32'd0;        // bit 31 = 0 -> todos inativos
                tamanhos[i]      <= TAM_PADRAO;   // 50 x 50
            end
        end else begin
            // as duas escritas sao independentes: podem acontecer no mesmo ciclo
            // (DELPOL zera atributo e tamanho de uma vez)
            if (we)     registradores[write_addr] <= write_data;
            if (tam_we) tamanhos[tam_addr]        <= tam_data;
        end
    end

    // leitura combinacional de um poligono (usada no read-modify-write da UC)
    assign read_data = registradores[read_addr];

    // poligono i fica em read_data_all[i*32 +: 32]
    assign read_data_all = {
        registradores[7], registradores[6], registradores[5], registradores[4],
        registradores[3], registradores[2], registradores[1], registradores[0]
    };

    // tamanho do poligono i fica em tam_all[i*17 +: 17]
    assign tam_all = {
        tamanhos[7], tamanhos[6], tamanhos[5], tamanhos[4],
        tamanhos[3], tamanhos[2], tamanhos[1], tamanhos[0]
    };

endmodule