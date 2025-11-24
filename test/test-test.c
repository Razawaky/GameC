/*
 * Campo Minado- Estruturas de Dados
 * Integração: Pilha (Undo), Fila (Flood Fill), Lista Dupla (Flags)
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --- CONFIGURAÇÕES E MACROS --- */

/* Tamanho máximo do buffer de entrada */
#define TAM_BUFFER_ENTRADA 128

/* Acesso à matriz linearizada */
#define CELULA_EM(tabuleiro, x, y) ((tabuleiro)->celulas[(y) * (tabuleiro)->largura + (x)])

/*  
    Manipulação de bits dentro de uma célula
    Bits usados:
        0–3 → número de minas vizinhas (0–8)
        5   → é mina?
        6   → tem bandeira?
        7   → está revelada?
*/

#define DESLOC_MINA      0x05
#define DESLOC_BANDEIRA  0x06
#define DESLOC_REVELADA  0x07
#define MASCARA_MINAS    0x0f

/* Leitura dos bits */
#define EH_MINA(cel)          (((cel) >> DESLOC_MINA)      & 0x1)
#define TEM_BANDEIRA(cel)     (((cel) >> DESLOC_BANDEIRA)  & 0x1)
#define ESTA_REVELADA(cel)    (((cel) >> DESLOC_REVELADA)  & 0x1)
#define NUM_MINAS(cel)        ((cel) & MASCARA_MINAS)

/* Escrita dos bits */
#define DEFINIR_MINA(cel, bit)        ((cel) = ((cel) & ~(0x1 << DESLOC_MINA))      | ((bit) << DESLOC_MINA))
#define DEFINIR_BANDEIRA(cel, bit)    ((cel) = ((cel) & ~(0x1 << DESLOC_BANDEIRA))  | ((bit) << DESLOC_BANDEIRA))
#define DEFINIR_REVELADA(cel, bit)    ((cel) = ((cel) & ~(0x1 << DESLOC_REVELADA))  | ((bit) << DESLOC_REVELADA))


/* --- DIREÇÕES DOS 8 VIZINHOS --- */
/* 
    Cada par representa:
    { deslocamento_x , deslocamento_y }

    Estes são os 8 vizinhos possíveis de uma célula:
    - diagonais (4)
    - horizontais e verticais (4)
*/

// Direções (8 vizinhos)
static const int direcoes[][2] = {
    {-1, -1}, {-1, 1}, {1, -1}, {1, 1},
    {0, -1}, {0, 1}, {-1, 0}, {1, 0},
};

// Tipo base da célula
typedef uint8_t Celula;



/* --- ESTRUTURAS DE DADOS --- */

/*
    LISTA DUPLA: usada para gerenciar bandeiras no tabuleiro.
    Permite percorrer rapidamente todas as bandeiras colocadas.
    Remoção é O(1) se tivermos o nó; caso contrário, O(N) para encontrar.
*/
typedef struct NoListaDupla {
    size_t x, y;                  /* posição da célula */
    struct NoListaDupla *anterior;
    struct NoListaDupla *proximo;
} NoListaDupla;

/*
    FILA: usada no algoritmo de expansão (flood fill) com BFS.
    Serve para revelar grandes áreas de células vazias.
*/
typedef struct NoFila {
    size_t x, y;              /* posição da célula */
    struct NoFila *proximo;
} NoFila;

/*
    PILHA: usada no sistema de desfazer (undo).
    Cada nó armazena a posição e o valor antigo da célula.
*/
typedef struct NoPilha {
    size_t x, y;              /* posição da célula */
    Celula valor_antigo;      /* valor antes da alteração */
    bool inicio_lote;         /* marca o início de um conjunto de revelações */
    struct NoPilha *proximo;
} NoPilha;


/* --- ESTADO DO JOGO --- */

typedef struct {
    size_t largura;
    size_t altura;
    size_t qtd_minas;
    Celula *celulas;
    
    // Cabeças das estruturas
    NoListaDupla *inicio_bandeiras; 
    NoPilha *pilha_desfazer; 
} Tabuleiro;

