#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define IN_BUF_SIZE 128

/* --- Macros de Acesso e Manipulação de Bits --- */
#define CELLS_AT(board, x, y) ((board)->cells[(y) * (board)->width + (x)])

#define MINE_OFFSET   0x05
#define FLAG_OFFSET   0x06
#define REVEAL_OFFSET 0x07
#define MINE_MASK     0x0f

#define IS_MINE(cell)     (((cell) >> MINE_OFFSET)   & 0x1) 
#define IS_FLAGGED(cell)  (((cell) >> FLAG_OFFSET)   & 0x1)
#define IS_REVEALED(cell) (((cell) >> REVEAL_OFFSET) & 0x1)
#define MINE_NUM(cell)    ((cell) & MINE_MASK)

#define SET_MINE(cell, bit)     ((cell) = ((cell) & ~(0x1 << MINE_OFFSET))   | ((bit) << 0x5))
#define SET_FLAGGED(cell, bit)  ((cell) = ((cell) & ~(0x1 << FLAG_OFFSET))   | ((bit) << 0x6))
#define SET_REVEALED(cell, bit) ((cell) = ((cell) & ~(0x1 << REVEAL_OFFSET)) | ((bit) << 0x7))

/* --- Direções para verificar vizinhos (8 direções) --- */
static const int dirs[][2] = {
    {-1, -1}, {-1, 1}, {1, -1}, {1, 1},
    {0, -1}, {0, 1}, {-1, 0}, {1, 0},
};

// Tipo base da célula
typedef uint8_t Cell;

/* --- ESTRUTURAS DE DADOS --- */

/**
 * @brief Nó para a Lista Duplamente Encadeada (Gerenciamento de Bandeiras).
 */
typedef struct DListNode {
    size_t x, y;
    struct DListNode *prev;
    struct DListNode *next;
} DListNode;

/**
 * @brief Nó para a Fila (Algoritmo de Flood Fill).
 */
typedef struct QueueNode {
    size_t x, y;
    struct QueueNode *next;
} QueueNode;

/**
 * @brief Nó para a Pilha (Sistema de Undo/Desfazer).
 * Armazena a coordenada e o estado anterior da célula.
 */
typedef struct StackNode {
    size_t x, y;
    Cell old_value;       // Valor da célula antes da modificação
    bool is_batch_start;  // Marca o início de um grupo de mudanças (ex: 1 clique revelou 10 células)
    struct StackNode *next;
} StackNode;

/* --- ESTRUTURA DO JOGO --- */

typedef struct {
    size_t width;
    size_t height;
    size_t mine_num;
    Cell *cells;
    
    // Ponteiros para as Estruturas de Dados
    DListNode *flags_head; // Cabeça da Lista Dupla de flags
    DListNode *flags_tail; // Cauda da Lista Dupla (para inserção rápida se necessário)
    
    StackNode *undo_stack; // Topo da Pilha de Undo
} Board;

/* Variável Global para contagem de células reveladas */
size_t revealed_cells = 0;

/* --- PROTÓTIPOS AUXILIARES DAS ESTRUTURAS --- */

/**
 * @brief Adiciona uma coordenada à lista dupla de bandeiras.
 * @param board Ponteiro para o tabuleiro.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 */
void dlist_add(Board *board, size_t x, size_t y) {
    DListNode *node = malloc(sizeof(DListNode));
    node->x = x;
    node->y = y;
    node->next = board->flags_head;
    node->prev = NULL;

    if (board->flags_head != NULL) {
        board->flags_head->prev = node;
    }
    board->flags_head = node;
}

/**
 * @brief Remove uma coordenada da lista dupla de bandeiras.
 * @param board Ponteiro para o tabuleiro.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 */
void dlist_remove(Board *board, size_t x, size_t y) {
    DListNode *current = board->flags_head;
    while (current != NULL) {
        if (current->x == x && current->y == y) {
            if (current->prev) current->prev->next = current->next;
            if (current->next) current->next->prev = current->prev;
            if (current == board->flags_head) board->flags_head = current->next;
            free(current);
            return;
        }
        current = current->next;
    }
}

/**
 * @brief Empilha uma alteração de estado para o sistema de Undo.
 * @param board Ponteiro para o tabuleiro.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 * @param old_val Valor antigo da célula.
 * @param batch_start Booleano indicando se esta é a primeira mudança de uma ação do usuário.
 */
