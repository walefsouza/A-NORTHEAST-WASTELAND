module motor_background (
    input wire clk,
    input wire reset,

    input wire [9:0] logic_x, // posicao x do pixel atual
    input wire [8:0] logic_y, // posicao y do pixel atual

    input wire [31:0] out_scroll_x, // deslocamento horizontal do cenario
    input wire [31:0] out_scroll_y, // deslocamento vertical do cenario

    input wire cpu_we, // habilita escrita no tilemap
    input wire [10:0] cpu_addr, // endereco onde a CPU quer escrever
    input wire [7:0] cpu_data, // id do tile que sera colocado nessa posicao

    output wire [13:0] endereco_memoria_padroes // endereco final na ROM de padroes
);

    // cada tile ocupa 8x8 pixels, entao dividimos X e Y por 8 (>>3)
    // para descobrir em qual coluna e linha do mapa o pixel esta
    wire [5:0] coluna_base = logic_x[8:3];
    wire [4:0] linha_base = logic_y[7:3];

    // soma o scroll para deslocar a camera pelo mapa
    wire [6:0] soma_x = coluna_base + out_scroll_x[5:0];
    wire [5:0] soma_y = linha_base + out_scroll_y[4:0];

    // o mapa tem 40 colunas e 30 linhas. quando a soma passa desse limite,
    // subtrai o tamanho do mapa para voltar ao inicio (wraparound)
    wire [5:0] coluna = (soma_x >= 7'd40)
                      ? (soma_x - 7'd40)
                      : soma_x[5:0];
    wire [4:0] linha = (soma_y >= 6'd30)
                     ? (soma_y - 6'd30)
                     : soma_y[5:0];

    // pega a posicao do pixel dentro do tile (0 a 7).
    // essa parte nao muda com o scroll, porque usamos os 3 bits inferiores de X e Y
    wire [2:0] px_in = logic_x[2:0];
    wire [2:0] py_in = logic_y[2:0];

    // transforma linha e coluna em um endereco linear do tilemap.
    // como 40 = 32 + 8, usamos deslocamentos em vez de multiplicacao:
    // linha*40 = (linha << 5) + (linha << 3)
    wire [10:0] tilemap_addr = (linha << 5) + (linha << 3) + coluna;

    // a CPU escreve no tilemap pela porta de escrita, enquanto este motor
    // consulta a porta de leitura para descobrir qual tile aparece na tela.
    // as duas portas funcionam de forma independente.
    wire [7:0] tile_id;
    ram_tilemap ip_memoria_cenario (
        .data(cpu_data),
        .wraddress(cpu_addr),
        .wren(cpu_we),
        .wrclock(clk),
        .rdaddress(tilemap_addr),
        .rdclock(clk),
        .q(tile_id)
    );

    // a RAM do tilemap demora 1 ciclo para devolver o tile_id.
    // por isso atrasamos px e py pelo mesmo tempo, para o pixel dentro do tile
    // continuar correspondendo ao tile que acabou de chegar
    reg [2:0] px_r;
    reg [2:0] py_r;

    always @(posedge clk) begin
        if (reset) begin
            px_r <= 3'd0;
            py_r <= 3'd0;
        end else begin
            px_r <= px_in;
            py_r <= py_in;
        end
    end

    // monta o endereco da ROM de padroes juntando o tile escolhido com a
    // linha e a coluna do pixel dentro dele. cada tile ocupa 64 posicoes (8x8).
    // tile_id escolhe qual figura buscar, enquanto py_r e px_r escolhem o pixel.
    assign endereco_memoria_padroes = {tile_id, py_r, px_r};

endmodule