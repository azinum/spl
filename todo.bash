#/usr/bin/bash

num_todos=$(grep -sn --color=auto "TODO" src/*.spl spl.spl | wc -l)
echo "${num_todos} todo(s):"
cols="$(tput cols)"
printf "%0.s-" $(seq 1 $cols)
printf "\n"
grep -sn --color=auto "TODO" src/*.spl spl.spl
