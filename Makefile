
VERSION=0.6

REFRESH=""

# normal CFLAGS
CFLAGS=-O2 -Wall -DREFRESH$(REFRESH)

# CFLAGS without optimization
CFLAGSNO=-O0 -Wall -DREFRESH$(REFRESH)

# targets
ALL: bin tmp bin/volrace bin/bufhrt bin/highrestest \
     bin/writeloop bin/catloop bin/playhrt bin/cptoshm bin/shmcat

bin:
	mkdir -p bin

tmp:
	mkdir -p tmp

src/version.h: Makefile
	echo "#define VERSION \""$(VERSION)"\""  > src/version.h

bin/volrace: src/version.h src/volrace.c
	$(CC) $(CFLAGS) -o bin/volrace src/volrace.c

tmp/net.o: src/net.h src/net.c
	$(CC) $(CFLAGS) -c -o tmp/net.o src/net.c

tmp/cprefresh_ass.o: src/cprefresh_default.s src/cprefresh_vfp.s src/cprefresh_arm.s
	if [ $(REFRESH) = "" ]; then \
	  $(CC) -c $(CFLAGSNO) -o tmp/cprefresh_ass.o src/cprefresh_default.s; \
	elif [ $(REFRESH) = "ARM" ]; then \
	  $(CC) -c $(CFLAGSNO) -marm -o tmp/cprefresh_ass.o src/cprefresh_arm.s; \
	elif [ $(REFRESH) = "VFP" ]; then \
	  $(CC) -c $(CFLAGSNO) -marm -mfpu=neon-vfpv4 -o tmp/cprefresh_ass.o src/cprefresh_vfp.s; \
	fi

tmp/cprefresh.o: src/cprefresh.h src/cprefresh.c 
	$(CC) -c $(CFLAGSNO) -o tmp/cprefresh.o src/cprefresh.c

bin/playhrt: src/version.h tmp/net.o src/playhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o
	$(CC) $(CFLAGSNO) -o bin/playhrt src/playhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt

bin/playhrt_ALSANC: src/version.h tmp/net.o src/playhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o
	$(CC) $(CFLAGSNO) -DALSANC -I$(ALSANC)/include -L$(ALSANC)/lib -o bin/playhrt_ALSANC src/playhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt 

bin/playhrt_static: src/version.h tmp/net.o src/playhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o
	$(CC) $(CFLAGSNO) -DALSANC -I$(ALSANC)/include -L$(ALSANC)/lib -o bin/playhrt_static src/playhrt.c tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o -lasound -lrt -lpthread -lm -ldl -static

bin/bufhrt: src/version.h tmp/net.o src/bufhrt.c tmp/cprefresh.o tmp/cprefresh_ass.o
	$(CC) $(CFLAGSNO) -D_FILE_OFFSET_BITS=64 -o bin/bufhrt tmp/net.o tmp/cprefresh.o tmp/cprefresh_ass.o src/bufhrt.c -lpthread -lrt

bin/highrestest: src/highrestest.c
	$(CC) $(CFLAGSNO) -o bin/highrestest src/highrestest.c -lrt

bin/writeloop: src/writeloop.c
	$(CC) $(CFLAGS) -D_FILE_OFFSET_BITS=64 -o bin/writeloop src/writeloop.c -lpthread -lrt

bin/catloop: src/catloop.c
	$(CC) $(CFLAGS) -o bin/catloop src/catloop.c -lpthread -lrt

bin/cptoshm: src/cptoshm.c tmp/cprefresh_ass.o tmp/cprefresh.o
	$(CC) $(CFLAGS) -o bin/cptoshm src/cptoshm.c tmp/cprefresh_ass.o tmp/cprefresh.o -lrt

bin/shmcat: src/shmcat.c tmp/cprefresh_ass.o tmp/cprefresh.o
	$(CC) $(CFLAGS) -o bin/shmcat tmp/cprefresh_ass.o tmp/cprefresh.o src/shmcat.c -lrt

clean: 
	rm -rf src/version.h bin tmp

veryclean: clean
	rm -f *~ */*~ */*/*~

# private, for bin in distribution
bin86: 
	make veryclean
	make
	cc -O2 -Wall -o bin/volrace src/volrace.c -static
	cc -O0 -Wall -D_FILE_OFFSET_BITS=64 -o bin/bufhrt tmp/net.o src/bufhrt.c -static -lpthread -lrt 
	cc -O0 -Wall -o bin/highrestest src/highrestest.c -static -lrt
	cc -O2 -Wall -o bin/writeloop src/writeloop.c -static -lpthread -lrt
	cc -O2 -Wall -o bin/catloop src/catloop.c -static -lpthread -lrt
	cc -O0 -Wall  -DALSANC -I$(ALSANC)/include -L$(ALSANC)/lib -o bin/playhrt src/playhrt.c tmp/net.o -lasound -lrt -lpthread -lm -ldl -static -lasound
	cd bin; \
	strip * ; \
	tar cvf frankl_stereo-$(VERSION)-bin-$(ARCH).tar * ; \
	gzip -9 frankl*.tar ; \
	mv frankl*gz ..
binPi: 
	make veryclean
	make
	cc -O2 -Wall -o bin/volrace src/volrace.c -static
	cc -O0 -Wall -D_FILE_OFFSET_BITS=64 -o bin/bufhrt tmp/net.o src/bufhrt.c -static -lpthread 
	cc -O0 -Wall -o bin/highrestest src/highrestest.c -static 
	cc -O2 -Wall -o bin/writeloop src/writeloop.c -static -lpthread -lrt
	cc -O2 -Wall -o bin/catloop src/catloop.c -static -lpthread -lrt
	cc -O0 -Wall  -DALSANC -I$(ALSANC)/include -L$(ALSANC)/lib -o bin/playhrt src/playhrt.c tmp/net.o -lasound -lpthread -lm -ldl -static -lasound
	cd bin; \
	strip * ; \
	tar cvf frankl_stereo-$(VERSION)-bin-$(ARCH).tar * ; \
	gzip -9 frankl*.tar ; \
	mv frankl*gz ..

	

