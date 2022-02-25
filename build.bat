@echo off

set FLAGS=-O3
set CC=gcc
%CC% cspl.c -o cspl %FLAGS%
