#!/bin/bash

FLAGS="-g3 -Wall -Wextra -pedantic -Wconversion"
FILES="t2j_linux.c t2j.c -o t2j"

case $1 in
    warn)
	gcc -03 $FLAGS $FILES;;
    dev)
	gcc $FLAGS $FILES;;
    tests)
        gcc $FLAGS $FILES
	for file in ./tests/*.txt; do
	    [ -f "$file" ] || break
	    echo "============="
	    echo "Running test: $file"
	    echo "  Bencode: $(cat $file)"
	    echo "  Result:  $(./t2j $file)"
	done
	;;
    *)
	gcc -03 $FILES;;
esac
