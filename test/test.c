/*
 * MineCweeper - Estruturas de Dados Edition
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

#define IN_BUF_SIZE 128

// Acesso à matriz linearizada
#define CELLS_AT(board, x, y) ((board)->cells[(y) * (board)->width + (x)])

// Manipulação de Bits (Bitwise) para economizar memória
// Bits: 0-3: Número de minas vizinhas (0-8)
// Bit 5: É mina?
// Bit 6: Tem bandeira?
// Bit 7: Está revelada?
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

// Direções (8 vizinhos)
static const int dirs[][2] = {
    {-1, -1}, {-1, 1}, {1, -1}, {1, 1},
    {0, -1}, {0, 1}, {-1, 0}, {1, 0},
};

// Tipo base da célula
typedef uint8_t Cell;

/* --- ESTRUTURAS DE DADOS --- */

/**
 * @brief LISTA DUPLA: Gerenciamento de Bandeiras.
 * Permite iteração rápida sobre flags e remoção O(1) se tivermos o nó (aqui O(N) pela busca).
 */
typedef struct DListNode {
    size_t x, y;
    struct DListNode *prev;
    struct DListNode *next;
} DListNode;

/**
 * @brief FILA: Algoritmo de Flood Fill (BFS).
 * Usada para revelar áreas vazias sem estourar a pilha de recursão.
 */
typedef struct QueueNode {
    size_t x, y;
    struct QueueNode *next;
} QueueNode;

/**
 * @brief PILHA: Sistema de Undo/Desfazer.
 * Armazena o estado anterior de uma célula modificada.
 */
typedef struct StackNode {
    size_t x, y;
    Cell old_value;       // Valor antes da modificação
    bool is_batch_start;  // Marca o início de uma jogada (ex: 1 clique pode revelar 50 células)
    struct StackNode *next;
} StackNode;

/* --- ESTADO DO JOGO --- */

typedef struct {
    size_t width;
    size_t height;
    size_t mine_num;
    Cell *cells;
    
    // Cabeças das Estruturas
    DListNode *flags_head; 
    StackNode *undo_stack; 
} Board;

// Global para controle rápido de vitória
size_t revealed_cells = 0;

/* --- IMPLEMENTAÇÃO DAS ESTRUTURAS DE DADOS --- */

/**
 * @brief Adiciona coordenada à Lista Dupla de flags.
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
 * @brief Remove coordenada da Lista Dupla de flags.
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
 * @brief Empilha uma alteração na Pilha de Undo.
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
 * @brief Desfaz a última jogada (reverte lote de alterações).
 * @return true se desfez algo, false se a pilha estava vazia.
 */
bool stack_undo(Board *board) {
    if (board->undo_stack == NULL) return false;

    bool first = true;
    while (board->undo_stack != NULL) {
        StackNode *top = board->undo_stack;
        
        // Se for início de lote e não é o primeiro nó que processamos, paramos.
        if (top->is_batch_start && !first) break;

        Cell current = CELLS_AT(board, top->x, top->y);
        
        // Atualiza globais e listas auxiliares ao reverter
        if (IS_REVEALED(current) && !IS_REVEALED(top->old_value)) {
            revealed_cells--;
        }
        
        // Sincroniza Lista de Flags com o Undo
        bool was_flagged = IS_FLAGGED(top->old_value);
        bool is_flagged_now = IS_FLAGGED(current);
        
        if (is_flagged_now && !was_flagged) {
            dlist_remove(board, top->x, top->y);
        } else if (!is_flagged_now && was_flagged) {
            dlist_add(board, top->x, top->y);
        }

        // Restaura valor
        CELLS_AT(board, top->x, top->y) = top->old_value;

        // Pop
        board->undo_stack = top->next;
        free(top);
        first = false;
    }
    return true;
}

/* --- LÓGICA DO JOGO --- */

/**
 * @brief Lê input do usuário removendo newline.
 */
