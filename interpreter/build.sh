#!/bin/sh

set -xe

spl dump debug-output test.spl
gcc src/interpreter.c -o interpreter -Iinclude -Wall -O0 -ggdb # -DVERBOSE -DSLOW_INTERPRET
