#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <string>
#include <algorithm>
#include <random>
#include "mpsi_protocol.hpp"
#include "benchmarking.hpp"
#include "experiments.hpp"

int main() {
    srand(time(NULL));

    run_experiment({ {1, 2, 3}, {1, 3, 4} }, // Clients
        {1, 3, 5} // Server
    );

    run_experiment({ {10, 11}, {12, 13} }, // Clients
        {14, 15} // Server
    );

    run_experiment({ {7, 8, 9}, {7, 8, 9} }, // Clients
        {7, 8, 9} // Server
    );

    run_experiment({ 
          {1, 2, 3, 4, 5}, // Client 1
          {5, 6, 7, 8, 9}, // Client 2
          {2, 5, 8, 10, 12} // Client 3
        },
        {5, 12, 100, 200} // Server
    );

    // 2, 5 and 10 parties, set sizes 2^4, 2^6 and 2^8
    benchmark(10, {2, 5, 10}, 16, 32);
    benchmark(10, {2, 5, 10}, 64, 128);
    benchmark(10, {2, 5, 10}, 256, 512);

    return 0;
}