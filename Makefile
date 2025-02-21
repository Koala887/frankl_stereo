
VERSION=0.9.dev

REFRESH=X8664
# TCP_NODELAY=YES

ARCH=$(shell uname -m)

# normal CFLAGS
CFLAGS=-O2 -Wall -z execstack -D_FILE_OFFSET_BITS=64 -fgnu89-inline -DREFRESH$(REFRESH)

# CFLAGS without optimization
CFLAGSNO=-O0 -Wall -z execstack -D_FILE_OFFSET_BITS=64 -fgnu89-inline -mwaitpkg -DREFRESH$(REFRESH)

# targets
#ALL: bin tmp bin/volrace bin/bufhrt bin/bufhrtmin bin/highrestest \
#     bin/writeloop bin/catloop bin/playhrt bin/playhrtmin bin/playhrtbuschel bin/cptoshm bin/shmcat \
#     bin/resample_soxr bin/cat64 bin/shownfinfo bin/music2nf
ALL: bin tmp bin/bufhrt bin/bufhrtmin bin/bufhrtmin-tsc bin/highrestest bin/highrestest-tsc bin/bufhrtmin-tpause\
     bin/writeloop bin/catloop bin/playhrt bin/playhrtmin bin/playhrtmin-tsc bin/playhrtmin-tpause\
	 bin/highrestest-tpause bin/highrestest-shift bin/bufhrtmin-shift

bin:
	mkdir -p bin

tmp:
	mkdir -p tmp

src/version.h: Makefile
	echo "#define VERSION \""$(VERSION)"\""  > src/version.h

bin/volrace: src/version.h src/volrace.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGS) -o bin/volrace src/volrace.c tmp/cprefresh.o tmp/cprefresh_ass.o 

tmp/net.o: src/net.h src/net.c |tmp 
	$(CC) $(CFLAGS) -c -o tmp/net.o src/net.c

tmp/cprefresh_ass.o: src/cprefresh_default.s src/cprefresh_vfp.s src/cprefresh_arm.s |tmp 
	if [ $(REFRESH) = "" ]; then \
	  $(CC) -c $(CFLAGSNO) -o tmp/cprefresh_ass.o src/cprefresh_default.s; \
	elif [ $(REFRESH) = "ARM" ]; then \
	  $(CC) -c $(CFLAGSNO) -marm -o tmp/cprefresh_ass.o src/cprefresh_arm.s; \
	elif [ $(REFRESH) = "VFP" ]; then \
	  $(CC) -c $(CFLAGSNO) -marm -mfpu=neon-vfpv4 -o tmp/cprefresh_ass.o src/cprefresh_vfp.s; \
	elif [ $(REFRESH) = "AA64" ]; then \
	  $(CC) -c $(CFLAGSNO) -march=native -o tmp/cprefresh_ass.o src/cprefresh_aa64.s; \
	elif [ $(REFRESH) = "X8664" ]; then \
	  $(CC) -c $(CFLAGSNO) -march=native -o tmp/cprefresh_ass.o src/cprefresh_x8664.s; \
	fi

tmp/cprefresh.o: src/cprefresh.h src/cprefresh.c |tmp 
	$(CC) -c $(CFLAGSNO) -o tmp/cprefresh.o src/cprefresh.c

bin/playhrt: src/version.h tmp/net.o src/playhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/playhrt src/playhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt

bin/playhrt_ALSANC: src/version.h tmp/net.o src/playhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -DALSANC -I$(ALSANC)/include -L$(ALSANC)/lib -o bin/playhrt_ALSANC src/playhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt 

bin/playhrt_static: src/version.h tmp/net.o src/playhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -DALSANC -I$(ALSANC)/include -L$(ALSANC)/lib -o bin/playhrt_static src/playhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt -lpthread -lm -ldl -static

bin/bufhrt: src/version.h tmp/net.o src/bufhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/bufhrt tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o src/bufhrt.c -lpthread -lrt

bin/highrestest: src/highrestest.c |bin
	$(CC) $(CFLAGSNO) -o bin/highrestest src/highrestest.c -lrt

bin/highrestest-tsc: src/highrestest-tsc.c |bin
	$(CC) $(CFLAGSNO) -o bin/highrestest-tsc src/highrestest-tsc.c -lrt

bin/highrestest-tpause: src/highrestest-tpause.c |bin
	$(CC) $(CFLAGSNO) -o bin/highrestest-tpause src/highrestest-tpause.c -lrt

bin/highrestest-shift: src/highrestest-shift.c |bin
	$(CC) $(CFLAGSNO) -o bin/highrestest-shift src/highrestest-shift.c -lrt

bin/writeloop: src/version.h src/nf_io.h src/writeloop.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGS) -o bin/writeloop tmp/cprefresh.o tmp/cprefresh_ass.o src/writeloop.c -lpthread -lrt

