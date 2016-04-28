P=toi
OBJECTS=$(P).o
CFLAGS = `pkg-config --cflags hiredis futile` -g -Wall -std=c11 -pedantic -O3
LDLIBS = `pkg-config --libs hiredis futile`

$(P): $(OBJECTS)

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@

run: $(P)
	./$(P) localhost

clean:
	rm -f $(OBJECTS) $(P)

valgrind: $(P)
	G_DEBUG=gc-friendly G_SLICE=always-malloc valgrind --leak-check=full ./$(P) localhost