// Global para controle rápido de vitória
size_t celulas_reveladas = 0;


/* --- IMPLEMENTAÇÃO DAS ESTRUTURAS DE DADOS --- */

// Adiciona coordenada à Lista Dupla de bandeiras.
void lista_dupla_adicionar(Tabuleiro *t, size_t x, size_t y) {
    NoListaDupla *no = malloc(sizeof(NoListaDupla));
    no->x = x;
    no->y = y;
    no->proximo = t->inicio_bandeiras;
    no->anterior = NULL;

    if (t->inicio_bandeiras != NULL) {
        t->inicio_bandeiras->anterior = no;
    }
    t->inicio_bandeiras = no;
}

// Remove coordenada da Lista Dupla de bandeiras.
void lista_dupla_remover(Tabuleiro *t, size_t x, size_t y) {
    NoListaDupla *atual = t->inicio_bandeiras;
    while (atual != NULL) {
        if (atual->x == x && atual->y == y) {
            if (atual->anterior) atual->anterior->proximo = atual->proximo;
            if (atual->proximo)  atual->proximo->anterior = atual->anterior;
            if (atual == t->inicio_bandeiras)
                t->inicio_bandeiras = atual->proximo;
            free(atual);
            return;
        }
        atual = atual->proximo;
    }
}

// Empilha uma alteração na Pilha de Undo.
void pilha_empurrar(Tabuleiro *t, size_t x, size_t y, Celula valor_antigo, bool inicio_lote) {
    NoPilha *no = malloc(sizeof(NoPilha));
    no->x = x;
    no->y = y;
    no->valor_antigo = valor_antigo;
    no->inicio_lote = inicio_lote;
    no->proximo = t->pilha_desfazer;
    t->pilha_desfazer = no;
}

// Desfaz a última jogada (reverte um lote inteiro).
bool pilha_desfazer(Tabuleiro *t) {
    if (t->pilha_desfazer == NULL) return false;

    bool primeiro = true;
    while (t->pilha_desfazer != NULL) {
        NoPilha *topo = t->pilha_desfazer;

        if (topo->inicio_lote && !primeiro) break;

        Celula atual = CELULA_EM(t, topo->x, topo->y);

        if (ESTA_REVELADA(atual) && !ESTA_REVELADA(topo->valor_antigo))
            celulas_reveladas--;

        bool era_bandeira   = TEM_BANDEIRA(topo->valor_antigo);
        bool eh_bandeira_agora = TEM_BANDEIRA(atual);

        if (eh_bandeira_agora && !era_bandeira)
            lista_dupla_remover(t, topo->x, topo->y);
        else if (!eh_bandeira_agora && era_bandeira)
            lista_dupla_adicionar(t, topo->x, topo->y);

        CELULA_EM(t, topo->x, topo->y) = topo->valor_antigo;

        t->pilha_desfazer = topo->proximo;
        free(topo);
        primeiro = false;
    }
    return true;
}

void empilhar_undo(Tabuleiro *t, size_t x, size_t y, Celula valor_antigo, bool inicio_lote)
{
    NoPilha *novo = malloc(sizeof(NoPilha));
    if (!novo) return;

    novo->x = x;
    novo->y = y;
    novo->valor_antigo = valor_antigo;
    novo->inicio_lote = inicio_lote;

    novo->proximo = t->pilha_desfazer;  // empilha
    t->pilha_desfazer = novo;
}

void alternar_bandeira(Tabuleiro *t, size_t x, size_t y) {
    Celula *cel = &CELULA_EM(t, x, y);

    // Não pode alternar bandeira se a célula já está revelada
    if (ESTA_REVELADA(*cel)) return;

    // Empilha estado antigo para undo
    empilhar_undo(t, x, y, *cel, true);

    if (TEM_BANDEIRA(*cel)) {
        // Remove bandeira da lista
        lista_dupla_remover(t, x, y);
        DEFINIR_BANDEIRA(*cel, false);
    } else {
        // Coloca bandeira na lista
        lista_dupla_adicionar(t, x, y);
        DEFINIR_BANDEIRA(*cel, true);
    }
}

