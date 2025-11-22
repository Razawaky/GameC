#ifndef SNAKE_H
#define SNAKE_H

#include <stdbool.h>

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef struct {
    int x, y;
} Segment;

typedef struct {
    Segment *body;    // array dinâmico da cobra
    int length;
    int capacity;
    Direction dir;
} Snake;

typedef struct {
    Snake snake;
    int width;
    int height;
    int food_x;
    int food_y;
    bool game_over;
    int score;
} Game;

void init_game(Game *g, int width, int height);
void free_snake(Snake *s);
void change_direction(Game *g, Direction d);
void update_game(Game *g);
void draw(const Game *g);

// Função utilitária para converter char para Direction (você deve definir)
Direction char_to_direction(char c);

#endif // SNAKE_H
