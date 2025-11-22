#include "input_queue.h"
#include <stdlib.h>

void queue_init(InputQueue *q, int capacity) {
    q->data = malloc(sizeof(char) * capacity);
    q->capacity = capacity;
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

bool queue_is_empty(const InputQueue *q) {
    return q->size == 0;
}

bool queue_is_full(const InputQueue *q) {
    return q->size == q->capacity;
}

bool queue_enqueue(InputQueue *q, char c) {
    if (queue_is_full(q)) return false;
    q->rear = (q->rear + 1) % q->capacity;
    q->data[q->rear] = c;
    q->size++;
    return true;
}

char queue_dequeue(InputQueue *q) {
    if (queue_is_empty(q)) return '\0';
    char c = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;
    return c;
}

void queue_free(InputQueue *q) {
    free(q->data);
    q->data = NULL;
    q->capacity = 0;
    q->size = 0;
}
