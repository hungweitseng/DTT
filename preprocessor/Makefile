SRC = dttc.c 

CFLAGS += -O2 -D_GNU_SOURCE -g

EXE = dttc

all: $(EXE)
	
dttc: dttc.c
	$(CC) $(CFLAGS) dttc.c -o dttc 

clean: $(SRC)
	rm -f $(EXE) 
