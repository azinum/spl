# Makefile

include config.mk

all: compile

compile:
	${CC} ${SRC} -o ${PROG} ${FLAGS}

clean:
	rm ${PROG}

run:
	./${PROG} test.spl
