#ifndef BENCHMARKING_HPP
#define BENCHMARKING_HPP

#include <vector>
#include "mpsi_protocol.hpp"

void benchmark(
    long repetitions, 
    std::vector<long> parties_list, 
    long set_size_clients, 
    long set_size_server,
    int false_positive_exponent
);

GT setup_pairings();
void setup_judge_keys(mcl::bn::G2& judge_pk, mcl::bn::Fr& judge_sk);

#endif