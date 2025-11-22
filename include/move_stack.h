#ifndef MOVE_STACK_H
#define MOVE_STACK_H

#include <stdbool.h>

typedef struct {
    char *data;
    int top;
    int capacity;
} MoveStack;

void stack_init(MoveStack *s, int capacity);
bool stack_is_empty(const MoveStack *s);
bool stack_is_full(const MoveStack *s);
bool stack_push(MoveStack *s, char c);
char stack_pop(MoveStack *s);
void stack_free(MoveStack *s);

#endif // MOVE_STACK_H
