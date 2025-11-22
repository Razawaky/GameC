CC = gcc
CFLAGS = -Wall -g -Iinclude

SRC = src/main.c src/input_queue.c src/move_stack.c src/snake.c
OBJ = $(SRC:.c=.o)
TARGET = snake_game

all: $(TARGET)

$(TARGET): $(OBJ)
    $(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
    $(CC) $(CFLAGS) -c $<

clean:
    rm -f src/*.o $(TARGET)