/* --- LÓGICA DO JOGO --- */

// Lê input do usuário removendo newline.
void ler_entrada(char *buf, int tamanho) {
    if (fgets(buf, tamanho, stdin) != NULL) {
        buf[strcspn(buf, "\n")] = 0;
    } else {
        buf[0] = 0;
    }
}

// Inicializa o tabuleiro e distribui minas.
void iniciar_jogo(Tabuleiro *t) {
    celulas_reveladas = 0;
    t->inicio_bandeiras = NULL;
    t->pilha_desfazer = NULL;

    t->celulas = realloc(t->celulas, t->largura * t->altura * sizeof(*t->celulas));
    if (!t->celulas) {
        perror("ERRO: malloc");
        exit(EXIT_FAILURE);
    }
    memset(t->celulas, 0, t->largura * t->altura * sizeof(Celula));

    // Distribuir minas
    for (size_t i = 0; i < t->qtd_minas; i++) {
        size_t x, y;
        do {
            x = rand() % t->largura;
            y = rand() % t->altura;
        } while (EH_MINA(CELULA_EM(t, x, y)));

        DEFINIR_MINA(CELULA_EM(t, x, y), true);

        for (size_t j = 0; j < 8; j++) {
            size_t nx = x + direcoes[j][0];
            size_t ny = y + direcoes[j][1];
            if (nx >= t->largura || ny >= t->altura) continue;
            if (EH_MINA(CELULA_EM(t, nx, ny))) continue;
            CELULA_EM(t, nx, ny)++;
        }
    }
}

// Imprime uma célula colorida.
void imprimir_celula(Celula c) {
    printf("\x1b[1m");

    if (ESTA_REVELADA(c)) {
        printf("\x1b[47m"); // Fundo claro

        if (EH_MINA(c)) {
            printf("\x1b[31m#");
        } else if (NUM_MINAS(c) != 0) {
            uint8_t num = NUM_MINAS(c);

            switch (num) {
                case 1: printf("\x1b[94m"); break; 
                case 2: printf("\x1b[32m"); break;
                case 3: printf("\x1b[91m"); break;
                case 4: printf("\x1b[34m"); break;
                case 5: printf("\x1b[31m"); break;
                case 6: printf("\x1b[36m"); break;
                case 7: printf("\x1b[30m"); break;
                case 8: printf("\x1b[90m"); break;
            }

            printf("%u", num);
        } else {
            printf(" ");
        }

    } else {
        printf("\x1b[100m");
        if (TEM_BANDEIRA(c)) {
            printf("\x1b[91m!");
        } else {
            printf("\x1b[37m.");
        }
    }

    printf(" \x1b[0m");
}


//Imprime o tabuleiro completo.
void imprimir_tabuleiro(Tabuleiro *t) {
    printf("   X ");
    for (size_t i = 0; i < t->largura; i++) {
        size_t unidade = i % 10;
        printf("%zu%c", unidade, " |"[unidade == 9]);
    }
    printf("\n Y\x1b[1;40;37m +");
    for (size_t i = 0; i < t->largura * 2 + 1; i++) printf("-");
    printf("+ \x1b[0m\n");

    for (size_t y = 0; y < t->altura; y++) {
        printf("%2zu\x1b[1;40;37m |\x1b[%dm ", 
               y, 
               ESTA_REVELADA(CELULA_EM(t, 0, y)) ? 47 : 100
        );

        for (size_t x = 0; x < t->largura; x++) {
            imprimir_celula(CELULA_EM(t, x, y));
        }

        printf("\x1b[1;40;37m| \x1b[0m\n");
    }

    printf("  \x1b[1;40;37m +");
    for (size_t i = 0; i < t->largura * 2 + 1; i++) printf("-");
    printf("+ \n\x1b[0m");
}


