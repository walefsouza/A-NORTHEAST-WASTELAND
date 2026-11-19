# A Northeast Wasteland: Console Digital com Coprocessador Gráfico em FPGA (DE1-SoC)

## Descrição do Projeto

Etapa final do console digital dedicado desenvolvido na disciplina. Nas fases anteriores, foi realizada a construção o **coprocessador gráfico programável** em FPGA (Problemas 1 e 2), com *ISA* própria de 32 bits, fila de instruções, unidade de controle, bancos de registradores e motores de *background*, *sprites* e polígonos, além do *driver* em *Assembly* ARM que o acessa por *MMIO*. Nesta fase, o produto final foi entregue: o jogo ***A Northeast Wasteland***, um jogo 2D de visão superior em ambientação nordestina, escrito em linguagem C, executado no processador ARM (*HPS*) da DE1-SoC sob Linux, controlado por teclado e mouse USB e exibido em monitor VGA.

Toda a imagem é gerada pelo coprocessador. A aplicação em C não acessa registradores, não desenha *pixels* e não conhece os detalhes internos do hardware: ela calcula a física e o estado da partida e compõe a cena exclusivamente por meio da API do *driver* em *Assembly*. Para viabilizar o jogo, o coprocessador sofreu ajustes de hardware (64 *sprites*, RAM de padrões gravável, ROM de *tiles* ampliada e sinal de confirmação de aceitação, ou *ACK*), e o *driver* foi estendido com validação de parâmetros, tratamento de *timeouts* e códigos de erro padronizados, mantendo a compatibilidade com a *ISA* documentada no Problema 2.

## Contexto Acadêmico e Autoria

Este projeto foi desenvolvido como parte do componente curricular **TEC499 - MI Sistemas Digitais**, ministrado pelo professor **Wild Freitas da Silva Santos**, na **Universidade Estadual de Feira de Santana (UEFS)**, como parte da metodologia de Aprendizagem Baseada em Projetos (PBL).

