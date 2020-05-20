SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
CC = gcc

CFLAGS = -Wall
LDFLAGS = -lpng

csteg : $(SRC)
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

debug : CFLAGS += -g
debug : csteg

.PHONY : clean
clean :
	rm -rf $(OBJ) csteg csteg.dSYM
