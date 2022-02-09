# Makefile

include config.mk

SPL_SRC=${subst .spl,, ${wildcard *.spl}}
SPL_SRC+=${subst .spl,, ${wildcard examples/*.spl}}
ONLY_COMP=0

all: compile_src

compile_src:
	${CC} ${SRC} -o ${PROG} ${FLAGS} && strip ${PROG}

clean:
	rm -f ${PROG} ${SPL_SRC} ${addsuffix .spl.o, ${SPL_SRC}} ${addsuffix .spl.asm, ${SPL_SRC}} *.o cspl debug.txt

run: test

${SPL_SRC}:
	./${PROG} $@.spl

.PHONY: test spl cspl
