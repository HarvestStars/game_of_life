#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "glider.h"  // Make sure to have this file in the same directory or adjust the include path

#define WIDTH 3000
#define HEIGHT 3000
#define ITERATIONS 5000

// Macro for handling MPI errors
#define MPI_CHECK(call) \
    do { \
        int err = (call); \
        if (err != MPI_SUCCESS) { \
            fprintf(stderr, "MPI error at %s:%d\n", __FILE__, __LINE__); \
            MPI_Abort(MPI_COMM_WORLD, err); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)

// Function to count the live neighbors of a cell
int count_live_neighbors(int x, int y, int** grid) {
    int count = 0;
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx != 0 || dy != 0) {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < WIDTH && ny >= 0 && ny < HEIGHT) {
                    count += grid[nx][ny];
                }
            }
        }
    }
    return count;
}

// Main function
int main(int argc, char** argv) {
    // Initialize MPI
    int rank, size;
    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &size));

    // Start the timer
    double start_time = MPI_Wtime();

    int rows_per_process = HEIGHT / size + 2; // Add 2 for the border ghost rows
    int** grid = malloc(rows_per_process * sizeof(int*));
    int** new_grid = malloc(rows_per_process * sizeof(int*));
    for (int i = 0; i < rows_per_process; i++) {
        grid[i] = malloc(WIDTH * sizeof(int));
        new_grid[i] = malloc(WIDTH * sizeof(int));
        memset(grid[i], 0, WIDTH * sizeof(int));  // Initialize all cells as dead
        memset(new_grid[i], 0, WIDTH * sizeof(int));
    }

    // Initialize the glider pattern at the center of the grid
    if (rank == HEIGHT / 2 / (rows_per_process - 2)) {
        int start_row = (HEIGHT / 2) % (rows_per_process - 2);
        for (int i = 0; i < GLIDER_HEIGHT; i++) {
            for (int j = 0; j < GLIDER_WIDTH; j++) {
                grid[start_row + i][WIDTH / 2 + j - GLIDER_WIDTH / 2] = glider[i][j];
            }
        }
    }

    // Main game loop
    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Add MPI communication to sync the ghost rows between processes
        // For root rank, just send the top ghost row to rank 1
        // For middle ranks, send the top ghost row to rank - 1 and bottom ghost row to rank + 1
        // For the last rank, just send the bottom ghost row to rank - 1
        if (rank > 0) {
            MPI_CHECK(MPI_Sendrecv(grid[1], WIDTH, MPI_INT, rank - 1, 0, 
                        grid[0], WIDTH, MPI_INT, rank - 1, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        }

        if (rank < size - 1) {
            MPI_CHECK(MPI_Sendrecv(grid[rows_per_process - 2], WIDTH, MPI_INT, rank + 1, 0, 
                        grid[rows_per_process - 1], WIDTH, MPI_INT, rank + 1, 0,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE));
        }

        // core part of the game of life
        for (int i = 1; i < rows_per_process - 1; i++) {
            for (int j = 0; j < WIDTH; j++) {
                int live_neighbors = count_live_neighbors(i, j, grid);
                int cell = grid[i][j];
                if (cell == 1 && (live_neighbors < 2 || live_neighbors > 3)) {
                    new_grid[i][j] = 0;
                } else if (cell == 0 && live_neighbors == 3) {
                    new_grid[i][j] = 1;
                } else {
                    new_grid[i][j] = cell;
                }
            }
        }

        // swap the grids by pointers, make efficient use of memory
        int** temp = grid;
        grid = new_grid;
        new_grid = temp;

        // wait for all processes to finish updating their grids before proceeding to the next iteration
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
    }

    // Count local live cells
    int local_alive_count = 0;
    for (int i = 1; i < rows_per_process - 1; i++) {  // avoid ghost rows
        for (int j = 0; j < WIDTH; j++) {
            if (grid[i][j] == 1) {
                local_alive_count++;
            }
        }
    }

    // Reduce all local counts to get the global count
    int global_alive_count = 0;
    MPI_CHECK(MPI_Reduce(&local_alive_count, &global_alive_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD));

    // Stop the timer
    double end_time = MPI_Wtime();

    if (rank == 0) {
        printf("MPI process size: %d, Total alive cells after %d iterations: %d\n", size, ITERATIONS, global_alive_count);
        printf("MPI process size: %d, Total time for %d iterations: %f seconds\n", size, ITERATIONS, end_time - start_time);
    }

    // Clean up
    for (int i = 0; i < rows_per_process; i++) {
        free(grid[i]);
        free(new_grid[i]);
    }
    free(grid);
    free(new_grid);

    MPI_CHECK(MPI_Finalize());
    return 0;
}
