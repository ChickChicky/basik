#!/usr/bin/env bash

excode=0

mkdir ./tests/tmp

python3 tests/python.py # && echo -e '\n\n'
excode=$?

rm -rf ./tests/tmp/

exit $excode