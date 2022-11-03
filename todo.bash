#/usr/bin/bash

TO_FIND="TODO"
FILES="src/*.spl spl.spl"

[ $# -gt 0 ] && TO_FIND="$1"
[ $# -gt 1 ] && FILES="$2"

num_todos=$(grep -sn --color=auto ${TO_FIND} ${FILES} | wc -l)
echo "found \`${TO_FIND}\` ${num_todos} times in \`${FILES}\`:"
printf "\n"
grep -sn --color=auto ${TO_FIND} ${FILES}