//Limpa a tela e redesenha interface com informações de debug.
void atualizar_tela(Tabuleiro *t) {
    printf("\x1b[H\x1b[2J"); 
    imprimir_tabuleiro(t);

    // Estatísticas das estruturas de dados
    size_t profundidade_pilha = 0;
    NoPilha *p = t->pilha_desfazer;
    while (p) { 
        profundidade_pilha++; 
        p = p->proximo; 
    }
    
    size_t total_bandeiras = 0;
    NoListaDupla *b = t->inicio_bandeiras;
    while (b) { 
        total_bandeiras++; 
        b = b->proximo; 
    }
    
    printf("--- Informações ---\n");
    printf("Jogadas Feitas: %zu | Bandeiras Ativas: %zu\n",
           profundidade_pilha,
           total_bandeiras
    );
}

//Revela uma célula usando Fila (BFS).
void revelar_celula(Tabuleiro *t, size_t x_inicio, size_t y_inicio) {
    if (ESTA_REVELADA(CELULA_EM(t, x_inicio, y_inicio)) ||
        TEM_BANDEIRA(CELULA_EM(t, x_inicio, y_inicio)))
        return;

    /* Macro local para enfileirar nós */
    NoFila *inicio = NULL, *fim = NULL;
    #define ENFILEIRAR(px, py) do { \
        NoFila *novo = malloc(sizeof(NoFila)); \
        novo->x = (px); \
        novo->y = (py); \
        novo->proximo = NULL; \
        if (fim) fim->proximo = novo; else inicio = novo; \
        fim = novo; \
    } while(0)

    ENFILEIRAR(x_inicio, y_inicio);
    
    /* Undo: início de lote */
    empilhar_undo(t, x_inicio, y_inicio, CELULA_EM(t, x_inicio, y_inicio), true);

    DEFINIR_REVELADA(CELULA_EM(t, x_inicio, y_inicio), true);
    celulas_reveladas++;

    /* Se clicou em número ou mina, não expande */
    if (NUM_MINAS(CELULA_EM(t, x_inicio, y_inicio)) != 0 ||
        EH_MINA(CELULA_EM(t, x_inicio, y_inicio))) {
        free(inicio);
        return;
    }

    /* BFS */
    while (inicio) {
        NoFila *atual = inicio;
        inicio = inicio->proximo;
        if (!inicio) fim = NULL;

        size_t cx = atual->x;
        size_t cy = atual->y;
        free(atual);

        for (size_t j = 0; j < 8; j++) {
            size_t nx = cx + direcoes[j][0];
            size_t ny = cy + direcoes[j][1];

            if (nx >= t->largura || ny >= t->altura) continue;

            Celula *prox = &CELULA_EM(t, nx, ny);

            if (ESTA_REVELADA(*prox) || TEM_BANDEIRA(*prox)) continue;

            /* Undo: continuação do lote */
            empilhar_undo(t, nx, ny, *prox, false);

            DEFINIR_REVELADA(*prox, true);
            celulas_reveladas++;

            /* Só expande células vazias */
            if (NUM_MINAS(*prox) == 0 && !EH_MINA(*prox)) {
                ENFILEIRAR(nx, ny);
            }
        }
    }

    #undef ENFILEIRAR
}

