P=toi
OBJECTS=util.o hash.o

CFLAGS = `pkg-config --cflags hiredis futile` -g -Wall -std=gnu11 -O3
LDLIBS = `pkg-config --libs hiredis futile`

$(P): $(P).o $(OBJECTS)

toi-diff: toi-diff.o $(OBJECTS)

toi-log: toi-log.o $(OBJECTS)

clean:
	rm -f $(P) $(OBJECTS) toi-log.o toi-log toi-diff toi-diff.o
