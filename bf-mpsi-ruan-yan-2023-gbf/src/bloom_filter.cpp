#include "bloom_filter.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cassert>

#define XXH_INLINE_ALL
#include "xxhash.h"

std::mt19937_64 rng(std::random_device{}()); 
std::uniform_int_distribution<uint64_t> dist(1, std::numeric_limits<uint64_t>::max());

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
        this->seeds.push_back(static_cast<uint64_t>(rand()) + 1);
    }
}

BloomFilter::BloomFilter(const BloomFilterParams& params) {
    this->seeds = params.seeds;
    this->bins.resize(params.bin_count, 0);
}

// void BloomFilter::insert(size_t element) {
//     size_t bin_count = bins.size();
//     size_t empty_slot = -1;
//     uint64_t final_share = static_cast<uint64_t>(element);
//     for (uint64_t seed : seeds) {
//         size_t index = hash_element(element, seed) % bin_count;
//         if (bins[index] == 0) {
//             if (empty_slot == static_cast<size_t>(-1)) {
//                 empty_slot = index;
//             } else {
//                 bins[index] = dist(rng);
//                 final_share ^= bins[index]; 
//             }
//         } else {
//             final_share ^= bins[index]; 
//         }
//     }
//     bins[empty_slot] = final_share;
// }

void BloomFilter::insert_set(const std::vector<long>& elements) {
    size_t bin_count = bins.size();

    // printf("Bloom filter before\n");
    // for (size_t i = 0; i < bins.size(); i++) 
    //     printf("Bin %zu: %lu\n", i, bins[i]);

    for(long element : elements) {
        long empty_slot = -1;
        uint64_t final_share = static_cast<uint64_t>(element);
        for (uint64_t seed : seeds) {
            size_t index = hash_element(element, seed) % bin_count;
            // printf("Seed used for hashing element %ld: %lu\n", element, seed);
            // printf("Inserting element %ld, checking bin index %zu, current bin value: %lu\n", element, index, bins[index] );
            if (bins[index] == 0 && empty_slot != index) {
                if (empty_slot == -1) {
                    empty_slot = static_cast<long>(index);
                } else {
                    bins[index] = dist(rng);   
                    // printf("Bin index %zu was empty, filling with random value %lu, final share now: %lu\n", index, bins[index], final_share ^ bins[index]);
                    final_share ^= bins[index]; 
                }
            } else {
                final_share ^= bins[index]; 
            }
        }
        // assert(empty_slot != -1);
        // printf("Inserting element %ld, empty slot at index %ld, final share to store: %lu\n", element, empty_slot, final_share);
        bins[empty_slot] = final_share;
    }

    // printf("Bloom filter bins after inserting set:\n");
    // for (size_t i = 0; i < bins.size(); i++) 
    //     printf("Bin %zu: %lu\n", i, bins[i]);

    for (size_t i = 0; i < bins.size(); i++) 
        if (bins[i] == 0) 
            bins[i] = dist(rng);

    uint64_t recovered = 0;
    for (size_t seed : seeds) {
        size_t index = hash_element(elements[0], seed) % bin_count;
        recovered ^= bins[index];
    }
    // printf("Recovered value after filling empty bins: %lu\n", recovered);

    // printf("Bloom filter bins after inserting set and remaining empty bins filled:\n");
    // for (size_t i = 0; i < bins.size(); i++) 
    //     printf("Bin %zu: %lu\n", i, bins[i]);
}

void BloomFilter::clear() {
    std::fill(bins.begin(), bins.end(), NULL);
}

bool BloomFilter::contains(const size_t& element) const {
    // printf("Checking if element %zu is in the Bloom filter\n", element);
    size_t bin_count = bins.size();
    uint64_t recovered = 0;
    for (uint64_t seed : seeds) {
        size_t index = hash_element(element, seed) % bin_count;
        recovered ^= bins[index];
    }
    // printf("Recovered value for element %zu: %lu\n", element, recovered);
    return recovered == element;
}