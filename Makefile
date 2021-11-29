# Makefile

include config.mk

all: compile

compile:
	${CC} ${SRC} -o ${PROG} ${FLAGS}

run:
	./${PROG}
clean:
	rm ${PROG}