void stack_push(Board *board, size_t x, size_t y, Cell old_val, bool batch_start) {
    StackNode *node = malloc(sizeof(StackNode));
    node->x = x;
    node->y = y;
    node->old_value = old_val;
    node->is_batch_start = batch_start;
    node->next = board->undo_stack;
    board->undo_stack = node;
}

/**
 * @brief Desempilha e reverte alterações até encontrar o início do "lote" (batch).
 * @param board Ponteiro para o tabuleiro.
 * @return true se algo foi desfeito, false se a pilha estava vazia.
 */
bool stack_undo(Board *board) {
    if (board->undo_stack == NULL) return false;

    bool first = true;
    while (board->undo_stack != NULL) {
        StackNode *top = board->undo_stack;
        
        // Se for o início de um novo lote e não é o primeiro item que pegamos, paramos.
        if (top->is_batch_start && !first) break;

        // Reverte o estado
        Cell current = CELLS_AT(board, top->x, top->y);
        
        // Atualiza contadores globais/listas auxiliares ao reverter
        if (IS_REVEALED(current) && !IS_REVEALED(top->old_value)) {
            revealed_cells--;
        }
        if (IS_FLAGGED(current) && !IS_FLAGGED(top->old_value)) {
            dlist_remove(board, top->x, top->y);
        } else if (!IS_FLAGGED(current) && IS_FLAGGED(top->old_value)) {
            dlist_add(board, top->x, top->y);
        }

        CELLS_AT(board, top->x, top->y) = top->old_value;

        // Remove da pilha
        board->undo_stack = top->next;
        free(top);
        first = false;
    }
    return true;
}

/* --- FUNÇÕES DO JOGO --- */

/**
 * @brief Inicializa o jogo, aloca memória para o tabuleiro e distribui as minas.
 * @param board Ponteiro para a estrutura do tabuleiro.
 */
void init_game(Board *board) {
    revealed_cells = 0;
    board->flags_head = NULL;
    board->undo_stack = NULL;

    board->cells = realloc(board->cells, board->width * board->height * sizeof(*board->cells));
    if (board->cells == NULL) {
        perror("ERROR: malloc");
        exit(EXIT_FAILURE);
    }
    memset(board->cells, 0, board->width * board->height * sizeof(Cell));

    for (size_t i = 0; i < board->mine_num; i++) {
        size_t x, y;
        do {
            x = rand() % board->width;
            y = rand() % board->height;
        } while (IS_MINE(CELLS_AT(board, x, y)));
        
        SET_MINE(CELLS_AT(board, x, y), true);
        
        // Atualiza números ao redor da mina
        for (size_t j = 0; j < 8; j++) {
            size_t nx = x + dirs[j][0];
            size_t ny = y + dirs[j][1];
            if (nx >= board->width || ny >= board->height) continue;
            if (IS_MINE(CELLS_AT(board, nx, ny))) continue;
            CELLS_AT(board, nx, ny)++;
        }
    }
}

/**
 * @brief Imprime uma única célula com formatação de cores ANSI.
 * @param cell O valor da célula a ser impressa.
 */
void print_cell(Cell cell) {
    printf("\x1b[1m");
    if (IS_REVEALED(cell)) {
        printf("\x1b[47m"); // Fundo branco/cinza claro
        if (IS_MINE(cell)) {
            printf("\x1b[31m#"); // Mina vermelha
        } else if (MINE_NUM(cell) != 0) {
            uint8_t num = MINE_NUM(cell);
            // Cores baseadas no Minesweeper clássico
            switch (num) {
                case 1: printf("\x1b[94m"); break; // Azul claro
                case 2: printf("\x1b[32m"); break; // Verde
                case 3: printf("\x1b[91m"); break; // Vermelho claro
                case 4: printf("\x1b[34m"); break; // Azul escuro
                case 5: printf("\x1b[31m"); break; // Vermelho
                case 6: printf("\x1b[36m"); break; // Ciano
                case 7: printf("\x1b[30m"); break; // Preto
                case 8: printf("\x1b[90m"); break; // Cinza
                default: assert(false && "Unreachable");
            }
            printf("%u", num);
        } else {
            printf(" ");
        }
    } else {
        printf("\x1b[100m"); // Fundo cinza escuro (não revelado)
        if (IS_FLAGGED(cell)) {
            printf("\x1b[91m!"); // Bandeira vermelha
        } else {
            printf("\x1b[37m."); // Ponto branco
        }
    }
    printf(" \x1b[0m"); // Reset
}

