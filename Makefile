CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99

TARGET = vm


OBJS = main.o src/vm.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)


main.o: main.c src/vm.h
	$(CC) $(CFLAGS) -c main.c -o main.o


src/vm.o: src/vm.c src/vm.h src/opcode.h
	$(CC) $(CFLAGS) -c src/vm.c -o src/vm.o

clean:
	rm -f main.o src/vm.o $(TARGET)


test: $(TARGET)
	./$(TARGET)

.PHONY: all clean test
