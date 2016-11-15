P=toi
DEP_OBJECTS=util.o hash.o

CFLAGS = `pkg-config --cflags futile hiredis` -g -Wall -std=gnu11 -O3
LDLIBS = `pkg-config --libs hiredis` -lm

$(P): $(P).o $(DEP_OBJECTS)

toi-diff: toi-diff.o $(DEP_OBJECTS)

toi-log: toi-log.o $(DEP_OBJECTS)

clean:
	rm -f $(P) $(P).o $(DEP_OBJECTS) toi-log.o toi-log toi-diff toi-diff.o
