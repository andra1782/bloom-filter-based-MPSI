#ifndef MPSI_PROTOCOL_HPP
#define MPSI_PROTOCOL_HPP

#include <vector>
#include <NTL/ZZ.h>
#include "el_gamal.hpp"
#include "bloom_filter.hpp"

std::vector<long> multiparty_psi(
    const std::vector<std::vector<long>>& client_sets,
    const std::vector<long>& server_set,
    BloomFilterParams& bf_params,
    const Keys& keys,
    double* client_prep_time,
    double* client_online_time,
    double* server_prep_time,
    double* server_online_time
);

#endif 