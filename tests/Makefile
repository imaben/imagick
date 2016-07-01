CC=gcc
CFLAGS=-g -o
PROC=
INCPATH=-I../src
INCLIB=

all:
	$(CC) $(INCPATH) $(CFLAGS) event event.c ../src/log.c ../src/events.c #event
	$(CC) $(INCPATH) $(CFLAGS) path path.c ../src/utils.c #path
clean:
	rm -rf src/$(PROC)
	rm -rf src/*.o
