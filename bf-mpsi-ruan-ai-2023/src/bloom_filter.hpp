#ifndef BLOOM_FILTER_HPP
#define BLOOM_FILTER_HPP

#include <vector>
#include <cstdint>
#include <cstddef> 

size_t hash_element(const size_t& element, uint64_t seed);

struct BloomFilterParams {
    size_t bin_count;
    std::vector<uint64_t> seeds;
    BloomFilterParams(size_t element_count, int64_t e_pow);
};

struct BloomFilter {
    std::vector<bool> bins; 
    std::vector<uint64_t> seeds;

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

#endif