SOURCES = $(wildcard *.c parsers/*.[ch] viminim/*.[ch])

all: ${SOURCES}
	gcc viminim.c -o vmm -lncurses