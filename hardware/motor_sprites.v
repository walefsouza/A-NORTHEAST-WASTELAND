module motor_sprites (
    input wire clk, // clock de 25 MHz
    input wire reset,
    input wire nova_linha_pulso, // avisa quando comeca uma nova linha e troca os buffers
    input wire [8:0] linha_alvo, // linha da tela que precisa ser montada (0 a 239)
    output wire [5:0] spr_addr, // indice do sprite que queremos ler no banco
    input wire [31:0] spr_data, // registro do sprite selecionado
    input wire [7:0] ram_dado, // pixel que chegou da RAM de padroes
    input wire [8:0] leitura_x, // coluna que o compositor esta lendo
    output reg [13:0] ram_endereco, // endereco da RAM: padrao, linha e coluna
    output wire [7:0] sprite_index, // cor do sprite na posicao leitura_x (0 = transparente)
    output reg linha_pronta // pulso avisando que terminou de montar a linha
);

    // sao dois buffers de 320 pixels: enquanto um e lido pelo compositor,
    // o outro recebe os pixels da proxima linha. a pagina controla essa troca.
    reg [7:0] buffer_a [0:319];
    reg [7:0] buffer_b [0:319];
    reg [5:0] prio_a [0:319];
    reg [5:0] prio_b [0:319];
    reg pagina; // 0: le A e escreve B / 1: le B e escreve A

    // aqui o compositor pega a cor do buffer que esta sendo lido no momento
    assign sprite_index = (pagina == 1'b0) ? buffer_a[leitura_x] : buffer_b[leitura_x];

    // estados da maquina: espera, limpa o buffer, procura sprites e desenha
    localparam OCIOSO = 2'd0;
    localparam LIMPAR_LINHA = 2'd1;
    localparam VARRE = 2'd2;
    localparam DESENHA = 2'd3;

    reg [1:0] estado;
    reg [8:0] linha_reg; // linha que esta sendo montada
    reg [8:0] contador_limpeza; // percorre os 320 pixels durante a limpeza
    reg [5:0] indice; // sprite atual (0 a 63)
    reg [4:0] cont_pixel; // conta os 16 pixels e mais 2 ciclos do pipeline

    // atributos do sprite atual, guardados quando ele e encontrado na varredura
    reg [8:0] sprite_x;
    reg sprite_espelho_h;
    reg sprite_espelho_v;
    reg [5:0] sprite_padrao;
    reg [5:0] sprite_prio;
    reg [3:0] pixel_y_no_sprite; // linha de dentro do sprite que sera desenhada

    // esses registradores acompanham o pixel enquanto ele espera o dado chegar da RAM
    reg [9:0] x_a;
    reg [9:0] x_b;
    reg val_a;
    reg val_b;

    assign spr_addr = indice; // o banco devolve os dados do sprite indicado por indice

    // separa os campos do registro recebido do banco de sprites
    wire sp_ativo = spr_data[31];
    wire [5:0] sp_padrao = spr_data[30:25];
    wire sp_esp_h = spr_data[24];
    wire sp_esp_v = spr_data[23];
    wire [8:0] sp_x = spr_data[22:14];
    wire [7:0] sp_y = spr_data[13:6];
    wire [5:0] sp_prio = spr_data[5:0];

    // como o sprite tem 16 pixels de altura, ele cruza a linha se ela estiver
    // entre Y e Y+16. assim conseguimos ignorar os sprites que nao aparecem nela.
    wire [9:0] fim_y = {2'b00, sp_y} + 10'd16;
    wire cruza = ({1'b0, linha_reg} >= {2'b00, sp_y}) && ({1'b0, linha_reg} < fim_y);

    // descobre qual das 16 linhas do sprite corresponde a linha atual da tela
    wire [8:0] linha_relativa = linha_reg - {1'b0, sp_y};

    wire [3:0] pixel_x = cont_pixel[3:0]; // coluna atual dentro do sprite (0 a 15)

    // pega a prioridade que ja esta gravada na mesma posicao X.
    // se o X estiver fora da tela, usa zero para nao acessar uma posicao invalida.
    wire [8:0] x_lido = (x_b < 10'd320) ? x_b[8:0] : 9'd0;
    wire [5:0] prio_atual = (pagina == 1'b0) ? prio_b[x_lido] : prio_a[x_lido];

    always @(posedge clk) begin
        if (reset) begin
            estado <= OCIOSO;
            pagina <= 1'b0;
            linha_pronta <= 1'b0;
            indice <= 6'd0;
            cont_pixel <= 5'd0;
            contador_limpeza <= 9'd0;
            val_a <= 1'b0;
            val_b <= 1'b0;
        end else begin
            linha_pronta <= 1'b0; // volta para zero para gerar um pulso de apenas 1 ciclo

            // no inicio de uma nova linha, o buffer que estava sendo montado
            // passa a ser lido e o outro fica livre para receber a proxima linha
            if (nova_linha_pulso)
                pagina <= ~pagina;

            // esse pipeline acompanha os dois ciclos de latencia da RAM
            // primeiro registra que existe um pedido, depois indica que o dado chegou
            val_a <= (estado == DESENHA) && (cont_pixel < 5'd16);
            val_b <= val_a;
            x_a <= {1'b0, sprite_x} + {6'd0, pixel_x}; // X final = posicao do sprite + coluna atual
            x_b <= x_a;

            // grava o pixel no buffer se ele chegou, nao for transparente, estiver
            // dentro da tela e tiver prioridade suficiente para ficar por cima do outro.
            // quando as prioridades empatam, o ultimo sprite desenhado vence.
            if (val_b && ram_dado != 8'd0 && x_b < 10'd320 && sprite_prio >= prio_atual) begin
                if (pagina == 1'b0) begin
                    buffer_b[x_b[8:0]] <= ram_dado;
                    prio_b[x_b[8:0]] <= sprite_prio;
                end else begin
                    buffer_a[x_b[8:0]] <= ram_dado;
                    prio_a[x_b[8:0]] <= sprite_prio;
                end
            end

            case (estado)
                OCIOSO: begin
                    // espera o pulso da nova linha e guarda qual linha precisa montar
                    if (nova_linha_pulso) begin
                        linha_reg <= linha_alvo;
                        contador_limpeza <= 9'd0;
                        estado <= LIMPAR_LINHA;
                    end
                end

                LIMPAR_LINHA: begin
                    // zera uma posicao por ciclo para nao sobrar pixel da linha anterior
                    if (pagina == 1'b0) begin
                        buffer_b[contador_limpeza] <= 8'd0;
                        prio_b[contador_limpeza] <= 6'd0;
                    end else begin
                        buffer_a[contador_limpeza] <= 8'd0;
                        prio_a[contador_limpeza] <= 6'd0;
                    end

                    if (contador_limpeza == 9'd319) begin
                        indice <= 6'd0; // terminou de limpar, comeca pelo primeiro sprite
                        estado <= VARRE;
                    end else begin
                        contador_limpeza <= contador_limpeza + 9'd1;
                    end
                end

                VARRE: begin
                    // olha se o sprite atual esta ativo e ocupa a linha que estamos montando
                    if (sp_ativo && cruza) begin
                        sprite_x <= sp_x;
                        sprite_espelho_h <= sp_esp_h;
                        sprite_espelho_v <= sp_esp_v;
                        sprite_padrao <= sp_padrao;
                        sprite_prio <= sp_prio;
                        pixel_y_no_sprite <= linha_relativa[3:0];
                        cont_pixel <= 5'd0;
                        estado <= DESENHA;
                    end else if (indice == 6'd63) begin
                        linha_pronta <= 1'b1; // ja verificou os 64 sprites e terminou a linha
                        estado <= OCIOSO;
                    end else begin
                        indice <= indice + 6'd1;
                    end
                end

                DESENHA: begin
                    // pede os 16 pixels da linha do sprite na RAM.
                    // o flip simplesmente inverte os bits da linha ou da coluna:
                    // em 4 bits, ~k equivale a 15-k, fazendo o espelhamento.
                    if (cont_pixel < 5'd16)
                        ram_endereco <= {sprite_padrao, sprite_espelho_v ? ~pixel_y_no_sprite : pixel_y_no_sprite, sprite_espelho_h ? ~pixel_x : pixel_x};

                    // depois do pixel 15, espera mais dois ciclos para os ultimos
                    // dados que ja foram pedidos terminarem de passar pelo pipeline
                    if (cont_pixel == 5'd17) begin
                        if (indice == 6'd63) begin
                            linha_pronta <= 1'b1; // terminou de desenhar o ultimo sprite
                            estado <= OCIOSO;
                        end else begin
                            indice <= indice + 6'd1; // passa para o proximo sprite
                            estado <= VARRE;
                        end
                    end else begin
                        cont_pixel <= cont_pixel + 5'd1;
                    end
                end

                default: begin
                    estado <= OCIOSO;
                end
            endcase
        end
    end

endmodule