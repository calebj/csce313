# makefile

.PHONY: all clean

all: memtest

clean:
	rm -f memtest memtest.o ackerman.o my_allocator.o

ackerman.o: ackerman.c ackerman.h my_allocator.h
	g++ $(CFLAGS) -c -g ackerman.c

my_allocator.o : my_allocator.c my_allocator.h
	g++ $(CFLAGS) -c -g my_allocator.c

memtest.o : memtest.c ackerman.h my_allocator.h
	g++ $(CFLAGS) -c -g memtest.c

memtest: memtest.o ackerman.o my_allocator.o
	g++ $(CFLAGS) -o memtest memtest.o ackerman.o my_allocator.o
