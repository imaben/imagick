CC=gcc
CFLAGS=-g -o
PROC=imagick
INCPATH=
INCLIB=-I./ncx_mempool/ -I./

all:
	cd src && $(CC) $(INCPATH) $(CFLAGS) $(PROC) imagick.c daemon.c process.c channel.c log.c events.c connection.c http_parser.c worker.c utils.c ncx_mempool/ncx_lock.c ncx_mempool/ncx_slab.c $(INCLIB)
clean:
	rm -rf src/$(PROC)
	rm -rf src/*.o
