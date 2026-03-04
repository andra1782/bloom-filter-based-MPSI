#ifndef BLOOM_FILTER_HPP
#define BLOOM_FILTER_HPP

#include <vector>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstddef> 
#include <NTL/ZZ.h>

using namespace NTL;

size_t hash_element(const size_t& element, uint64_t seed);

struct BloomFilterParams {
    size_t bin_count;
    std::vector<uint64_t> seeds;
    ZZ p;
    BloomFilterParams(size_t element_count, int64_t e_pow, ZZ p);
};

struct BloomFilter {
    std::vector<bool> bins;  // m bins
    std::vector<uint64_t> seeds; // k hash functions

    explicit BloomFilter(const BloomFilterParams& params);

    void insert(size_t element);
    void clear();
    bool contains(const size_t& element) const;
    
    bool contains_bit(size_t index) const { return bins[index]; }
    void set_bit_manually(size_t index, bool val) { bins[index] = val; }

    void bitwise_and(const BloomFilter& other);
    size_t get_size_in_bytes() const;
    size_t get_set_bits_count() const;
};

struct GarbledBloomFilter {
    std::vector<ZZ> bins;  // m bins
    std::vector<uint64_t> seeds; // k hash functions
    ZZ p;

    explicit GarbledBloomFilter(const BloomFilterParams& params);
    
    bool insert_set(const std::vector<long>& elements); // returns false if the element cannot be inserted
    void clear();
    bool contains(const size_t& element) const;

    ZZ generate_random_share() const;
};

#endif