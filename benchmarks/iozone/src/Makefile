#
# RISC-V musl build with no threads, no largefiles, no async I/O
#
all: iozone-rw iozone-rw-1m iozone-rw-adm iozone-rw-adm-1m

iozone-rw: iozone-rw.o libbif.o
	$(CC) -O $(LDFLAGS) -static iozone-rw.o libbif.o -o iozone-rw

iozone-rw-1m: iozone-rw-1m.o libbif.o
	$(CC) -O $(LDFLAGS) -static iozone-rw-1m.o libbif.o -o iozone-rw-1m

iozone-rw-adm: iozone-rw-adm.o libbif.o
	$(CC) -O $(LDFLAGS) -static iozone-rw-adm.o libbif.o -lkeystone-edge -lkeystone-app-syscall -o iozone-rw-adm

iozone-rw-adm-1m: iozone-rw-adm-1m.o libbif.o
	$(CC) -O $(LDFLAGS) -static iozone-rw-adm-1m.o libbif.o -lkeystone-edge -lkeystone-app-syscall -o iozone-rw-adm-1m

iozone-rw.o: iozone.c
	$(CC) -c -Wno-implicit-function-declaration -DARGS_VARIANT=3 -Dgeneric -Dunix -DHAVE_ANSIC_C -DNO_THREADS -DNO_SOCKET -DNO_FORK -DNO_MADVISE -DNO_SIGNAL \
		-DNAME='"RISC-V"' $(CFLAGS) iozone.c -o iozone-rw.o

iozone-rw-1m.o: iozone.c
	$(CC) -c -Wno-implicit-function-declaration -DARGS_VARIANT=2 -Dgeneric -Dunix -DHAVE_ANSIC_C -DNO_THREADS -DNO_SOCKET -DNO_FORK -DNO_MADVISE -DNO_SIGNAL \
		-DNAME='"RISC-V 1M"' $(CFLAGS) iozone.c -o iozone-rw-1m.o

iozone-rw-adm.o: iozone.c
	$(CC) -c -Wno-implicit-function-declaration -DARGS_VARIANT=3 -Dgeneric -Dunix -DHAVE_ANSIC_C -DNO_THREADS -DNO_SOCKET -DNO_FORK -DNO_MADVISE -DNO_SIGNAL -DADM_MALLOC \
		-DNAME='"Keystone ADM"' $(CFLAGS) iozone.c -o iozone-rw-adm.o

iozone-rw-adm-1m.o: iozone.c
	$(CC) -c -Wno-implicit-function-declaration -DARGS_VARIANT=2 -Dgeneric -Dunix -DHAVE_ANSIC_C -DNO_THREADS -DNO_SOCKET -DNO_FORK -DNO_MADVISE -DNO_SIGNAL -DADM_MALLOC \
		-DNAME='"Keystone ADM 1M"' $(CFLAGS) iozone.c -o iozone-rw-adm-1m.o

libbif.o: libbif.c
	$(CC) -c -Dgeneric -Dunix -DHAVE_ANSIC_C -DNO_THREADS -DNO_SOCKET -DNO_FORK -DNO_MADVISE -DNO_SIGNAL \
		$(CFLAGS) libbif.c -o libbif.o

