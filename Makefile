P=toi
OBJECTS=$(P).o
CFLAGS = `pkg-config --cflags hiredis futile` -g -Wall -std=gnu11 -O3
LDLIBS = `pkg-config --libs hiredis futile`
#LDLIBS += -static -lc

$(P): $(OBJECTS)

print: $(P)
	./$(P) -b toi.bin -c print

hash: $(P)
	./$(P) -b toi.bin -c hash

dump: $(P)
	./$(P) -r localhost -c dump -d toi.bin

diff: $(P)
	./$(P) -b toi.bin -c diff -z 10/157/354-10/321/440-16
clean:
	rm -f $(OBJECTS) $(P)

valgrind: $(P)
	valgrind --leak-check=full ./$(P) -b toi.bin -c diff -z 10/157/354-10/321/440-16
