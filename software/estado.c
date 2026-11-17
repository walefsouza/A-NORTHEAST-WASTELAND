// estado.c - gerenciador da maquina de estados
#include "estado.h"

static const Estado *atual = NULL;     // estado rodando agora
static const Estado *proximo = NULL;   // troca pendente (NULL = nenhuma)

// agenda a troca de estado; so vale no inicio do proximo quadro,
// assim o estado atual termina o quadro corrente sem ser interrompido
void estado_ir_para(const Estado *e)
{
    proximo = e;
}

// roda 1 quadro: aplica troca pendente, depois atualizar() e desenhar()
void estado_rodar_quadro(void)
{
    if (proximo)
    {
        atual = proximo;
        proximo = NULL;
        if (atual->entrar)
        {
            atual->entrar();           // inicializacao 1x do novo estado
        }
    }

    if (atual) {
        if (atual->atualizar)
        {
            atual->atualizar();        // logica
        }
        if (atual->desenhar)
        {
            atual->desenhar();         // comandos graficos
        }
    }
}

const Estado *estado_atual(void) { return atual; }
