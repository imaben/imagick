CC=gcc
CFLAGS=-g -o
PROC=imagick
INCPATH=-I./ncx_mempool/ -I./
INCLIB=

all:
	cd src && $(CC) $(INCPATH) $(CFLAGS) $(PROC) imagick.c daemon.c process.c channel.c log.c events.c connection.c http_parser.c worker.c utils.c ncx_mempool/ncx_lock.c ncx_mempool/ncx_slab.c hash.c $(INCLIB)
clean:
	rm -rf src/$(PROC)
	rm -rf src/*.o
