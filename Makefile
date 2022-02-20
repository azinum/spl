# Makefile

include config.mk

all: cspl

SPL_FLAGS=

${SRC}:
	${CC} $@.c -o $@ ${FLAGS} && strip $@

clean:
	rm -f *.o ${PROG} ${SPL_SRC} ${addsuffix .o, ${SPL_SRC}} ${addsuffix .spl.asm, ${SPL_SRC}} ${addsuffix .spl.debug, ${SPL_SRC}} ${addsuffix .spl.ir, ${SPL_SRC}}

run: test

${SPL_SRC}:
	./${PROG} ${SPL_FLAGS} $@.spl

examples: ${SPL_EXAMPLES}

.PHONY: test ${SRC} examples/*
