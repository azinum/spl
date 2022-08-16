# config.mk

CC=clang

SRC=${subst .c,, ${wildcard *.c}}

PROG=cspl

FLAGS=-Wall -Wextra -pedantic -ggdb -O3

SPL_SRC=${subst .spl,, ${wildcard *.spl}}

SPL_EXAMPLES=${subst .spl,, ${wildcard examples/*.spl}}

SPL_TEST=${subst .spl,, ${wildcard test/*.spl}}

SPL_SRC+=${SPL_EXAMPLES} ${SPL_TEST}

INSTALL_DIR=/usr/local/bin