void get_input(char *buf, int size) {
    if (fgets(buf, size, stdin) != NULL) {
        buf[strcspn(buf, "\n")] = 0;
    } else {
        buf[0] = 0;
    }
}

/**
 * @brief Inicializa o tabuleiro e distribui minas.
 */
void init_game(Board *board) {
    revealed_cells = 0;
    board->flags_head = NULL;
    board->undo_stack = NULL;

    board->cells = realloc(board->cells, board->width * board->height * sizeof(*board->cells));
    if (!board->cells) {
        perror("ERROR: malloc");
        exit(EXIT_FAILURE);
    }
    memset(board->cells, 0, board->width * board->height * sizeof(Cell));

    // Distribuição de Minas
    for (size_t i = 0; i < board->mine_num; i++) {
        size_t x, y;
        do {
            x = rand() % board->width;
            y = rand() % board->height;
        } while (IS_MINE(CELLS_AT(board, x, y)));
        
        SET_MINE(CELLS_AT(board, x, y), true);
        
        // Incrementa números vizinhos
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
 * @brief Imprime uma célula colorida.
 */
void print_cell(Cell cell) {
    printf("\x1b[1m");
    if (IS_REVEALED(cell)) {
        printf("\x1b[47m"); // Fundo Claro
        if (IS_MINE(cell)) {
            printf("\x1b[31m#");
        } else if (MINE_NUM(cell) != 0) {
            uint8_t num = MINE_NUM(cell);
            switch (num) {
                case 1: printf("\x1b[94m"); break; // Azul
                case 2: printf("\x1b[32m"); break; // Verde
                case 3: printf("\x1b[91m"); break; // Vermelho
                case 4: printf("\x1b[34m"); break; // Azul Escuro
                case 5: printf("\x1b[31m"); break;
                case 6: printf("\x1b[36m"); break;
                case 7: printf("\x1b[30m"); break;
                case 8: printf("\x1b[90m"); break;
                default: break;
            }
            printf("%u", num);
        } else {
            printf(" ");
        }
    } else {
        printf("\x1b[100m"); // Fundo Escuro
        if (IS_FLAGGED(cell)) {
            printf("\x1b[91m!");
        } else {
            printf("\x1b[37m.");
        }
    }
    printf(" \x1b[0m");
}

/**
 * @brief Imprime o tabuleiro completo.
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
 * @brief Limpa tela e redesenha interface com debug.
 */
void refresh_screen(Board *board) {
    printf("\x1b[H\x1b[2J"); 
    print_board(board);

    // Stats das estruturas de dados
    size_t stack_depth = 0;
    StackNode *s = board->undo_stack;
    while(s) { stack_depth++; s = s->next; }
    
    size_t flag_count = 0;
    DListNode *f = board->flags_head;
    while(f) { flag_count++; f = f->next; }
    
    printf("--- Info ---\n");
    printf("Undo Stack Depth: %zu | Active Flags: %zu\n", stack_depth, flag_count);
}

/**
 * @brief Alterna bandeira (Flag). Usa Lista Dupla e Pilha.
 */
void toggle_flag(Board *board, size_t x, size_t y) {
    stack_push(board, x, y, CELLS_AT(board, x, y), true);

    bool is_flagged = IS_FLAGGED(CELLS_AT(board, x, y));
    SET_FLAGGED(CELLS_AT(board, x, y), !is_flagged);

    if (!is_flagged) dlist_add(board, x, y);
    else dlist_remove(board, x, y);
}

/**
 * @brief Revela célula usando Fila (BFS).
 */
void reveal_cell(Board *board, size_t start_x, size_t start_y) {
    if (IS_REVEALED(CELLS_AT(board, start_x, start_y)) || IS_FLAGGED(CELLS_AT(board, start_x, start_y))) return;

    // Macro local para Enfileirar
    QueueNode *head = NULL, *tail = NULL;
    #define ENQUEUE(qx, qy) do { \
        QueueNode *nn = malloc(sizeof(QueueNode)); \
        nn->x = (qx); nn->y = (qy); nn->next = NULL; \
        if (tail) tail->next = nn; else head = nn; \
        tail = nn; \
    } while(0)

    ENQUEUE(start_x, start_y);
    
    // Undo: Início de lote
    stack_push(board, start_x, start_y, CELLS_AT(board, start_x, start_y), true);
    
    SET_REVEALED(CELLS_AT(board, start_x, start_y), true);
    revealed_cells++;

    // Se clicou em número ou mina, não expande
    if (MINE_NUM(CELLS_AT(board, start_x, start_y)) != 0 || IS_MINE(CELLS_AT(board, start_x, start_y))) {
        free(head);
        return;
    }

    // BFS
    while (head) {
        QueueNode *curr = head;
        head = head->next;
        if (!head) tail = NULL;

        size_t cx = curr->x;
        size_t cy = curr->y;
        free(curr);

        for (size_t j = 0; j < 8; j++) {
            size_t nx = cx + dirs[j][0];
            size_t ny = cy + dirs[j][1];

            if (nx >= board->width || ny >= board->height) continue;
            Cell *n_cell = &CELLS_AT(board, nx, ny);
            
            if (IS_REVEALED(*n_cell) || IS_FLAGGED(*n_cell)) continue;

            // Undo: Continuação do lote
            stack_push(board, nx, ny, *n_cell, false);

            SET_REVEALED(*n_cell, true);
            revealed_cells++;

            if (MINE_NUM(*n_cell) == 0 && !IS_MINE(*n_cell)) {
                ENQUEUE(nx, ny);
            }
        }
    }
    #undef ENQUEUE
}

/**
 * @brief Tenta revelar ao redor se flags baterem com número (Chord).
 */
bool reveal_around(Board *board, size_t x, size_t y) {
    size_t mine_num = MINE_NUM(CELLS_AT(board, x, y));
    for (size_t i = 0; i < 8; i++) {
        size_t nx = x + dirs[i][0];
        size_t ny = y + dirs[i][1];
        if (nx >= board->width || ny >= board->height) continue;
        if (IS_FLAGGED(CELLS_AT(board, nx, ny))) mine_num -= 1;
    }

    bool hit_mine = false;
    if (mine_num <= 0) {
        for (size_t i = 0; i < 8; i++) {
            size_t nx = x + dirs[i][0];
            size_t ny = y + dirs[i][1];
            if (nx >= board->width || ny >= board->height) continue;
            
            Cell cell = CELLS_AT(board, nx, ny);
            if (!IS_FLAGGED(cell) && !IS_REVEALED(cell)) {
                // Cada reveal gera seu próprio lote de undo neste caso simplificado
                reveal_cell(board, nx, ny);
                if (!hit_mine && IS_MINE(CELLS_AT(board, nx, ny))) hit_mine = true;
            }
        }
    }
    return hit_mine;
}

/**
 * @brief Revela todo o tabuleiro (Game Over).
 */
void reveal_board(Board *board) {
    for (size_t y = 0; y < board->height; y++) {
        for (size_t x = 0; x < board->width; x++) {
             SET_REVEALED(CELLS_AT(board, x, y), true);
        }
    }
}

/**
 * @brief Verifica vitória.
 */
bool check_victory(Board *board) {
    size_t total_safe = (board->width * board->height) - board->mine_num;
    return revealed_cells == total_safe;
}

/**
 * @brief Lista bandeiras usando a Lista Dupla.
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
    get_input(tmp, 10);
}

/**
 * @brief Limpa memória.
 */
void free_game_memory(Board *board) {
    while(stack_undo(board)); 
    while(board->flags_head) dlist_remove(board, board->flags_head->x, board->flags_head->y);
    if (board->cells) {
        free(board->cells);
        board->cells = NULL;
    }
}

void print_menu(void) {
    printf("\x1b[H\x1b[2J");
    printf("MineCweeper (Stack/Queue/List Edition)\n"
           "(E)asy   - 9x9, 10 mines\n"
           "(M)edium - 16x16, 40 mines\n"
           "(H)ard   - 30x16, 99 mines\n"
           "Choose difficulty (or 'quit'):\n");
}

void print_help_menu(void) {
    printf("Commands:\n"
           "r y x  : reveal cell (y=row, x=col)\n"
           "f y x  : flag/unflag cell\n"
           "u      : undo last move\n"
           "lf     : list flags\n"
           "help   : help\n"
           "quit   : exit\n"
           "Press Enter...");
    char tmp[10];
    get_input(tmp, 10);
}

/* --- MAIN --- */

int main(void) {
    srand(time(NULL));
    Board board = {0};
    char buf[IN_BUF_SIZE] = {0};

_begin_game:
                         // --- SELEÇÃO DE DIFICULDADE ---
    for (1) {
        print_menu();
        printf("> ");
        get_input(buf, IN_BUF_SIZE);

        if (strcmp(buf, "quit") == 0) goto _exit_game;

        if (strcmp(buf, "E") == 0) { board.width = 9;  board.height = 9;  board.mine_num = 10; }
        else if (strcmp(buf, "M") == 0) { board.width = 16; board.height = 16; board.mine_num = 40; }
        else if (strcmp(buf, "H") == 0) { board.width = 30; board.height = 16; board.mine_num = 99; }
        else continue;
        break;
    }

    init_game(&board);
    refresh_screen(&board);

    // --- LOOP PRINCIPAL ---
    for (1) {
        printf("\nCommand > ");
        get_input(buf, IN_BUF_SIZE);

        if (strcmp(buf, "quit") == 0) goto _exit_game;
        if (strcmp(buf, "help") == 0) { print_help_menu(); refresh_screen(&board); continue; }
        if (strcmp(buf, "u") == 0) {
            if (stack_undo(&board)) printf("Undo performed.\n");
            else printf("Nothing to undo.\n");
            refresh_screen(&board);
            continue;
        }
        if (strcmp(buf, "lf") == 0) { list_flags(&board); refresh_screen(&board); continue; }

        char action;
        size_t x, y;
        if (sscanf(buf, "%c %zu %zu", &action, &y, &x) == 3) {
            if (x >= board.width || y >= board.height) {
                printf("Invalid coordinates.\n");
                continue;
            }

            if (action == 'f') {
                toggle_flag(&board, x, y);
                refresh_screen(&board);
            } 
            else if (action == 'r') {
                Cell current = CELLS_AT(&board, x, y);
                if (IS_FLAGGED(current)) {
                    printf("Cell is flagged. Unflag first.\n");
                    continue;
                }

                bool hit_mine = false;
                if (IS_REVEALED(current)) {
                    hit_mine = reveal_around(&board, x, y);
                } else {
                    reveal_cell(&board, x, y);
                    if (IS_MINE(CELLS_AT(&board, x, y))) hit_mine = true;
                }

                if (hit_mine) {
                    reveal_board(&board);
                    refresh_screen(&board);
                    printf("\n\x1b[31mBOOM! You hit a mine!\x1b[0m\n");
                    break; 
                }
                if (check_victory(&board)) {
                    refresh_screen(&board);
                    printf("\n\x1b[32mCONGRATULATIONS! You cleared the field!\x1b[0m\n");
                    break;
                }
                refresh_screen(&board);
            }
        } else {
            printf("Invalid command.\n");
        }
    }

    // --- REINICIAR? ---
    for (1) {
        printf("Play again? (Y/N) > ");
        get_input(buf, IN_BUF_SIZE);
        if (strcmp(buf, "Y") == 0 || strcmp(buf, "y") == 0) {
            free_game_memory(&board);
            goto _begin_game;
        } else if (strcmp(buf, "N") == 0 || strcmp(buf, "n") == 0) {
            break;
        }
    }

_exit_game:
    free_game_memory(&board);
    printf("Bye!\n");
    return 0;
}