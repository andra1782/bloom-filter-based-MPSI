#ifndef BLOOM_FILTER_HPP
#define BLOOM_FILTER_HPP

#include <vector>
#include <cstdint>
#include <cstddef> 
#include <random>

size_t hash_element(const size_t& element, uint64_t seed);

struct BloomFilterParams {
    size_t bin_count;
    std::vector<uint64_t> seeds;
    BloomFilterParams(size_t element_count, int64_t e_pow);
};

struct BloomFilter {
    std::vector<uint64_t> bins;  // m bins
    std::vector<uint64_t> seeds; // k hash functions

    explicit BloomFilter(const BloomFilterParams& params);

    // void insert(size_t element);
    void insert_set(const std::vector<long>& elements);
    void clear();
    bool contains(const size_t& element) const;
    
    bool is_bin_empty(size_t index) const { return bins[index] == 0; }
    void set_bin_manually(size_t index, uint64_t val) { bins[index] = val; }

    void bitwise_and(const BloomFilter& other);
    size_t get_size_in_bytes() const;
    size_t get_set_bits_count() const;
};

#endif