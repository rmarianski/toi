P=toi
OBJECTS=$(P).o
CFLAGS = `pkg-config --cflags hiredis futile` -g -Wall -std=gnu11 -O3
LDLIBS = `pkg-config --libs hiredis futile`

$(P): $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

print: $(P)
	./$(P) -b toi.bin -c print

hash: $(P)
	./$(P) -b toi.bin -c hash

dump: $(P)
	./$(P) -r localhost -c dump -d toi.bin

clean:
	rm -f $(OBJECTS) $(P)

valgrind: $(P)
	G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind --leak-check=full ./$(P) -b toi.bin -c print
