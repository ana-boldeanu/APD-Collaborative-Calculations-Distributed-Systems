#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MASTER 0

void print_result(int *array, int size) {
    printf("Rezultat:");
    for (int i = 0; i < size; i++) {
        printf(" %d", array[i]);
    }
    printf("\n");
}

void print_message(int src, int dest) {
    printf("M(%d,%d)\n", src, dest);
}

void print_topology_for_coord(int coord, int *parents, int size) {
    bool more = false;
    printf("%d:", coord);
    for (int i = 0; i < size; i++) {
        if (parents[i] == coord) {
            if (more) {
                printf(",%d", i);
            } else {
                printf("%d", i);
                more = true;
            }
        }
    }
}

void print_topology(int rank, int *parents, int size) {
    printf("%d -> ", rank);
    print_topology_for_coord(0, parents, size);
    printf(" ");
    print_topology_for_coord(1, parents, size);
    printf(" ");
    print_topology_for_coord(2, parents, size);
    printf(" ");
    print_topology_for_coord(3, parents, size);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int num_tasks, rank, ret;           // Process data
    MPI_Status status;
    int array_size = atoi(argv[1]);     // The size of the array
    int error_type = atoi(argv[2]);

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_tasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    FILE *input_file;                   // Initial cluster data
    int num_workers, worker_rank;
    int *parents = (int *) malloc(num_tasks * sizeof(int));     // The topology of the system
    int *array = (int *) malloc(array_size * sizeof(int));      // The array to operate on
    int *results = (int *) malloc(array_size * sizeof(int));    // The array containing the results
    int chunk_size, chunk_id;
    int start, end;

    /* Run coordinators in a ring, considering that the [0-1] link is broken.
    To determine the topology, each coordinator will:
    1. Identify its workers.
    2. Send a partial topology to the next coordinator, in the order 0->3->2->1
    (this way, coordinator 1 will know the complete topology first).
    3. Send the complete topology to its workers and the next coordinator, in
    the order 1->2->3->0.
    4. At the end, the workers will know the complete topology and they can
    extract their cluster rank from there as well.
    */
    switch (rank) {
        case 0: 
            /* ================================================================
            ========================   Coordinator 0   ========================
            ===================================================================
            ===============   STEP 1 - Determine the Topology   ===============
            ===================================================================
            */
            input_file = fopen("cluster0.txt", "r");
            fscanf(input_file, "%d\n", &num_workers);

            // Initialize the topology with unkown values for all workers
            for (int i = 0; i < num_tasks; i++) {
                parents[i] = -1;
            }

            // Identify this coordinator's workers and update the topology
            for (int i = 0; i < num_workers; i++) {
                fscanf(input_file, "%d\n", &worker_rank);
                parents[worker_rank] = rank;
            }
            fclose(input_file);

            // Send the partial topology to the next coordinator
            ret = MPI_Send(parents, num_tasks, MPI_INT, 3, 0, MPI_COMM_WORLD);
            print_message(rank, 3);

            // Receive the complete topology from that coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, 3, 0, MPI_COMM_WORLD, &status);
            print_topology(rank, parents, num_tasks);

            // Send the complete topology to all workers
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Send(parents, num_tasks, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);
                }
            }

            /* ================================================================
            ==================   STEP 2 - Compute Results   ===================
            ===================================================================
            */
            // Build the array
            int val = array_size - 1;
            for (int i = 0;  i < array_size; i++) {
                array[i] = val;
                val --;
            }

            // Find the number of available workers in the topology
            int total_workers = 0;
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] != -1) {
                    total_workers++;
                }
            }
            // printf("There are %d workers\n", total_workers);

            // Compute the approx. number of elements that each worker has to compute
            chunk_size = array_size / total_workers;

            // Send the array and chunk_size to the next coordinator
            ret = MPI_Send(array, array_size, MPI_INT, 3, 0, MPI_COMM_WORLD);
            print_message(rank, 3);

            ret = MPI_Send(&chunk_size, 1, MPI_INT, 3, 0, MPI_COMM_WORLD);
            print_message(rank, 3);

            // Send the array, start index and end index to all workers
            chunk_id = 0;
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    start = chunk_id * chunk_size;
                    end = (chunk_id + 1) * chunk_size;
                    
                    ret = MPI_Send(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    chunk_id++;
                }
            }
            
            // Send the chunk_id to the next coordinator
            ret = MPI_Send(&chunk_id, 1, MPI_INT, 3, 0, MPI_COMM_WORLD);
            print_message(rank, 3);

            // Receive the partial results from the previous coordinator
            ret = MPI_Recv(results, array_size, MPI_INT, 3, 0, MPI_COMM_WORLD, &status);

            // Receive the partial results from the workers and copy them into the results array
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Recv(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

                    for (int i = start; i < end; i++) {
                        results[i] = array[i];
                    }
                }
            }

            // Print the final result
            print_result(results, array_size);
            break;
        
        case 3:
            /* ================================================================
            ========================   Coordinator 3   ========================
            ===================================================================
            ===============   STEP 1 - Determine the Topology   ===============
            ===================================================================
            */
            input_file = fopen("cluster3.txt", "r");
            fscanf(input_file, "%d\n", &num_workers);

            // Receive the partial topology from the previous coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

            // Identify this coordinator's workers and update the topology
            for (int i = 0; i < num_workers; i++) {
                fscanf(input_file, "%d\n", &worker_rank);
                parents[worker_rank] = rank;
            }
            fclose(input_file);

            // Send the partial topology to the next coordinator
            ret = MPI_Send(parents, num_tasks, MPI_INT, 2, 0, MPI_COMM_WORLD);
            print_message(rank, 2);

            // Receive the complete topology from that coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);
            print_topology(rank, parents, num_tasks);
            
            // Send the complete topology to the previous coordinator
            ret = MPI_Send(parents, num_tasks, MPI_INT, 0, 0, MPI_COMM_WORLD);
            print_message(rank, 0);

            // Send the complete topology to all workers
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Send(parents, num_tasks, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);
                }
            }

            /* ================================================================
            ==================   STEP 2 - Compute Results   ===================
            ===================================================================
            */
            // Receive the array, chunk_size and chunk_id from the previous coordinator
            ret = MPI_Recv(array, array_size, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&chunk_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&chunk_id, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);

            // Send the array and chunk_size to the next coordinator
            ret = MPI_Send(array, array_size, MPI_INT, 2, 0, MPI_COMM_WORLD);
            print_message(rank, 2);

            ret = MPI_Send(&chunk_size, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);
            print_message(rank, 2);

            // Send the array, start index and end index to all workers
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    start = chunk_id * chunk_size;
                    end = (chunk_id + 1) * chunk_size;
                    
                    ret = MPI_Send(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    chunk_id++;
                }
            }
            
            // Send the chunk_id to the next coordinator
            ret = MPI_Send(&chunk_id, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);
            print_message(rank, 2);

            // Receive the partial results from the previous coordinator
            ret = MPI_Recv(results, array_size, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);

            // Receive the partial results from the workers and copy them into the results array
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Recv(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

                    for (int i = start; i < end; i++) {
                        results[i] = array[i];
                    }
                }
            }

            // Send the partial results to the next coordinator
            ret = MPI_Send(results, array_size, MPI_INT, 0, 0, MPI_COMM_WORLD);
            print_message(rank, 0);
            break;

        case 2:
            /* ================================================================
            ========================   Coordinator 2   ========================
            ===================================================================
            ===============   STEP 1 - Determine the Topology   ===============
            ===================================================================
            */
            input_file = fopen("cluster2.txt", "r");
            fscanf(input_file, "%d\n", &num_workers);

            // Receive the partial topology from the previous coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, 3, 0, MPI_COMM_WORLD, &status);

            // Identify this coordinator's workers and update the topology
            for (int i = 0; i < num_workers; i++) {
                fscanf(input_file, "%d\n", &worker_rank);
                parents[worker_rank] = rank;
            }
            fclose(input_file);

            // Send the partial topology to the next coordinator
            ret = MPI_Send(parents, num_tasks, MPI_INT, 1, 0, MPI_COMM_WORLD);
            print_message(rank, 1);

            // Receive the complete topology from that coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);
            print_topology(rank, parents, num_tasks);

            // Send the complete topology to the previous coordinator
            ret = MPI_Send(parents, num_tasks, MPI_INT, 3, 0, MPI_COMM_WORLD);
            print_message(rank, 3);

            // Send the complete topology to all workers
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Send(parents, num_tasks, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);
                }
            }

            /* ================================================================
            ==================   STEP 2 - Compute Results   ===================
            ===================================================================
            */
            // Receive the array, chunk_size and chunk_id from the previous coordinator
            ret = MPI_Recv(array, array_size, MPI_INT, 3, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&chunk_size, 1, MPI_INT, 3, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&chunk_id, 1, MPI_INT, 3, 0, MPI_COMM_WORLD, &status);

            // Send the array and chunk_size to the next coordinator
            ret = MPI_Send(array, array_size, MPI_INT, 1, 0, MPI_COMM_WORLD);
            print_message(rank, 1);

            ret = MPI_Send(&chunk_size, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
            print_message(rank, 1);

            // Send the array, start index and end index to all workers
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    start = chunk_id * chunk_size;
                    end = (chunk_id + 1) * chunk_size;

                    ret = MPI_Send(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    chunk_id++;
                }
            }
            
            // Send the chunk_id to the next coordinator
            ret = MPI_Send(&chunk_id, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
            print_message(rank, 1);

            // Receive the partial results from the previous coordinator
            ret = MPI_Recv(results, array_size, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);

            // Receive the partial results from the workers and copy them into the results array
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Recv(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

                    for (int i = start; i < end; i++) {
                        results[i] = array[i];
                    }
                }
            }

            // Send the partial results to the next coordinator
            ret = MPI_Send(results, array_size, MPI_INT, 3, 0, MPI_COMM_WORLD);
            print_message(rank, 3);
            break;

        case 1:
            /* ================================================================
            ========================   Coordinator 1   ========================
            ===================================================================
            ===============   STEP 1 - Determine the Topology   ===============
            ===================================================================
            */
            input_file = fopen("cluster1.txt", "r");
            fscanf(input_file, "%d\n", &num_workers);

            // Receive the partial topology from the previous coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);

            // Identify this coordinator's workers and update the topology
            for (int i = 0; i < num_workers; i++) {
                fscanf(input_file, "%d\n", &worker_rank);
                parents[worker_rank] = rank;
            }
            fclose(input_file);

            // Now coordinator 1 knows the complete topology
            print_topology(rank, parents, num_tasks);

            // Send the complete topology to the previous coordinator
            ret = MPI_Send(parents, num_tasks, MPI_INT, 2, 0, MPI_COMM_WORLD);
            print_message(rank, 2);

            // Send the complete topology to all workers
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Send(parents, num_tasks, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);
                }
            }

            /* ================================================================
            ==================   STEP 2 - Compute Results   ===================
            ===================================================================
            */
            // Receive the array, chunk_size and chunk_id from the previous coordinator
            ret = MPI_Recv(array, array_size, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&chunk_size, 1, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&chunk_id, 1, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);

            // Send the array, start index and end index to all workers
            int worker_id = 0;  // For the last worker, give end of array instead of chunk

            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    start = chunk_id * chunk_size;
                    end = (chunk_id + 1) * chunk_size;

                    worker_id++;
                    if (worker_id == num_workers) {
                        end = array_size;
                    }

                    ret = MPI_Send(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    ret = MPI_Send(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    print_message(rank, i);

                    chunk_id++;
                }
            }

            // Receive the partial results from the workers and copy them into the results array
            for (int i = 0; i < num_tasks; i++) {
                if (parents[i] == rank) {
                    ret = MPI_Recv(array, array_size, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&start, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
                    ret = MPI_Recv(&end, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

                    for (int i = start; i < end; i++) {
                        results[i] = array[i];
                    }
                }
            }

            // Send the partial results to the next coordinator
            ret = MPI_Send(results, array_size, MPI_INT, 2, 0, MPI_COMM_WORLD);
            print_message(rank, 2);
            break;
        
        default:
            /* ================================================================
            =========================   Any Worker   ==========================
            ===================================================================
            ===============   STEP 1 - Determine the Topology   ===============
            ===================================================================
            */
            // Receive the complete topology from the parent coordinator
		    ret = MPI_Recv(parents, num_tasks, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
            
            print_topology(rank, parents, num_tasks);

            // Determine this worker's parent from the topology
            int parent = parents[rank];

            /* ================================================================
            ==================   STEP 2 - Compute Results   ===================
            ===================================================================
            */
            // Receive the array, start index and end index from the coordinator
            ret = MPI_Recv(array, array_size, MPI_INT, parent, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&start, 1, MPI_INT, parent, 0, MPI_COMM_WORLD, &status);
            ret = MPI_Recv(&end, 1, MPI_INT, parent, 0, MPI_COMM_WORLD, &status);

            // Compute the results for given chunk
            for (int i = start; i < end; i++) {
                array[i] *= 5;
            }

            // Send the computed array, start and end to the coordinator
            ret = MPI_Send(array, array_size, MPI_INT, parent, 0, MPI_COMM_WORLD);
            print_message(rank, parent);

            ret = MPI_Send(&start, 1, MPI_INT, parent, 0, MPI_COMM_WORLD);
            print_message(rank, parent);

            ret = MPI_Send(&end, 1, MPI_INT, parent, 0, MPI_COMM_WORLD);
            print_message(rank, parent);
    }

    MPI_Finalize();

    return 0;
}