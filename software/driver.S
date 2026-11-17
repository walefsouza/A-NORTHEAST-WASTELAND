@ =====================================================================
@ driver.S - API do coprocessador grafico ANW (Assembly ARM, AAPCS)
@
@ O C chama funcoes com parametros normais; AQUI e montada a palavra de
@ 32 bits (opcode + campos da ISA) e feito o envio pelo PIO com strobe.
@ A aplicacao nao conhece opcode, bit de campo nem registrador.
@
@ Convencao: argumentos em r0..r3, retorno em r0.
@ Funcoes de desenho retornam 0 = aceito pela FIFO; -1 = parametro invalido;
@ -2 = timeout esperando espaco; -3 = timeout de ACK (resultado incerto).
@
@ Sincronismo:  valid = strobe 0->1 no PIO_CTRL
@               ack   = toggle em STATUS[2], apos push aceito na FIFO
@               busy  = FIFO cheia (STATUS[1])
@               done  = FIFO vazia e UC livre (esperar_fim)
@ =====================================================================

    .syntax unified
    .arch   armv7-a
    .arm
    .text

    .equ LW_BASE,    0xFF200000      @ ponte lightweight HPS->FPGA
    .equ LW_SPAN,    0x5000
    .equ PIO_INSTR,  0x00            @ ARM -> FPGA: instrucao
    .equ PIO_CTRL,   0x10            @ ARM -> FPGA: bit0 strobe, bit1 limpar
    .equ PIO_STATUS, 0x20            @ FPGA -> ARM: status

    .equ VAZIA,      0x01            @ status bit 0
    .equ CHEIA,      0x02            @ status bit 1
    .equ ACK,        0x04            @ status bit 2: toggle a cada push aceito
    .equ OCUPADA,    0x08            @ status bit 3
    .equ HANDSHAKE_TIMEOUT, 100000   @ limite provisório de leituras; calibrar em bancada
    .equ FRAME_TIMEOUT,     1000000  @ limite provisório; calibrar em bancada
    .equ DONE_TIMEOUT,      1000000  @ limite provisório; calibrar em bancada
                                     @ bit 5 erro, [23:8] quadros, bit 24 overflow

@ ---- macros -----------------------------------------------------------
@ BASE: r1 = endereco virtual da ponte (gravado pelo abrir)
    .macro BASE
    ldr     r1, =base
    ldr     r1, [r1]
    .endm

@ LIMITE: valor sem sinal >= max -> retorna -1 (negativo tambem cai aqui)
    .macro LIMITE reg, max
    cmp     \reg, #\max
    bhs     invalido
    .endm

@ FUNC: declara funcao exportada (o .type e obrigatorio: o C da placa
@ compila em Thumb e precisa saber que aqui e ARM, senao segfault)
    .macro FUNC nome
    .global \nome
    .type   \nome, %function
\nome:
    .endm

@ =====================================================================
@ 1) ACESSO: mapeamento MMIO, envio e status
@ =====================================================================

