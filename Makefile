# Makefile

include config.mk

all: spl

SPL_FLAGS=

clean:
	rm -f *.o ${PROG} ${SPL_SRC} ${addsuffix .o, ${SPL_SRC}} ${addsuffix .spl.asm, ${SPL_SRC}} ${addsuffix .spl.debug, ${SPL_SRC}} ${addsuffix .spl.ir, ${SPL_SRC}}

${SPL_SRC}:
	./${PROG} ${SPL_FLAGS} $@.spl

examples: ${SPL_EXAMPLES}

test: SPL_FLAGS+=run
test: ${SPL_TEST}

run:
	./${PROG} spl.spl

update_bootstrap:
	cp spl.spl.asm bootstrap/spl_linux_nasm_x86_64.asm

bootstrap:
	nasm -f elf64 bootstrap/spl_linux_nasm_x86_64.asm -o spl.o && ld -arch x86_64 spl.o -o spl

performance_test:
	perf record -e cycles -c 2000000 ./spl spl.spl
	perf report -n -f
	rm -f perf.data perf.data.old

install:
	chmod o+x ${PROG}
	cp ${PROG} ${INSTALL_DIR}/${PROG}

.PHONY: spl main test examples/* bootstrap
