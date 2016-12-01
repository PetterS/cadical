#!/bin/sh
../build/cadical --block $1
a=$?
../build/cadical --no-block $1
b=$?
test $a = $b
exit $?