bin/catloop: src/version.h src/catloop.c |bin
	$(CC) $(CFLAGS) -o bin/catloop src/catloop.c -lpthread -lrt

bin/cptoshm: src/version.h src/cptoshm.c tmp/cprefresh_ass.o tmp/cprefresh.o |bin
	$(CC) $(CFLAGS) -o bin/cptoshm src/cptoshm.c tmp/cprefresh_ass.o tmp/cprefresh.o -lrt

bin/shmcat: src/version.h src/shmcat.c tmp/cprefresh_ass.o tmp/cprefresh.o |bin
	$(CC) $(CFLAGS) -o bin/shmcat tmp/cprefresh_ass.o tmp/cprefresh.o src/shmcat.c -lrt

bin/resample_soxr: src/version.h src/nf_io.h src/resample_soxr.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGS) -o bin/resample_soxr src/resample_soxr.c tmp/cprefresh.o tmp/cprefresh_ass.o -lsoxr -lsndfile -lrt

resampler: bin/resample_soxr

bin/cat64: src/version.h src/cat64.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGS) -o bin/cat64 src/cat64.c  tmp/cprefresh.o tmp/cprefresh_ass.o -lsndfile -lrt

bin/shownfinfo: src/version.h src/shownfinfo.c src/nf_io.h  |bin
	$(CC) $(CFLAGS) -o bin/shownfinfo src/shownfinfo.c 

bin/music2nf: src/version.h src/nf_io.h src/music2nf.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGS) -o bin/music2nf src/music2nf.c  tmp/cprefresh.o tmp/cprefresh_ass.o -lsndfile -lrt



# experimental, only compiled on demand
bin/clreg86: tmp src/clreg86.c src/cprefresh_x8664.s |bin 
	$(CC) $(CFLAGSNO) -c -o tmp/cprefresh_x8664.o src/cprefresh_x8664.s
	$(CC) $(CFLAGSNO) -o bin/clreg86 src/clreg86.c tmp/cprefresh_x8664.o

bin/clreg: tmp src/clreg.c src/cprefresh_aa64.s |bin 
	$(CC) $(CFLAGSNO) -c -o tmp/cprefresh_aa64.o src/cprefresh_aa64.s
	$(CC) $(CFLAGSNO) -o bin/clreg src/clreg.c tmp/cprefresh_aa64.o

# undocumented private version, stripped some code
bin/myplayhrt: src/version.h tmp/net.o src/myplayhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/myplayhrt src/myplayhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lpthread -lrt

bin/by4: src/by4.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/by4 src/by4.c tmp/cprefresh.o tmp/cprefresh_ass.o

bin/playhrtmin: src/version.h tmp/net.o src/playhrtmin.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/playhrtmin src/playhrtmin.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt

bin/playhrtmin-tsc: src/version.h tmp/net.o src/playhrtmin-tsc.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/playhrtmin-tsc src/playhrtmin-tsc.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt

bin/playhrtmin-tpause: src/version.h tmp/net.o src/playhrtmin-tpause.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/playhrtmin-tpause src/playhrtmin-tpause.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt

bin/playhrtbuschel: src/version.h tmp/net.o src/playhrtbuschel.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/playhrtbuschel src/playhrtbuschel.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt

bin/bufhrtmin: src/version.h tmp/net.o src/bufhrtmin.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/bufhrtmin src/bufhrtmin.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lpthread -lrt

bin/bufhrtmin-tsc: src/version.h tmp/net.o src/bufhrtmin-tsc.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/bufhrtmin-tsc src/bufhrtmin-tsc.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lpthread -lrt

bin/bufhrtmin-shift: src/version.h tmp/net.o src/bufhrtmin-shift.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/bufhrtmin-shift src/bufhrtmin-shift.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lpthread -lrt

bin/bufhrtmin-tpause: src/version.h tmp/net.o src/bufhrtmin-tpause.c tmp/cprefresh.o tmp/cprefresh_ass.o |bin
	$(CC) $(CFLAGSNO) -o bin/bufhrtmin-tpause src/bufhrtmin-tpause.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lpthread -lrt

clean: 
	rm -rf src/version.h bin bin86 tmp

veryclean: clean
	rm -f *~ */*~ */*/*~

# private, for bin in distribution
bin86: 
	make clean
	make veryclean
	make
	mkdir -p bin86 ; \
	cd bin; \
	strip * ; \
	cp * ../bin86  
#	tar cvf frankl_stereo-$(VERSION)-bin-$(ARCH).tar * ; \
#	gzip -9 frankl*.tar ; \
#	mv frankl*gz ..
binPi: 
	make veryclean
	make REFRESH=VFP
	cd bin; \
	strip * ; \
	tar cvf frankl_stereo-$(VERSION)-bin-$(ARCH).tar * ; \
	gzip -9 frankl*.tar ; \
	mv frankl*gz ..

	