@ int abrir(void): open + mmap. 0 = ok; DRIVER_ERRO_IO (-7) em falha.
@ Se mmap falhar depois de open, fecha o fd e restaura fd=-1/base=0.
FUNC abrir
    push    {r4, lr}
    bl      fechar                  @ seguro tambem se ainda nao havia recursos abertos

    @ Estado inicial conhecido depois de fechar qualquer abertura anterior.
    ldr     r1, =base
    mov     r2, #0
    str     r2, [r1]
    ldr     r1, =fd
    mvn     r2, #0
    str     r2, [r1]

    ldr     r0, =caminho
    ldr     r1, =0x101002            @ O_RDWR | O_SYNC
    bl      open
    cmp     r0, #0
    blt     abrir_erro

    ldr     r1, =fd
    str     r0, [r1]
    mov     r4, r0

    sub     sp, sp, #8               @ mmap(0, LW_SPAN, 3, 1, fd, LW_BASE)
    str     r4, [sp]                 @ 5o e 6o argumentos vao na pilha
    ldr     r0, =LW_BASE
    str     r0, [sp, #4]
    mov     r0, #0
    ldr     r1, =LW_SPAN
    mov     r2, #3                   @ PROT_READ | PROT_WRITE
    mov     r3, #1                   @ MAP_SHARED
    bl      mmap
    add     sp, sp, #8

    cmn     r0, #1                   @ MAP_FAILED == -1?
    beq     abrir_mmap_erro

    ldr     r1, =base
    str     r0, [r1]
    mov     r0, #0
    pop     {r4, pc}

abrir_mmap_erro:
    @ open funcionou, mas mmap falhou: nao deixar o descritor vazando.
    ldr     r0, =fd
    ldr     r0, [r0]
    bl      close
    b       abrir_erro

abrir_erro:
    ldr     r1, =base
    mov     r2, #0
    str     r2, [r1]
    ldr     r1, =fd
    mvn     r2, #0                   @ fd = -1
    str     r2, [r1]
    mvn     r0, #6                   @ DRIVER_ERRO_IO = -7
    pop     {r4, pc}

@ void fechar(void): libera somente recursos que estao validos.
FUNC fechar
    push    {r4, lr}

    ldr     r4, =base
    ldr     r0, [r4]
    cmp     r0, #0
    beq     fechar_fd
    ldr     r1, =LW_SPAN
    bl      munmap
    mov     r0, #0
    str     r0, [r4]

fechar_fd:
    ldr     r4, =fd
    ldr     r0, [r4]
    cmp     r0, #0
    blt     fechar_fim
    bl      close
    mvn     r1, #0                   @ fd = -1
    ldr     r4, =fd
    str     r1, [r4]

fechar_fim:
    pop     {r4, pc}

@ segura: 16 leituras do status = tempo para a FPGA (25 MHz, 2 FFs de
@ sincronizacao) enxergar o nivel do strobe. Usa r2/r3, preserva r0/r1.
    .type   segura, %function
segura:
    mov     r2, #16
segura_laco:
    ldr     r3, [r1, #PIO_STATUS]
    subs    r2, r2, #1
    bne     segura_laco
    bx      lr

@ int enviar_inst(unsigned inst): espera espaco, envia DATA e aguarda ACK.
@ Retornos: 0 aceito; -2 timeout esperando espaco; -3 timeout de ACK
@ (resultado incerto: a instrucao pode ter sido aceita; nao reenviar cegamente).
FUNC enviar_inst
    push    {r4, r5, r6, lr}
    BASE

    @ Primeiro espera a FIFO ter espaco. Timeout evita espera infinita.
    ldr     r3, =HANDSHAKE_TIMEOUT
espera_fifo:
    ldr     r2, [r1, #PIO_STATUS]
    tst     r2, #CHEIA
    beq     fifo_disponivel
    subs    r3, r3, #1
    bne     espera_fifo
    mvn     r0, #1                   @ -2: timeout antes de iniciar a transferencia
    pop     {r4, r5, r6, pc}

fifo_disponivel:
    @ Guarda o ACK atual para reconhecer a proxima alternancia.
    and     r5, r2, #ACK

    @ DATA precisa estar visivel antes de VALID e permanecer estavel ate o ACK.
    str     r0, [r1, #PIO_INSTR]
    dmb     sy                      @ ordena a escrita MMIO de DATA antes de VALID
    mov     r4, #1
    str     r4, [r1, #PIO_CTRL]      @ VALID: strobe 0->1

    @ O ACK so muda quando fifo_hab_escrita && !fifo_cheia.
    ldr     r3, =HANDSHAKE_TIMEOUT
espera_ack:
    ldr     r2, [r1, #PIO_STATUS]
    and     r2, r2, #ACK
    cmp     r2, r5
    bne     ack_recebido
    subs    r3, r3, #1
    bne     espera_ack

    @ Timeout apos VALID: o resultado e incerto; rearma o strobe, mas nao reenvia.
    mov     r4, #0
    str     r4, [r1, #PIO_CTRL]
    dmb     sy
    bl      segura
    mvn     r0, #2                   @ -3: ACK nao observado; pode ter sido aceito
    pop     {r4, r5, r6, pc}

ack_recebido:
    @ A FIFO ja capturou DATA. Desativa VALID e espera a sincronizacao do nivel baixo
    @ para garantir uma nova borda de subida na proxima chamada.
    mov     r4, #0
    str     r4, [r1, #PIO_CTRL]
    dmb     sy
    bl      segura
    mov     r0, #0
    pop     {r4, r5, r6, pc}

@ unsigned ler_status(void): palavra crua do PIO_STATUS
FUNC ler_status
    BASE
    ldr     r0, [r1, #PIO_STATUS]
    bx      lr

@ int erro_inst(void): bit0 = opcode invalido, bit1 = overflow da FIFO
FUNC erro_inst
    BASE
    ldr     r2, [r1, #PIO_STATUS]
    ubfx    r0, r2, #5, #1           @ status[5]  -> bit 0
    ubfx    r2, r2, #24, #1          @ status[24] -> bit 1
    orr     r0, r0, r2, lsl #1
    bx      lr

@ void limpar_erro(void): pulso no bit 1 do PIO_CTRL
FUNC limpar_erro
    push    {r4, lr}
    BASE
    mov     r4, #2
    str     r4, [r1, #PIO_CTRL]
    bl      segura
    mov     r4, #0
    str     r4, [r1, #PIO_CTRL]
    bl      segura
    pop     {r4, pc}

@ =====================================================================
@ 2) CONTROLE DE QUADRO (o jogo usa para manter o ritmo sem usleep)
@ =====================================================================

@ unsigned quadro_atual(void): contador de quadros do hardware (16 bits)
FUNC quadro_atual
    BASE
    ldr     r0, [r1, #PIO_STATUS]
    ubfx    r0, r0, #8, #16
    bx      lr

@ int esperar_quadro(void): espera o contador mudar.
@ Sucesso: novo contador (0..65535); timeout: DRIVER_TIMEOUT_QUADRO (-4).
FUNC esperar_quadro
    BASE
    ldr     r0, [r1, #PIO_STATUS]
    ubfx    r0, r0, #8, #16          @ quadro de agora
    ldr     r3, =FRAME_TIMEOUT
quadro_laco:
    ldr     r2, [r1, #PIO_STATUS]
    ubfx    r2, r2, #8, #16
    cmp     r2, r0
    bne     quadro_novo
    subs    r3, r3, #1
    bne     quadro_laco
    mvn     r0, #3                   @ -4: timeout esperando novo quadro
    bx      lr
quadro_novo:
    mov     r0, r2
    bx      lr

@ int esperar_fim(void): espera FIFO vazia e UC livre.
@ Retorna 0, DRIVER_TIMEOUT_FIM (-5) ou DRIVER_ERRO_FPGA (-6).
FUNC esperar_fim
    BASE
    ldr     r3, =DONE_TIMEOUT
fim_laco:
    ldr     r2, [r1, #PIO_STATUS]
    and     r0, r2, #(VAZIA | OCUPADA)
    cmp     r0, #VAZIA
    beq     fim_concluido
    subs    r3, r3, #1
    bne     fim_laco
    mvn     r0, #4                   @ -5: timeout esperando conclusao
    bx      lr

fim_concluido:
    @ So depois de drenar a FIFO consultamos os erros persistentes da FPGA.
    ubfx    r0, r2, #5, #1           @ bit 0 do retorno: opcode invalido
    ubfx    r3, r2, #24, #1          @ bit 1 do retorno: overflow
    orr     r0, r0, r3, lsl #1
    cmp     r0, #0
    beq     fim_sem_erro
    mvn     r0, #5                   @ -6: erro reportado pela FPGA
    bx      lr
fim_sem_erro:
    mov     r0, #0
    bx      lr

@ =====================================================================
@ 3) COMANDOS DA ISA - cada funcao monta a palavra e salta para o envio
@    bfi rd, rn, #lsb, #largura  = coloca o campo rn nos bits da ISA
@    "b enviar_inst" = chamada de cauda: o envio retorna direto ao C
@ =====================================================================

invalido:                            @ destino comum dos LIMITE
    mvn     r0, #0                   @ -1, nada e enviado
    bx      lr

@ ---- controle ---------------------------------------------------------
@ int waitvblank(void): a UC para ate o proximo vblank
FUNC waitvblank
    mov     r0, #0x00
    b       enviar_inst

@ int pricomp(bg, pol, spr): prioridade de camadas no compositor
FUNC pricomp
    LIMITE  r0, 2
    LIMITE  r1, 2
    LIMITE  r2, 2
    mov     r3, #0x05
    bfi     r3, r2, #5, #1
    bfi     r3, r1, #6, #1
    bfi     r3, r0, #7, #1
    mov     r0, r3
    b       enviar_inst

@ ---- background -------------------------------------------------------
@ int delbg(liga): 1 = fundo ligado, 0 = desligado
FUNC delbg
    LIMITE  r0, 2
    mov     r3, #0x09
    bfi     r3, r0, #5, #1
    mov     r0, r3
    b       enviar_inst

@ int movbgx(x): scroll horizontal em tiles (0..39)
FUNC movbgx
    LIMITE  r0, 40
    mov     r3, #0x0A
    bfi     r3, r0, #10, #6
    mov     r0, r3
    b       enviar_inst

@ int movbgy(y): scroll vertical em tiles (0..29)
FUNC movbgy
    LIMITE  r0, 30
    mov     r3, #0x0E
    bfi     r3, r0, #10, #5
    mov     r0, r3
    b       enviar_inst

@ int pdrbg(endereco, tile): celula do tilemap (0..1199 = linha*40+col)
FUNC pdrbg
    LIMITE  r0, 1200
    LIMITE  r1, 256
    mov     r3, #0x0B
    bfi     r3, r1, #5, #8
    bfi     r3, r0, #13, #11
    mov     r0, r3
    b       enviar_inst

@ ---- sprites (id/padrao/prioridade 0..63) -----------------------------
@ int insspr(id, padrao)
FUNC insspr
    LIMITE  r0, 64
    LIMITE  r1, 64
    mov     r3, #0x10
    b       spr_id_campo

@ int pdrspr(id, padrao)
FUNC pdrspr
    LIMITE  r0, 64
    LIMITE  r1, 64
    mov     r3, #0x13
    b       spr_id_campo

@ int prispr(id, prioridade)
FUNC prispr
    LIMITE  r0, 64
    LIMITE  r1, 64
    mov     r3, #0x15
spr_id_campo:                        @ comum: ID [10:5] + campo [16:11]
    bfi     r3, r0, #5, #6
    bfi     r3, r1, #11, #6
    mov     r0, r3
    b       enviar_inst

@ int delspr(id)
FUNC delspr
    LIMITE  r0, 64
    mov     r3, #0x11
    bfi     r3, r0, #5, #6
    mov     r0, r3
    b       enviar_inst

@ int movspr(id, x, y): x 0..511, y 0..255 (tela visivel 320x240)
@ ATENCAO: layout de sprite = X [27:19], Y [18:11]; poligono = X [26:18], Y [17:10]
FUNC movspr
    LIMITE  r0, 64
    LIMITE  r1, 512
    LIMITE  r2, 256
    mov     r3, #0x12
    bfi     r3, r0, #5, #6
    bfi     r3, r1, #19, #9          @ X [27:19] (sprites: 1 bit acima dos poligonos)
    bfi     r3, r2, #11, #8          @ Y [18:11]
    mov     r0, r3
    b       enviar_inst

@ int invspr(id, h, v): espelhamento (sprite: H [12], V [11]; poligono: H [11], V [10])
FUNC invspr
    LIMITE  r0, 64
    LIMITE  r1, 2
    LIMITE  r2, 2
    mov     r3, #0x14
    bfi     r3, r0, #5, #6
    bfi     r3, r1, #12, #1          @ H [12]
    bfi     r3, r2, #11, #1          @ V [11]
    mov     r0, r3
    b       enviar_inst

@ int pixspr(endereco, cor): escreve um pixel na RAM de padroes de sprites
@ endereco = {padrao[5:0], linha[3:0], coluna[3:0]} (0..16383)
@ cor = indice RRRGGGBB (0..255)
@ Instrucao PIXSPR: endereco [26:13], cor [12:5], opcode [4:0]
FUNC pixspr
    LIMITE  r0, 16384
    LIMITE  r1, 256
    mov     r3, #0x16
    bfi     r3, r0, #13, #14
    bfi     r3, r1, #5, #8
    mov     r0, r3
    b       enviar_inst

@ int carregar_padrao(int padrao, const unsigned char pixels[256])
@ Carrega um sprite 16x16 (256 pixels) na RAM de padroes via PIXSPR.
@ r0 = padrao (0..63), r1 = ponteiro para 256 bytes
@ Retorno: 0 = ok, -1 = padrao fora da faixa, ponteiro NULL ou falha no envio
FUNC carregar_padrao
    LIMITE  r0, 64                   @ padrao 0..63 (sem sinal)
    cmp     r1, #0                   @ pixels == NULL ?
    beq     invalido

    push    {r4, r5, r6, lr}         @ callee-saved (AAPCS); 16 bytes = pilha alinhada
    lsl     r4, r0, #8               @ r4 = padrao * 256 = {padrao[5:0], 8'd0}
    mov     r5, r1                   @ r5 = ponteiro dos pixels
    mov     r6, #0                   @ r6 = indice do pixel (0..255)

laco_carregar_padrao:
    ldrb    r1, [r5, r6]             @ cor = pixels[r6]
    add     r0, r4, r6               @ endereco = (padrao << 8) + indice
    bl      pixspr                   @ monta PIXSPR e envia pela FIFO
    cmp     r0, #0                   @ pixspr falhou?
    bne     carregar_fim             @ propaga o erro (r0 = -1)
    add     r6, r6, #1
    cmp     r6, #256
    blt     laco_carregar_padrao

    mov     r0, #0                   @ sucesso
carregar_fim:
    pop     {r4, r5, r6, pc}

@ ---- poligonos (id 0..7) ---------------------------------------------
@ int inspol(id, forma, cor): forma 0 quadrado, 1 retangulo, 2 triangulo,
@ 3 triangulo equilatero, 4 triangulo escaleno (o rasterizador so tem 0..4)
FUNC inspol
    LIMITE  r0, 8
    LIMITE  r1, 5
    LIMITE  r2, 256
    mov     r3, #0x18
    bfi     r3, r0, #5, #3
    bfi     r3, r1, #10, #3
    bfi     r3, r2, #13, #8
    mov     r0, r3
    b       enviar_inst

@ int delpol(id)
FUNC delpol
    LIMITE  r0, 8
    mov     r3, #0x19
    bfi     r3, r0, #5, #3
    mov     r0, r3
    b       enviar_inst

@ int colpol(id, cor)
FUNC colpol
    LIMITE  r0, 8
    LIMITE  r1, 256
    mov     r3, #0x1B
    bfi     r3, r0, #5, #3
    bfi     r3, r1, #13, #8
    mov     r0, r3
    b       enviar_inst

@ int invpol(id, h, v)
FUNC invpol
    LIMITE  r0, 8
    LIMITE  r1, 2
    LIMITE  r2, 2
    mov     r3, #0x1C
    bfi     r3, r0, #5, #3
campo_hv:                            @ comum: H [11] + V [10]
    bfi     r3, r1, #11, #1
    bfi     r3, r2, #10, #1
    mov     r0, r3
    b       enviar_inst

@ int movpol(id, x, y)
FUNC movpol
    LIMITE  r0, 8
    LIMITE  r1, 512
    LIMITE  r2, 256
    mov     r3, #0x1A
    bfi     r3, r0, #5, #3
    b       campo_xy

@ int tampol(id, w, h): ordem obrigatoria TAMPOL -> INSPOL -> MOVPOL
FUNC tampol
    LIMITE  r0, 8
    LIMITE  r1, 512
    LIMITE  r2, 256
    mov     r3, #0x1D
    bfi     r3, r0, #5, #3
campo_xy:                            @ comum: X/W [26:18] + Y/H [17:10]
    bfi     r3, r1, #18, #9
    bfi     r3, r2, #10, #8
    mov     r0, r3
    b       enviar_inst

    .ltorg

    .data
    .align 2
base:    .word 0
fd:      .word -1
caminho: .asciz "/dev/mem"

    .section .note.GNU-stack,"",%progbits