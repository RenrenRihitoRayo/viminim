SOURCES = $(wildcard *.c **/*.[ch])

.PHONY: uninstall

all: ${SOURCES}
	git clone https://github.com/renrenrihitorayo/mila --depth=1
	gcc viminim.c -o vmm -lncurses -fsanitize=address -O0 -lm
	rm -rf mila

release: ${SOURCES}
	git clone https://github.com/renrenrihitorayo/mila --depth=1
	gcc viminim.c -o vmm -lncurses -O3 -lm
	rm -rf mila

install: ${SOURCES}
	git clone https://github.com/renrenrihitorayo/mila --depth=1
	gcc viminim.c -o vmm -lncurses -O3 -lm
	rm -rf mila
	echo ". ~/.vmmrc.sh" >> ~/.bashrc
	echo "export PATH=\"\$$PATH:`pwd`\" # VIMINIM DO NOT MODIFY" > ~/.vmmrc.sh
	chmod +x ~/.vmmrc.sh
	~/.vmmrc.sh
