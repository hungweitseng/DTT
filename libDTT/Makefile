CC = gcc

all: libDTT.so libDTT.a

libDTT.so: dtt.o
	$(CC) -shared -Wl,-soname,libDTT.so.1 -o libDTT.so dtt.o

libDTT.a: dtt.o 
	$(AR) rcs libDTT.a dtt.o  

dtt.o: dtt.c dtt.h
	$(CC) -O2 -fPIC dtt.c -c  -lm -lpthread
	
clean:
	rm -f dtt.o libDTT.*
	