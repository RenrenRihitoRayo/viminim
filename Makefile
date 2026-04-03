SOURCES = $(wildcard *.c **/*.[ch])

.PHONY: uninstall

all: ${SOURCES}
	gcc viminim.c -o vmm -lncurses -fsanitize=address -O0 -lm

release: ${SOURCES}
	gcc viminim.c -o vmm -lncurses -O3 -lm

install: ${SOURCES}
	gcc viminim.c -o vmm -lncurses -O3 -lm
	echo ". ~/.vmmrc.sh" >> ~/.bashrc
	echo "export PATH=\"\$$PATH:`pwd`\" # VIMINIM DO NOT MODIFY" > ~/.vmmrc.sh
	chmod +x ~/.vmmrc.sh
	~/.vmmrc.sh