**Equipe:**
* Airam Santos Macêdo ([@imthepassionfruit-lab](https://github.com/imthepassionfruit-lab));
* Nicolas Carvalho Pimentel ([@nicolascrp07](https://github.com/nicolascrp07));
* Walef Souza da Silva ([@walefsouza](https://github.com/walefsouza)).

## Sumário

1. [Requisitos Funcionais e Não Funcionais](#1-requisitos-funcionais-e-não-funcionais)
2. [O Produto: A Northeast Wasteland](#2-o-produto-a-northeast-wasteland)
3. [Arquitetura Geral do Sistema](#3-arquitetura-geral-do-sistema)
4. [Mudanças no Hardware](#4-mudanças-no-hardware)
5. [Driver em Assembly ARM e API](#5-driver-em-assembly-arm-e-api)
6. [Arquitetura de Software da Aplicação](#6-arquitetura-de-software-da-aplicação)
7. [Integração Hardware-Software](#7-integração-hardware-software)
8. [Software e Hardware usados para Prototipação](#8-software-e-hardware-usados-para-prototipação)
9. [Demonstração e Testes de Funcionamento](#9-demonstração-e-testes-de-funcionamento)
10. [Análise de Recursos e de Desempenho](#10-análise-de-recursos-e-de-desempenho)
11. [Instalação, Configuração e Execução](#11-instalação-configuração-e-execução)
12. [Decisões Técnicas Registradas](#12-decisões-técnicas-registradas)
13. [Limitações e Possíveis Melhorias](#13-limitações-e-possíveis-melhorias)
14. [Referências Bibliográficas](#14-referências)

## 1. Requisitos Funcionais e Não Funcionais

### **1.1. Funcionais:**

* **RF01**: Aplicação do jogo em linguagem C, executada no processador ARM (*HPS*) da DE1-SoC sob Linux, com saída em monitor VGA.
* **RF02**: Tela de abertura e menu inicial, partida com pontuação e progressão de dificuldade, pausa, reinício e fim de jogo com exibição da pontuação final.
* **RF03**: Lógica do jogo estruturada como máquina de estados finitos (abertura, partida, transição, pausa e fim de jogo/vitória).
* **RF04**: Mecânica com pontuação, movimentação de objetos e eliminação de inimigos e agrupamentos (bandos), conforme a proposta do problema de referência.
* **RF05**: Leitura simultânea de teclado e mouse USB conectados à placa, com movimentação, mira, disparo e pausa por entradas combinadas.
* **RF06**: Atualização da cena exclusivamente por meio da API do *driver* (tiles, *sprites* e polígonos), sem bibliotecas gráficas de terceiros e sem desenho por *pixel* no processador.
* **RF07**: *Driver* em *Assembly* ARM com API estável, validação de parâmetros, carregamento de recursos gráficos, comandos de desenho, controle de quadro e consulta de status.
* **RF08**: Tratamento da sincronização com o coprocessador (*valid*, *busy*, *done*) e dos comandos inválidos, com propagação de erro até a aplicação.
* **RF09**: Medição de desempenho integrada: atualizações da lógica por segundo, taxa de vídeo, latência de entrada, latência de comandos e uso de CPU.

### **1.2. Não Funcionais:**

* **RNF01**: Taxa de atualização da lógica de, no mínimo, **30 atualizações por segundo**.
* **RNF02**: Resposta a comandos de entrada em, no máximo, **100 ms**.
* **RNF03**: Imagem estável, sem perda de sincronismo, durante toda a execução.
* **RNF04**: Arquitetura modular em camadas (aplicação em C, *driver* em *Assembly*, coprocessador em FPGA), com a lógica do jogo isolada do hardware.
* **RNF05**: Ajustes no coprocessador devem preservar a compatibilidade com a *ISA* documentada no Problema 2.
* **RNF06**: Fechamento de *timing* do domínio de 25 MHz do coprocessador, sem violações de *setup* e *hold*.
* **RNF07**: Fluxo de compilação e instruções de execução reproduzíveis a partir do repositório.

## 2. O Produto: A Northeast Wasteland

<!-- PENDENTE: capturas de tela do jogo (fases manhã e noite); arquivo esperado: images/fase3/produto-controles.png. Descomentar ao adicionar a imagem.
<div align="center">
  <img src="images/fase3/produto-controles.png" alt="Controles e capturas do jogo" width="900">
  <br>
  <i>Figura ?: Esquema de controles e capturas de tela do jogo.</i>
</div>
<br>
-->

<!-- INSERIR: slide 3 (PRODUTO: A NORTHEAST WASTELAND), capturas das fases manhã e noite (o esquema de controles já está na seção 2.1) -->

O jogo coloca o jogador no papel de um cangaceiro que enfrenta, em três fases (manhã, tarde e noite), ondas de inimigos inspirados na fauna e no folclore do sertão. A fase avança conforme as ondas se esgotam, e a última termina com um chefe.

### 2.1. Controles

| Entrada | Ação |
|:---|:---|
| **W A S D** | Movimentação do cangaceiro (diagonal por combinação de duas teclas) |
| **Setas** | Disparo na direção indicada (duas setas: diagonal) |
| **Mouse (movimento)** | Posição da mira e navegação nos menus |
| **Mouse (botão esquerdo)** | Disparo na direção do cursor; confirma botões nos menus |
| **ENTER** | Confirma e avança (menu, transição, fim de jogo); retoma a partida na pausa |
| **P** | Pausa e retoma a partida |
| **R** | Volta ao menu (a partir da pausa ou do fim de jogo) |
| **ESC** | Encerra o jogo |

<div align="center">
  <img src="images/controles_anw.png" alt="Esquema de controles do jogo" width="900">
  <br>
  <i>Figura 1: Esquema de controles do jogo (teclado e mouse).</i>
</div>
<br>

> [!NOTE]
> As teclas **N** e **B** são atalhos de depuração que levam diretamente à fase 3 e ao chefe, respectivamente, e existem apenas para demonstração e teste.

### 2.2. Fases e inimigos

<div align="center">

Fase | Ondas | Inimigos | Observações
:---|:---|:---|:---
Manhã | 3 | calango (persegue), marimbondo (aproxima, para e dispara), cacto (lento, atira espinhos) | Onda 1 apenas com calangos
Tarde | 3 | cascavel (persegue em zigue-zague), urubu (bando que cruza a tela) | Velocidades alternadas por fileira no bando
Noite | 2 + chefe | morcego (voo errático, em bando), corpo-seco, escorpião, lobisomem (elite, a partir da onda 2) | O chefe alterna perseguição, investida e garras em leque

</div>

### 2.3. Itens (*power-ups*)

O jogo possui uma tabela de nove itens, com duração em quadros (60 Hz) e peso no sorteio de *drop*:

<div align="center">

Item | Tipo | Efeito
:---|:---|:---
Bacamarte | Arma (10 s) | Tiro triplo em leque
Estrela | Arma (8 s) | Tiro em 8 direções
Cartucheira | *Buff* (10 s) | Cadência de tiro dobrada
Rapadura | *Buff* (10 s) | Velocidade +50%
Cantil | Instantâneo | Escudo que absorve 1 dano
Fogueira | Instantâneo | Elimina os inimigos comuns da tela (exceto o chefe)
Coração | Instantâneo | +1 vida (até 3)
Moedas | Instantâneo | +50 ou +250 pontos

</div>

## 3. Arquitetura Geral do Sistema

O sistema é organizado nas três camadas exigidas pelo enunciado. A aplicação em C só conhece a API do *driver*; o *driver* é a única fronteira com o hardware; e o coprocessador gera continuamente o sinal VGA.

<div align="center">
  <img src="images/arquitetura-camadas.png" alt="Arquitetura em três camadas" width="700">
  <br>
  <i>Figura 2: Arquitetura em três camadas (aplicação C, driver Assembly e coprocessador FPGA).</i>
</div>
<br>

### 3.1. Responsabilidades por camada

* **Aplicação (C):** laço principal, máquina de estados, física, colisões, inteligência dos inimigos, leitura de teclado e mouse, e composição da cena. Opera sobre estruturas em RAM e nunca manipula bits da *ISA*.
* ***Driver* (*Assembly* ARMv7):** mapeia a ponte *Lightweight HPS-to-FPGA* via `/dev/mem`, valida os parâmetros, monta a palavra de 32 bits da *ISA* com a instrução `bfi` e executa o protocolo de sincronização com a FPGA.
* **Coprocessador (FPGA):** recebe as instruções em uma *FIFO*, decodifica e atualiza os bancos de registradores, e gera a imagem VGA de forma contínua, independente da execução das instruções.

### 3.2. Pipeline de um comando gráfico

```text
gfx_sprite() ─► movspr() ─► enviar_inst() ─► PIO_INSTR/PIO_CTRL ─► FIFO (32) ─► UC ─► banco ─► motor ─► compositor ─► VGA
   (C)           (ASM)        (ASM)             (ponte LW)            (FPGA)
```

## 4. Mudanças no Hardware

Para que o coprocessador atendesse às necessidades do jogo, o hardware do Problema 2 recebeu os ajustes resumidos abaixo. Nenhum deles altera o significado das instruções documentadas anteriormente: as instruções existentes continuam válidas, e a única instrução nova (PIXSPR) ocupa um *opcode* antes livre.

### 4.1. Resumo das alterações

<div align="center">

Item | Antes (Fase 2) | Depois (Fase 3)
:---|:---|:---
*Sprites* | 32 (ID de 5 bits) | 64 (ID de 6 bits)
Registro do *sprite* | padrão [30:26], X [23:15], Y [14:7], prioridade [6:2] | padrão [30:25], H [24], V [23], X [22:14], Y [13:6], prioridade [5:0]
Prioridade | 5 bits (0 a 31) | 6 bits (0 a 63)
Padrões de *sprite* | ROM fixa, 32 padrões | RAM gravável, 64 padrões
Endereço dos padrões | 13 bits | 14 bits
Escrita de *pixel* | não existia | instrução PIXSPR
ROM de *tiles* | 11 bits (2.048 posições, 32 *tiles*) | 14 bits (16.384 posições, 256 *tiles*)
Bit 2 do status | fifo_quase_cheia | ACK (*toggle* por *push* aceito)
Travessia de *clock* do ACK | não existia | 2 *flip-flops* (25 MHz → 50 MHz)
Campos da UC (*sprite*) | ID [9:5], X [26:18], Y [17:10] | ID [10:5], X [27:19], Y [18:11]
Restrição de *timing* do `clk_25` | não documentada na fase 2 | `create_generated_clock` no SDC

</div>

### 4.2. Sessenta e quatro *sprites*

O jogo precisa de uma quantidade de objetos simultâneos superior aos 32 *sprites* da fase anterior: o *pool* de entidades ocupa 47 IDs (jogador, até 12 obstáculos, inimigos, tiros e itens) e a interface (mira, vidas, pontos, ícones e texto) ocupa outros 17. Por isso, o `banco_sprites` passou a ter **64 registradores de 32 bits**, e o ID, o padrão e a prioridade passaram a 6 bits. O mapa de bits de cada registrador é:

<div align="center">

Bits | Campo | Descrição
:---|:---|:---
[31] | ATIVO | 1 = o *sprite* é desenhado
[30:25] | PADRÃO | Índice do padrão gráfico (0 a 63)
[24] | H | Espelhamento horizontal
[23] | V | Espelhamento vertical
[22:14] | X | Posição horizontal (0 a 511)
[13:6] | Y | Posição vertical (0 a 255)
[5:0] | PRIORIDADE | Prioridade na composição (0 a 63)

</div>

O banco continua com uma porta de leitura individual (usada pelo *read-modify-write* da UC) e exporta os 64 registradores em um barramento de 2.048 bits.

### 4.3. Atualização da *ISA*

A *ISA* passou a ter **19 instruções**. Os campos das instruções de *sprite* foram deslocados em 1 bit para acomodar o ID de 6 bits, e foi acrescentada a instrução PIXSPR no grupo de *sprites* (*opcode*[4:3] = 10).

<div align="center">

Grupo | *opcode*[4:3] | Instruções
:---|:---|:---
Controle | 00 | WAITVBLANK, PRICOMP
*Background* | 01 | DELBG, MOVBGX, PDRBG, MOVBGY
*Sprites* | 10 | INSSPR, DELSPR, MOVSPR, PDRSPR, INVSPR, PRISPR, **PIXSPR**
Polígonos | 11 | INSPOL, DELPOL, MOVPOL, COLPOL, INVPOL, TAMPOL

</div>

<details>
<summary><h3>Instruções de Sprites (formato atualizado)</h3></summary>

Todas as instruções de *sprites* usam o ID nos bits [10:5] (0 a 63).

#### INSSPR, PDRSPR e PRISPR

<div align="center">

Nome do Campo | Descrição | Tamanho | Bit final | Bit inicial
:---|:---|:---|:---|:---
Opcode | 10000 (INSSPR), 10011 (PDRSPR) ou 10101 (PRISPR) | 5 bits | 4 | 0
ID | Índice do *sprite* | 6 bits | 10 | 5
OP | Padrão (INSSPR, PDRSPR) ou prioridade (PRISPR), 0 a 63 | 6 bits | 16 | 11
Não usados | | 15 bits | 31 | 17

</div>

> **Atenção:** INSSPR ativa o *sprite* e zera posição, espelho e prioridade. Para que o *sprite* apareça no lugar desejado, deve-se enviar MOVSPR em seguida.

#### DELSPR

<div align="center">

Nome do Campo | Descrição | Tamanho | Bit final | Bit inicial
:---|:---|:---|:---|:---
Opcode | Código da operação (10001) | 5 bits | 4 | 0
ID | Índice do *sprite* | 6 bits | 10 | 5
Não usados | | 21 bits | 31 | 11

</div>

#### MOVSPR

<div align="center">

Nome do Campo | Descrição | Tamanho | Bit final | Bit inicial
:---|:---|:---|:---|:---
Opcode | Código da operação (10010) | 5 bits | 4 | 0
ID | Índice do *sprite* | 6 bits | 10 | 5
Y | Posição vertical (0 a 255) | 8 bits | 18 | 11
X | Posição horizontal (0 a 511) | 9 bits | 27 | 19
Não usados | | 4 bits | 31 | 28

</div>

#### INVSPR

<div align="center">

Nome do Campo | Descrição | Tamanho | Bit final | Bit inicial
:---|:---|:---|:---|:---
Opcode | Código da operação (10100) | 5 bits | 4 | 0
ID | Índice do *sprite* | 6 bits | 10 | 5
V | Espelho vertical | 1 bit | 11 | 11
H | Espelho horizontal | 1 bit | 12 | 12
Não usados | | 19 bits | 31 | 13

</div>

> **Nota:** nas instruções de polígonos, os campos H e V ficam em [11] e [10], respectivamente. Os formatos de polígono, *background* e controle permaneceram inalterados em relação à fase anterior.

#### PIXSPR (nova)

<div align="center">

Nome do Campo | Descrição | Tamanho | Bit final | Bit inicial
:---|:---|:---|:---|:---
Opcode | Código da operação (10110, 0x16) | 5 bits | 4 | 0
COR | Cor do *pixel* (RRRGGGBB; 0 = transparente) | 8 bits | 12 | 5
ENDEREÇO | {padrão[5:0], linha[3:0], coluna[3:0]} | 14 bits | 26 | 13
Não usados | | 5 bits | 31 | 27

</div>

Grava um *pixel* na RAM de padrões de *sprites*. O endereço de 14 bits comporta 64 padrões de 16×16 *pixels* (64 × 256 = 16.384 posições).

</details>

### 4.4. RAM de padrões de *sprites* e instrução PIXSPR

Na fase anterior, os desenhos dos *sprites* ficavam em uma ROM inicializada por arquivo `.mif`, o que exigia nova síntese a cada alteração de arte. Nesta fase, os padrões passaram para uma **RAM de duas portas de 16.384 posições × 8 bits** (`ram_sprites`), com blocos M10K:

* **porta de escrita:** alimentada pela UC ao executar PIXSPR (`pad_we`, `pad_addr`, `pad_data`);
* **porta de leitura:** consultada pelo motor de *sprites* com o endereço {padrão, linha, coluna}.

Com isso, o jogo carrega os desenhos em tempo de execução, pelo *driver*, e a mesma síntese serve a qualquer conjunto de *sprites*. O arquivo `padroes_sprites.mif` permanece como conteúdo inicial da memória.

> [!NOTE]
> Um padrão de 16×16 *pixels* custa 256 instruções PIXSPR. A entrada em uma partida carrega os 45 padrões comuns mais os da fase (7, 12 ou 18 padrões, conforme manhã, tarde ou noite), o que equivale a 13.312, 14.592 e 16.128 instruções, respectivamente. O menu carrega 38 padrões (9.728 instruções).

### 4.5. ROM de *tiles* ampliada

A ROM de padrões do *background* (`rom_tiles`, inicializada por `padroes_tiles.mif`) passou de 11 para **14 bits de endereço**, ou seja, de 2.048 para 16.384 posições de 8 bits. Como cada *tile* ocupa 64 posições (8×8 *pixels*), a capacidade foi de 32 para **256 *tiles* distintos**. O endereço é montado pelo `motor_background` como {tile_id[7:0], linha[2:0], coluna[2:0]}.

A instrução PDRBG permaneceu com o mesmo formato: ela grava o ID do *tile* (8 bits, [12:5]) em uma posição do *tilemap* (11 bits, [23:13]), com posições visíveis de 0 a 1.199 (linha × 40 + coluna). O que mudou foi o repertório de *tiles* disponível. Os *pixels* dos padrões continuam residindo em ROM, definidos em síntese, enquanto o mapa (qual *tile* está em cada célula) é escrito em tempo de execução.

### 4.6. Registrador de status e confirmação de aceitação (ACK)

O bit 2 do registrador de status, antes ocupado pela *flag* fifo_quase_cheia, passou a ser o **ACK**: um bit que **alterna (*toggle*) a cada instrução efetivamente aceita pela FIFO**. A alternância só ocorre quando há escrita solicitada e a fila não está cheia (`fifo_hab_escrita && !fifo_cheia`). Uma tentativa de escrita com a fila cheia não gera ACK e ativa a *flag* de *overflow*.

<div align="center">

Bits | Campo | Descrição
:---|:---|:---
[0] | fifo_vazia | Fila sem instruções pendentes
[1] | fifo_cheia | Fila com 32 instruções
[2] | **ack_toggle** | **Alterna a cada instrução aceita pela FIFO**
[3] | uc_ocupada | UC executando ou com instruções pendentes
[4] | esperando_vblank | UC parada em ESPERA_VBLANK
[5] | erro_opcode | Um *opcode* inválido foi descartado
[6] | em_vblank | Varredura dentro do *vertical blank*
[7] | front_sel | Página do *frame buffer* exibida no momento
[23:8] | contador_quadros | Contador de quadros de 16 bits
[24] | overflow | Houve tentativa de escrita com a fila cheia
[31:25] | done_cnt | Contador de instruções concluídas (circular, 7 bits)

</div>

Como o ACK é um *toggle*, e não um pulso, o ARM o enxerga mesmo lendo o status por consulta (*polling*): basta comparar o valor lido antes do envio com o valor lido depois. A indicação de fila quase cheia deixou de existir no status, mas continua visível no LEDR[2] da placa.

> [!IMPORTANT]
> **ACK não é *done*.** O ACK confirma que a instrução **entrou na FIFO**. A **execução concluída** é indicada pelo *done* global (FIFO vazia e UC livre), consultado pela função `esperar_fim()` do *driver*.

### 4.7. Travessia de *clock* do ACK

O coprocessador opera em 25 MHz (`clk_25`, derivado do `CLOCK_50` por divisão por 2), enquanto os PIOs do *Platform Designer* operam em 50 MHz. Como o ACK é gerado no domínio de 25 MHz e lido pelo domínio de 50 MHz, o bit passa por um **sincronizador de 2 *flip-flops*** (`ack_pio_meta` e `ack_pio_sync`) antes de chegar ao PIO_STATUS.

No sentido contrário (HPS → FPGA), o *strobe* de escrita e o pedido de limpeza de erro já passavam por sincronizadores de 2 estágios, e o *strobe* é convertido em um único pulso de escrita por um detector de borda de subida. A palavra de dados de 32 bits (PIO_INSTR) não é sincronizada bit a bit: ela é escrita pelo ARM antes de o *strobe* subir e permanece estável até o ACK, e a FPGA só a amostra após detectar a borda do *strobe* já sincronizado.

> [!NOTE]
> Os demais bits do status (contador de quadros, *flags* de FIFO etc.) seguem diretamente do domínio de 25 MHz para o PIO, sem sincronizadores individuais. Como esse registrador é consultado apenas por leitura, o *software* inclui uma verificação diagnóstica de coerência (`perf_status.h`): ela acusa saltos impossíveis do contador de quadros entre leituras próximas e FIFO simultaneamente vazia e cheia. Essa verificação detecta sintomas de leitura inconsistente, mas não substitui a análise de CDC.

### 4.8. Motor de *sprites* com 64 *sprites*

O motor mantém a organização da fase anterior (dois *line buffers* de 320 posições, com cor e prioridade, trocados a cada linha lógica), com a FSM de montagem em quatro estados (OCIOSO, LIMPAR_LINHA, VARRE e DESENHA). As mudanças foram a varredura de **64** *sprites* e a prioridade de **6 bits**.

O custo de montagem de uma linha, calculado a partir da FSM, é:

* limpeza dos 320 *pixels* do *buffer* de escrita: 320 ciclos;
* para cada *sprite* que cruza a linha: 1 ciclo de varredura + 18 ciclos de desenho (16 *pixels* + 2 de esvaziamento do *pipeline*) = 19 ciclos;
* para cada *sprite* que não cruza: 1 ciclo.

Cada linha lógica dura duas linhas VGA, ou **1.600 ciclos** de 25 MHz. No pior caso teórico, com os 64 *sprites* cruzando a mesma linha, o custo é de 320 + 64 × 19 = **1.536 ciclos (96% do tempo disponível)**. Na fase anterior, o pior caso (32 *sprites*) usava cerca de 58%. A folga ainda existe, mas é pequena.

> [!NOTE]
> **Prioridade entre *sprites*:** vence o *sprite* com maior valor de prioridade (PRISPR). Em caso de empate, vence o de maior ID, pois é o último a ser desenhado.

### 4.9. Restrições de *timing* (SDC)

O arquivo `soc_system_timing.sdc` declara explicitamente o *clock* de 25 MHz gerado pelo divisor do `ghrd_top`:

```tcl
create_generated_clock -name clk_25 \
    -source [get_ports {CLOCK_50}] \
    -divide_by 2 \
    [get_registers {clk_25}]
```

Com a declaração, o *Timing Analyzer* trata o `clk_25` como um *clock* relacionado ao `CLOCK_50` (relação 2:1), e não como um domínio assíncrono, o que representa o circuito real. Os resultados da análise estão na seção 10.2.

### 4.10. O que não foi alterado

* **Paleta programável:** dispensada pelo tutor para esta fase. As cores são fornecidas diretamente no formato adotado pelo projeto (RRRGGGBB de 8 bits, com o índice 0 como transparente).
* **FIFO, UC, motores de polígonos e *background*, compositor, *frame buffer* e controlador VGA:** mantidos conforme a fase anterior (FIFO de 32 posições, UC com 3 estados, 2 ciclos por instrução, VGA 640×480 a 60 Hz com *double buffering* 320×240).

## 5. Driver em Assembly ARM e API

O *driver* (`driver.S` e `driver.h`) é a única fronteira entre a aplicação em C e o coprocessador. Na fase anterior, ele apenas transportava palavras de 32 bits montadas pelo programa de teste. Nesta fase, a montagem das instruções foi incorporada ao *driver*: cada comando recebe parâmetros comuns em C, valida as faixas, monta a palavra da *ISA* e a envia. A aplicação não conhece *opcodes*, posições de campo nem registradores.

### 5.1. Convenção de chamadas e organização

* **ABI ARM (AAPCS):** argumentos em r0 a r3 e retorno em r0. Funções que usam registradores *callee-saved* (r4 a r6) os preservam na pilha.
* **Mapeamento:** `abrir()` abre `/dev/mem` com `O_RDWR | O_SYNC` e mapeia a ponte *Lightweight HPS-to-FPGA* (base 0xFF200000, 0x5000 bytes).
* **Registradores acessados:** PIO_INSTR (offset 0x00, ARM → FPGA), PIO_CTRL (0x10: bit 0 = *strobe* de escrita, bit 1 = limpar erro) e PIO_STATUS (0x20, FPGA → ARM).
* **Padrão de cada comando:** valida os parâmetros (macro `LIMITE`), monta a palavra com `bfi` (inserção de campo de bits) e faz uma chamada de cauda (`b enviar_inst`), de modo que o retorno do envio volta diretamente ao C.

### 5.2. Protocolo de envio (`enviar_inst`)

O envio implementa o protocolo de sincronização com os sinais *valid*, *busy*/*ready* e *ack*:

<div align="center">

Sinal | Implementação | Interpretação
:---|:---|:---
**DATA** | PIO_INSTR (HPS → FPGA) | Palavra de 32 bits, estável do momento em que é escrita até o ACK
**VALID** | PIO_CTRL[0], *strobe* 0→1 | Solicita um *push* na FIFO
**READY / BUSY** | STATUS[1] = FIFO cheia | Com a FIFO cheia, o ARM não envia; com espaço, pode enviar
**ACK** | STATUS[2], *toggle* | Confirma que a instrução entrou na FIFO
**DONE** | STATUS[0] = 1 e STATUS[3] = 0 | FIFO vazia e UC livre: todo o lote pendente foi executado

</div>

Sequência executada por `enviar_inst(inst)`:

1. **Espera de espaço:** lê o status até `FIFO_CHEIA = 0`, com limite de tentativas. Se estourar, retorna `DRIVER_TIMEOUT_FIFO` (-2) sem ter iniciado a transferência.
2. **Registro do ACK atual:** guarda o valor do bit 2 para detectar a próxima alternância.
3. **Escrita do dado:** escreve a palavra em PIO_INSTR.
4. **Barreira de memória:** executa `dmb sy`, garantindo que a escrita de DATA seja ordenada antes de VALID.
5. **VALID:** escreve 1 em PIO_CTRL[0] (borda 0→1 do *strobe*).
6. **Espera do ACK:** lê o status até o bit 2 mudar, com limite de tentativas. Se estourar, baixa o *strobe*, aguarda e retorna `DRIVER_TIMEOUT_ACK` (-3).
7. **Rearmar:** baixa PIO_CTRL[0] e realiza 16 leituras do status para que a FPGA enxergue o nível baixo antes do próximo envio, garantindo uma nova borda de subida na chamada seguinte.

<!-- PENDENTE: diagrama temporal do handshake; arquivo esperado: images/fase3/handshake-temporal.png. Descomentar ao adicionar a imagem.
<div align="center">
  <img src="images/fase3/handshake-temporal.png" alt="Diagrama temporal do handshake" width="800">
  <br>
  <i>Figura ?: Diagrama temporal do protocolo de envio (DATA, VALID, ACK e rearme).</i>
</div>
<br>
-->

<!-- INSERIR (SUGESTÃO, não existe nos slides): diagrama temporal DATA/VALID/FIFO cheia/ACK/rearme -->

> [!WARNING]
> **Timeout após VALID deixa o resultado incerto.** Se o ACK não for observado, a instrução pode ou não ter sido aceita. Por isso, o *driver* **não reenvia automaticamente**: reenviar às cegas poderia duplicar um comando. A aplicação decide como recuperar (consultar o status, reiniciar a cena etc.).

Uma consequência de projeto relevante: o *driver* **não espera a UC ficar livre antes de cada envio**. Ele só espera haver espaço na FIFO, preservando o benefício do *buffer* (o ARM pode enfileirar até 32 instruções enquanto a UC consome).

### 5.3. Códigos de retorno

Todas as funções que enviam comandos retornam um código padronizado, declarado em `driver.h`:

<div align="center">

Código | Constante | Significado
:---|:---|:---
0 | `DRIVER_OK` | Aceito (para esperas: concluído)
-1 | `DRIVER_ERRO_PARAMETRO` | Parâmetro fora da faixa; nada é enviado
-2 | `DRIVER_TIMEOUT_FIFO` | Timeout esperando espaço na FIFO (transferência não iniciada)
-3 | `DRIVER_TIMEOUT_ACK` | Timeout esperando o ACK (resultado incerto)
-4 | `DRIVER_TIMEOUT_QUADRO` | Timeout esperando um novo quadro
-5 | `DRIVER_TIMEOUT_FIM` | Timeout esperando FIFO vazia e UC livre
-6 | `DRIVER_ERRO_FPGA` | Erro reportado pela FPGA (opcode inválido ou *overflow*)
-7 | `DRIVER_ERRO_IO` | Falha em `open`/`mmap` em `abrir()`

</div>

<div align="center">
  <img src="images/tabela_codigos.png" alt="Códigos de retorno do driver" width="700">
  <br>
  <i>Figura 3: Códigos de retorno do <i>driver</i> (resumo visual).</i>
</div>
<br>

> [!NOTE]
> Os limites de espera são **contagens de leituras MMIO** (100.000 para o envio e 1.000.000 para quadro e conclusão), declarados como provisórios no código. Eles garantem que nenhuma espera seja infinita, mas **não correspondem a um intervalo de tempo calibrado**.

### 5.4. Funções da API

<div align="center">

Grupo | Função | Descrição
:---|:---|:---
Acesso | `abrir()` / `fechar()` | Mapeia/desmapeia a ponte. Se o `mmap` falhar após o `open`, o descritor é fechado (sem vazamento)
Acesso | `enviar_inst(w)` | Envia uma palavra crua de 32 bits com o protocolo acima
Acesso | `ler_status()` | Retorna o PIO_STATUS cru
Acesso | `erro_inst()` | Bit 0 = opcode inválido; bit 1 = *overflow*
Acesso | `limpar_erro()` | Pulso em PIO_CTRL[1]
Quadro | `quadro_atual()` | Contador de quadros do hardware (16 bits)
Quadro | `esperar_quadro()` | Espera o contador mudar; retorna o novo valor ou `DRIVER_TIMEOUT_QUADRO`
Quadro | `esperar_fim()` | Espera FIFO vazia e UC livre; retorna 0, `DRIVER_TIMEOUT_FIM` ou `DRIVER_ERRO_FPGA`
Controle | `waitvblank()`, `pricomp(bg, pol, spr)` | WAITVBLANK e PRICOMP
*Background* | `delbg(liga)`, `movbgx(x)`, `movbgy(y)`, `pdrbg(end, tile)` | Liga/desliga, *scroll* (0 a 39 e 0 a 29) e *tilemap* (0 a 1.199, *tile* 0 a 255)
*Sprites* | `insspr`, `delspr`, `movspr`, `pdrspr`, `invspr`, `prispr` | IDs, padrões e prioridades de 0 a 63; X de 0 a 511; Y de 0 a 255
*Sprites* | `pixspr(end, cor)` | Escreve um *pixel* na RAM de padrões (endereço 0 a 16.383, cor 0 a 255)
*Sprites* | `carregar_padrao(padrao, pixels[256])` | Carrega um padrão 16×16 por 256 chamadas de PIXSPR; interrompe e propaga o erro na primeira falha
Polígonos | `inspol`, `delpol`, `movpol`, `colpol`, `invpol`, `tampol` | IDs de 0 a 7; forma de 0 a 4; X/largura de 0 a 511; Y/altura de 0 a 255

</div>

### 5.5. Correções e restrições de uso

* **INVSPR:** o campo H fica no bit 12 e o V no bit 11, conforme a decodificação da UC (os polígonos usam os bits 11 e 10).
* **Formas de polígono:** a função `inspol()` aceita apenas as formas 0 a 4, as que o rasterizador implementa.
* **Ordem de criação de polígonos:** TAMPOL → INSPOL → MOVPOL. A UC zera o tamanho em DELPOL, e INSPOL mantém o tamanho, mas zera posição e espelho.
* **Ordem de criação de *sprites*:** INSSPR zera posição, espelho e prioridade; MOVSPR deve ser enviado em seguida.
* **Um único produtor:** o envio é serial e a API não é segura para uso por múltiplas *threads* sem arbitragem externa. No jogo, apenas a *thread* principal escreve no PIO.
* **Erros persistentes:** as *flags* de opcode inválido e de *overflow* permanecem ativas até `limpar_erro()`.

## 6. Arquitetura de Software da Aplicação

A aplicação foi organizada em **seis blocos funcionais**, com dependências unidirecionais até a fronteira do *driver*. O princípio central é o **desacoplamento**: nenhum módulo de lógica ou mecânica escreve em registradores mapeados em memória; toda interação com o hardware ocorre por meio do contrato `driver.h`.

<div align="center">
  <img src="images/arquitetura_software.png" alt="Arquitetura de software" width="900">
  <br>
  <i>Figura 4: Arquitetura de software e fluxo entre os módulos.</i>
</div>
<br>


### 6.1. Bloco 1: Núcleo e Telemetria

Responsável pelo ciclo de vida do processo, pelo contexto global da campanha e pela medição de desempenho.

* **`main.c`:** ponto de entrada. Lê os caminhos do teclado e do mouse por argumento ou pelas variáveis `ANW_TECLADO` e `ANW_MOUSE` (se faltar algum, exibe a forma de uso e encerra com código 1), abre o *driver* (`abrir`), inicia as entradas (`entrada_iniciar`), liga o medidor de desempenho e coloca o jogo no menu. O **laço principal** executa, a cada quadro:
  1. `esperar_quadro()`: sincroniza com o próximo quadro de vídeo da FPGA;
  2. `teclas_quadro()`: captura uma única vez o estado das entradas;
  3. `estado_rodar_quadro()`: lógica e desenho do estado atual;
  4. verificação do erro gráfico acumulado (`gfx_ultimo_erro()`);
  5. `esperar_fim()`: espera a FIFO esvaziar e a UC ficar livre;
  6. conferência das *flags* da FPGA (opcode inválido e *overflow*).
  
  **Política de falhas:** qualquer erro de *driver* (*timeout*, erro gráfico, *flag* da FPGA) interrompe o laço, libera os recursos e encerra com **código de saída 2**; encerramento normal retorna 0. A escolha por interromper (*fail-stop*) evita que o jogo continue com uma cena inconsistente.
* **`jogo.c` / `jogo.h`:** estrutura global `Jogo`, que persiste entre as fases: fase (0 manhã, 1 tarde, 2 noite), onda, pontos, vidas, `venceu` e `sair`. `jogo_novo()` zera a campanha (fase 0, onda 0, 0 pontos e 3 vidas) e `jogo_pontuar()` soma pontos com teto de 999.999, o limite dos 6 dígitos do HUD.
* **`config.h`:** constantes globais: tela de 320×240, *tile* de 8 px (mapa de 40×30), *sprite* de 16×16, e a divisão dos 64 IDs de *sprite* (seção 6.7). Define também o `EntityType` (jogador, NPC, inimigo, projétil e item).
* **`perf.c` / `perf.h` / `perf_status.h`:** instrumentação (seção 10.3).

### 6.2. Bloco 2: Máquina de Estados Finitos (FSM)

<div align="center">
  <img src="images/maquina_estados.png" alt="Máquina de estados do jogo" width="900">
  <br>
  <i>Figura 5: Máquina de estados do jogo e interface de estados (<code>estado.c</code>).</i>
</div>
<br>

Cada estado é uma estrutura com **três ponteiros de função**: `entrar` (executa uma vez ao entrar), `atualizar` (somente lógica) e `desenhar` (somente comandos ao hardware). Essa separação isola as regras do jogo do envio de comandos ao coprocessador.

* **`estado.c` / `estado.h`:** gerenciador. `estado_ir_para()` apenas **agenda** a troca; ela acontece no início do próximo quadro (`estado_rodar_quadro()`), nunca no meio de uma atualização.
* **Estados (cinco):**

<div align="center">

Estado | Arquivo | Função
:---|:---|:---
Menu | `estado_menu.c` | Tela inicial com botões JOGAR e SAIR (navegação por W/S, setas ou mouse; ENTER ou clique confirma). O fundo é um *tilemap*, e os botões trocam de *tiles* ao serem selecionados
Transição | `estado_transicao.c` | Tela "FASE n" com o nome da fase, por 150 quadros (2,5 s) ou até ENTER
Partida | `estado_partida.c` | *Gameplay*: carrega os recursos da fase na entrada e, a cada quadro, atualiza jogador, ondas, entidades e itens
Pausa | `estado_pausa.c` | Painel de polígonos sobre a partida congelada; P ou ENTER retoma, R volta ao menu
Game over / Vitória | `estado_gameover.c` | Tela de encerramento (`jogo.venceu` distingue derrota de vitória), com a pontuação final, botões JOGAR (ENTER) e MENU (R)

</div>

* **Transições:** Menu → Transição → Partida; Partida → Pausa → Partida (retomada) ou Menu; Partida → Transição (fase concluída com `fase < 3`); Partida → Game over/Vitória (vidas = 0 ou fase 3 concluída); Game over → Transição (jogar de novo) ou Menu.
* **Pausa e retomada:** a pausa devolve o controle à partida **sem reiniciá-la**. Uma *flag* interna (`retomando`) faz o `entrar` da partida apenas fechar o painel de pausa, preservando vidas, pontos, ondas e entidades. A *flag* é limpa quando a partida é abandonada pela pausa para o menu (`partida_cancelar_retomada()`), de modo que uma nova partida sempre começa do estado inicial.

### 6.3. Bloco 3: Subsistema de Entrada (*Linux Input*)

<div align="center">
  <img src="images/fluxo_entrada.png" alt="Fluxo da interface de entrada" width="900">
  <br>
  <i>Figura 6: Fluxo da leitura de teclado e mouse até o laço do jogo.</i>
</div>
<br>

* **`entrada.c` / `entrada.h`:** lê os dispositivos de entrada do Linux (`/dev/input/...`) diretamente, com **uma *thread* por dispositivo** (teclado e mouse), cada uma esperando eventos com `poll()` em paralelo ao jogo. Isso impede que a espera por entrada bloqueie o processamento dos quadros. O estado (teclas, botões e posição do cursor) é compartilhado e protegido por ***mutex***.
  * **Posição do mouse:** os eventos relativos (REL_X, REL_Y) são acumulados e limitados à tela (0 a 319 e 0 a 239).
  * **Robustez:** `EVIOCGRAB` (uso exclusivo do dispositivo) é tentado, mas sua falha apenas emite um aviso, e o jogo continua; após `SYN_DROPPED` (perda de eventos), o estado é ressincronizado; se um dispositivo for removido, o estado é limpo e a *thread* tenta reabri-lo periodicamente pelo mesmo caminho (útil com *links* estáveis de `/dev/input/by-id`).
  * **Latência:** o *timestamp* do evento no *kernel* é obtido com o relógio `CLOCK_MONOTONIC` (via `EVIOCSCLOCKID`) e usado pelo medidor de desempenho.
* **`teclas.c` / `teclas.h`:** a função `teclas_quadro()` roda **uma vez por quadro** e guarda uma "fotografia" das teclas e do botão esquerdo do mouse, garantindo que toda a lógica do quadro enxergue o mesmo estado. Duas consultas derivam dela:
  * `tecla_apertada(k)`: a tecla está pressionada neste instante (usada para movimento);
  * `tecla_pressionou(k)`: verdadeira **apenas no quadro em que a tecla desceu** (`atual && !anterior`), usada para menus e pausa; `mouse_clicou()` aplica a mesma detecção de borda ao botão esquerdo.

O módulo exige privilégios de administrador (acesso a `/dev/input` e `/dev/mem`) e a compilação com `-pthread`.

<div align="center">
  <img src="images/modulo_entrada.png" alt="Módulo de entrada" width="900">
  <br>
  <i>Figura 7: Visão geral do módulo de entrada (<code>entrada.c</code> e <code>teclas.c</code>): dispositivos, <i>threads</i>, estado e uso pelo jogo.</i>
</div>
<br>

<div align="center">
  <img src="images/anw_entrada.gif" alt="Painel de teste da leitura de entradas" width="900">
  <br>
  <i>Figura 8: Painel de teste da leitura de teclado e mouse: estado das teclas e posição do cursor lidos pelas funções de <code>teclas.h</code> e <code>entrada.h</code>.</i>
</div>
<br>

### 6.4. Bloco 4: Domínio do Jogo e Mecânicas

Concentra as regras do jogo, a inteligência dos inimigos e a física 2D, operando exclusivamente sobre estruturas em RAM.

* **`entity.c` / `entity.h`:** a **entidade universal**. Uma única estrutura (`Entity`) serve a jogador, obstáculos, inimigos, tiros e itens: posição, dimensões, área de colisão (`solidArea`), velocidade, direção, animação, padrão do *sprite* na FPGA e dois ponteiros de função (`update` e `interact`), que funcionam como métodos por tipo. As entidades vivem em um ***pool* fixo de 47 vagas** (`entities[47]`), sem alocação dinâmica: `entity_spawn` ocupa a primeira vaga livre e `entity_despawn` a zera. **O índice da entidade no *pool* é também o ID do *sprite* na FPGA**, o que dispensa qualquer tabela de associação.

<div align="center">
  <img src="images/entity.png" alt="Entidade universal e pool" width="900">
  <br>
  <i>Figura 9: Entidade universal, <i>pool</i> fixo de 47 vagas e funções de <code>entity.c</code>.</i>
</div>
<br>

* **`collision.c` / `collision.h` e `mapas.c` / `mapas.h`:** colisão por **AABB** (caixas alinhadas aos eixos). A colisão é testada **antes** do movimento, com a área de colisão deslocada pela velocidade: `collision_try_move()` executa `checkBounds` (bordas da tela), `checkTile` (*tiles* sólidos do mapa) e `checkEntity` (entidades sólidas), e só chama `entity_move()` se nenhuma bloquear. `checkTile` converte pixel em célula (÷ 8) e consulta a tabela `tiles_solidos[id]`, que `mapa_colisao[fase]` associa ao desenho da fase. O jogador usa colisão por eixo, de modo que o movimento diagonal é um passo em cada eixo. Atualmente, apenas os *tiles* de borda de cada fase (IDs 9, 20 e 31) são sólidos; os obstáculos internos (pedras e mandacarus) são entidades sólidas.

<div align="center">
  <img src="images/mapas.png" alt="Colisão com tiles" width="900">
  <br>
  <i>Figura 10: Cálculo de colisão com os <i>tiles</i> do mapa (jogador movendo-se para a esquerda).</i>
</div>
<br>

<div align="center">
  <img src="images/collision.png" alt="Colisão com entidade" width="900">
  <br>
  <i>Figura 11: Cálculo de colisão com uma entidade sólida (jogador movendo-se para a direita, contra um mandacaru).</i>
</div>
<br>

* **`jogador.c` / `jogador.h`:** o jogador é sempre a entidade 0 do *pool*. Move-se com WASD, mira com as setas ou com o cursor do mouse (botão esquerdo atira na direção do cursor), com cadência controlada por contador de quadros. Ao levar dano, fica invulnerável por alguns segundos (o *sprite* pisca); o escudo (cantil) absorve um dano. Possui uma área de contato menor que o *sprite* (mais justa para o jogador) e animação de morte.
* **`projeteis.c` / `projeteis.h`:** os tiros usam o mesmo *pool* de entidades, com posição em **ponto fixo q4** (16 unidades = 1 pixel por quadro), o que dispensa ponto flutuante. Há limite de tiros por dono (10 do jogador e 6 dos inimigos). O tiro desaparece ao sair da tela ou ao bater em pedra ou mandacaru; a bala do jogador causa dano ao inimigo, e o espinho do inimigo causa dano ao jogador.
* **`inimigos.c` / `inimigos.h`:** um comportamento por tipo, por *update* próprio (calango persegue; marimbondo aproxima, para e dispara em linha reta; cacto fica a distância e atira espinhos; cascavel persegue em zigue-zague; urubu cruza a tela em bando; morcego tem voo errático, com troca de rumo a cada 24 quadros; escorpião anda de lado até alinhar e dá o bote; lobisomem é a elite; o chefe da fase 3 alterna perseguição, investida e garras em leque e, ao enfurecer, fica mais rápido). Todo inimigo nasce **piscando por 0,5 s** (`AVISO_FRAMES`) e não causa nem sofre dano nesse período. **Bandos:** urubus e morcegos nascem em grupos de 3 a 4 unidades.
* **`powerups.c` / `powerups.h`:** tabela dos nove itens (nome, padrão, duração em quadros e peso no sorteio), *drops* ao eliminar inimigos (chance de 10%), um item garantido por onda, no máximo 2 itens na tela, e aplicação dos efeitos e temporizadores. O HUD mostra o ícone do último item temporizado com uma barra do tempo restante.
* **`fase.c` / `fase.h` e `ondas.c` / `ondas.h`:** `fase.c` descreve cada fase em uma estrutura (*tilemap*, cargas de padrões, número de ondas, chefe, elite, *pool* de inimigos por onda e 12 obstáculos fixos, sempre com o centro livre). O *pool* de inimigos de cada onda é um **sorteio ponderado**: repetir o tipo na lista o torna mais comum. `ondas.c` é o **diretor de ondas**:
  * cada onda dura 1.800 quadros (cerca de 30 s);
  * o intervalo entre *spawns* cai de 90 para 48 quadros (1,5 s para 0,8 s), e o máximo de inimigos na tela sobe de 12 para 16, interpolados entre a primeira e a última onda (**progressão de dificuldade**);
  * *spawn* seguro: nasce na borda da tela, a pelo menos 96 px do jogador, nunca na borda para a qual ele caminha e nunca dentro de obstáculo;
  * ao fim da última onda entra o chefe (se a fase tiver um), e a fase termina quando ele é derrotado; fases sem chefe limpam a tela e concluem.
* **`mat.c` / `mat.h`:** utilitários numéricos sem `math.h`: gerador pseudoaleatório (*xorshift*), raiz quadrada inteira, valor absoluto, sinal e vetor normalizado em ponto fixo q4.

> [!IMPORTANT]
> **Agrupamentos e eliminação.** A mecânica de agrupamentos do jogo se dá por **bandos**: urubus e morcegos nascem em grupos de 3 a 4 unidades e podem ser eliminados individualmente por tiros (cada inimigo vale seus pontos) ou **coletivamente** pelo item Fogueira, que elimina todos os inimigos comuns da tela e pontua cada um (o chefe não é afetado). Não há associação persistente entre os integrantes de um bando (identificador de grupo), e o avanço de onda ocorre pelo tempo, não pela eliminação do grupo. A equivalência dessa regra com a do problema de referência deve ser conferida e registrada na seção 12.

### 6.5. Bloco 5: Camada de Apresentação e Recursos

Traduz o estado da simulação em chamadas ao *driver*. O jogo nunca fala diretamente com o *driver*: usa `gfx_sprite()`, `gfx_fundo()` e `gfx_poligono()`.

* **`gfx.c` / `gfx.h`:** camada gráfica com três responsabilidades principais.
  * **Cache do estado do hardware** (64 *sprites*, 8 polígonos e o *tilemap* de 40×30), com validade por campo (padrão, prioridade, posição, forma, cor, tamanho). A camada só envia um comando quando o campo mudou, poupando a FIFO. 
  * **Cache só após aceitação:** o cache é atualizado **apenas depois que o *driver* retorna sucesso (ACK)**. Se um comando falha no meio de uma operação composta, o cache não afirma que os comandos seguintes foram aceitos.
  * **Propagação de erro:** o primeiro erro fica registrado até `gfx_limpar_erro()`, e o `main()` consulta `gfx_ultimo_erro()` a cada quadro. Funções chamadas sem tratamento de retorno não escondem falhas.
  * **Validação e conversões:** IDs, padrões, formas e cores são validados antes do envio; coordenadas fora da faixa do hardware tornam o *sprite* invisível (remoção); no polígono, a ordem exigida pelo hardware, TAMPOL antes de INSPOL, é respeitada; a prioridade do *sprite* é derivada da posição vertical, `(y + 16) >> 2` (quem está mais abaixo aparece na frente); o espelhamento é guardado como "desejado" e enviado somente quando muda, permitindo que um inimigo vire para o jogador sem *sprites* duplicados.
  * `gfx_entidades()` percorre o *pool* de 47 entidades uma vez por quadro e envia ao hardware apenas o que mudou.
* **`sprites.c` / `sprites.h`:** dados visuais: matrizes de 256 *pixels* (1 byte RRRGGGBB por *pixel*, 0 = transparente) de cada padrão, organizadas em tabelas de carga (`CARGA_COMUM`, `CARGA_MANHA`, `CARGA_TARDE`, `CARGA_NOITE` e `CARGA_MENU`) e o mapa de posições fixas dos padrões (jogador, itens, dígitos, letras, inimigos e chefe).
* **`mapas.c` / `mapas.h` e `menu_mapas.c` / `menu_mapas.h`:** `mapas.c` guarda os *tilemaps* de 40×30 das três fases e a tabela de *tiles* sólidos; `menu_mapas.c` guarda os *tilemaps* das telas de título, game over, vitória e transição, além dos *tiles* dos botões (normal e selecionado).
* **`hud.c` / `hud.h` e `texto.c` / `texto.h`:** o HUD mostra pontos (6 dígitos), corações de vida, ícone e barra do *power-up* ativo, ícone de escudo, barra de tempo da onda e banners ("ONDA 2", "CHEFE"). O hardware não possui fonte: **cada letra é um *sprite* 16×16**, com avanço de 14 px por letra e 8 px por espaço, e duas fontes (a do jogo, com dígitos e poucas letras, e a do menu, com 0-9, A-Z e "!"). As barras e o painel de pausa usam o **motor de polígonos** (retângulos coloridos), sem consumir *sprites*.

### 6.6. Orçamento de recursos gráficos

<div align="center">
  <img src="images/config_h.png" alt="Constantes de config.h e divisão dos sprites" width="900">
  <br>
  <i>Figura 12: Constantes globais de <code>config.h</code> e divisão dos 64 IDs de <i>sprite</i> da FPGA.</i>
</div>
<br>

<div align="center">

Recurso | IDs | Uso
:---|:---|:---
*Sprites* (*pool* de entidades) | 0 a 46 | Jogador (0), até 12 obstáculos, inimigos, tiros e itens
*Sprites* (interface) | 47 a 63 | Mira (47), vidas (48 a 50), pontos (51 a 56), ícone de *power-up* (57), escudo (58), texto (59 a 63)
Polígonos | 0 a 7 | Barra de tempo da onda (0), barra do item (1), painel de pausa (2 e 3), botões de game over/vitória (4 a 7)

</div>

> [!NOTE]
> Nos estados de menu, transição e game over, os IDs de *sprite* são reutilizados pelo texto da tela, e a mira do mouse usa o ID 63.

### 6.7. Matriz consolidada de módulos

<div align="center">

Subsistema | Arquivos | Responsabilidade | Principais conexões
:---|:---|:---|:---
1. Núcleo e Telemetria | `main.c`, `jogo.c/.h`, `config.h`, `perf.c/.h`, `perf_status.h` | Inicialização, laço principal, contexto global e medição | Aciona entrada, estados e `gfx`; consulta `driver.h`
2. Máquina de Estados | `estado.c/.h`, `estado_menu.c`, `estado_partida.c`, `estado_pausa.c`, `estado_transicao.c`, `estado_gameover.c` | Fluxo de telas e orquestração do estado ativo | Consome `teclas.h`; aciona o domínio e a apresentação
3. Entrada | `entrada.c/.h`, `teclas.c/.h` | Captura de eventos USB e detecção de borda por quadro | Lê `/dev/input/*`; alimenta FSM e `jogador.c`
4. Domínio | `entity`, `jogador`, `inimigos`, `projeteis`, `powerups`, `fase`, `ondas`, `collision`, `mat` | Física 2D, colisão, IA, mira, disparos e progressão | Orquestrado por `estado_partida.c`; fornece dados a `gfx` e `hud`
5. Apresentação | `gfx`, `sprites`, `mapas`, `menu_mapas`, `hud`, `texto` | Composição da cena, fontes, HUD e dados visuais | Lê o domínio; traduz para chamadas de `driver.h`
6. Abstração de hardware | `driver.h`, `driver.S` | ABI ARMv7, MMIO e protocolo valid/busy/ack/done | Recebe chamadas de `gfx`, `sprites` e `perf`

</div>

## 7. Integração Hardware-Software

<div align="center">
  <img src="images/hardware-software.png" alt="Integração hardware-software" width="900">
  <br>
  <i>Figura 13: Integração entre o processador ARM (software) e o coprocessador gráfico (hardware) pela ponte <i>Lightweight HPS-to-FPGA</i>.</i>
</div>
<br>

* **Mapeamento de memória (*MMIO*):** a comunicação entre o ARM e a FPGA ocorre pela ponte *Lightweight HPS-to-FPGA*, acessada por `/dev/mem` e `mmap`.
* **Empacotamento em 32 bits:** a aplicação envia parâmetros de alto nível; o *driver* converte e empacota tudo em uma única palavra de instrução de 32 bits (*opcode* + campos).
* **Pipeline de execução:** instrução → FIFO (32 posições) → unidade de controle (decodificador) → bancos e motores gráficos → compositor → monitor VGA.
* **Desacoplamento visual:** o processador não desenha *pixels*; atua como um maestro que envia coordenadas e IDs, transferindo todo o esforço de renderização para o hardware.

### 7.1. Ciclo de um quadro

O ritmo do jogo é dado pelo próprio vídeo: o laço principal espera o contador de quadros do hardware mudar (cerca de 60 Hz) em vez de usar temporizadores do sistema operacional.

```text
 esperar_quadro()  ->  teclas_quadro()  ->  estado_rodar_quadro()  ->  esperar_fim()
 (vblank do HW)        (foto das entradas)   (lógica + comandos)         (FIFO vazia, UC livre)
                                              └ cada comando: enviar_inst() espera ACK
```

Como o *frame buffer* é duplo e a troca de página ocorre no início do *vertical blank*, uma alteração só chega ao monitor após a próxima troca de página. Com o temporizador do VGA (800 × 525 ciclos a 25 MHz), o período de um quadro é de aproximadamente 16,8 ms (59,5 Hz).

<!-- PENDENTE: linha do tempo de um quadro; arquivo esperado: images/fase3/ciclo-quadro.png. Descomentar ao adicionar a imagem.
<div align="center">
  <img src="images/fase3/ciclo-quadro.png" alt="Ciclo de um quadro" width="800">
  <br>
  <i>Figura ?: Decomposição do período de um quadro (espera, lógica e dreno da FIFO).</i>
</div>
<br>
-->

<!-- INSERIR (SUGESTÃO, não existe nos slides): linha do tempo de um quadro com esperar_quadro, lógica, esperar_fim e vblank -->

### 7.2. Política de erros

O erro é propagado por três níveis: o *driver* devolve um código, a camada `gfx` registra o primeiro erro do quadro, e o `main()` os consulta ao final de cada quadro, junto com as *flags* persistentes da FPGA (opcode inválido e *overflow* da FIFO), antes de limpá-las. Um comando inválido, portanto, é sempre **detectado e sinalizado**, e a aplicação encerra de forma controlada, sem travar silenciosamente.

## 8. Software e Hardware usados para Prototipação

* **Plataforma Física:** Kit acadêmico Terasic DE1-SoC, FPGA Intel Cyclone V SoC 5CSEMA5F31C6, com processador ARM Cortex-A9 (*HPS*);
* **Periféricos:** teclado e mouse USB com fio (qualquer modelo) conectados ao *HPS* e monitor com entrada VGA analógica (640×480 @ 60 Hz);
* **Linguagens:** Verilog-2001 (coprocessador), *Assembly* ARMv7 (*driver*) e C (aplicação, padrão GNU99);
* **Ferramentas de Síntese:** Intel Quartus Prime Lite Edition 25.1std (Build 1129) e *Platform Designer* (Qsys) para os PIOs;
* **Compilação do Software:** GCC no Linux embarcado da DE1-SoC, com `-pthread`;
* **Memórias:** blocos dedicados de SRAM internos do chip (M10K).

## 9. Demonstração e Testes de Funcionamento

Os testes foram executados na própria DE1-SoC, com teclado e mouse USB e monitor VGA, e registrados em GIFs. A seção 9.1 resume o que cada teste verifica e aponta o GIF que o evidencia; a seção 9.2 reúne os GIFs, agrupados por etapa do jogo. As medições de desempenho e estabilidade (30 s e 60 s) estão na seção 10.3.

### 9.1. Matriz de testes

<div align="center">

ID | Teste | Critério de aceite | Evidência
:---|:---|:---|:---
T01 | Menu e início de partida | O menu exibe JOGAR e SAIR; ENTER ou clique leva à tela de transição da fase 1 e, em seguida, à partida | Figura 14
T02 | Combate e pontuação | Os tiros eliminam inimigos e a pontuação do HUD aumenta; os ataques dos inimigos são exibidos e causam dano | Figuras 15 e 16
T03 | Itens (*power-ups*) | Os itens aparecem na tela e, ao serem coletados, aplicam o efeito (por exemplo, vida e arma) | Figura 17
T04 | Pausa e retomada | O painel de pausa congela a partida; ao retomar, pontos, vidas e ondas são preservados | Figuras 19 e 20
T05 | Progressão de ondas e fases | O banner de onda aparece; a conclusão de uma fase leva à tela de transição da fase seguinte (manhã, tarde e noite) | Figuras 18, 21, 23 e 26
T06 | Cenas com muitos inimigos | Vários inimigos simultâneos nas fases 1, 2 e 3, sem perda de sincronismo da imagem | Figuras 16, 22, 24 e 25
T07 | Fogueira (eliminação coletiva) | O item elimina os inimigos comuns da tela | Figura 25
T08 | Chefe | O chefe entra após a última onda da fase 3 e executa seus ataques | Figuras 26 e 27
T09 | Fim de jogo e reinício | A derrota exibe pontuação, fase e onda; JOGAR e MENU funcionam; uma nova partida começa do estado inicial | Figuras 29 e 30
T10 | Vitória | A derrota do chefe leva à tela de vitória com a pontuação final | Figura 28
T11 | Desempenho e estabilidade | Em 30 s e 60 s: lógica ≥ 30 atualizações/s, resposta de entrada ≤ 100 ms e leituras do STATUS coerentes | Seção 10.3

</div>

### 9.2. Demonstração em GIFs

#### Menu e início da partida

<div align="center">
  <img src="images/menu_inicial_jogar.gif" alt="Menu inicial e início da partida" width="480">
  <br>
  <i>Figura 14: Menu inicial, seleção de JOGAR, tela de transição "FASE 1 MANHÃ" e início da partida (onda 1).</i>
</div>
<br>

#### Fase 1 (manhã): combate, itens e ondas

<table>
  <tr>
    <td align="center">
      <img src="images/ataque_inimigos_fase1.gif" alt="Ataques dos inimigos na fase 1" width="480">
      <br>
      <i>Figura 15: Ataques dos inimigos na fase 1; a pontuação do HUD aumenta com as eliminações.</i>
    </td>
    <td align="center">
      <img src="images/varios_inimigos_ataque_fase1.gif" alt="Vários inimigos na fase 1" width="480">
      <br>
      <i>Figura 16: Vários inimigos simultâneos atacando na fase 1.</i>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="images/powerups_coracao_arma.gif" alt="Itens coração e arma" width="480">
      <br>
      <i>Figura 17: Itens na fase 1 (coração e arma): aparecimento e coleta.</i>
    </td>
    <td align="center">
      <img src="images/ataques_fase1_trans_onda3.gif" alt="Transição para a onda 3" width="480">
      <br>
      <i>Figura 18: Ataques na fase 1 e transição para a onda 3 (banner "ONDA 3").</i>
    </td>
  </tr>
</table>

#### Pausa e retomada

<table>
  <tr>
    <td align="center">
      <img src="images/power_up_pausa.gif" alt="Pausa com item na fase 1" width="480">
      <br>
      <i>Figura 19: Pausa na fase 1 durante o uso de itens; pontos e vidas permanecem iguais após retomar.</i>
    </td>
    <td align="center">
      <img src="images/pausa_fase2.gif" alt="Pausa na fase 2" width="480">
      <br>
      <i>Figura 20: Pausa e retomada na fase 2; a partida continua de onde parou, com a pontuação preservada.</i>
    </td>
  </tr>
</table>

#### Transições entre fases

<table>
  <tr>
    <td align="center">
      <img src="images/transicao_fase2.gif" alt="Transição da fase 1 para a fase 2" width="480">
      <br>
      <i>Figura 21: Fim da fase 1, tela "FASE 2 TARDE" e início da onda 1 da fase 2.</i>
    </td>
    <td align="center">
      <img src="images/fase2_varios_inimigos.gif" alt="Vários inimigos na fase 2" width="480">
      <br>
      <i>Figura 22: Fase 2 (tarde) com vários inimigos simultâneos.</i>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="images/inimigos2_trans_fase3.gif" alt="Transição da fase 2 para a fase 3" width="480">
      <br>
      <i>Figura 23: Fim da fase 2, tela "FASE 3 NOITE" e início da onda 1 da fase 3.</i>
    </td>
    <td></td>
  </tr>
</table>

#### Fase 3 (noite): inimigos, Fogueira e chefe

<table>
  <tr>
    <td align="center">
      <img src="images/varios_inimigos_fase3.gif" alt="Vários inimigos na fase 3" width="480">
      <br>
      <i>Figura 24: Fase 3 (noite) com vários inimigos simultâneos.</i>
    </td>
    <td align="center">
      <img src="images/inimigos_fase3_fogo_todos.gif" alt="Item Fogueira na fase 3" width="480">
      <br>
      <i>Figura 25: Sequência longa na fase 3, com uso do item Fogueira para eliminar os inimigos comuns da tela.</i>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="images/onda3_trans_boss.gif" alt="Entrada do chefe" width="480">
      <br>
      <i>Figura 26: Fim da última onda e entrada do chefe (banner "CHEFE").</i>
    </td>
    <td align="center">
      <img src="images/ataques_boss_final.gif" alt="Ataques do chefe" width="480">
      <br>
      <i>Figura 27: Ataques do chefe na fase 3.</i>
    </td>
  </tr>
</table>

#### Fim de jogo

<table>
  <tr>
    <td align="center">
      <img src="images/vitoria_boss.gif" alt="Vitória ao derrotar o chefe" width="480">
      <br>
      <i>Figura 28: Derrota do chefe e tela de vitória com a pontuação final.</i>
    </td>
    <td align="center">
      <img src="images/fase3_morte_menu.gif" alt="Morte na fase 3" width="480">
      <br>
      <i>Figura 29: Morte na fase 3 e tela de fim de jogo com pontuação, fase e onda.</i>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="images/morte_fase1_voltar_menu.gif" alt="Morte na fase 1 e retorno ao menu" width="480">
      <br>
      <i>Figura 30: Fim de jogo, retorno ao menu e nova partida iniciada do estado inicial (pontuação zerada).</i>
    </td>
    <td></td>
  </tr>
</table>

### 9.3. Tratamento de comandos inválidos

Uma instrução com parâmetro fora da faixa é rejeitada pelo *driver* antes do envio (retorno -1). Um *opcode* inexistente, quando enviado como palavra crua por `enviar_inst()`, é descartado pela UC, que ativa a *flag* de erro; o jogo detecta a *flag* ao final do quadro, registra a mensagem em `stderr`, limpa o erro e encerra com código 2. O *overflow* da FIFO é tratado do mesmo modo.

## 10. Análise de Recursos e de Desempenho

### 10.1. Recursos da plataforma

Valores do relatório de síntese final (*Fitter Summary*, Quartus Prime 25.1std, 5CSEMA5F31C6):

<div align="center">

Recurso | Uso
:---|:---
ALMs necessários | 12.288 / 32.070 (38%)
ALMs na alocação final | 14.616 (46%)
LABs usados | 2.273 / 3.207 (71%)
Registradores | 16.274
Blocos M10K | 191 / 397 (48%)
Bits de memória | 1.510.272 / 4.065.280 (37%)
Bits por implementação | 1.955.840 (48%)
DSP | 32 / 87 (37%)
Pinos | 382 / 457 (84%)

</div>

<!-- PENDENTE: captura do Flow Summary do Quartus; arquivo esperado: images/fase3/resource-usage.png. Descomentar ao adicionar a imagem.
<div align="center">
  <img src="images/fase3/resource-usage.png" alt="Resource Usage Summary" width="500">
  <br>
  <i>Figura ?: Resumo de utilização de recursos, Quartus Prime Lite Edition.</i>
</div>
<br>
-->

<!-- INSERIR: captura do Flow Summary do Quartus (soc_system.fit.summary) -->

### 10.2. Análise de *timing*

O *Timing Analyzer* não apontou violações de *setup* nem de *hold* nos dois *clocks* do projeto:

<div align="center">

Clock | Fmax | Folga de *setup* | Folga de *hold*
:---|:---|:---|:---
`clock_50_1` | 82,65 MHz | 7,901 ns | 0,023 ns
`clk_25` | 45,34 MHz | 6,472 ns | 0,173 ns

</div>

O `clk_25` alimenta todo o coprocessador. Sua Fmax de 45,34 MHz representa uma folga de aproximadamente **1,8x** sobre os 25 MHz exigidos pela varredura VGA.

<!-- PENDENTE: captura do Timing Analyzer; arquivo esperado: images/fase3/timing-summary.png. Descomentar ao adicionar a imagem.
<div align="center">
  <img src="images/fase3/timing-summary.png" alt="Timing Analyzer Summary" width="700">
  <br>
  <i>Figura ?: Resumo do Timing Analyzer e Fmax do clk_25.</i>
</div>
<br>
-->

<!-- INSERIR: capturas do Multicorner Timing Analysis Summary e do Fmax Summary (soc_system.sta) -->

### 10.3. Desempenho da solução integrada

O software inclui o módulo `perf` (`perf.c`, `perf.h` e `perf_status.h`), que mede o desempenho durante a partida e exibe um painel ao vivo no terminal. As medições abaixo foram feitas em execuções de 30 s e de 60 s na DE1-SoC:

<div align="center">

Métrica | 30 s | 60 s
:---|:---|:---
Quadros da lógica / vídeo (PIO_STATUS) | 1.780 | 3.565
Taxa lógica / vídeo (quadros/s) | 59,3 / 59,5 | 59,4 / 59,5
Ocupação por quadro (méd. / máx.) | 0,14 / 0,24 ms | 0,22 / 0,69 ms
Latência de entrada | 16,83 ms | 16,85 ms
CPU útil (lógica / período) | 5% | 10%
Leituras de STATUS incoerentes | 0 de 56.960 | 0 de 114.080
Entidades (máx.) | 20 | 27

</div>

<!-- INSERIR: captura do painel de desempenho no terminal -->

Em ambas as execuções, a lógica do jogo acompanhou o vídeo (cerca de 59 atualizações por segundo, acima do mínimo de 30 do RNF01), a latência de entrada ficou em cerca de 16,8 ms (limite de 100 ms do RNF02) e nenhuma leitura incoerente do STATUS foi detectada.

> [!NOTE]
> A latência de entrada termina quando a lógica trata o evento. O atraso até a imagem aparecer no monitor (troca de página do *frame buffer* no *vertical blank* seguinte) soma-se a esse valor; trata-se de uma estimativa derivada da arquitetura de *double buffering*, ainda não confirmada experimentalmente.

## 11. Instalação, Configuração e Execução

### Pré-requisitos:

- Quartus Prime (Standard ou Lite Edition) instalado, com o driver USB-Blaster reconhecido pelo sistema operacional;
- Placa DE1-SoC conectada via cabo USB-Blaster, saída VGA conectada a um monitor, e teclado e mouse USB conectados às portas USB do *HPS*;
- Linux embarcado da DE1-SoC em execução, com acesso ao terminal, GCC e privilégios de administrador.

### Passos (hardware):

1. Abra o arquivo `soc_system.qpf` no Quartus Prime.
2. Mantenha os arquivos `.mif` (`padroes_sprites.mif`, `padroes_tiles.mif` e `cenario.mif`) no diretório do projeto, pois inicializam as memórias na síntese.
3. As memórias (RAM de *sprites*, ROM de *tiles*, RAM do *tilemap* e *frame buffers*) e os PIOs do *Platform Designer* já estão configurados.
4. Compile o projeto (**Processing > Start Compilation**).
5. Grave o `soc_system.sof` na FPGA pelo **Programmer**.

### Passos (software):

1. Copie o diretório `software/` para o Linux da placa.
2. Compile todos os módulos com o *driver*:
   ```bash
   gcc -std=gnu99 -pthread *.c driver.S -o jogo
   ```
3. Execute com permissão de administrador (necessária para `/dev/mem` e `/dev/input`), informando os dispositivos de teclado e de mouse:
   ```bash
   sudo ./jogo /dev/input/event0 /dev/input/event1
   ```

### Estrutura do repositório:

<!-- PREENCHER com a árvore final do repositório (software/, hardware/, images/, tests/, reports/, README.md) -->

```text
.
├── hardware/      # projeto Quartus (Verilog, .qsys, .mif, .sdc) e .sof final
├── software/      # driver.S, driver.h e módulos em C do jogo
├── images/        # figuras e GIFs do relatório
└── README.md
```

## 12. Decisões Técnicas Registradas

<div align="center">

Decisão | Registro
:---|:---
Paleta programável | Dispensada pelo tutor para esta fase; não foi implementada. As cores são fornecidas no formato adotado pelo projeto (RRRGGGBB de 8 bits, índice 0 transparente)
Estratégia de *tiles* | PDRBG (*opcode* 0x0B) grava o ID do *tile* (8 bits, [12:5]) na célula do *tilemap* (11 bits, [23:13], 0 a 1.199). Os *pixels* dos padrões residem na ROM `rom_tiles`, inicializada por `padroes_tiles.mif` em síntese (256 *tiles*); a ROM não é regravável em tempo de execução
Padrões de *sprites* | RAM regravável de 64 padrões, carregada em tempo de execução pela instrução PIXSPR
*Handshake* | READY derivado de !FIFO_CHEIA; BUSY = STATUS[3]; ACK = *toggle* em STATUS[2] por *push* aceito; DONE global = FIFO_VAZIA e !UC_OCUPADA. O ACK confirma aceitação, o DONE confirma execução
Reenvio após *timeout* | Não há reenvio automático após VALID: o resultado é incerto e a decisão fica com a aplicação
Política de falhas | *Fail-stop*: qualquer erro de *driver* ou da FPGA interrompe o laço e encerra com código 2
Agrupamentos | Bandos de 3 a 4 inimigos (urubus e morcegos) e eliminação coletiva pela Fogueira. <!-- CONFERIR com a regra do problema de referência e registrar o teste que a comprova -->
Entradas | Uma *thread* por dispositivo, estado compartilhado com *mutex* e "fotografia" por quadro; caminhos dos dispositivos configuráveis

</div>

## 13. Limitações e Possíveis Melhorias

**Melhorias futuras (hardware e gráficos):**

- **Expansão do motor gráfico:** otimizar o uso de blocos de memória na FPGA para suportar mais *sprites* simultâneos e explorar o motor de polígonos para chefes e cenários dinâmicos.
- **Coprocessador de áudio:** implementar um módulo de hardware dedicado para processar música de fundo e efeitos sonoros, explorando o *Audio Codec* nativo da placa DE1-SoC.

**Melhorias futuras (*software* e *game design*):**

- **Geração dinâmica de fases:** substituir os mapas estáticos por um sistema de fases adaptáveis e geradas proceduralmente, aumentando a rejogabilidade.
- **Sistema de ranking (*leaderboard*):** criar um menu de pontuação persistente, utilizando o sistema de arquivos do Linux no *HPS* para salvar e exibir os recordes (*High Scores*) dos jogadores.

## 14. Referências

ADAMS, V. Hunter. VGA Driver in Verilog. [S. l.], [s. d.]. Disponível em: https://vanhunteradams.com/DE1/VGA_Driver/Driver.html. Acesso em: 17 ago. 2026.

CHU, Pong P. FPGA prototyping by Verilog examples: Xilinx Spartan-3 version. Hoboken, NJ: John Wiley & Sons, 2008.

CHU, Pong P. FPGA prototyping by SystemVerilog examples: Xilinx MicroBlaze MCS SoC edition. 2. ed. Hoboken, NJ: Wiley, 2018.

PATTERSON, David A.; HENNESSY, John L. Computer organization and design: the hardware/software interface: ARM edition. Waltham, MA: Morgan Kaufmann, 2017.

TOCCI, Ronald J.; WIDMER, Neal S.; MOSS, Gregory L. Sistemas digitais: princípios e aplicações. 12. ed. São Paulo: Pearson Universidades, 2017.

FPGA4STUDENT. Verilog code for FIFO memory. [S. l.], jan. 2017. Disponível em: https://www.fpga4student.com/2017/01/verilog-code-for-fifo-memory.html. Acesso em: 18 out. 2026.