//Tenta revelar ao redor se a quantidade de bandeiras bater com o número da célula (Chord).
bool revelar_ao_redor(Tabuleiro *t, size_t x, size_t y) {
    size_t num_minas = NUM_MINAS(CELULA_EM(t, x, y));

    /* Conta quantas bandeiras existem ao redor */
    for (size_t i = 0; i < 8; i++) {
        size_t nx = x + direcoes[i][0];
        size_t ny = y + direcoes[i][1];

        if (nx >= t->largura || ny >= t->altura)
            continue;

        if (TEM_BANDEIRA(CELULA_EM(t, nx, ny)))
            num_minas -= 1;
    }

    bool acertou_mina = false;

    /* Se bandeiras suficientes, revela vizinhos */
    if (num_minas <= 0) {
        for (size_t i = 0; i < 8; i++) {
            size_t nx = x + direcoes[i][0];
            size_t ny = y + direcoes[i][1];

            if (nx >= t->largura || ny >= t->altura)
                continue;
            
            Celula cel = CELULA_EM(t, nx, ny);

            if (!TEM_BANDEIRA(cel) && !ESTA_REVELADA(cel)) {
                /* Cada revelação abre seu próprio lote de undo */
                revelar_celula(t, nx, ny);

                if (!acertou_mina && EH_MINA(CELULA_EM(t, nx, ny)))
                    acertou_mina = true;
            }
        }
    }

    return acertou_mina;
}

//Revela todo o tabuleiro (Fim de jogo).
void revelar_tabuleiro(Tabuleiro *tab) {
    for (size_t y = 0; y < tab->altura; y++) {
        for (size_t x = 0; x < tab->largura; x++) {
             DEFINIR_REVELADA(CELULA_EM(tab, x, y), true);
        }
    }
}

//Verifica condição de vitória.
bool verificar_vitoria(Tabuleiro *tab) {
    size_t total_seguras = (tab->largura * tab->altura) - tab->qtd_minas;
    return celulas_reveladas == total_seguras;
}

//Lista todas as bandeiras usando a lista duplamente encadeada.
void listar_bandeiras(Tabuleiro *tab) {
    printf("Células com Bandeira: ");
    NoListaDupla *atual = tab->inicio_bandeiras;
    if (!atual) printf("(Nenhuma)");

    while (atual) {
        printf("[%zu, %zu] ", atual->x, atual->y);
        atual = atual->proximo;
    }

    printf("\nPressione Enter...");
    char tmp[10];
    ler_entrada(tmp, 10);
}

//Libera toda a memória usada pelo jogo.
void liberar_memoria_jogo(Tabuleiro *tab) {
    while(pilha_desfazer(tab)); 

    while(tab->inicio_bandeiras)
        lista_dupla_remover(tab, tab->inicio_bandeiras->x, tab->inicio_bandeiras->y);

    if (tab->celulas) {
        free(tab->celulas);
        tab->celulas = NULL;
    }
}

void imprimir_menu(void) {
    printf("\x1b[H\x1b[2J"); // Limpar tela
    printf("Campo Minado (Edição Pilha/Fila/Lista)\n"
           "(F)ácil   - 9x9, 10 minas\n"
           "(M)édio   - 16x16, 40 minas\n"
           "(D)ifícil - 30x16, 99 minas\n"
           "Escolha a dificuldade (digite 'ajuda' ou 'sair'):\n");
}

void imprimir_menu_ajuda(void) {
    printf("Comandos:\n"
           "r y x  : revelar célula (y=linha, x=coluna)\n"
           "b y x  : marcar/desmarcar bandeira\n"
           "d      : desfazer última jogada\n"
           "lb     : listar bandeiras\n"
           "ajuda  : mostrar ajuda\n"
           "sair   : encerrar jogo\n"
           "Pressione Enter...");
    char tmp[10];
    ler_entrada(tmp, 10);
}


/* --- MAIN --- */

