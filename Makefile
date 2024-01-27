# Makefile

include config.mk

all: spl

SPL_FLAGS=

${SPL_SRC}:
	./${PROG} ${SPL_FLAGS} $@.spl

examples: SPL_FLAGS+=verbose-asm
examples: ${SPL_EXAMPLES}

run_examples: SPL_FLAGS+=run
run_examples: ${SPL_EXAMPLES}

test: SPL_FLAGS+=verbose-asm

main: SPL_FLAGS+=debug-output verbose verbose-asm
main: main.spl

spl: SPL_FLAGS+=verbose
spl: src/spl.spl

run:
	./${PROG} ${SPL_FLAGS} spl.spl

update_bootstrap:
	cp .cache/spl.fasm bootstrap/spl_linux_fasm_x86_64.fasm

update_bootstrap_nasm:
	cp .cache/spl.asm bootstrap/spl_linux_nasm_x86_64.asm

bootstrap:
	fasm -m 2024583 bootstrap/spl_linux_fasm_x86_64.fasm
	mv bootstrap/spl_linux_fasm_x86_64 spl
	chmod +x spl

bootstrap_nasm:
	nasm -f elf64 bootstrap/spl_linux_nasm_x86_64.asm -o spl.o && gcc spl.o -o spl -nostdlib -no-pie

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

clean:
	rm -f *.o ${SPL_SRC} ${addsuffix .o, ${SPL_SRC}} ${addsuffix .spl.asm, ${SPL_SRC}} ${addsuffix .spl.debug, ${SPL_SRC}} ${addsuffix .spl.html, ${SPL_SRC}}
	rm -drf .cache

.SUFFIXES:
.PHONY: ${SPL_SRC} bootstrap
