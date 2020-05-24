#!/usr/bin/bash
gcc main.c blake2bmod.c miniutil.c -o compute -pthread -lm -Wall -Werror -Wfatal-errors -O3 -flto -fomit-frame-pointer -march=native -mtune=native