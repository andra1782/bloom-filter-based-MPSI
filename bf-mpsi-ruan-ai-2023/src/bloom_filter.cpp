#include "bloom_filter.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

#define XXH_INLINE_ALL
#include "xxhash.h"

size_t hash_element(const size_t& element, uint64_t seed) {
    uint64_t element_u64 = static_cast<uint64_t>(element);
    XXH64_hash_t hash = XXH3_64bits_withSeed(&element_u64, sizeof(element_u64), seed);
    return static_cast<size_t>(hash);
}

BloomFilterParams::BloomFilterParams(size_t element_count, int64_t e_pow) {
    // k = - ln(epsilon) / ln(2), since epsilon = 2^e_pow, k = -e_pow
    size_t hash_count = static_cast<size_t>(-e_pow);

    // m = - (n * ln(epsilon)) / (ln(2)^2)
    // m = - (n * e_pow * ln(2)) / (ln(2) * ln(2))
    // m = - (n * e_pow) / ln(2)
    double ln2 = std::log(2.0);
    double m_float = -(static_cast<double>(element_count) * static_cast<double>(e_pow)) / ln2;
    
    this->bin_count = static_cast<size_t>(std::ceil(m_float));
    
    this->seeds.reserve(hash_count);
    for(size_t i = 0; i < hash_count; ++i) {
        this->seeds.push_back(static_cast<uint64_t>(i));
    }
}

BloomFilter::BloomFilter(const BloomFilterParams& params) {
    this->seeds = params.seeds;
    this->bins.resize(params.bin_count, false);
}

void BloomFilter::insert(size_t element) {
    size_t bin_count = bins.size();
    for (uint64_t seed : seeds) {
        bins[hash_element(element, seed) % bin_count] = true;
    }
}

void BloomFilter::clear() {
    std::fill(bins.begin(), bins.end(), false);
}

bool BloomFilter::contains(const size_t& element) const {
    size_t bin_count = bins.size();
    for (uint64_t seed : seeds) {
        if (!bins[hash_element(element, seed) % bin_count]) {
            return false;
        }
    }
    return true;
}