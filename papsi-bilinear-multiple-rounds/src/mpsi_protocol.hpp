#ifndef MPSI_PROTOCOL_HPP
#define MPSI_PROTOCOL_HPP

#include <vector>
#include <NTL/ZZ.h>
#include "bloom_filter.hpp"

std::vector<long> multiparty_psi(
    const std::vector<std::vector<long>>& client_sets,
    const std::vector<long>& server_set,
    BloomFilterParams& bf_params,
    const mcl::bn::G2& judge_pk,  
    const mcl::bn::Fr& judge_sk, 
    int p_fraction, 
    double* client_computation_time,
    double* leader_client_computation_time,
    double* server_authorize_time,
    double* server_intersect_time,
    double* judge_computation_time,
    size_t* server_sent_bytes, 
    size_t* server_received_bytes,
    size_t* client_sent_bytes,
    size_t* client_received_bytes,
    size_t* leader_client_sent_bytes,
    size_t* leader_client_received_bytes,
    size_t* judge_sent_bytes,
    size_t* judge_received_bytes
);

#endif 