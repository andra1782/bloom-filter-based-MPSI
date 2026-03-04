#include "bloom_filter.hpp"

#define XXH_INLINE_ALL
#include "xxhash.h"

size_t hash_element(const size_t& element, uint64_t seed) {
    uint64_t element_u64 = static_cast<uint64_t>(element);
    XXH64_hash_t hash = XXH3_64bits_withSeed(&element_u64, sizeof(element_u64), seed);
    return static_cast<size_t>(hash);
}

BloomFilterParams::BloomFilterParams(size_t element_count, int64_t e_pow, ZZ p) {
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
    this->p = p;
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

GarbledBloomFilter::GarbledBloomFilter(const BloomFilterParams& params) {
    this->seeds = params.seeds;
    this->bins.resize(params.bin_count, to_ZZ(0));
    this->p = params.p;
}

ZZ GarbledBloomFilter::generate_random_share() const {
    return RandomBnd(p - 1) + 1; // [1, p-1]
}

bool GarbledBloomFilter::insert_set(const std::vector<long>& elements) {
    size_t bin_count = bins.size();
    bool allInserted = true;
    for(size_t element : elements) {
        long emptySlot = -1;
        ZZ finalShare = to_ZZ(1); // all shares 1 mod p
        std::unordered_set<size_t> visited_bins;

        for (uint64_t seed : seeds) {
            size_t j = hash_element(element, seed) % bin_count;
            if(visited_bins.count(j) > 0) 
                continue;
            visited_bins.insert(j);
            
            if (bins[j] == to_ZZ(0)) {
                if (emptySlot == -1) {
                    emptySlot = static_cast<long>(j); 
                } else {
                    ZZ new_share = generate_random_share();
                    bins[j] = new_share;
                    finalShare = MulMod(finalShare, InvMod(new_share, p), p);
                }
            } else {
                finalShare = MulMod(finalShare, InvMod(bins[j], p), p);
            }
        }

        if (emptySlot == -1) {
            allInserted = false; // no empty slot found for this element, cannot insert
            std::cerr << "An element could not be inserted into the Garbled Bloom Filter." << std::endl;
        } else 
            bins[emptySlot] = finalShare;
    }

     for (size_t i = 0; i < bins.size(); ++i) {
        if (bins[i] == to_ZZ(0)) {
            bins[i] = generate_random_share();
        }
    }
    return allInserted;
}

bool GarbledBloomFilter::contains(const size_t& element) const {
    size_t bin_count = bins.size();
    ZZ recovered = to_ZZ(1); 
    std::unordered_set<size_t> visited_bins;
    
    for (uint64_t seed : seeds) {
        size_t j = hash_element(element, seed) % bin_count;
        if (visited_bins.count(j) > 0) 
            continue;
        visited_bins.insert(j);
        
        recovered = MulMod(recovered, bins[j], p);
    }
    return recovered == to_ZZ(1);
}

void GarbledBloomFilter::clear() {
    std::fill(bins.begin(), bins.end(), to_ZZ(0));
}