/* driver.h - API do coprocessador ANW (implementada em driver.S)
 *
 * Retornos das funcoes que enviam comandos:
 *   0  = sucesso / instrucao aceita pela FIFO (ACK)
 *  -1  = parametro fora da faixa (nao envia)
 *  -2  = timeout esperando espaco na FIFO (transferencia nao iniciada)
 *  -3  = timeout esperando ACK (resultado incerto; nao reenviar cegamente)
 *
 * Esperas:
 *  -4  = timeout esperando novo quadro
 *  -5  = timeout esperando FIFO vazia + UC livre
 *  -6  = erro reportado pela FPGA (consultar erro_inst())
 *  -7  = erro de open/mmap em abrir()
 *
 * Os limites atuais sao contagens de leituras MMIO provisórias; calibrar
 * com medidas na DE1-SoC antes de afirmar um limite temporal em milissegundos.
 * ACK confirma ACEITACAO na FIFO, nao conclusao da execucao.
 * Faixas: sprite id 0..63, padrao/prioridade 0..63, poligono id 0..7,
 *         x/w 0..511, y/h 0..255 (tela 320x240), cor 0..255 (RRRGGGBB,
 *         0 = transparente), forma 0..4, h/v/flags 0..1,
 *         scroll x 0..39 e y 0..29 (em tiles), tilemap 0..1199, tile 0..255.
 */
#ifndef DRIVER_H
#define DRIVER_H

#define DRIVER_OK                  0
#define DRIVER_ERRO_PARAMETRO     (-1)
#define DRIVER_TIMEOUT_FIFO       (-2)
#define DRIVER_TIMEOUT_ACK        (-3)
#define DRIVER_TIMEOUT_QUADRO     (-4)
#define DRIVER_TIMEOUT_FIM        (-5)
#define DRIVER_ERRO_FPGA          (-6)
#define DRIVER_ERRO_IO            (-7)

/* acesso */
int      abrir(void);              /* 0 ok; DRIVER_ERRO_IO se open/mmap falhar */
void     fechar(void);             /* desfaz mmap e fecha o descritor de memoria */
int      enviar_inst(unsigned w);  /* palavra crua: 0 apos ACK de aceitacao */
unsigned ler_status(void);         /* STATUS[2] = ACK toggle; [31:25] = done_cnt */
int      erro_inst(void);          /* bit0 opcode invalido, bit1 overflow */
void     limpar_erro(void);        /* limpa flags de erro pendentes no hardware */

/* controle de quadro */
unsigned quadro_atual(void);       /* contador de quadros (16 bits) */
int      esperar_quadro(void);     /* contador novo (0..65535) ou timeout negativo */
int      esperar_fim(void);        /* 0, timeout ou DRIVER_ERRO_FPGA */

/* controle */
int waitvblank(void);              /* instrucao de sincronizacao com o VBlank */
int pricomp(int bg, int pol, int spr); /* prioridades do compositor (bg, poligonos, sprites) */

/* background */
int delbg(int liga);               /* ativa (1) ou desativa (0) a exibicao do tilemap */
int movbgx(int x);                 /* define o deslocamento horizontal (scroll X: 0..39) */
int movbgy(int y);                 /* define o deslocamento vertical (scroll Y: 0..29) */
int pdrbg(int endereco, int tile); /* grava o indice do tile (0..255) na posicao (0..1199) */

/* sprites */
int insspr(int id, int padrao);    /* ativa o sprite (0..63) com o padrao grafico (0..63) */
int delspr(int id);                /* desativa e oculta o sprite (0..63) */
int movspr(int id, int x, int y);  /* posiciona o sprite nas coordenadas (x: 0..511, y: 0..255) */
int pdrspr(int id, int padrao);    /* altera o padrao grafico (0..63) de um sprite ativo */
int invspr(int id, int h, int v);  /* espelha o sprite horizontalmente (h) e/ou verticalmente (v) */
int prispr(int id, int prioridade);/* define a prioridade de camada do sprite (0..63) */
int pixspr(int endereco, int cor); /* endereco 0..16383: 64 padroes x 16 x 16 */
int carregar_padrao(int padrao, const unsigned char pixels[256]); /* grava matriz 16x16 completa no padrao */

/* poligonos */
int inspol(int id, int forma, int cor); /* ativa o poligono (0..7) com forma (0..4) e cor RRRGGGBB */
int delpol(int id);                /* desativa e oculta o poligono (0..7) */
int movpol(int id, int x, int y);  /* posiciona o poligono nas coordenadas (x, y) */
int colpol(int id, int cor);       /* altera a cor do poligono (0..255, RRRGGGBB) */
int invpol(int id, int h, int v);  /* espelha o poligono horizontalmente (h) e/ou verticalmente (v) */
int tampol(int id, int w, int h);  /* define largura (w: 0..511) e altura (h: 0..255) do poligono */

#endif