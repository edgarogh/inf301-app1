CC=clang
CFLAGS=-Wall -g

EXEC=app1

all: $(EXEC)

app1: app1.o client.o

client.o: client.c client.h

clean:
	rm -f *.o

clear:
	rm -f $(EXEC)
