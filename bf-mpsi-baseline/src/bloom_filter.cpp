#include "bloom_filter.hpp"
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <vector>

#define XXH_INLINE_ALL
#include "xxhash.h"

size_t hash_element(const size_t& element, uint64_t seed) {
    // let element_bytes = (*element as u64).encode::<u64>().unwrap();
    uint64_t element_u64 = static_cast<uint64_t>(element);
    // hash64_with_seed(&element_bytes, seed) as usize
    XXH64_hash_t hash = XXH3_64bits_withSeed(&element_u64, sizeof(element_u64), seed);
    return static_cast<size_t>(hash);
}

BloomFilterParams::BloomFilterParams(size_t element_count, int64_t e_pow) {
    // let hash_count = (-e_pow) as usize;
    size_t hash_count = static_cast<size_t>(-e_pow); 

    // let funny_looking_thing = 2f64.powf(e_pow as f64 / hash_count as f64);
    // let bin_count = (-(hash_count as f64) * (element_count as f64 + 0.5) / (1. - funny_looking_thing).ln()).ceil() + 1.;
    double e_pow_f = static_cast<double>(e_pow);
    double hash_count_f = static_cast<double>(hash_count);
    double element_count_f = static_cast<double>(element_count);

    double funny_looking_thing = std::pow(2.0, e_pow_f / hash_count_f);
    double bin_count_f = std::ceil(-hash_count_f * (element_count_f + 0.5) / std::log(1.0 - funny_looking_thing)) + 1.0;
    this->bin_count = static_cast<size_t>(bin_count_f);

    // let seeds: Vec<u64> = (0..hash_count).map(|x| x as u64).collect();
    this->seeds.reserve(hash_count);
    for(size_t i = 0; i < hash_count; ++i) {
        this->seeds.push_back(static_cast<uint64_t>(i));
    }
}

BloomFilter::BloomFilter(const BloomFilterParams& params) {
    this->bins.resize(params.bin_count, false);
    this->seeds = params.seeds; 
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

bool BloomFilter::contains(const size_t& element) const{
    size_t bin_count = bins.size();
    for (uint64_t seed : seeds) {
        if (!bins[hash_element(element, seed) % bin_count]) {
            return false;
        }
    }
    return true;
}

void BloomFilter::bitwise_and(const BloomFilter& other) {
    if (bins.size() != other.bins.size()) {
        throw std::runtime_error("Cannot intersect Bloom Filters of different sizes");
    }

    for (size_t i = 0; i < bins.size(); ++i) {
        bins[i] = bins[i] && other.bins[i];
    }
}

size_t BloomFilter::get_size_in_bytes() const {
    return (bins.size() + 7) / 8;
}

size_t BloomFilter::get_set_bits_count() const {
    return std::count(bins.begin(), bins.end(), true);
}