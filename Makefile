CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lX11 -lm

SRC = main.c screen.c pty.c
OBJ = $(SRC:.c=.o)
TARGET = myTerm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
