#ifndef MPSI_PROTOCOL_HPP
#define MPSI_PROTOCOL_HPP

#include <vector>
#include <NTL/ZZ.h>
#include "el_gamal.hpp"
#include "bloom_filter.hpp"

std::vector<size_t> multiparty_psi(
    const std::vector<std::vector<size_t>>& client_sets,
    const std::vector<size_t>& server_set,
    BloomFilterParams& bf_params,
    const Keys& keys
);

#endif 