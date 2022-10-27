#/usr/bin/bash

TO_FIND="TODO"

[ $# -gt 0 ] && TO_FIND="$1"


num_todos=$(grep -sn --color=auto ${TO_FIND} src/*.spl spl.spl | wc -l)
echo "found \`${TO_FIND}\` ${num_todos} times:"
printf "\n"
grep -sn --color=auto ${TO_FIND} src/*.spl spl.spl
