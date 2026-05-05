#include "bloom_filter.hpp"

#define XXH_INLINE_ALL
#include "xxhash.h"

extern cybozu::RandomGenerator rg;

size_t hash_element(const size_t& element, uint64_t seed) {
    uint64_t element_u64 = static_cast<uint64_t>(element);
    XXH64_hash_t hash = XXH3_64bits_withSeed(&element_u64, sizeof(element_u64), seed);
    return static_cast<size_t>(hash);
}

BloomFilterParams::BloomFilterParams(size_t element_count, int64_t e_pow, GT base_gt) {
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
    this->base_gt = base_gt; 
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
    this->bins.resize(params.bin_count);
    this->is_empty.resize(params.bin_count, true);
    this->base_gt = params.base_gt;
}

GT GarbledBloomFilter::generate_random_share() const {
    Fr r;
    r.setByCSPRNG();
    GT random_share;
    GT::pow(random_share, base_gt, r); // base_gt^r = random GT element
    return random_share;
}

bool GarbledBloomFilter::insert_set(const std::vector<long>& elements, const std::vector<GT>& targets) {
    size_t bin_count = bins.size();
    int elements_not_inserted = 0;
    
    for (size_t i = 0; i < elements.size(); i++) {
        long element = elements[i];
        GT finalShare = targets[i]; 
        long emptySlot = -1;
        std::unordered_set<size_t> visited_bins;

        for (uint64_t seed : seeds) {
            size_t j = hash_element(element, seed) % bin_count;
            if(visited_bins.count(j) > 0) 
                continue;
            visited_bins.insert(j);
            
            if (is_empty[j]) {
                if (emptySlot == -1) {
                    emptySlot = static_cast<long>(j); 
                } else {
                    GT new_share = generate_random_share();
                    bins[j] = new_share;
                    is_empty[j] = false;
                    
                    // finalShare = finalShare * (new_share)^-1
                    GT inv_share;
                    GT::inv(inv_share, new_share);
                    GT::mul(finalShare, finalShare, inv_share);
                }
            } else {
                // finalShare = finalShare * (bins[j])^-1
                GT inv_share;
                GT::inv(inv_share, bins[j]);
                GT::mul(finalShare, finalShare, inv_share);
            }
        }

        if (emptySlot == -1) 
            elements_not_inserted++;
        else {
            bins[emptySlot] = finalShare;
            is_empty[emptySlot] = false;
        }
    }

     for (size_t i = 0; i < bins.size(); ++i) {
        if (is_empty[i]) {
            bins[i] = generate_random_share();
            is_empty[i] = false;
        }
    }
    
    return elements_not_inserted == 0;
}

bool GarbledBloomFilter::contains(const size_t& element, const GT& expected_target) const {
    size_t bin_count = bins.size();
    
    GT recovered;
    recovered.clear(); // 0
    GT::add(recovered, recovered, 1); // 0 + 1 = 1

    std::unordered_set<size_t> visited_bins;
    
    for (uint64_t seed : seeds) {
        size_t j = hash_element(element, seed) % bin_count;
        if (visited_bins.count(j) > 0) {
            continue; 
        }
        visited_bins.insert(j);
        
        // recovered = recovered * bins[j]
        GT::mul(recovered, recovered, bins[j]);
    }
    return recovered == expected_target;
}

void GarbledBloomFilter::clear() {
    this->bins.clear();
    this->bins.resize(this->is_empty.size());
    std::fill(this->is_empty.begin(), this->is_empty.end(), true);
}