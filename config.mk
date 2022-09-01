# config.mk

PROG=spl

SPL_SRC=${subst .spl,, ${wildcard *.spl}}

SPL_EXAMPLES=${subst .spl,, ${wildcard examples/*.spl}}

SPL_TEST=${subst .spl,, ${wildcard test/*.spl}}

SPL_SRC+=${SPL_EXAMPLES} ${SPL_TEST}

INSTALL_DIR=/usr/local/bin

LIB_DIR=/usr/share
