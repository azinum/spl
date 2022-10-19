# config.mk

PROG=spl

SPL_SRC=${subst .spl,, ${wildcard *.spl}}

SPL_EXAMPLES=${subst .spl,, ${shell find examples/ -type f -name '*.spl'}}

SPL_SRC+=${SPL_EXAMPLES}

INSTALL_DIR=/usr/local/bin

LIB_DIR=/usr/share
