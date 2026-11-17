// ============================================================================
// Copyright (c) 2013 by Terasic Technologies Inc.
// ============================================================================
//
// Permission:
//
//   Terasic grants permission to use and modify this code for use
//   in synthesis for all Terasic Development Boards and Altera Development 
//   Kits made by Terasic.  Other use of this code, including the selling 
//   ,duplication, or modification of any portion is strictly prohibited.
//
// Disclaimer:
//
//   This VHDL/Verilog or C/C++ source code is intended as a design reference
//   which illustrates how these types of functions can be implemented.
//   It is the user's responsibility to verify their design for
//   consistency and functionality through the use of formal
//   verification methods.  Terasic provides no warranty regarding the use 
//   or functionality of this code.
//
// ============================================================================
//           
//  Terasic Technologies Inc
//  9F., No.176, Sec.2, Gongdao 5th Rd, East Dist, Hsinchu City, 30070. Taiwan
//  
//  
//                     web: http://www.terasic.com/  
//                     email: support@terasic.com
//
// ============================================================================
//Date:  Mon Jun 17 20:35:29 2013
// ============================================================================

`define ENABLE_HPS

module ghrd_top(

      ///////// ADC /////////
      output             ADC_CONVST,
      output             ADC_DIN,
      input              ADC_DOUT,
      output             ADC_SCLK,

      ///////// AUD /////////
      input              AUD_ADCDAT,
      inout              AUD_ADCLRCK,
      inout              AUD_BCLK,
      output             AUD_DACDAT,
      inout              AUD_DACLRCK,
      output             AUD_XCK,

      ///////// CLOCK2 /////////
      input              CLOCK2_50,

      ///////// CLOCK3 /////////
      input              CLOCK3_50,

      ///////// CLOCK4 /////////
      input              CLOCK4_50,

      ///////// CLOCK /////////
      input              CLOCK_50,

      ///////// DRAM /////////
      output      [12:0] DRAM_ADDR,
      output      [1:0]  DRAM_BA,
      output             DRAM_CAS_N,
      output             DRAM_CKE,
      output             DRAM_CLK,
      output             DRAM_CS_N,
      inout       [15:0] DRAM_DQ,
      output             DRAM_LDQM,
      output             DRAM_RAS_N,
      output             DRAM_UDQM,
      output             DRAM_WE_N,

      ///////// FAN /////////
      output             FAN_CTRL,

      ///////// FPGA /////////
      output             FPGA_I2C_SCLK,
      inout              FPGA_I2C_SDAT,

      ///////// GPIO /////////
      inout     [35:0]         GPIO_0,
      inout     [35:0]         GPIO_1,
 

      ///////// HEX0 /////////
      output      [6:0]  HEX0,

      ///////// HEX1 /////////
      output      [6:0]  HEX1,

      ///////// HEX2 /////////
      output      [6:0]  HEX2,

      ///////// HEX3 /////////
      output      [6:0]  HEX3,

      ///////// HEX4 /////////
      output      [6:0]  HEX4,

      ///////// HEX5 /////////
      output      [6:0]  HEX5,

`ifdef ENABLE_HPS
      ///////// HPS /////////
      inout              HPS_CONV_USB_N,
      output      [14:0] HPS_DDR3_ADDR,
      output      [2:0]  HPS_DDR3_BA,
      output             HPS_DDR3_CAS_N,
      output             HPS_DDR3_CKE,
      output             HPS_DDR3_CK_N,
      output             HPS_DDR3_CK_P,
      output             HPS_DDR3_CS_N,
      output      [3:0]  HPS_DDR3_DM,
      inout       [31:0] HPS_DDR3_DQ,
      inout       [3:0]  HPS_DDR3_DQS_N,
      inout       [3:0]  HPS_DDR3_DQS_P,
      output             HPS_DDR3_ODT,
      output             HPS_DDR3_RAS_N,
      output             HPS_DDR3_RESET_N,
      input              HPS_DDR3_RZQ,
      output             HPS_DDR3_WE_N,
      output             HPS_ENET_GTX_CLK,
      inout              HPS_ENET_INT_N,
      output             HPS_ENET_MDC,
      inout              HPS_ENET_MDIO,
      input              HPS_ENET_RX_CLK,
      input       [3:0]  HPS_ENET_RX_DATA,
      input              HPS_ENET_RX_DV,
      output      [3:0]  HPS_ENET_TX_DATA,
      output             HPS_ENET_TX_EN,
      inout       [3:0]  HPS_FLASH_DATA,
      output             HPS_FLASH_DCLK,
      output             HPS_FLASH_NCSO,
      inout              HPS_GSENSOR_INT,
      inout              HPS_I2C1_SCLK,
      inout              HPS_I2C1_SDAT,
      inout              HPS_I2C2_SCLK,
      inout              HPS_I2C2_SDAT,
      inout              HPS_I2C_CONTROL,
      inout              HPS_KEY,
      inout              HPS_LED,
      inout              HPS_LTC_GPIO,
      output             HPS_SD_CLK,
      inout              HPS_SD_CMD,
      inout       [3:0]  HPS_SD_DATA,
      output             HPS_SPIM_CLK,
      input              HPS_SPIM_MISO,
      output             HPS_SPIM_MOSI,
      inout              HPS_SPIM_SS,
      input              HPS_UART_RX,
      output             HPS_UART_TX,
      input              HPS_USB_CLKOUT,
      inout       [7:0]  HPS_USB_DATA,
      input              HPS_USB_DIR,
      input              HPS_USB_NXT,
      output             HPS_USB_STP,
