#ifndef BENCHMARKING_HPP
#define BENCHMARKING_HPP

#include <vector>

void benchmark(
    long repetitions, 
    std::vector<long> parties_list, 
    long set_size_clients, 
    long set_size_server,
    int false_positive_exponent
);

#endif