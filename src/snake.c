#include "snake.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// Cria inicialização da cobra e do jogo
void init_game(Game *g, int width, int height) {
    g->width = width;
    g->height = height;
    g->score = 0;
    g->game_over = false;

    // Inicializa a cobra
    g->snake.capacity = width * height;
    g->snake.body = malloc(sizeof(Segment) * g->snake.capacity);
    g->snake.length = 1;
    g->snake.body[0].x = width / 2;
    g->snake.body[0].y = height / 2;
    g->snake.dir = DIR_RIGHT;

    // Semente para aleatoriedade da comida
    srand((unsigned) time(NULL));
    g->food_x = rand() % width;
    g->food_y = rand() % height;
}

// Libera memória da cobra
void free_snake(Snake *s) {
    free(s->body);
    s->body = NULL;
    s->length = 0;
    s->capacity = 0;
}

// Muda a direção da cobra
void change_direction(Game *g, Direction d) {
    // Impede inversão direta — por exemplo, se a cobra está indo para cima, não deixa ir para baixo diretamente
    Direction curr = g->snake.dir;
    if ((curr == DIR_UP && d == DIR_DOWN) ||
        (curr == DIR_DOWN && d == DIR_UP) ||
        (curr == DIR_LEFT && d == DIR_RIGHT) ||
        (curr == DIR_RIGHT && d == DIR_LEFT)) {
        return;  // ignora inversão
    }
    g->snake.dir = d;
}

// Atualiza o estado do jogo (posição da cobra, comida, colisões)
void update_game(Game *g) {
    Snake *s = &g->snake;

    // calcula nova cabeça
    Segment new_head = s->body[s->length - 1];
    switch (s->dir) {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }

    // Verifica borda
    if (new_head.x < 0 || new_head.x >= g->width ||
        new_head.y < 0 || new_head.y >= g->height) {
        g->game_over = true;
        return;
    }

    // Move o corpo para frente (shift)
    for (int i = 0; i < s->length - 1; i++) {
        s->body[i] = s->body[i + 1];
    }
    s->body[s->length - 1] = new_head;

    // Verifica se comeu
    if (new_head.x == g->food_x && new_head.y == g->food_y) {
        // aumenta cobra
        if (s->length < s->capacity) {
            s->body[s->length] = new_head;
            s->length++;
        }
        g->score++;

        // gera nova comida
        g->food_x = rand() % g->width;
        g->food_y = rand() % g->height;
    }
}

// Desenha estado do jogo no terminal (só exemplo simples)
void draw(const Game *g) {
    // limpa tela
    printf("\033[H\033[J");
    for (int y = 0; y < g->height; y++) {
        for (int x = 0; x < g->width; x++) {
            bool printed = false;
            // cobra
            for (int i = 0; i < g->snake.length; i++) {
                if (g->snake.body[i].x == x && g->snake.body[i].y == y) {
                    printf("O");
                    printed = true;
                    break;
                }
            }
            // comida
            if (!printed && x == g->food_x && y == g->food_y) {
                printf("X");
                printed = true;
            }
            if (!printed) {
                printf(".");
            }
        }
        printf("\n");
    }
    printf("Score: %d\n", g->score);
}

// Converte char lido (teclado) para direção
Direction char_to_direction(char c) {
    switch (c) {
        case 'w': return DIR_UP;
        case 's': return DIR_DOWN;
        case 'a': return DIR_LEFT;
        case 'd': return DIR_RIGHT;
        default: return DIR_RIGHT; // fallback
    }
}
