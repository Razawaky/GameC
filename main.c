/*CAMPO MINADO  VINÍCIUS DUARTE E VINÍCIUS SANTANA*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- CONFIGURAÇÕES E MACROS ---

//criações de constantes sem usar a memória

// Tamanho máximo do buffer de entrada
#define TAM_BUFFER_ENTRADA 128

// Acesso à matriz linearizada
#define CELULA_EM(tabuleiro, x, y) ((tabuleiro)->celulas[(y) * (tabuleiro)->largura + (x)])

//cada célula é uma variável de 1 byte = 8bits, ou seja, uma celula guarda essas informações
#define DESLOC_MINA      0x05 //uma mina
#define DESLOC_BANDEIRA  0x06 //uma bandeira
#define DESLOC_REVELADA  0x07 //uma celula que foi revelada 
#define MASCARA_MINAS    0x0f //e o número de minas vizinhas (0,1,2,3)

// Leitura dos bits
#define EH_MINA(cel)          (((cel) >> DESLOC_MINA)      & 0x1)
#define TEM_BANDEIRA(cel)     (((cel) >> DESLOC_BANDEIRA)  & 0x1)
#define ESTA_REVELADA(cel)    (((cel) >> DESLOC_REVELADA)  & 0x1)
#define NUM_MINAS(cel)        ((cel) & MASCARA_MINAS)

// Escrita dos bits
#define DEFINIR_MINA(cel, bit)        ((cel) = ((cel) & ~(0x1 << DESLOC_MINA))      | ((bit) << DESLOC_MINA))
#define DEFINIR_BANDEIRA(cel, bit)    ((cel) = ((cel) & ~(0x1 << DESLOC_BANDEIRA))  | ((bit) << DESLOC_BANDEIRA))
#define DEFINIR_REVELADA(cel, bit)    ((cel) = ((cel) & ~(0x1 << DESLOC_REVELADA))  | ((bit) << DESLOC_REVELADA))


// --- DIREÇÕES DOS 8 VIZINHOS ---

//constante em que define as coordenadas dos 8 espaços em volta de uma celula
const int direcoes[][2] = { 
    {-1, -1}, {-1, 1}, {1, -1}, {1, 1},
    {0, -1}, {0, 1}, {-1, 0}, {1, 0},
};

// Tipo base da célula
typedef uint8_t Celula;

// --- ESTRUTURAS DE DADOS ---

//LISTA DUPLA: gerencia bandeira - guarda bandeira colocada, mostra na ordem que foi colocada e tambem remove... ela verifica se a bandeira que vvc quer colocar ja existe ou não
typedef struct NoListaDupla {
    size_t x, y;
    struct NoListaDupla *anterior;
    struct NoListaDupla *proximo;
} NoListaDupla;

//FILA: revela grandes areas de mina
typedef struct NoFila {
    size_t x, y;
    struct NoFila *proximo;
} NoFila;

//PILHA: fazer o undo, pois a ultima informação que entrou, é a primeira que sai
typedef struct NoPilha {
    size_t x, y;
    Celula valor_antigo;
    bool inicio_lote;
    struct NoPilha *proximo;
} NoPilha;

// --- ESTADO DO JOGO ---

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


// --- IMPLEMENTAÇÃO DAS ESTRUTURAS DE DADOS ---

// Adiciona coordenada à Lista Dupla de bandeiras.
void lista_dupla_adicionar(Tabuleiro *t, size_t x, size_t y) {
    NoListaDupla *no = malloc(sizeof(NoListaDupla)); //aloca memoria para um nó da lista e retorna um ponteiro 
    no->x = x; //armazena coords. x no campo 'x' do novo nó
    no->y = y; //armazena coords. y no campo 'y' do novo nó
    no->proximo = t->inicio_bandeiras; // faz ponteiro do novo nó apontar para o que está no começo da lista
    no->anterior = NULL; // não existe nó anterior ao primeiro

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
            if (atual->anterior){ atual->anterior->proximo = atual->proximo; }
            if (atual->proximo) { atual->proximo->anterior = atual->anterior; }
            if (atual == t->inicio_bandeiras){
                t->inicio_bandeiras = atual->proximo;
            }
            free(atual);
            return;
        }
        atual = atual->proximo;
    }
}

// Desfaz a última jogada (reverte um lote inteiro).
bool pilha_desfazer(Tabuleiro *t) {
    if (t->pilha_desfazer == NULL) return false;

    NoPilha *n = t->pilha_desfazer;

    // Se topo é marcador e nada depois dele → nada a desfazer
    if (n->inicio_lote && n->proximo == NULL)
        return false;

    // Remove marcador do topo (se for o caso)
    if (n->inicio_lote) {
        t->pilha_desfazer = n->proximo;
        free(n);
        n = t->pilha_desfazer;
    }

    // Agora desfaz até encontrar um marcador
    while (n && !n->inicio_lote) {

        // restaurar célula
        CELULA_EM(t, n->x, n->y) = n->valor_antigo;

        // estatísticas
        if (!ESTA_REVELADA(n->valor_antigo))
            celulas_reveladas--;

        // próximo item
        NoPilha *tmp = n;
        n = n->proximo;
        free(tmp);
    }

    // remover o marcador
    if (n && n->inicio_lote) {
        NoPilha *tmp = n;
        t->pilha_desfazer = n->proximo;
        free(tmp);
    } else {
        t->pilha_desfazer = NULL;
    }

    return true;
}

//Guarda jogada antiga na Pilha do Undo
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

//Funcionalidade da bandeira
void alternar_bandeira(Tabuleiro *t, size_t x, size_t y) {
    Celula *cel = &CELULA_EM(t, x, y);

    // Não pode alternar bandeira se a célula já está revelada
    if (ESTA_REVELADA(*cel)) return;

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

//Inicia um nó que inicia um lote das celulas reveladas
void empilhar_inicio_lote(Tabuleiro *t) {
    NoPilha *n = malloc(sizeof(NoPilha));
    n->inicio_lote = true;
    n->proximo = t->pilha_desfazer;
    t->pilha_desfazer = n;
}

// --- LÓGICA DO JOGO ---

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

// Imprime célula colorida.
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

//Limpa a tela e redesenha interface com informações
void atualizar_tela(Tabuleiro *t) {
    printf("\x1b[H\x1b[2J"); 
    imprimir_tabuleiro(t);

    // Estatísticas das estruturas de dados
    size_t jogadas_feitas = 0;
    NoPilha *p = t->pilha_desfazer;
    while (p) {
        if (p->inicio_lote) jogadas_feitas++;
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
           jogadas_feitas,
           total_bandeiras
    );
}

//Revela uma célula usando Fila
void revelar_celula(Tabuleiro *t, size_t x_inicio, size_t y_inicio) {
    if (ESTA_REVELADA(CELULA_EM(t, x_inicio, y_inicio)) ||
        TEM_BANDEIRA(CELULA_EM(t, x_inicio, y_inicio)))
        return;
    empilhar_inicio_lote(t);

    // Macro local para enfileirar nós
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
    
    // Undo: início de lote
    empilhar_undo(t, x_inicio, y_inicio, CELULA_EM(t, x_inicio, y_inicio), false);

    DEFINIR_REVELADA(CELULA_EM(t, x_inicio, y_inicio), true);
    celulas_reveladas++;

    // Se clicou em número ou mina, não expande
    if (NUM_MINAS(CELULA_EM(t, x_inicio, y_inicio)) != 0 ||
        EH_MINA(CELULA_EM(t, x_inicio, y_inicio))) {
        free(inicio);
        return;
    }

    // BFS
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

            // Undo: continuação do lote
            empilhar_undo(t, nx, ny, *prox, false);

            DEFINIR_REVELADA(*prox, true);
            celulas_reveladas++;

            // Só expande células vazias
            if (NUM_MINAS(*prox) == 0 && !EH_MINA(*prox)) {
                ENFILEIRAR(nx, ny);
            }
        }
    }

    #undef ENFILEIRAR
}

//Tenta revelar ao redor se a quantidade de bandeiras bater com o número da célula
bool revelar_ao_redor(Tabuleiro *t, size_t x, size_t y) {
    size_t num_minas = NUM_MINAS(CELULA_EM(t, x, y));

    // Conta quantas bandeiras existem ao redor
    for (size_t i = 0; i < 8; i++) {
        size_t nx = x + direcoes[i][0];
        size_t ny = y + direcoes[i][1];

        if (nx >= t->largura || ny >= t->altura)
            continue;

        if (TEM_BANDEIRA(CELULA_EM(t, nx, ny)))
            num_minas -= 1;
    }

    bool acertou_mina = false;

    // Se bandeiras suficientes, revela vizinhos
    if (num_minas <= 0) {
        for (size_t i = 0; i < 8; i++) {
            size_t nx = x + direcoes[i][0];
            size_t ny = y + direcoes[i][1];

            if (nx >= t->largura || ny >= t->altura)
                continue;
            
            Celula cel = CELULA_EM(t, nx, ny);

            if (!TEM_BANDEIRA(cel) && !ESTA_REVELADA(cel)) {
                // Cada revelação abre seu próprio lote de undo
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

//Imprimir menu
void imprimir_menu(void) {
    printf("\x1b[H\x1b[2J"); // Limpar tela
    printf("**** Campo Minado ****\n"
           "(F)ácil   - 9x9, 10 minas\n"
           "(M)édio   - 16x16, 40 minas\n"
           "(D)ifícil - 30x16, 99 minas\n"
           "Escolha a dificuldade (digite 'ajuda' ou 'sair'):\n");
}

//Imprimir ajuda
void imprimir_menu_ajuda(void) {
    printf("\nComandos:\n"
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

// --- MAIN ---

int main(void) {
    srand(time(NULL));
    Tabuleiro tabuleiro = {0};
    char buf[TAM_BUFFER_ENTRADA] = {0};

_inicio_do_jogo:

    // --- SELEÇÃO DE DIFICULDADE ---
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

    // --- LOOP PRINCIPAL ---
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

                // usar os defines que você tem
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


    // --- REINICIAR JOGO ---
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