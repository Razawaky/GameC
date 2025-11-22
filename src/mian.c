// main.c
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>      // para usleep no Unix-like (ou Sleep no Windows)
#include "input_queue.h"
#include "move_stack.h"
#include "snake.h"

#define WIDTH  20
#define HEIGHT 10
#define TICK_USEC 200000    // 200 ms entre “frames”

int main(void) {
    // Inicialização
    InputQueue iq;
    queue_init(&iq, 32);

    MoveStack ms;
    stack_init(&ms);

    Game game;
    init_game(&game, WIDTH, HEIGHT);   // módulo snake.h / snake.c

    bool running = true;
    while (running && !game.game_over) {
        // 1. Ler input não-bloqueante e enfileirar se houver
        char c = '\0';
        if (kbhit()) {
            c = getchar();
            queue_enqueue(&iq, c);
        }

        // 2. Se houver comando na fila, usar o próximo
        if (!queue_is_empty(&iq)) {
            char next = queue_dequeue(&iq);
            change_direction(&game, char_to_direction(next));  // você cria char_to_direction
        }

        // 3. Atualizar jogo
        update_game(&game);

        // 4. Empilhar o movimento (direção) para histórico
        stack_push(&ms, game.snake.dir);

        // 5. Desenhar estado
        draw(&game);

        // 6. Aguardar próximo “tick”
        usleep(TICK_USEC);
    }

    // Game-over ou saída
    printf("Game Over! Score: %d\n", game.score);

    // Liberar recursos
    queue_free(&iq);
    stack_free(&ms);
    free_snake(&game.snake);

    return 0;
}
