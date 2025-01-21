#!/bin/bash
module load 2023
module load OpenMPI/4.1.5-GCC-12.3.0
mpicc --version
mpirun --version

mpicc -o game_of_life game_mpi.c -std=c99 -O2 -Wall
