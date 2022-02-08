# Makefile

include config.mk

SPL_SRC=${subst .spl,, ${wildcard *.spl}}
ONLY_COMP=0

all: compile_src

compile_src:
	${CC} ${SRC} -o ${PROG} ${FLAGS} && strip ${PROG}

clean:
	rm -f ${PROG} test.spl.asm spl.spl.asm *.o test spl cspl debug.txt

run: test

${SPL_SRC}:
	./${PROG} $@.spl

.PHONY: test spl cspl
