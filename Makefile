CC = gcc
CFLAGS = -Wall -O2 $(shell pkg-config --cflags xft)
LDLIBS = $(shell pkg-config --libs xft) -lX11 -lm

SRC = main.c screen.c pty.c
OBJ = $(SRC:.c=.o)
TARGET = myTerm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: config.h screen.h
screen.o: screen.h

clean:
	rm -f $(OBJ) $(TARGET)

format:
	clang-format -i *.c

install: all
	cp myTerm ~/.local/bin/
