###############################################################################
# SPECtools2000
#   Version 0.3
#   This makefile should NOT be changed other than to customize for a 
#   particular benchmark.  Users may overide variables by placing them
#   in the Makefile.spec file.  Or by using the "runspec" program to 
#   build the Makefile.spec file for them.
###############################################################################
#	BENCHMARK	-- standard definitions for this benchmark

DTT_PATH = ../libDTT
DTTC = ../preprocessor/dttc

all: equake

equake: quake.c
	$(CC) -O2 -static quake.c -o equake -lm -I$(DTT_PATH) -L$(DTT_PATH) -lDTT -lpthread

quake.c: quake.dtt 
	$(DTTC) quake.dtt 

clean:
	rm -f quake.c equake 
	