`endif /*ENABLE_HPS*/

      ///////// IRDA /////////
      input              IRDA_RXD,
      output             IRDA_TXD,

      ///////// KEY /////////
      input       [3:0]  KEY,

      ///////// LEDR /////////
      output      [9:0]  LEDR,

      ///////// PS2 /////////
      inout              PS2_CLK,
      inout              PS2_CLK2,
      inout              PS2_DAT,
      inout              PS2_DAT2,

      ///////// SW /////////
      input       [9:0]  SW,

      ///////// TD /////////
      input              TD_CLK27,
      input      [7:0]  TD_DATA,
      input             TD_HS,
      output             TD_RESET_N,
      input             TD_VS,

      ///////// VGA /////////
      output      [7:0]  VGA_B,
      output             VGA_BLANK_N,
      output             VGA_CLK,
      output      [7:0]  VGA_G,
      output             VGA_HS,
      output      [7:0]  VGA_R,
      output             VGA_SYNC_N,
      output             VGA_VS,

      ///////// UART /////////	
      output                     UART_TX,
      input                      UART_RX,
      output                     UART_RTS,
      input                      UART_CTS,

      ///////// QSPI /////////	
      output                     QSPI_FLASH_SCLK,
      inout           [ 3: 0]    QSPI_FLASH_DATA,
      output                     QSPI_FLASH_CE_n,

      ///////// RISC-V JTAG /////////
      input                      RISCV_JTAG_TCK,
      input                      RISCV_JTAG_TDI,
      output                     RISCV_JTAG_TDO,
      input                      RISCV_JTAG_TMS 
);

// internal wires and registers declaration
wire [3:0]  fpga_debounced_buttons;
wire [9:0]  fpga_led_internal;
wire        hps_fpga_reset_n;
wire [2:0]  hps_reset_req;
wire        hps_cold_reset;
wire        hps_warm_reset;
wire        hps_debug_reset;
wire [27:0] stm_hw_events;

// connection of internal logics
assign stm_hw_events    = {{3{1'b0}},SW, fpga_led_internal, fpga_debounced_buttons};




// logica de video e controle do DE1_SOC_golden_top integrada ao ghrd_top
// a interface e a infraestrutura HPS original do ghrd_top permanecem intactas


    // clock de pixel do vga: divide o clock de 50 MHz por 2 (25 MHz), todo o sistema roda nele
    reg clk_25 = 1'b0;

    always @(posedge CLOCK_50)
        clk_25 <= ~clk_25;

    wire clk = clk_25;

    // sincroniza os botoes com 2 flops (KEY e ativo em 0, por isso o ~)
    reg [3:0] key_sync1 = 4'd0;
    reg [3:0] key_sync2 = 4'd0;

    always @(posedge clk) begin
        key_sync1 <= ~KEY;
        key_sync2 <= key_sync1;
    end

    // reset do sistema: KEY0 apertado
    wire reset = key_sync2[0];

    assign VGA_CLK = clk_25;

    // pixel_x_raw/pixel_y_raw: contadores fisicos do vga (640x480)
    // logic_x/logic_y: coordenadas logicas 320x240 usadas pelos motores
    wire video_on;
    wire vga_hs_raw, vga_vs_raw, vga_blank_n_raw, vga_sync_n_raw;
    wire [9:0] logic_x;
    wire [9:0] logic_y_full;                 // o vga_driver entrega 10 bits
    wire [8:0] logic_y = logic_y_full[8:0];  // 0..239 cabe em 9 bits
    wire [9:0] pixel_x_raw;
    wire [9:0] pixel_y_raw;

    vga_driver vga_d (
        .clk(clk),
        .reset(reset),
        .VGA_HS(vga_hs_raw),
        .VGA_VS(vga_vs_raw),
        .video_on(video_on),
        .VGA_BLANK_N(vga_blank_n_raw),
        .VGA_SYNC_N(vga_sync_n_raw),
        .x(logic_x),
        .y(logic_y_full),
        .pixel_x_raw(pixel_x_raw),
        .pixel_y_raw(pixel_y_raw)
    );

    // vblank_start_pulse: 1 ciclo quando pixel_y_raw chega na linha 480 (fim da area visivel)
    // em_vblank: fica em 1 durante todo o vblank
    reg vblank_d;

    always @(posedge clk)
        vblank_d <= (pixel_y_raw == 10'd480);

    wire vblank_start_pulse = (pixel_y_raw == 10'd480) && !vblank_d;
    wire em_vblank          = (pixel_y_raw >= 10'd480);

    // pulso no comeco de cada linha logica (a cada 2 linhas fisicas, so na area visivel),
    // avisa o motor de sprites pra preparar a proxima linha
    wire nova_linha_pulso = (pixel_x_raw == 10'd0) &&
                            (pixel_y_raw[0] == 1'b0) &&
                            (pixel_y_raw < 10'd480);

    // linha_alvo e a proxima linha a ser exibida (a que o motor de sprites prepara), volta pra 0 depois da 239
    wire [8:0] linha_display_atual = pixel_y_raw[9:1];
    wire [8:0] linha_alvo = (linha_display_atual == 9'd239) ? 9'd0 : linha_display_atual + 9'd1;

    // fila de instrucoes: o HPS escreve, a unidade de controle consome
    wire [31:0] instrucao_saida;
    wire fifo_vazia, fifo_cheia, fifo_quase_cheia, fifo_quase_vazia;
    wire fifo_hab_leitura;


    // interface com o HPS (PIOs do Platform Designer)
    wire [31:0] pio_instr_out;   // instrucao que o HPS manda
    wire [1:0]  pio_ctrl_out;   // [0] strobe de escrita na fila | [1] limpar erro
    wire [31:0] pio_status_in;   // status que o HPS le

    // strobe e limpar erro vem do HPS (clock de 50 MHz): sincroniza para o clk
    reg strobe_s1, strobe_s2, strobe_d;   // strobe_d = strobe_s2 atrasado 1 ciclo, pra detectar a borda
    reg limpar_s1, limpar_s2;

    always @(posedge clk) begin
        if (reset) begin
            strobe_s1 <= 1'b0;
            strobe_s2 <= 1'b0;
            strobe_d  <= 1'b0;
            limpar_s1 <= 1'b0;
            limpar_s2 <= 1'b0;
        end else begin
            strobe_s1 <= pio_ctrl_out[0];
            strobe_s2 <= strobe_s1;
            strobe_d  <= strobe_s2;
            limpar_s1 <= pio_ctrl_out[1];
            limpar_s2 <= limpar_s1;
        end
    end

    // borda de subida do strobe = 1 escrita na fila, mesmo se o HPS deixar o strobe em 1 por varios ciclos
    wire fifo_hab_escrita = strobe_s2 & ~strobe_d;

    fifo_instrucoes f_instrucoes (
        .clock(clk),
        .reset(reset),
        .hab_escrita(fifo_hab_escrita),
        .instrucao_entrada(pio_instr_out),
        .hab_leitura(fifo_hab_leitura),
        .instrucao_saida(instrucao_saida),
        .cheia(fifo_cheia),
        .quase_cheia(fifo_quase_cheia),
        .vazia(fifo_vazia),
        .quase_vazia(fifo_quase_vazia)
    );

    wire bg_we;
    wire [2:0] bg_write_addr;
    wire [31:0] bg_write_data;

    wire tilemap_we;
    wire [10:0] tilemap_addr;
    wire [7:0] tilemap_data;

    wire spr_we;
    wire [5:0] spr_write_addr;   // MUDOU DE [4:0] PARA [5:0] (64 sprites)
    wire [31:0] spr_write_data;
    wire [5:0] spr_read_addr;    // MUDOU DE [4:0] PARA [5:0] (64 sprites)
    wire [31:0] spr_read_data;

    wire poly_we;
    wire [2:0] poly_write_addr;
    wire [31:0] poly_write_data;
    wire [2:0] poly_read_addr;
    wire [31:0] poly_read_data;
    wire        poly_tam_we;
    wire [2:0]  poly_tam_addr;
    wire [16:0] poly_tam_data;
    wire [135:0] poly_tam;   // 8 poligonos x 17 bits (W e H)
	 
	 wire        pad_we;
	 wire [13:0] pad_addr;
	 wire [7:0]  pad_data;

    wire [2:0] prioridade_compositor;

    wire uc_ocupada;
    wire uc_esperando_vblank;
    wire uc_opcode_invalido;
    wire uc_instr_done;

    // unidade de controle: busca a instrucao da fila, decodifica e escreve nos bancos
    unidade_controle u_controle (
        .clk(clk),
        .reset(reset),
        .instrucao_saida(instrucao_saida),
        .vazia(fifo_vazia),
        .hab_leitura(fifo_hab_leitura),
        .vblank_start_pulse(vblank_start_pulse),
        .bg_we(bg_we),
        .bg_write_addr(bg_write_addr),
        .bg_write_data(bg_write_data),
        .tilemap_we(tilemap_we),
        .tilemap_addr(tilemap_addr),
        .tilemap_data(tilemap_data),
        .spr_we(spr_we),
        .spr_write_addr(spr_write_addr),
        .spr_write_data(spr_write_data),
        .spr_read_addr(spr_read_addr),
        .spr_read_data(spr_read_data),
		  .pad_we(pad_we), 
		  .pad_addr(pad_addr), 
		  .pad_data(pad_data),
        .poly_we(poly_we),
        .poly_write_addr(poly_write_addr),
        .poly_write_data(poly_write_data),
        .poly_read_addr(poly_read_addr),
        .poly_read_data(poly_read_data),
        .poly_tam_we(poly_tam_we),
        .poly_tam_addr(poly_tam_addr),
        .poly_tam_data(poly_tam_data),
        .prioridade_compositor(prioridade_compositor),
        .ocupada(uc_ocupada),
        .esperando_vblank(uc_esperando_vblank),
        .opcode_invalido(uc_opcode_invalido),
        .instr_done_pulse(uc_instr_done)
    );

    // bancos de atributos: guardam o estado de cada camada, os motores so leem
    wire [31:0] out_video_ctrl, out_scroll_x, out_scroll_y;

    banco_background b_background (
        .clk(clk),
        .reset(reset),
        .cpu_write_en(bg_we),
        .cpu_write_addr(bg_write_addr),
        .cpu_write_data(bg_write_data),
        .out_video_ctrl(out_video_ctrl),
        .out_scroll_x(out_scroll_x),
        .out_scroll_y(out_scroll_y)
    );

    wire [2047:0] sprite_attr;   // MUDOU DE [1023:0] PARA [2047:0] (64 sprites x 32 bits)

    banco_sprites b_sprites (
        .clk(clk),
        .reset(reset),
        .we(spr_we),
        .write_addr(spr_write_addr),
        .write_data(spr_write_data),
        .read_addr(spr_read_addr),
        .read_data(spr_read_data),
        .read_data_all(sprite_attr)
    );

    wire [255:0] poly_attr;   // 8 poligonos x 32 bits

    banco_poligonos b_poligonos (
        .clk(clk),
        .reset(reset),
        .we(poly_we),
        .write_addr(poly_write_addr),
        .write_data(poly_write_data),
        .read_addr(poly_read_addr),
        .read_data(poly_read_data),
        .read_data_all(poly_attr),
        .tam_we(poly_tam_we),
        .tam_addr(poly_tam_addr),
        .tam_data(poly_tam_data),
        .tam_all(poly_tam)
    );

    wire [13:0] endereco_padroes_bg;   // endereco na rom de tiles

    motor_background m_background (
        .clk(clk),
        .reset(reset),
        .logic_x(logic_x),
        .logic_y(logic_y),
        .out_scroll_x(out_scroll_x),
        .out_scroll_y(out_scroll_y),
        .cpu_we(tilemap_we),
        .cpu_addr(tilemap_addr),
        .cpu_data(tilemap_data),
        .endereco_memoria_padroes(endereco_padroes_bg)
    );

    wire [7:0] poly_color_out;   // indice de cor do poligono no pixel atual

    motor_poligonos m_poligonos (
        .clk(clk),
        .reset(reset),
        .logic_x(logic_x),
        .logic_y(logic_y),
        .poly_attr(poly_attr),
        .poly_tam(poly_tam),
        .poly_color_out(poly_color_out)
    );

    // atrasa x, y e video_on em 3 ciclos pra alinhar com o caminho do background
    // (ram do tilemap, rom de tiles e registrador bg_index_d)
    // o frame buffer escreve com essas coordenadas atrasadas
    reg [9:0] x_pipe1, x_pipe2, x_pipe3;
    reg [8:0] y_pipe1, y_pipe2, y_pipe3;
    reg video_on_p1, video_on_p2, video_on_p3;

    always @(posedge clk) begin
        if (reset) begin
            x_pipe1 <= 10'd0;
            x_pipe2 <= 10'd0;
            x_pipe3 <= 10'd0;
            y_pipe1 <= 9'd0;
            y_pipe2 <= 9'd0;
            y_pipe3 <= 9'd0;
            video_on_p1 <= 1'b0;
            video_on_p2 <= 1'b0;
            video_on_p3 <= 1'b0;
        end else begin
            x_pipe1 <= logic_x;
            y_pipe1 <= logic_y;
            video_on_p1 <= video_on;
            x_pipe2 <= x_pipe1;
            y_pipe2 <= y_pipe1;
            video_on_p2 <= video_on_p1;
            x_pipe3 <= x_pipe2;
            y_pipe3 <= y_pipe2;
            video_on_p3 <= video_on_p2;
        end
    end

    wire [13:0] ram_addr_sprite;
    wire [7:0] ram_data_sprite;
    wire [7:0] sprite_index;
    wire linha_pronta;

    // porta de leitura do motor: ele endereça (spr_addr) e recebe a palavra do sprite (spr_data)
    // o sprite i ocupa sprite_attr[i*32 +: 32]
    wire [5:0]  spr_motor_addr;
    wire [31:0] spr_motor_data = sprite_attr[{spr_motor_addr, 5'd0} +: 32];

    // prepara a linha_alvo a cada nova_linha_pulso e entrega o indice do sprite
    // em x_pipe3, alinhado com os outros motores
    motor_sprites m_sprites (
        .clk(clk),
        .reset(reset),
        .nova_linha_pulso(nova_linha_pulso),
        .linha_alvo(linha_alvo),
        .spr_addr(spr_motor_addr),
        .spr_data(spr_motor_data),
        .ram_dado(ram_data_sprite),
        .leitura_x(x_pipe3[8:0]),
        .ram_endereco(ram_addr_sprite),
        .sprite_index(sprite_index),
        .linha_pronta(linha_pronta)
    );


    // Memorias de padroes: ROM de tiles do fundo e RAM editavel dos sprites.
    wire [7:0] bg_index_raw;

    rom_tiles r_padroes_tiles (
        .address(endereco_padroes_bg[13:0]),   // so usa os 11 bits de baixo do endereco (32 tiles de 8x8)
        .clock(clk),
        .q(bg_index_raw)
    );

    ram_sprites r_padroes_sprites (
		.clock(clk),
		.data(pad_data),
		.wraddress(pad_addr),
		.wren(pad_we),
		.rdaddress(ram_addr_sprite),
		.q(ram_data_sprite)
	 );

    // registra os indices do fundo e do poligono antes do compositor
    // fundo desligado (video_ctrl[0]) vira indice 0 (transparente)
    reg [7:0] bg_index_d;
    reg [7:0] poly_index_d;

    always @(posedge clk) begin
        if (reset) begin
            bg_index_d <= 8'd0;
            poly_index_d <= 8'd0;
        end else begin
            bg_index_d <= out_video_ctrl[0] ? 8'd0 : bg_index_raw;
            poly_index_d <= poly_color_out;
        end
    end

    // escolhe a camada de cada pixel pela prioridade
    wire [7:0] cor_composta;

    compositor comp (
        .bg_index(bg_index_d),
        .poly_index(poly_index_d),
        .sprite_index(sprite_index),
        .prioridade(prioridade_compositor),
        .final_index(cor_composta)
    );

    // escreve o pixel composto na pagina de tras (com as coordenadas atrasadas)
    // e le a posicao atual do vga na pagina da frente
    wire [7:0] color_index_final;
    wire front_sel;

    frame_buffer f_buffer (
        .clk(clk),
        .wr_en(video_on_p3),
        .wr_x(x_pipe3),
        .wr_y(y_pipe3),
        .wr_color_index(cor_composta),
        .rd_x(logic_x),
        .rd_y(logic_y),
        .rd_color_index(color_index_final),
        .vblank_start_pulse(vblank_start_pulse),
        .front_sel(front_sel)
    );

    // atrasa os sinais de sync 1 ciclo pra alinhar com a leitura do frame buffer
    reg vga_hs_d, vga_vs_d, vga_blank_n_d, vga_sync_n_d, video_on_d;

    always @(posedge clk) begin
        if (reset) begin
            vga_hs_d <= 1'b1;
            vga_vs_d <= 1'b1;
            vga_blank_n_d <= 1'b0;
            vga_sync_n_d <= 1'b0;
            video_on_d <= 1'b0;
        end else begin
            vga_hs_d <= vga_hs_raw;
            vga_vs_d <= vga_vs_raw;
            vga_blank_n_d <= vga_blank_n_raw;
            vga_sync_n_d <= vga_sync_n_raw;
            video_on_d <= video_on;
        end
    end

    assign VGA_HS = vga_hs_d;
    assign VGA_VS = vga_vs_d;
    assign VGA_BLANK_N = vga_blank_n_d;
    assign VGA_SYNC_N = vga_sync_n_d;

    // o indice de cor sai direto como rrrgggbb (3-3-2), preto fora da area visivel
    assign VGA_R = (video_on_d && vga_blank_n_d) ? {color_index_final[7:5],5'd0} : 8'd0;
    assign VGA_G = (video_on_d && vga_blank_n_d) ? {color_index_final[4:2],5'd0} : 8'd0;
    assign VGA_B = (video_on_d && vga_blank_n_d) ? {color_index_final[1:0],6'd0} : 8'd0;

    wire [31:0] status;
    // A escrita gera ACK somente quando a FIFO realmente aceita o push.
    wire instr_accepted_pulse = fifo_hab_escrita && !fifo_cheia;
    // Tentativa de escrita com a FIFO cheia gera overflow, sem ACK.
    wire overflow_pulse = fifo_hab_escrita && fifo_cheia;


    registrador_status r_status (
        .clk(clk),
        .reset(reset),
        .fifo_vazia(fifo_vazia),
        .fifo_cheia(fifo_cheia),
        .instr_accepted_pulse(instr_accepted_pulse),
        .uc_ocupada(uc_ocupada),
        .esperando_vblank(uc_esperando_vblank),
        .opcode_invalido(uc_opcode_invalido),
		.overflow_pulse   (overflow_pulse),
		.instr_done_pulse (uc_instr_done),
        .limpar_erro(limpar_s2 | key_sync2[1]),
        .em_vblank(em_vblank),
        .front_sel(front_sel),
        .vblank_start_pulse(vblank_start_pulse),
        .status(status)
    );

    // LEDs: mantem fifo_quase_cheia visivel no LEDR[2], embora STATUS[2]
    // tenha sido reservado ao ACK para o HPS.
    assign LEDR = {status[14:13], status[7:3], fifo_quase_cheia, status[1:0]};

    // STATUS[2] (ACK toggle) cruza de clk_25 para o clock de 50 MHz dos PIOs.
    // Sincronizamos esse bit em dois estagios antes de entrega-lo ao HPS.
    // Os demais campos de STATUS mantem o caminho existente e serao avaliados
    // no item 3.5 (CDC/STA).
    reg ack_pio_meta = 1'b0;
    reg ack_pio_sync = 1'b0;

    always @(posedge CLOCK_50) begin
        if (reset) begin
            ack_pio_meta <= 1'b0;
            ack_pio_sync <= 1'b0;
        end else begin
            ack_pio_meta <= status[2];
            ack_pio_sync <= ack_pio_meta;
        end
    end

    wire [31:0] status_pio = {status[31:3], ack_pio_sync, status[1:0]};
    assign pio_status_in = status_pio;


// compatibilidade com a infraestrutura original do ghrd_top
assign fpga_led_internal = LEDR;
assign fpga_debounced_buttons = key_sync2;

soc_system u0 (
    .clk_clk                               ( CLOCK_50           ),      //                            clk.clk
    .reset_reset_n                         ( hps_fpga_reset_n   ),      //                          reset.reset_n

    .memory_mem_a                          ( HPS_DDR3_ADDR  ),          //                         memory.mem_a
    .memory_mem_ba                         ( HPS_DDR3_BA    ),          //                               .mem_ba
    .memory_mem_ck                         ( HPS_DDR3_CK_P  ),          //                               .mem_ck
    .memory_mem_ck_n                       ( HPS_DDR3_CK_N  ),          //                               .mem_ck_n
    .memory_mem_cke                        ( HPS_DDR3_CKE   ),          //                               .mem_cke
    .memory_mem_cs_n                       ( HPS_DDR3_CS_N  ),          //                               .mem_cs_n
    .memory_mem_ras_n                      ( HPS_DDR3_RAS_N ),          //                               .mem_ras_n
    .memory_mem_cas_n                      ( HPS_DDR3_CAS_N ),          //                               .mem_cas_n
    .memory_mem_we_n                       ( HPS_DDR3_WE_N  ),          //                               .mem_we_n
    .memory_mem_reset_n                    ( HPS_DDR3_RESET_N   ),      //                               .mem_reset_n
    .memory_mem_dq                         ( HPS_DDR3_DQ    ),          //                               .mem_dq
    .memory_mem_dqs                        ( HPS_DDR3_DQS_P ),          //                               .mem_dqs
    .memory_mem_dqs_n                      ( HPS_DDR3_DQS_N ),          //                               .mem_dqs_n
    .memory_mem_odt                        ( HPS_DDR3_ODT   ),          //                               .mem_odt
    .memory_mem_dm                         ( HPS_DDR3_DM    ),          //                               .mem_dm
    .memory_oct_rzqin                      ( HPS_DDR3_RZQ   ),          //                               .oct_rzqin
   		
    .hps_0_hps_io_hps_io_emac1_inst_TX_CLK ( HPS_ENET_GTX_CLK),         //                   hps_0_hps_io.hps_io_emac1_inst_TX_CLK
    .hps_0_hps_io_hps_io_emac1_inst_TXD0   ( HPS_ENET_TX_DATA[0] ),     //                               .hps_io_emac1_inst_TXD0
    .hps_0_hps_io_hps_io_emac1_inst_TXD1   ( HPS_ENET_TX_DATA[1] ),     //                               .hps_io_emac1_inst_TXD1
    .hps_0_hps_io_hps_io_emac1_inst_TXD2   ( HPS_ENET_TX_DATA[2] ),     //                               .hps_io_emac1_inst_TXD2
    .hps_0_hps_io_hps_io_emac1_inst_TXD3   ( HPS_ENET_TX_DATA[3] ),     //                               .hps_io_emac1_inst_TXD3
    .hps_0_hps_io_hps_io_emac1_inst_RXD0   ( HPS_ENET_RX_DATA[0] ),     //                               .hps_io_emac1_inst_RXD0
    .hps_0_hps_io_hps_io_emac1_inst_MDIO   ( HPS_ENET_MDIO ),           //                               .hps_io_emac1_inst_MDIO
    .hps_0_hps_io_hps_io_emac1_inst_MDC    ( HPS_ENET_MDC  ),           //                               .hps_io_emac1_inst_MDC
    .hps_0_hps_io_hps_io_emac1_inst_RX_CTL ( HPS_ENET_RX_DV),           //                               .hps_io_emac1_inst_RX_CTL
    .hps_0_hps_io_hps_io_emac1_inst_TX_CTL ( HPS_ENET_TX_EN),           //                               .hps_io_emac1_inst_TX_CTL
    .hps_0_hps_io_hps_io_emac1_inst_RX_CLK ( HPS_ENET_RX_CLK),          //                               .hps_io_emac1_inst_RX_CLK
    .hps_0_hps_io_hps_io_emac1_inst_RXD1   ( HPS_ENET_RX_DATA[1] ),     //                               .hps_io_emac1_inst_RXD1
    .hps_0_hps_io_hps_io_emac1_inst_RXD2   ( HPS_ENET_RX_DATA[2] ),     //                               .hps_io_emac1_inst_RXD2
    .hps_0_hps_io_hps_io_emac1_inst_RXD3   ( HPS_ENET_RX_DATA[3] ),     //                               .hps_io_emac1_inst_RXD3
    
	  
    .hps_0_hps_io_hps_io_qspi_inst_IO0     ( HPS_FLASH_DATA[0]    ),    //                               .hps_io_qspi_inst_IO0
    .hps_0_hps_io_hps_io_qspi_inst_IO1     ( HPS_FLASH_DATA[1]    ),    //                               .hps_io_qspi_inst_IO1
    .hps_0_hps_io_hps_io_qspi_inst_IO2     ( HPS_FLASH_DATA[2]    ),    //                               .hps_io_qspi_inst_IO2
    .hps_0_hps_io_hps_io_qspi_inst_IO3     ( HPS_FLASH_DATA[3]    ),    //                               .hps_io_qspi_inst_IO3
    .hps_0_hps_io_hps_io_qspi_inst_SS0     ( HPS_FLASH_NCSO    ),       //                               .hps_io_qspi_inst_SS0
    .hps_0_hps_io_hps_io_qspi_inst_CLK     ( HPS_FLASH_DCLK    ),       //                               .hps_io_qspi_inst_CLK
    
    .hps_0_hps_io_hps_io_sdio_inst_CMD     ( HPS_SD_CMD    ),           //                               .hps_io_sdio_inst_CMD
    .hps_0_hps_io_hps_io_sdio_inst_D0      ( HPS_SD_DATA[0]     ),      //                               .hps_io_sdio_inst_D0
    .hps_0_hps_io_hps_io_sdio_inst_D1      ( HPS_SD_DATA[1]     ),      //                               .hps_io_sdio_inst_D1
    .hps_0_hps_io_hps_io_sdio_inst_CLK     ( HPS_SD_CLK   ),            //                               .hps_io_sdio_inst_CLK
    .hps_0_hps_io_hps_io_sdio_inst_D2      ( HPS_SD_DATA[2]     ),      //                               .hps_io_sdio_inst_D2
    .hps_0_hps_io_hps_io_sdio_inst_D3      ( HPS_SD_DATA[3]     ),      //                               .hps_io_sdio_inst_D3
    		  
    .hps_0_hps_io_hps_io_usb1_inst_D0      ( HPS_USB_DATA[0]    ),      //                               .hps_io_usb1_inst_D0
    .hps_0_hps_io_hps_io_usb1_inst_D1      ( HPS_USB_DATA[1]    ),      //                               .hps_io_usb1_inst_D1
    .hps_0_hps_io_hps_io_usb1_inst_D2      ( HPS_USB_DATA[2]    ),      //                               .hps_io_usb1_inst_D2
    .hps_0_hps_io_hps_io_usb1_inst_D3      ( HPS_USB_DATA[3]    ),      //                               .hps_io_usb1_inst_D3
    .hps_0_hps_io_hps_io_usb1_inst_D4      ( HPS_USB_DATA[4]    ),      //                               .hps_io_usb1_inst_D4
    .hps_0_hps_io_hps_io_usb1_inst_D5      ( HPS_USB_DATA[5]    ),      //                               .hps_io_usb1_inst_D5
    .hps_0_hps_io_hps_io_usb1_inst_D6      ( HPS_USB_DATA[6]    ),      //                               .hps_io_usb1_inst_D6
    .hps_0_hps_io_hps_io_usb1_inst_D7      ( HPS_USB_DATA[7]    ),      //                               .hps_io_usb1_inst_D7
    .hps_0_hps_io_hps_io_usb1_inst_CLK     ( HPS_USB_CLKOUT    ),       //                               .hps_io_usb1_inst_CLK
    .hps_0_hps_io_hps_io_usb1_inst_STP     ( HPS_USB_STP    ),          //                               .hps_io_usb1_inst_STP
    .hps_0_hps_io_hps_io_usb1_inst_DIR     ( HPS_USB_DIR    ),          //                               .hps_io_usb1_inst_DIR
    .hps_0_hps_io_hps_io_usb1_inst_NXT     ( HPS_USB_NXT    ),          //                               .hps_io_usb1_inst_NXT
    		  
    .hps_0_hps_io_hps_io_spim1_inst_CLK    ( HPS_SPIM_CLK  ),           //                               .hps_io_spim1_inst_CLK
    .hps_0_hps_io_hps_io_spim1_inst_MOSI   ( HPS_SPIM_MOSI ),           //                               .hps_io_spim1_inst_MOSI
    .hps_0_hps_io_hps_io_spim1_inst_MISO   ( HPS_SPIM_MISO ),           //                               .hps_io_spim1_inst_MISO
    .hps_0_hps_io_hps_io_spim1_inst_SS0    ( HPS_SPIM_SS ),             //                               .hps_io_spim1_inst_SS0
  		
    .hps_0_hps_io_hps_io_uart0_inst_RX     ( HPS_UART_RX    ),          //                               .hps_io_uart0_inst_RX
    .hps_0_hps_io_hps_io_uart0_inst_TX     ( HPS_UART_TX    ),          //                               .hps_io_uart0_inst_TX
	
    .hps_0_hps_io_hps_io_i2c0_inst_SDA     ( HPS_I2C1_SDAT    ),        //                               .hps_io_i2c0_inst_SDA
    .hps_0_hps_io_hps_io_i2c0_inst_SCL     ( HPS_I2C1_SCLK    ),        //                               .hps_io_i2c0_inst_SCL
	
    .hps_0_hps_io_hps_io_i2c1_inst_SDA     ( HPS_I2C2_SDAT    ),        //                               .hps_io_i2c1_inst_SDA
    .hps_0_hps_io_hps_io_i2c1_inst_SCL     ( HPS_I2C2_SCLK    ),        //                               .hps_io_i2c1_inst_SCL
    
    .hps_0_hps_io_hps_io_gpio_inst_GPIO09  ( HPS_CONV_USB_N),           //                               .hps_io_gpio_inst_GPIO09
    .hps_0_hps_io_hps_io_gpio_inst_GPIO35  ( HPS_ENET_INT_N),           //                               .hps_io_gpio_inst_GPIO35
    .hps_0_hps_io_hps_io_gpio_inst_GPIO40  ( HPS_LTC_GPIO),             //                               .hps_io_gpio_inst_GPIO40
    .hps_0_hps_io_hps_io_gpio_inst_GPIO48  ( HPS_I2C_CONTROL),          //                               .hps_io_gpio_inst_GPIO48
    .hps_0_hps_io_hps_io_gpio_inst_GPIO53  ( HPS_LED),                  //                               .hps_io_gpio_inst_GPIO53
    .hps_0_hps_io_hps_io_gpio_inst_GPIO54  ( HPS_KEY),                  //                               .hps_io_gpio_inst_GPIO54
    .hps_0_hps_io_hps_io_gpio_inst_GPIO61  ( HPS_GSENSOR_INT),          //                               .hps_io_gpio_inst_GPIO61

    .hps_0_f2h_stm_hw_events_stm_hwevents  (stm_hw_events),             //        hps_0_f2h_stm_hw_events.stm_hwevents
    .hps_0_h2f_reset_reset_n               (hps_fpga_reset_n),          //                hps_0_h2f_reset.reset_n
    .hps_0_f2h_warm_reset_req_reset_n      (~hps_warm_reset),           //       hps_0_f2h_warm_reset_req.reset_n
    .hps_0_f2h_debug_reset_req_reset_n     (~hps_debug_reset),          //      hps_0_f2h_debug_reset_req.reset_n
    .hps_0_f2h_cold_reset_req_reset_n      (~hps_cold_reset),           //       hps_0_f2h_cold_reset_req.reset_n

    .pio_instr_external_connection_export  ( pio_instr_out ),
    .pio_ctrl_external_connection_export   ( pio_ctrl_out ),
    .pio_status_external_connection_export ( pio_status_in )
);
  
// Source/Probe megawizard instance
hps_reset hps_reset_inst (
    .source_clk (CLOCK_50),
    .source     (hps_reset_req)
);

altera_edge_detector pulse_cold_reset (
    .clk       (CLOCK_50),
    .rst_n     (hps_fpga_reset_n),
    .signal_in (hps_reset_req[0]),
    .pulse_out (hps_cold_reset)
);
defparam pulse_cold_reset.PULSE_EXT = 6;
defparam pulse_cold_reset.EDGE_TYPE = 1;
defparam pulse_cold_reset.IGNORE_RST_WHILE_BUSY = 1;

altera_edge_detector pulse_warm_reset (
    .clk       (CLOCK_50),
    .rst_n     (hps_fpga_reset_n),
    .signal_in (hps_reset_req[1]),
    .pulse_out (hps_warm_reset)
);
defparam pulse_warm_reset.PULSE_EXT = 2;
defparam pulse_warm_reset.EDGE_TYPE = 1;
defparam pulse_warm_reset.IGNORE_RST_WHILE_BUSY = 1;
  
altera_edge_detector pulse_debug_reset (
    .clk       (CLOCK_50),
    .rst_n     (hps_fpga_reset_n),
    .signal_in (hps_reset_req[2]),
    .pulse_out (hps_debug_reset)
);
defparam pulse_debug_reset.PULSE_EXT = 32;
defparam pulse_debug_reset.EDGE_TYPE = 1;
defparam pulse_debug_reset.IGNORE_RST_WHILE_BUSY = 1;



endmodule