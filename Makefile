# Makefile

include config.mk

SPL_SRC=${subst .spl,, ${wildcard *.spl}}
ONLY_COMP=1

all: compile_src

compile_src:
	${CC} ${SRC} -o ${PROG} ${FLAGS} && strip ${PROG}

clean:
	rm -f ${PROG} test.spl.asm spl.spl.asm *.o test spl cspl

run: test

${SPL_SRC}:
	@if [ ${ONLY_COMP} -eq 1 ]; then \
		./${PROG} $@.spl; \
	else \
		./${PROG} $@.spl && \
		nasm -f elf64 $@.spl.asm && \
		ld $@.spl.o -o $@ && \
		strip $@ && \
		./$@; \
	fi

.PHONY: test spl
