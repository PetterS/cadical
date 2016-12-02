#!/bin/sh
../build/cadical --block $1
a=$?
case $a in
  0|10|20) exit 0;;
esac
../build/cadical --no-block $1
b=$?
case $b in
  0|10|20) exit 0;;
esac
test $a = $b
exit $?
