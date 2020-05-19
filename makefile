SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

LDFLAGS = -lpng

csteg : $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY : clean
clean :
	rm -rf $(OBJ) csteg
