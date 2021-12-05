# Makefile

include config.mk

all: compile

compile:
	${CC} ${SRC} -o ${PROG} ${FLAGS}

clean:
	rm ${PROG} test.spl.asm *.o test

run:
	./${PROG} test.spl
	nasm -f elf64 test.spl.asm
	ld test.spl.o -o test
