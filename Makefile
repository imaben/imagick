CC=gcc
CFLAGS=-g -o
PROC=imagick
INCPATH=
INCLIB=

all:
	$(CC) $(INCPATH) $(CFLAGS) $(PROC) imagick.c daemon.c process.c channel.c log.c $(INCLIB)
clean:
	rm -rf $(PROC)
	rm -rf *.o
