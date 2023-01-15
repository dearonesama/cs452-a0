#/bin/bash

gcc -g -Wall -Wextra test.c ../util.c -o test.out
./test.out
