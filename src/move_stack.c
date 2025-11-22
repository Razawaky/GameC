#include "move_stack.h"
#include <stdlib.h>

void stack_init(MoveStack *s, int capacity) {
    s->data = malloc(sizeof(char) * capacity);
    s->capacity = capacity;
    s->top = -1;
}

bool stack_is_empty(const MoveStack *s) {
    return s->top == -1;
}

bool stack_is_full(const MoveStack *s) {
    return s->top == s->capacity - 1;
}

bool stack_push(MoveStack *s, char c) {
    if (stack_is_full(s)) return false;
    s->data[++s->top] = c;
    return true;
}

char stack_pop(MoveStack *s) {
    if (stack_is_empty(s)) return '\0';
    return s->data[s->top--];
}

void stack_free(MoveStack *s) {
    free(s->data);
    s->data = NULL;
    s->capacity = 0;
    s->top = -1;
}
