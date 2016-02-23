PREFIX=${PARSECDIR}/pkgs/apps/blackscholes/inst/${PARSECPLAT}

LIBS += -lpthread
LIBS += /homes/h1tseng/libpfm-4.1.0/lib/libpfm.a 
LDFLAGS += -L/homes/h1tseng/libpfm-4.1.0/lib -lpfm
#CXXFLAGS += $(MT) $(SZ) $(NCO) $(FUNC) $(ERR) $(CSRC) $(LIBS)
CXXFLAGS += $(MT) $(SZ) $(NCO) $(FUNC) $(ERR) $(CSRC) $(LIBS)
#CXXFLAGS += -DDTT_POLLING -DPFM -I/h/g1/h1tseng/libpfm-3.10/include
#LDFLAGS += -L/h/g1/h1tseng/libpfm-3.10/lib -lpfm
#CXXFLAGS += -DDTT_THRESHOLD=100000 -DRESET
#CXXFLAGS += -DDTT_POLLING
#CXXFLAGS += -DNODTT -DDTT_POLLING
CXXFLAGS += -DSINGLE_THREAD -g
#CXXFLAGS += -DSMT
#CXXFLAGS += -DSMP
#CXXFLAGS += -DDTT_POLLING -DPFM -DPFM4 -I/homes/h1tseng/libpfm-4.1.0/include -g 
#CFLAGS += -DDTT_POLLING -DPFM  -I/h/g1/h1tseng/libpfm-3.10/include
#CFLAGS += -DDTT_POLLING -DPFM -DPFM4 -I/homes/h1tseng/libpfm-4.1.0/include /homes/h1tseng/libpfm-4.1.0/lib/libpfm.a -g
#CFLAGS += -DDTT_POLLING
CFLAGS += -DSINGLE_THREAD -g
#CFLAGS += -DSMT
#CFLAGS += -DSMP
#CFLAGS += -DDTT_THRESHOLD=100000 -DRESET
#CFLAGS += -DNODTT -DDTT_POLLING 
CFLAGS += -g -lpthread -D__STDC__  -D_GNU_SOURCE 
CXXFLAGS += -g -pthread -D__STDC__  -D_GNU_SOURCE

ifdef source
	ifeq "$(source)" "simd" 
		SRC = blackscholes.simd.c 
		CXXFLAGS += -msse3
	endif
else
	SRC	= blackscholes.c 
	ifdef version
		ifeq "$(version)" "tbb"
			SRC	= blackscholes.tbb.cpp
		endif
	endif
endif

CSRC    = $(SRC)  
TARGET  = blackscholes 
M4_BASE = .
MACROS  = c.m4.pthreads

ifdef version
  ifeq "$(version)" "pthreads"
    M4_SRC    = blackscholes.m4.cpp
    CSRC      = $(M4_SRC)
    MT        = -DENABLE_THREADS
    CXXFLAGS += -pthread
  endif
  ifeq "$(version)" "openmp"
    MT        = -DENABLE_OPENMP
  endif
  ifeq "$(version)" "tbb"
    MT        = -DTBB_VERSION
  endif
else
  MT        = 
endif

# Default build single precision version
NCO     = -DNCO=4

ifdef chk_err
ERR     = -DERR_CHK
endif

ifdef single
NCO = -DNCO=4
endif

ifdef size
SZ = -DN=$(size)
else
SZ = -DN=960
endif

ifdef double
NCO = -DNCO=2
endif


all: $(TARGET) 

$(TARGET): clean $(CSRC) 
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIBS) -o $(TARGET)


$(M4_SRC): $(SRC)  
	$(M4) $(M4_BASE)/$(MACROS) $< > $@

#dtt.o: dtt.c 
#	$(CC) -c $(CFLAGS) $(LDFLAGS) $(LIBS) dtt.c -o dtt.o 

	
clean:
	rm -f $(TARGET) $(M4_SRC)

install:
	mkdir -p $(PREFIX)/bin
	cp -f $(TARGET) $(PREFIX)/bin/$(TARGET)