/**
 * @brief Renderiza o tabuleiro completo no terminal.
 * @param board Ponteiro para o tabuleiro.
 */
void print_board(Board *board) {
    printf("   X ");
    for (size_t i = 0; i < board->width; i++) {
        size_t units = i % 10;
        printf("%zu%c", units, " |"[units == 9]);
    }
    printf("\n Y\x1b[1;40;37m +");
    for (size_t i = 0; i < board->width*2 + 1; i++) printf("-");
    printf("+ \x1b[0m\n");

    for (size_t y = 0; y < board->height; y++) {
        printf("%2zu\x1b[1;40;37m |\x1b[%dm ", y, IS_REVEALED(CELLS_AT(board, 0, y)) ? 47 : 100);
        for (size_t x = 0; x < board->width; x++) {
            print_cell(CELLS_AT(board, x, y));
        }
        printf("\x1b[1;40;37m| \x1b[0m\n");
    }

    printf("  \x1b[1;40;37m +");
    for (size_t i = 0; i < board->width*2 + 1; i++) printf("-");
    printf("+ \n\x1b[0m");
}

/**
 * @brief Lê uma linha de comando do usuário.
 * @param buf Buffer onde a string será armazenada.
 */
void read_command(char *buf) {
    printf("> ");
    if (fgets(buf, IN_BUF_SIZE, stdin) == NULL) {
        perror("ERROR: fgets");
        exit(EXIT_FAILURE);
    }
    buf[strlen(buf) - 1] = '\0';
}

/**
 * @brief Revela uma célula e seus vizinhos vazios usando uma FILA (Queue).
 * Implementa o algoritmo BFS (Breadth-First Search) para evitar recursão.
 * * @param board Ponteiro para o tabuleiro.
 * @param start_x Coordenada X inicial.
 * @param start_y Coordenada Y inicial.
 */
void reveal_cell(Board *board, size_t start_x, size_t start_y) {
    // Se já revelado ou marcado, ignora
    if (IS_REVEALED(CELLS_AT(board, start_x, start_y)) || IS_FLAGGED(CELLS_AT(board, start_x, start_y))) return;

    // Inicializa a Fila
    QueueNode *head = NULL;
    QueueNode *tail = NULL;

    // Função auxiliar local (via ponteiro) ou bloco de código para Enfileirar
    #define ENQUEUE(qx, qy) do { \
        QueueNode *new_node = malloc(sizeof(QueueNode)); \
        new_node->x = (qx); new_node->y = (qy); \
        new_node->next = NULL; \
        if (tail) tail->next = new_node; \
        else head = new_node; \
        tail = new_node; \
    } while(0)

    ENQUEUE(start_x, start_y);
    
    // Salva estado para Undo (apenas o início do "lote")
    stack_push(board, start_x, start_y, CELLS_AT(board, start_x, start_y), true);
    
    // Marca como revelado imediatamente para evitar re-enfileirar
    SET_REVEALED(CELLS_AT(board, start_x, start_y), true);
    revealed_cells++;

    // Se a primeira célula clicada não for vazia (tem número ou mina), processamos apenas ela e paramos.
    if (MINE_NUM(CELLS_AT(board, start_x, start_y)) != 0 || IS_MINE(CELLS_AT(board, start_x, start_y))) {
        free(head); // Limpa fila de um elemento
        return;
    }

    // Processamento da Fila (BFS)
    bool first_node = true; // Controla o push no stack para os vizinhos
    while (head != NULL) {
        // Dequeue
        QueueNode *curr = head;
        head = head->next;
        if (head == NULL) tail = NULL;

        size_t cx = curr->x;
        size_t cy = curr->y;
        free(curr);

        // Verifica vizinhos
        for (size_t j = 0; j < 8; j++) {
            size_t nx = cx + dirs[j][0];
            size_t ny = cy + dirs[j][1];

            if (nx >= board->width || ny >= board->height) continue;
            
            Cell *n_cell = &CELLS_AT(board, nx, ny);
            if (IS_REVEALED(*n_cell) || IS_FLAGGED(*n_cell)) continue;

            // Salva no stack de Undo (como parte do mesmo lote, então is_batch_start = false)
            stack_push(board, nx, ny, *n_cell, false);

            SET_REVEALED(*n_cell, true);
            revealed_cells++;

            // Se o vizinho for vazio (0), adiciona à fila para expandir
            if (MINE_NUM(*n_cell) == 0 && !IS_MINE(*n_cell)) {
                ENQUEUE(nx, ny);
            }
        }
        first_node = false;
    }
    #undef ENQUEUE
}

