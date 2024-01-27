#!/bin/sh

set -xe

gcc src/interpreter.c -o interpreter -Iinclude -Wall -O2 -DVERBOSE
