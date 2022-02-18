# Makefile

include config.mk

all: compile_src

compile_src:
	${CC} ${SRC} -o ${PROG} ${FLAGS} && strip ${PROG}

clean:
	rm -f *.o ${PROG} ${SPL_SRC} ${addsuffix .spl.o, ${SPL_SRC}} ${addsuffix .spl.asm, ${SPL_SRC}} ${addsuffix .spl.debug, ${SPL_SRC}}

run: test

${SPL_SRC}:
	./${PROG} $@.spl

examples: ${SPL_EXAMPLES}

.PHONY: test spl cspl examples/*
