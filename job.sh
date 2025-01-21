#!/bin/bash
#SBATCH --job-name=game_of_life    # job name
#SBATCH --output=game_out_%j.txt
#SBATCH --error=game_error_%j.txt
#SBATCH --ntasks=64                 # MPI process num
#SBATCH --nodes=1                  # nodes needed
#SBATCH --ntasks-per-node=128
#SBATCH --time=03:00:00            # max run time 5 mins
#SBATCH --partition=rome

# load mpi 
module load 2023
module load OpenMPI/4.1.5-GCC-12.3.0

# exec path
EXECUTABLE="./game_of_life"

# run mpi
mpirun -np $SLURM_NTASKS $EXECUTABLE

# done
echo "Job completed at $(date)"