/**
 * @brief Revela células ao redor de uma célula numerada se o número de bandeiras corresponder (Chord).
 * @param board Ponteiro para o tabuleiro.
 * @param x Coordenada X.
 * @param y Coordenada Y.
 * @return true se uma mina explodiu, false caso contrário.
 */
bool reveal_around(Board *board, size_t x, size_t y) {
    size_t mine_num = MINE_NUM(CELLS_AT(board, x, y));
    
    // Conta flags ao redor
    for (size_t i = 0; i < 8; i++) {
        size_t nx = x + dirs[i][0];
        size_t ny = y + dirs[i][1];
        if (nx >= board->width || ny >= board->height) continue;
        if (IS_FLAGGED(CELLS_AT(board, nx, ny))) mine_num -= 1;
    }

    bool hit_mine = false;
    // Se flags suficientes, revela o resto
    if (mine_num <= 0) {
        bool first_reveal = true; // Para controlar o "Undo Batch"
        
        for (size_t i = 0; i < 8; i++) {
            size_t nx = x + dirs[i][0];
            size_t ny = y + dirs[i][1];
            if (nx >= board->width || ny >= board->height) continue;
            
            Cell cell = CELLS_AT(board, nx, ny);
            if (!IS_FLAGGED(cell) && !IS_REVEALED(cell)) {
                // Truque: chamamos reveal_cell, mas precisamos garantir que o stack saiba que é um grupo
                // A reveal_cell já cria um batch start=true. 
                // Se quisermos agrupar o "Chord" num único Undo, teríamos que passar um flag para reveal_cell.
                // Para simplificar, cada célula aberta pelo Chord será um "passo" de undo individual.
                reveal_cell(board, nx, ny);
                
                if (!hit_mine && IS_MINE(CELLS_AT(board, nx, ny))) {
                    hit_mine = true;
                }
            }
        }
    }
    return hit_mine;
}

/**
 * @brief Revela todo o tabuleiro (Game Over).
 * @param board Ponteiro para o tabuleiro.
 */
void reveal_board(Board *board) {
    // Não usamos stack aqui pois o jogo acabou
    for (size_t y = 0; y < board->height; y++) {
        for (size_t x = 0; x < board->width; x++) {
             SET_REVEALED(CELLS_AT(board, x, y), true);
        }
    }
}

/**
 * @brief Limpa a tela e redesenha o tabuleiro.
 * @param board Ponteiro para o tabuleiro.
 */
void refresh_screen(Board *board) {
    printf("\x1b[H\x1b[2J"); // ANSI escape codes para limpar tela
    print_board(board);
    
    // Mostra status da Pilha e Lista para debug/info
    size_t stack_depth = 0;
    StackNode *s = board->undo_stack;
    while(s) { stack_depth++; s = s->next; }
    
    size_t flag_count = 0;
    DListNode *f = board->flags_head;
    while(f) { flag_count++; f = f->next; }
    
    printf("Undo Stack Depth: %zu | Active Flags (List): %zu\n", stack_depth, flag_count);
}

/**
 * @brief Imprime as coordenadas de todas as bandeiras usando a Lista Dupla.
 * @param board Ponteiro para o tabuleiro.
 */
void list_flags(Board *board) {
    printf("Flagged Cells: ");
    DListNode *curr = board->flags_head;
    if (!curr) printf("(None)");
    while (curr) {
        printf("[%zu, %zu] ", curr->x, curr->y);
        curr = curr->next;
    }
    printf("\nPress Enter...");
    char tmp[10];
    fgets(tmp, 10, stdin);
}

/**
 * @brief Menu principal de seleção de dificuldade.
 */
void print_menu(void) {
    printf("\x1b[H\x1b[2J");
    printf("MineCweeper (Data Structures Edition)\n"
           "Difficulty options\n"
           "(E)asy   - 9x9 grid, 10 mines\n"
           "(M)edium - 16x16 grid, 40 mines\n"
           "(H)ard   - 30x16 grid, 99 mines\n"
           "Choose difficulty:\n\n"
           "(Type 'quit' to quit)\n\x1b[2A");
}

/**
 * @brief Menu de ajuda com comandos.
 */
