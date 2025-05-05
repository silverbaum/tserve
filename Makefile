CC=cc
CFLAGS=-Wall -Wextra -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L

SRC=src
OBJS=util.o sock.o
INCLUDE=-Iinclude

unity: $(SRC)/main.c $(SRC)/tserve.c
	$(CC) $(CFLAGS) -o tserve.o -c $(SRC)/main.c

tserve: $(SRC)/tserve.c $(OBJS)
	$(CC) $(CFLAGS) -c $(SRC)/tserve.c $(INC)

util.o: $(SRC)/util.c
	$(CC) $(CFLAGS) -c $(SRC)/util.c $(INC)
sock.o: $(SRC)/sock.c
	$(CC) $(CFLAGS) -c $(SRC)/sock.c $(INC)

DEBUG: 
	$(CC) $(CFLAGS) -c $(SRC)/main.c -o tserve_debug.o -DDEBUG -ggdb -fsanitize=address

clean:
	rm *.o
