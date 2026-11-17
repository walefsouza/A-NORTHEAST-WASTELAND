module compositor (
    input [7:0] bg_index,      // indice de cor do fundo (background)
    input [7:0] poly_index,    // indice de cor vindo do motor de poligonos
    input [7:0] sprite_index,  // indice de cor vindo do sprite
    input [2:0] prioridade,    // {bg, poly, sprite}: 1 = camada promovida acima das demais
    output reg [7:0] final_index // indice de cor final que vai pra tela
);

    // decide qual camada aparece em cada pixel, por prioridade.
    // ordem padrao (prioridade = 3'b000): sprite > poligono > fundo
    //
    // o vetor prioridade tem um bit por motor: prioridade[0] = sprite,
    // prioridade[1] = poligono, prioridade[2] = fundo. bit em 1 promove aquela
    // camada: ela e avaliada ANTES das camadas nao promovidas. entre camadas
    // do mesmo grupo (promovidas ou nao promovidas) vale a ordem padrao
    // sprite > poligono > fundo.
    //
    // indice 0 e tratado como "transparente" (nao desenhado ali), por isso o
    // teste e != 8'd0 - se for 0 passa pra proxima camada
    //
    // como funciona: as atribuicoes vao da MENOR pra MAIOR prioridade, entao a
    // ultima atribuicao valida sobrescreve as anteriores e e a que vence.
    // se nenhuma camada tiver pixel, final_index fica 0
    always @(*) begin
        final_index = 8'd0;

        // grupo nao promovido (menor prioridade), do fundo pro topo
        if (!prioridade[2] && (bg_index != 8'd0))
            final_index = bg_index;       // fundo presente, so vence se nada acima dele desenhar
        if (!prioridade[1] && (poly_index != 8'd0))
            final_index = poly_index;     // poligono presente, cobre o fundo
        if (!prioridade[0] && (sprite_index != 8'd0))
            final_index = sprite_index;   // sprite presente, cobre poligono e fundo

        // grupo promovido (maior prioridade), do fundo pro topo
        if (prioridade[2] && (bg_index != 8'd0))
            final_index = bg_index;       // fundo promovido cobre tudo que nao foi promovido
        if (prioridade[1] && (poly_index != 8'd0))
            final_index = poly_index;     // poligono promovido cobre o fundo promovido
        if (prioridade[0] && (sprite_index != 8'd0))
            final_index = sprite_index;   // sprite promovido, topo absoluto quando presente
    end
endmodule