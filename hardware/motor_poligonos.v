
// =============================================================================
// motor_poligonos.v - RASTERIZADOR DE POLIGONOS (por funcao de aresta)
// =============================================================================
// O QUE FAZ
//   Para cada pixel (logic_x, logic_y) que o VGA pede, diz se ele esta dentro
//   de algum dos 8 poligonos e, se estiver, qual cor usar. Nao existe frame
//   buffer de poligonos: a decisao e feita pixel a pixel, ao vivo.
//
// TRES ETAPAS REGISTRADAS (pipeline de 3 ciclos)
//   ETAPA 0 - PRE-CALCULO (depende so do banco, nao do pixel)
//       Cada poligono vira um quadrilatero A -> B -> C -> D. Os vertices saem
//       da posicao (X,Y), do tamanho (W,H) e da forma, usando SO somas e
//       deslocamentos (shifts), sem multiplicador nem divisor. Os flips
//       espelham os vertices dentro da caixa do poligono.
//       Resultado: 4 arestas por poligono = 32 arestas, cada uma guardada como
//       (origem ax, ay) e (vetor dx, dy).
//
//   ETAPA 1 - PRODUTO VETORIAL (32 arestas em paralelo)
//       Para o pixel P e a aresta A->B:
//           e = dx*(py - ay) - dy*(px - ax)
//       e o componente z do produto vetorial (B-A) x (P-A). O SINAL de e diz
//       de que lado da aresta o pixel esta. 32 arestas x 2 multiplicacoes
//       = 64 multiplicacoes 12x12 bits por ciclo (blocos DSP).
//
//   ETAPA 2 - DENTRO/FORA E PRIORIDADE
//       Pixel DENTRO do poligono se as 4 arestas tiverem o MESMO sinal
//       (todas >= 0 ou todas <= 0). Testar os dois sinais faz funcionar nos
//       dois sentidos de percurso (o flip inverte o sentido dos vertices).
//       Se mais de um poligono cobre o pixel, vence o de MAIOR ID.
//
// A cor sai com 3 ciclos de atraso; o ghrd_top compensa atrasando x/y
// (x_pipe1..3) antes do compositor.
// =============================================================================
module motor_poligonos (
    input  wire         clk,
    input  wire         reset,
    input  wire [9:0]   logic_x,         // coluna do pixel atual (0..319)
    input  wire [8:0]   logic_y,         // linha do pixel atual (0..239)
    input  wire [255:0] poly_attr,       // 8 x 32 bits de atributos (banco_poligonos)
    input  wire [135:0] poly_tam,        // 8 x 17 bits de tamanhos  (banco_poligonos)
    output reg  [7:0]   poly_color_out   // cor do pixel (0 = nenhum poligono)
);

    integer i, k;

    // =========================================================================
    // Pre-calculo por poligono (depende so dos atributos)
    // =========================================================================
    // saidas registradas: 8 poligonos x 4 arestas
    // (indice da aresta = poligono*4 + numero da aresta)
    reg signed [11:0] r_ax [0:31];   // vertice de origem da aresta
    reg signed [11:0] r_ay [0:31];
    reg signed [11:0] r_dx [0:31];   // Bx - Ax
    reg signed [11:0] r_dy [0:31];   // By - Ay
    reg               r_ok [0:7];    // ligado e com area valida
    reg        [7:0]  r_cor[0:7];

    // variaveis do bloco combinacional de pre-calculo
    // (12 bits COM SINAL: as diferencas e espelhamentos podem dar negativo)
    reg        p_enable, p_esp_h, p_esp_v;
    reg [2:0]  p_shape;
    reg signed [11:0] x0, y0, w, h, larg, alt, topo, xe, xd, yc, yb;
    reg signed [11:0] ax, ay, bx, by, cx, cy, dx, dy;
    reg               valida;

    // valores combinacionais (antes do registrador)
    reg signed [11:0] c_ax [0:31];
    reg signed [11:0] c_ay [0:31];
    reg signed [11:0] c_dx [0:31];
    reg signed [11:0] c_dy [0:31];
    reg               c_ok [0:7];
    reg        [7:0]  c_cor[0:7];

    // =========================================================================
    // ETAPA 0: vertices de cada poligono (combinacional, so somas e shifts)
    // =========================================================================
    always @(*) begin
        for (i = 0; i < 8; i = i + 1) begin
            // ---- extrai os campos do poligono i (mapa do banco_poligonos) ----
            p_enable = poly_attr[i*32 + 31];         // [31]    ativo
            p_shape  = poly_attr[i*32 + 30 -: 3];    // [30:28] forma
            p_esp_h  = poly_attr[i*32 + 19];         // [19]    flip H
            p_esp_v  = poly_attr[i*32 + 18];         // [18]    flip V
            x0 = $signed({3'b000,  poly_attr[i*32 + 17 -: 9]});   // [17:9] X
            y0 = $signed({4'b0000, poly_attr[i*32 + 8  -: 8]});   // [8:1]  Y
            w  = $signed({3'b000,  poly_tam[i*17 + 16 -: 9]});    // W
            h  = $signed({4'b0000, poly_tam[i*17 + 7  -: 8]});    // H

            // tamanho efetivo de cada forma (so shifts, sem divisor)
            //   >>> 3 = /8   >>> 6 = /64   >>> 1 = /2   >>> 4 = /16
            larg = w;
            case (p_shape)
                3'd0:    alt = w;                            // quadrado: W x W
                3'd3:    alt = w - (w >>> 3) - (w >>> 6);    // ~0,86*W
                                                             // (W - W/8 - W/64 = 0,859W,
                                                             //  altura do triangulo equilatero)
                default: alt = h;
            endcase
            // x do vertice de cima dos triangulos, medido a partir da esquerda
            topo = (p_shape == 3'd4) ? (w >>> 3) + (w >>> 4) + (w >>> 6)  // ~W/5
                                     : (w >>> 1);                         // W/2
                                     // (W/8 + W/16 + W/64 = 0,203W)

            // caixa do poligono: esquerda/direita (xe, xd), cima/baixo (yc, yb)
            xe = x0;  xd = x0 + larg - 1;
            yc = y0;  yb = y0 + alt  - 1;

            // tamanhos 0x0 ou 1x1 nao sao desenhados
            valida = (larg >= 2) && (alt >= 2);
            case (p_shape)
                // quadrado e retangulo: os 4 cantos, em sentido horario
                //   A(esq,cima) B(dir,cima) C(dir,baixo) D(esq,baixo)
                3'd0, 3'd1: begin
                    ax=xe; ay=yc;  bx=xd; by=yc;  cx=xd; cy=yb;  dx=xe; dy=yb;
                end
                // triangulos: A no topo, B embaixo a direita, C embaixo a esquerda
                // e D = A (vertice repetido). A aresta D->A tem vetor (0,0), logo
                // e = 0 para qualquer pixel: ela sempre "concorda" com as outras,
                // e o quadrilatero vira um triangulo.
                3'd2, 3'd3, 3'd4: begin
                    ax=x0+topo; ay=yc;  bx=xd; by=yb;  cx=xe; cy=yb;  dx=ax; dy=ay;
                end
                default: begin
                    ax=0; ay=0; bx=0; by=0; cx=0; cy=0; dx=0; dy=0;
                    valida = 1'b0;                    // forma inexistente
                end
            endcase

            // ---- flips: espelha cada vertice dentro da caixa ----
            //   horizontal: x' = xe + xd - x     vertical: y' = yc + yb - y
            if (p_esp_h) begin
                ax=xe+xd-ax; bx=xe+xd-bx; cx=xe+xd-cx; dx=xe+xd-dx;
            end
            if (p_esp_v) begin
                ay=yc+yb-ay; by=yc+yb-by; cy=yc+yb-cy; dy=yc+yb-dy;
            end

            // arestas A->B, B->C, C->D, D->A
            // cada aresta = origem (ax, ay) + vetor (dx, dy) = destino - origem
            c_ax[i*4+0] = ax;  c_ay[i*4+0] = ay;  c_dx[i*4+0] = bx-ax;  c_dy[i*4+0] = by-ay;
            c_ax[i*4+1] = bx;  c_ay[i*4+1] = by;  c_dx[i*4+1] = cx-bx;  c_dy[i*4+1] = cy-by;
            c_ax[i*4+2] = cx;  c_ay[i*4+2] = cy;  c_dx[i*4+2] = dx-cx;  c_dy[i*4+2] = dy-cy;
            c_ax[i*4+3] = dx;  c_ay[i*4+3] = dy;  c_dx[i*4+3] = ax-dx;  c_dy[i*4+3] = ay-dy;

            c_ok[i]  = p_enable && valida;           // so desenha se ativo e com area
            c_cor[i] = poly_attr[i*32 + 27 -: 8];    // [27:20] cor
        end
    end

    // registra o pre-calculo: corta o caminho combinacional longo
    // (assim as somas/shifts e as multiplicacoes ficam em ciclos diferentes)
    always @(posedge clk) begin
        for (k = 0; k < 32; k = k + 1) begin
            r_ax[k] <= c_ax[k];
            r_ay[k] <= c_ay[k];
            r_dx[k] <= c_dx[k];
            r_dy[k] <= c_dy[k];
        end
        for (k = 0; k < 8; k = k + 1) begin
            r_ok[k]  <= reset ? 1'b0 : c_ok[k];
            r_cor[k] <= c_cor[k];
        end
    end

    // =========================================================================
    // Estagio 1: funcao de aresta de cada uma das 32 arestas
    // =========================================================================
    // pixel atual em 12 bits com sinal (para subtrair dos vertices)
    wire signed [11:0] px = $signed({2'b00,  logic_x});
    wire signed [11:0] py = $signed({3'b000, logic_y});

    reg [31:0] s_pos, s_neg;     // aresta >= 0 / aresta <= 0
    reg [7:0]  s1_ok;            // r_ok atrasado 1 ciclo (alinhado com s_pos/s_neg)
    reg [7:0]  s1_cor [0:7];     // r_cor atrasado 1 ciclo

    // "generate" cria 32 copias deste hardware: as 32 arestas sao calculadas
    // AO MESMO TEMPO (2 multiplicadores por aresta = 64 multiplicadores)
    genvar g;
    generate
        for (g = 0; g < 32; g = g + 1) begin : aresta
            // vetor do vertice de origem ate o pixel: P - A
            wire signed [11:0] rx = px - r_ax[g];
            wire signed [11:0] ry = py - r_ay[g];
            // produto vetorial (componente z): (B-A) x (P-A)
            //   e = dx*ry - dy*rx
            wire signed [23:0] m1 = r_dx[g] * ry;     // 12x12 -> 24
            wire signed [23:0] m2 = r_dy[g] * rx;
            wire signed [24:0] e  = m1 - m2;

            // so o SINAL importa: guarda 2 bits por aresta
            always @(posedge clk) begin
                if (reset) begin
                    s_pos[g] <= 1'b0;
                    s_neg[g] <= 1'b0;
                end else begin
                    s_pos[g] <= (e >= 0);   // pixel do lado "positivo" (ou em cima da aresta)
                    s_neg[g] <= (e <= 0);   // pixel do lado "negativo" (ou em cima da aresta)
                end
            end
        end
    endgenerate

    // atrasa "ativo" e "cor" 1 ciclo para acompanhar os sinais das arestas
    always @(posedge clk) begin
        for (k = 0; k < 8; k = k + 1) begin
            s1_ok[k]  <= reset ? 1'b0 : r_ok[k];
            s1_cor[k] <= r_cor[k];
        end
    end

    // =========================================================================
    // Estagio 2: dentro/fora e prioridade (indice maior vence)
    // =========================================================================
    reg       achou;
    reg [7:0] cor;
    reg       hit;

    always @(*) begin
        achou = 1'b0;
        cor   = 8'd0;
        // percorre os poligonos do 0 ao 7; como cada acerto SOBRESCREVE o
        // anterior, o ultimo (maior ID) que cobrir o pixel e o que vale
        for (i = 0; i < 8; i = i + 1) begin
            // dentro = ativo E (as 4 arestas >= 0  OU  as 4 arestas <= 0)
            //   &s_pos[i*4 +: 4] -> AND dos 4 bits do poligono i
            hit = s1_ok[i] &&
                  ((&s_pos[i*4 +: 4]) || (&s_neg[i*4 +: 4]));
            if (hit) begin
                achou = 1'b1;
                cor   = s1_cor[i];
            end
        end
    end

    // registra a cor final (0 = nenhum poligono neste pixel -> transparente)
    always @(posedge clk) begin
        if (reset)
            poly_color_out <= 8'd0;
        else
            poly_color_out <= achou ? cor : 8'd0;
    end

endmodule