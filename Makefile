CC = gcc
CFLAGS = -Wall -O2
LDLIBS = -lX11

SRC = main.c
OBJ = $(SRC:.c=.o)
TARGET = myTerm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
