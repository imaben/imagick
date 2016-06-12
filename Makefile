CC=gcc
CFLAGS=-g -o
PROC=imagick
INCPATH=
INCLIB=

all:
	cd src && $(CC) $(INCPATH) $(CFLAGS) $(PROC) imagick.c daemon.c process.c channel.c log.c events.c connection.c http_parser.c worker.c $(INCLIB)
clean:
	rm -rf src/$(PROC)
	rm -rf src/*.o
