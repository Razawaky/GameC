#ifndef INPUT_QUEUE_H
#define INPUT_QUEUE_H

#include <stdbool.h>

typedef struct {
    char *data;
    int front;
    int rear;
    int size;
    int capacity;
} InputQueue;

void queue_init(InputQueue *q, int capacity);
bool queue_is_empty(const InputQueue *q);
bool queue_is_full(const InputQueue *q);
bool queue_enqueue(InputQueue *q, char c);
char queue_dequeue(InputQueue *q);
void queue_free(InputQueue *q);

#endif // INPUT_QUEUE_H
