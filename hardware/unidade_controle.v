module unidade_controle (
    input  wire        clk,
    input  wire        reset,

    // fila de instrucoes
    input  wire [31:0] instrucao_saida,   // instrucao na frente da fila
    input  wire        vazia,
    output reg         hab_leitura,       // pop da fila

    input  wire        vblank_start_pulse,

    // background (banco de scroll/liga-desliga e ram do tilemap)
    output reg         bg_we,
    output reg  [2:0]  bg_write_addr,
    output reg  [31:0] bg_write_data,
    output reg         tilemap_we,
    output reg  [10:0] tilemap_addr,
    output reg  [7:0]  tilemap_data,

    // sprites (64 sprites: enderecos de 6 bits [5:0])
    output reg         spr_we,
    output reg  [5:0]  spr_write_addr,
    output reg  [31:0] spr_write_data,
    output wire [5:0]  spr_read_addr,
    input  wire [31:0] spr_read_data,
    output reg         pad_we,
    output reg  [13:0] pad_addr,
    output reg  [7:0]  pad_data,

    // poligonos (o tamanho W/H fica num banco separado, poly_tam_*)
    output reg         poly_we,
    output reg  [2:0]  poly_write_addr,
    output reg  [31:0] poly_write_data,
    output wire [2:0]  poly_read_addr,
    input  wire [31:0] poly_read_data,
    output reg         poly_tam_we,
    output reg  [2:0]  poly_tam_addr,
    output reg  [16:0] poly_tam_data,

    // compositor
    output reg  [2:0]  prioridade_compositor,   // {bg, poly, sprite}

    // sinais para o registrador de status
    output wire        ocupada,           // 0 so quando parada em BUSCA com fila vazia
    output wire        esperando_vblank,  // 1 enquanto aguarda o inicio do vblank
    output reg         opcode_invalido,   // pulso de 1 ciclo ao descartar opcode fora da ISA
    output wire        instr_done_pulse   // pulso de 1 ciclo ao concluir uma instrucao
);

    // mapa de opcodes: ir[4:3] escolhe o grupo, ir[2:0] a instrucao dentro dele
    // 00xxx - controle
    localparam OP_WAITVBLANK = 5'b00_000;
    localparam OP_PRICOMP    = 5'b00_101;   // "PRI" do compositor

    // 01xxx - background
    localparam OP_DELBG      = 5'b01_001;   // ir[5]: 0 desliga, 1 liga
    localparam OP_MOVBGX     = 5'b01_010;   // ir[15:10]: scroll X em tiles (0..39)
    localparam OP_PDRBG      = 5'b01_011;   // [23:13] TMADDR, [12:5] TILE
    localparam OP_MOVBGY     = 5'b01_110;   // ir[14:10]: scroll Y em tiles (0..29)

    // 10xxx - sprites (ID de 6 bits em ir[10:5])
    localparam OP_INSSPR     = 5'b10_000;   // ID [10:5], OP [16:11] = padrao
    localparam OP_DELSPR     = 5'b10_001;   // ID [10:5]
    localparam OP_MOVSPR     = 5'b10_010;   // X [27:19], Y [18:11], ID [10:5]
    localparam OP_PDRSPR     = 5'b10_011;   // ID [10:5], OP [16:11] = padrao
    localparam OP_INVSPR     = 5'b10_100;   // ID [10:5], H = ir[12], V = ir[11]
    localparam OP_PRISPR     = 5'b10_101;   // ID [10:5], OP [16:11] = prioridade
    localparam OP_PIXSPR     = 5'b10_110;   // ADDR [26:13], COLOR [12:5]

    // 11xxx - poligonos (ID de 3 bits em ir[7:5])
    localparam OP_INSPOL     = 5'b11_000;   // COLOR = ir[20:13], ID [7:5], SHAPE = ir[12:10]
    localparam OP_DELPOL     = 5'b11_001;   // ID [7:5]
    localparam OP_MOVPOL     = 5'b11_010;   // A = X [26:18], B = Y [17:10], ID [7:5]
    localparam OP_COLPOL     = 5'b11_011;   // COLOR = ir[20:13], ID [7:5]
    localparam OP_INVPOL     = 5'b11_100;   // ID [7:5], H = ir[11], V = ir[10]
    localparam OP_TAMPOL     = 5'b11_101;   // A = W [26:18], B = H [17:10], ID [7:5]

    // limites do tilemap (usados para limitar o scroll)
    localparam [5:0] MAPA_COLUNAS = 6'd40;
    localparam [4:0] MAPA_LINHAS  = 5'd30;

    // registrador de instrucao e campos decodificados
    reg  [31:0] ir;

    wire [4:0]  opcode         = ir[4:0];

    // Campos especificos de Sprites (64 sprites -> ID em [10:5], campos deslocados +1 bit)
    wire [5:0]  id_spr         = ir[10:5];
    wire [5:0]  campo_op_spr   = ir[16:11];  // padrao/prioridade (0..63); HV usa [1:0] (bits 12 e 11)
    wire [8:0]  campo_x_spr    = ir[27:19];  // X do MOVSPR (0..511)
    wire [7:0]  campo_y_spr    = ir[18:11];  // Y do MOVSPR (0..255)
    wire [13:0] campo_pad_addr = ir[26:13]; // ADDR do sprite na RAM (PIXSPR), 64 x 256 bytes
    wire [7:0]  campo_pad_data = ir[12:5];   // COLOR que vai para o endereco (PIXSPR)

    // Campos de Poligonos, Background e Controle (mantidos nas posicoes originais)
    wire [8:0]  campo_a        = ir[26:18];  // X, W dos poligonos
    wire [7:0]  campo_b        = ir[17:10];  // Y, H dos poligonos e scroll Y
    wire [2:0]  id_pol         = ir[7:5];
    wire [2:0]  campo_op_pol   = ir[12:10];  // SHAPE = [2:0] (bits 12..10) | HV = [1:0] (bits 11..10)
    wire [7:0]  campo_cor      = ir[20:13];  // COLOR (INSPOL, COLPOL)
    wire [2:0]  campo_pri      = ir[7:5];    // PRICOMP: [2] BG | [1] POL | [0] SPR
    wire        campo_liga     = ir[5];      // DELBG: 0 desliga, 1 liga

    // formato T (PDRBG)
    wire [10:0] campo_tmaddr   = ir[23:13];
    wire [7:0]  campo_tile     = ir[12:5];

    // leitura dos bancos para as instrucoes read-modify-write
    assign spr_read_addr  = id_spr;
    assign poly_read_addr = id_pol;

    // scroll limitado ao tamanho do mapa (modulo)
    wire [5:0] scroll_x_bruto = ir[15:10];
    wire [4:0] scroll_y_bruto = campo_b[4:0];
    wire [5:0] scroll_x_lim = (scroll_x_bruto >= MAPA_COLUNAS) ? scroll_x_bruto - MAPA_COLUNAS
                                                              : scroll_x_bruto;
    wire [4:0] scroll_y_lim = (scroll_y_bruto >= MAPA_LINHAS)  ? scroll_y_bruto - MAPA_LINHAS
                                                              : scroll_y_bruto;

    localparam BUSCA         = 2'd0;
    localparam EXECUTA       = 2'd1;
    localparam ESPERA_VBLANK = 2'd2;

    reg [1:0] estado;

    // pop combinacional: acontece no mesmo ciclo em que o IR captura
    always @(*) begin
        hab_leitura = (estado == BUSCA) && !vazia;
    end

    assign ocupada          = !((estado == BUSCA) && vazia);
    assign esperando_vblank = (estado == ESPERA_VBLANK);

    // WAITVBLANK nao conta ao executar, so conta quando o vblank chega e a espera acaba
    assign instr_done_pulse = ((estado == EXECUTA) && (opcode != OP_WAITVBLANK)) || ((estado == ESPERA_VBLANK) && vblank_start_pulse);

    always @(posedge clk) begin
        if (reset) begin
            estado                <= BUSCA;
            ir                    <= 32'd0;
            opcode_invalido       <= 1'b0;
            bg_we                 <= 1'b0;
            tilemap_we            <= 1'b0;
            spr_we                <= 1'b0;
            poly_we               <= 1'b0;
            poly_tam_we           <= 1'b0;
            pad_we                <= 1'b0;
            prioridade_compositor <= 3'b000;
            bg_write_addr   <= 3'd0;   bg_write_data   <= 32'd0;
            tilemap_addr    <= 11'd0;  tilemap_data    <= 8'd0;
            spr_write_addr  <= 6'd0;   spr_write_data  <= 32'd0;
            poly_write_addr <= 3'd0;   poly_write_data <= 32'd0;
            poly_tam_addr   <= 3'd0;   poly_tam_data   <= 17'd0;
            pad_addr        <= 14'd0;  pad_data        <= 8'd0;
        end else begin
            // strobes duram exatamente 1 ciclo
            bg_we           <= 1'b0;
            tilemap_we      <= 1'b0;
            spr_we          <= 1'b0;
            poly_we         <= 1'b0;
            pad_we          <= 1'b0;
            poly_tam_we     <= 1'b0;
            opcode_invalido <= 1'b0;

            case (estado)
                BUSCA: begin
                    if (!vazia) begin
                        ir     <= instrucao_saida;
                        estado <= EXECUTA;
                    end
                end

                EXECUTA: begin
                    estado <= BUSCA;   // padrao: volta a buscar

                    case (opcode)
                        // controle
                        OP_WAITVBLANK: begin
                            estado <= ESPERA_VBLANK;
                        end

                        OP_PRICOMP: begin
                            prioridade_compositor <= campo_pri;
                        end

                        // background
                        OP_DELBG: begin
                            bg_we         <= 1'b1;
                            bg_write_addr <= 3'd0;
                            bg_write_data <= {31'd0, ~campo_liga};
                        end

                        OP_MOVBGX: begin
                            bg_we         <= 1'b1;
                            bg_write_addr <= 3'd1;
                            bg_write_data <= {26'd0, scroll_x_lim};
                        end

                        OP_MOVBGY: begin
                            bg_we         <= 1'b1;
                            bg_write_addr <= 3'd2;
                            bg_write_data <= {27'd0, scroll_y_lim};
                        end

                        OP_PDRBG: begin
                            tilemap_we   <= 1'b1;
                            tilemap_addr <= campo_tmaddr;
                            tilemap_data <= campo_tile;
                        end

                        OP_INSSPR: begin
                            spr_we         <= 1'b1;
                            spr_write_addr <= id_spr;
                            spr_write_data <= {1'b1, campo_op_spr, 25'd0};
                        end

                        OP_DELSPR: begin
                            spr_we         <= 1'b1;
                            spr_write_addr <= id_spr;
                            spr_write_data <= 32'd0;
                        end

                        OP_MOVSPR: begin
                            spr_we         <= 1'b1;
                            spr_write_addr <= id_spr;
                            spr_write_data <= {spr_read_data[31:23], campo_x_spr, campo_y_spr, spr_read_data[5:0]};
                        end

                        OP_PDRSPR: begin
                            spr_we         <= 1'b1;
                            spr_write_addr <= id_spr;
                            spr_write_data <= {spr_read_data[31], campo_op_spr, spr_read_data[24:0]};
                        end

                        OP_INVSPR: begin
                            spr_we         <= 1'b1;
                            spr_write_addr <= id_spr;
                            spr_write_data <= {spr_read_data[31:25], campo_op_spr[1], campo_op_spr[0], spr_read_data[22:0]};
                        end

                        OP_PRISPR: begin
                            spr_we         <= 1'b1;
                            spr_write_addr <= id_spr;
                            spr_write_data <= {spr_read_data[31:6], campo_op_spr};
                        end

                        OP_PIXSPR: begin
                            pad_we   <= 1'b1;
                            pad_addr <= campo_pad_addr;
                            pad_data <= campo_pad_data;
                        end

                        // poligonos
                        OP_INSPOL: begin
                            poly_we         <= 1'b1;
                            poly_write_addr <= id_pol;
                            poly_write_data <= {1'b1, campo_op_pol[2:0], campo_cor, 20'd0};
                        end

                        OP_DELPOL: begin
                            poly_we         <= 1'b1;
                            poly_write_addr <= id_pol;
                            poly_write_data <= 32'd0;
                            poly_tam_we     <= 1'b1;
                            poly_tam_addr   <= id_pol;
                            poly_tam_data   <= 17'd0;
                        end

                        OP_MOVPOL: begin
                            poly_we         <= 1'b1;
                            poly_write_addr <= id_pol;
                            poly_write_data <= {poly_read_data[31:18], campo_a, campo_b, poly_read_data[0]};
                        end

                        OP_COLPOL: begin
                            poly_we         <= 1'b1;
                            poly_write_addr <= id_pol;
                            poly_write_data <= {poly_read_data[31:28], campo_cor, poly_read_data[19:0]};
                        end

                        OP_INVPOL: begin
                            poly_we         <= 1'b1;
                            poly_write_addr <= id_pol;
                            poly_write_data <= {poly_read_data[31:20], campo_op_pol[1], campo_op_pol[0], poly_read_data[17:0]};
                        end

                        OP_TAMPOL: begin
                            poly_tam_we   <= 1'b1;
                            poly_tam_addr <= id_pol;
                            poly_tam_data <= {campo_a, campo_b};
                        end

                        // opcode fora da ISA
                        default: begin
                            opcode_invalido <= 1'b1;
                        end
                    endcase
                end

                ESPERA_VBLANK: begin
                    if (vblank_start_pulse)
                        estado <= BUSCA;
                end

                default: estado <= BUSCA;
            endcase
        end
    end

endmodule