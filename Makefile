SOURCES = $(wildcard *.c **/*.[ch])

all: ${SOURCES}
	gcc viminim.c -o vmm -lncurses -fsanitize=address -O0 -lm

release: ${SOURCES}
	gcc viminim.c -o vmm -lncurses -O3 -lm