int main(void) {
    srand(time(NULL));
    Tabuleiro tabuleiro = {0};
    char buf[TAM_BUFFER_ENTRADA] = {0};

_inicio_do_jogo:
    /* --- SELEÇÃO DE DIFICULDADE --- */
    for (;;) {
        imprimir_menu();
        printf("> ");
        ler_entrada(buf, TAM_BUFFER_ENTRADA);

        if (strcmp(buf, "sair") == 0) goto _sair_do_jogo;

        if (strcmp(buf, "ajuda") == 0) {
            imprimir_menu_ajuda();
            atualizar_tela(&tabuleiro);
            continue;
        }

        if (strcmp(buf, "F") == 0) { 
            tabuleiro.largura = 9;  
            tabuleiro.altura = 9;  
            tabuleiro.qtd_minas = 10; 
        }
        else if (strcmp(buf, "M") == 0) { 
            tabuleiro.largura = 16; 
            tabuleiro.altura = 16; 
            tabuleiro.qtd_minas = 40; 
        }
        else if (strcmp(buf, "D") == 0) { 
            tabuleiro.largura = 30; 
            tabuleiro.altura = 16; 
            tabuleiro.qtd_minas = 99; 
        }
        else continue;

        break;
    }

    iniciar_jogo(&tabuleiro);
    atualizar_tela(&tabuleiro);

    /* --- LOOP PRINCIPAL --- */
    for (;;) {
        printf("\nComando > ");
        ler_entrada(buf, TAM_BUFFER_ENTRADA);

        if (strcmp(buf, "sair") == 0) goto _sair_do_jogo;
        
        if (strcmp(buf, "ajuda") == 0) {
            imprimir_menu_ajuda();
            atualizar_tela(&tabuleiro);
            continue;
        }

        if (strcmp(buf, "d") == 0) {
            if (pilha_desfazer(&tabuleiro))
                printf("Desfeito.\n");
            else
                printf("Nada para desfazer.\n");

            atualizar_tela(&tabuleiro);
            continue;
        }
        if (strcmp(buf, "lb") == 0) {
            listar_bandeiras(&tabuleiro);
            atualizar_tela(&tabuleiro);
            continue;
        }

        char acao;
        size_t x, y;
        if (sscanf(buf, "%c %zu %zu", &acao, &y, &x) == 3) {
            if (x >= tabuleiro.largura || y >= tabuleiro.altura) {
                printf("Coordenadas inválidas.\n");
                continue;
            }

            if (acao == 'b') {
                alternar_bandeira(&tabuleiro, x, y);   // função abaixo
                atualizar_tela(&tabuleiro);
            }
            else if (acao == 'r') {
                Celula atual = CELULA_EM(&tabuleiro, x, y);

                /* usar os defines que você tem */
                if (TEM_BANDEIRA(atual)) {
                    printf("A célula está marcada com bandeira. Remova primeiro.\n");
                    continue;
                }

                bool acertou_mina = false;

                if (ESTA_REVELADA(atual)) {
                    acertou_mina = revelar_ao_redor(&tabuleiro, x, y);
                } else {
                    revelar_celula(&tabuleiro, x, y);
                    if (EH_MINA(CELULA_EM(&tabuleiro, x, y))) acertou_mina = true;
                }

                if (acertou_mina) {
                    revelar_tabuleiro(&tabuleiro);
                    atualizar_tela(&tabuleiro);
                    printf("\n\x1b[31mBOOM! Você acertou uma mina!\x1b[0m\n");
                    break;
                }

                if (verificar_vitoria(&tabuleiro)) {
                    atualizar_tela(&tabuleiro);
                    printf("\n\x1b[32mPARABÉNS! Você limpou o campo!\x1b[0m\n");
                    break;
                }

                atualizar_tela(&tabuleiro);
            }
        } else {
            printf("Comando inválido.\n");
        }
    }


    /* --- REINICIAR JOGO? --- */
    for (;;) {
        printf("Jogar novamente? (S/N) > ");
        ler_entrada(buf, TAM_BUFFER_ENTRADA);

        if (strcmp(buf, "S") == 0 || strcmp(buf, "s") == 0) {
            liberar_memoria_jogo(&tabuleiro);
            goto _inicio_do_jogo;
        } 
        else if (strcmp(buf, "N") == 0 || strcmp(buf, "n") == 0) {
            break;
        }
    }

_sair_do_jogo:
    liberar_memoria_jogo(&tabuleiro);
    printf("Até mais!\n");
    return 0;
}