void print_help_menu(void) {
    printf("Commands:\n"
           "r y x  : reveal cell (y=row, x=col)\n"
           "f y x  : flag/unflag cell\n"
           "u      : undo last move (uses Stack)\n"
           "lf     : list all flags (uses Doubly Linked List)\n"
           "help   : prints this list\n"
           "quit   : quits the game\n"
           "Press Enter...");
    char tmp[10];
    fgets(tmp, 10, stdin);
}

/**
 * @brief Função principal.
 */
int main(void) {
    srand(time(NULL));

    Board board = {0};
    char buf[IN_BUF_SIZE] = {0};

_begin_game:
    for (;;) {
        print_menu();
        read_command(buf);
        if (strcmp(buf, "quit") == 0) goto _exit_game;

        if (strcmp(buf, "E") == 0) {
            board.width = board.height = 9;
            board.mine_num = 10;
        } else if (strcmp(buf, "M") == 0) {
            board.width = board.height = 16;
            board.mine_num = 40;
        } else if (strcmp(buf, "H") == 0) {
            board.width = 30;
            board.height = 16;
            board.mine_num = 99;
        } else {
            continue;
        }
        break;
    }

    init_game(&board);
    refresh_screen(&board);

    for (;;) {
        read_command(buf);

        if (strcmp(buf, "quit") == 0) goto _exit_game;
        if (strcmp(buf, "help") == 0) {
            print_help_menu();
            refresh_screen(&board);
            continue;
        }
        if (strcmp(buf, "u") == 0) {
            if (stack_undo(&board)) {
                printf("Undo performed.\n");
            } else {
                printf("Nothing to undo.\n");
            }
            refresh_screen(&board);
            continue;
        }
        if (strcmp(buf, "lf") == 0) {
            list_flags(&board);
            refresh_screen(&board);
            continue;
        }

        char action;
        size_t x, y;
        // Parse: char + int + int
        if (sscanf(buf, "%c %zu %zu", &action, &y, &x) == 3) {
            if (x >= board.width || y >= board.height) {
                printf("Invalid coordinates (%zu,%zu).\n", x, y);
                continue;
            }

            if (action == 'f') {
                // Salva estado para Undo
                stack_push(&board, x, y, CELLS_AT(&board, x, y), true);
                
                bool is_flagged = IS_FLAGGED(CELLS_AT(&board, x, y));
                SET_FLAGGED(CELLS_AT(&board, x, y), !is_flagged);
                
                // Atualiza Lista Dupla
                if (!is_flagged) dlist_add(&board, x, y);
                else dlist_remove(&board, x, y);

            } else if (action == 'r') {
                if (IS_FLAGGED(CELLS_AT(&board, x, y))) {
                    printf("Cell is flagged. Unflag first.\n");
                    continue;
                }

                if (IS_REVEALED(CELLS_AT(&board, x, y))) {
                    // Tenta revelar ao redor (Chord)
                    bool hit_mine = reveal_around(&board, x, y);
                    if (hit_mine) {
                        reveal_board(&board);
                        refresh_screen(&board);
                        printf("\x1b[31mYou hit a mine! You lose!\x1b[0m\n");
                        break;
                    }
                } else {
                    // Revela célula (Usa Fila internamente)
                    reveal_cell(&board, x, y);
                    
                    if (IS_MINE(CELLS_AT(&board, x, y))) {
                        reveal_board(&board);
                        refresh_screen(&board);
                        printf("\x1b[31mYou hit a mine! You lose!\x1b[0m\n");
                        break;
                    }
                }
                
                // Checa vitória
                if (revealed_cells == board.width*board.height - board.mine_num) {
                    refresh_screen(&board);
                    printf("\x1b[32mYou won! Congratulations!\x1b[0m\n");
                    break;
                }
            }
            refresh_screen(&board);
        } else {
            printf("Invalid command: '%s'. Use 'help' for info.\n", buf);
        }
    }

    for (;;) {
        printf("Try again? (Y/N) > ");
        read_command(buf);
        if (strcmp(buf, "Y") == 0 || strcmp(buf, "y") == 0) {
            // Limpa estruturas antes de reiniciar
            while(stack_undo(&board)); // Esvazia stack
            while(board.flags_head) dlist_remove(&board, board.flags_head->x, board.flags_head->y); // Esvazia lista
            goto _begin_game;
        } else if (strcmp(buf, "N") == 0 || strcmp(buf, "n") == 0) {
            break;
        }
    }

_exit_game:
    // Limpeza de memória final
    while(stack_undo(&board));
    while(board.flags_head) dlist_remove(&board, board.flags_head->x, board.flags_head->y);
    free(board.cells);
    return 0;
}