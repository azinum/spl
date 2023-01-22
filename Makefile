# Makefile

include config.mk

all: spl

SPL_FLAGS=

clean:
	rm -f *.o ${SPL_SRC} ${addsuffix .o, ${SPL_SRC}} ${addsuffix .spl.asm, ${SPL_SRC}} ${addsuffix .spl.debug, ${SPL_SRC}} ${addsuffix .spl.html, ${SPL_SRC}}

${SPL_SRC}:
	./${PROG} ${SPL_FLAGS} $@.spl

examples: SPL_FLAGS+=verbose-asm
examples: ${SPL_EXAMPLES}

run_examples: SPL_FLAGS+=run
run_examples: ${SPL_EXAMPLES}

test: SPL_FLAGS+=verbose-asm run

main: SPL_FLAGS+=verbose-asm run
main: main.spl

run:
	./${PROG} ${SPL_FLAGS} spl.spl

update_bootstrap:
	cp .cache/spl.spl.asm bootstrap/spl_linux_nasm_x86_64.asm

bootstrap:
	nasm -f elf64 bootstrap/spl_linux_nasm_x86_64.asm -o spl.o && ld -arch x86_64 spl.o -o spl

performance_test:
	perf record -e cycles -c 2000000 ./spl spl.spl verbose-asm
	perf report -n -f
	rm -f perf.data perf.data.old

memory_leak_test:
	valgrind --leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--verbose \
		./spl spl.spl

stack_usage:
	valgrind --tool=drd --show-stack-usage=yes ./spl spl.spl

install:
	chmod o+x ${PROG}
	cp ${PROG} ${INSTALL_DIR}/${PROG}
	mkdir -p ${LIB_DIR}/${PROG}
	cp -rp lib ${LIB_DIR}/${PROG}

uninstall:
	rm ${INSTALL_DIR}/${PROG}
	rm -dr ${LIB_DIR}/${PROG}

.SUFFIXES:
.PHONY: ${SPL_SRC} bootstrap
