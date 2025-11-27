#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <unordered_set>
#include "bloom_filter.hpp"

std::vector<size_t> generate_set(size_t set_size, size_t universe_size, size_t seed) {
    std::mt19937_64 rng(seed); 
    std::unordered_set<size_t> unique;
    
    while(unique.size() < set_size) {
        unique.insert(rng() % universe_size);
    }

    return {unique.begin(), unique.end()};
}

int main() {
    const size_t NUM_PARTIES = 5;
    const size_t SET_SIZE_K = 1 << 16;
    const size_t UNIVERSE_SIZE = 1 << 28;
    const int64_t E_POW = -30; 

    BloomFilterParams params(SET_SIZE_K, E_POW);
    std::cout << "Bloom Filter-based MPSI Baseline for " 
        << NUM_PARTIES << " parties, " 
        << SET_SIZE_K << " items each, " 
        << UNIVERSE_SIZE << " universe size and " 
        << "2^" << E_POW << " error rate" << std::endl;
    std::cout << "Number of bins (m): " << params.bin_count << std::endl;
    std::cout << "Number of seeds: " << params.seeds.size() << std::endl;

    std::vector<std::vector<size_t>> party_sets;
    for(size_t i=0; i<NUM_PARTIES; ++i) {
        party_sets.push_back(generate_set(SET_SIZE_K, UNIVERSE_SIZE, i));
    }

    std::vector<BloomFilter> bloom_filters;
    bloom_filters.reserve(NUM_PARTIES);
    long long total_construction_time = 0;

    for(size_t i=0; i<NUM_PARTIES; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        BloomFilter bloom_filter(params);
        for(size_t item : party_sets[i]) {
            bloom_filter.insert(item);
        }
        bloom_filters.push_back(std::move(bloom_filter));

        auto end = std::chrono::high_resolution_clock::now();
        total_construction_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    std::cout << "Computation - Average Construction Time: " << (total_construction_time / NUM_PARTIES) << " microseconds" << std::endl;
    std::cout << "Communication - Filter Size: " << bloom_filters[0].get_size_in_bytes() << " bytes" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for(size_t i=1; i<NUM_PARTIES; ++i) {
        bloom_filters[0].bitwise_and(bloom_filters[i]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    long long intersection_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Computation - Intersection Time (Star Topology): " << intersection_time << " microseconds" << std::endl;
    
    return 0;
}