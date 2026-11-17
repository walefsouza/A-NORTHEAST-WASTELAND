// =============================================================================
// banco_background.v - REGISTRADORES DE CONTROLE DO FUNDO (background)
// =============================================================================
// Banco pequeno (8 x 32 bits) escrito pela Unidade de Controle e lido o tempo
// todo pelo motor de background. So 3 posicoes sao usadas hoje:
//
//   reg 0  video_ctrl : bit 0 = 1 -> fundo DESLIGADO (o ghrd_top troca o indice
//                       do fundo por 0 = transparente). Escrito pelo DELBG; a UC
//                       grava ~LIGA, por isso DELBG 1 liga e DELBG 0 desliga.
//   reg 1  scroll X   : deslocamento horizontal, em TILES (0..39) - MOVBGX
//   reg 2  scroll Y   : deslocamento vertical,   em TILES (0..29) - MOVBGY
//   reg 3..7          : livres (reservados para extensoes)
//
// O tilemap (qual tile vai em cada posicao 40 x 30) NAO fica aqui: fica na
// RAM ram_tilemap, dentro do motor_background, escrita pelo PDRBG.
//
// Leitura assincrona: cada saida e ligada direto a uma posicao fixa, entao uma
// mudanca de scroll vale a partir do ciclo seguinte a escrita.
// =============================================================================
module banco_background (
    input clk,
    input reset,

    input        cpu_write_en,     // habilita escrita no banco
    input  [2:0] cpu_write_addr,   // indice do registrador a escrever
    input  [31:0] cpu_write_data,  // valor a escrever

    output [31:0] out_video_ctrl,  // controle de video (reg 0)
    output [31:0] out_scroll_x,    // scroll horizontal (reg 1)
    output [31:0] out_scroll_y     // scroll vertical (reg 2)
);

    // banco de 8 registradores de 32 bits, endereçavel pela cpu
    reg [31:0] reg_array [0:7];
    integer i;

    always @(posedge clk) begin
        if (reset) begin
            for (i = 0; i < 8; i = i + 1) begin
                reg_array[i] <= 32'd0;
            end
        end else if (cpu_write_en) begin
            reg_array[cpu_write_addr] <= cpu_write_data;
        end
    end

    // leitura assincrona: cada saida aponta pra uma posiçao fixa do banco
    assign out_video_ctrl = reg_array[0];
    assign out_scroll_x   = reg_array[1];
    assign out_scroll_y   = reg_array[2];

endmodule