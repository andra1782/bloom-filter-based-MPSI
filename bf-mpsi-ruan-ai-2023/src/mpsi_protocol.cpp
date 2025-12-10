#include "mpsi_protocol.hpp"
#include <iostream>

std::vector<Ciphertext> initialization(
    BloomFilterParams& bf_params,
    const std::vector<size_t>& client_set, 
    long m_bits, 
    long k_hashes, 
    const ZZ& pk, 
    const PublicParameters& params) 
{
    BloomFilter bf(bf_params);
    for (size_t element : client_set) {
        bf.insert(element);
    }

    std::vector<Ciphertext> encrypted_bf;
    encrypted_bf.reserve(m_bits);

    for(size_t j=0; j<m_bits; ++j) {
        ZZ message;
        // if bit is 1: encrypt(1), else: encrypt(random > 1)
        if (bf.contains_bit(j)) { 
             message = ZZ(1);
        } else {
             message = NTL::RandomBnd(params.p - 2) + 2; // random in [2, p-1]
        }
        encrypted_bf.push_back(encrypt(message, pk, params));
    }

    return encrypted_bf;
}

std::vector<size_t> multiparty_psi(
    const std::vector<std::vector<size_t>>& client_sets,
    const std::vector<size_t>& server_set,
    BloomFilterParams& bf_params,
    const Keys& keys) 
{
    size_t num_clients_t = client_sets.size();
    size_t m_bits = bf_params.bin_count;
    size_t k_hashes = bf_params.seeds.size();
    
    // Initialization - clients generate encrypted Bloom Filters
    std::vector<std::vector<Ciphertext>> client_ebfs;
    client_ebfs.reserve(num_clients_t);
    for(size_t i=0; i<num_clients_t; ++i) {
        client_ebfs.push_back(initialization(
            bf_params,
            client_sets[i], 
            m_bits, 
            k_hashes, 
            keys.key_pairs[i].pk, 
            keys.params
        ));
    }

    // Computation Intersection
    // Steps 1 & 2 - Server computes combined encrypted BF
    std::vector<ZZ> combined_ebf;
    combined_ebf.reserve(m_bits);

    for(long j=0; j<m_bits; ++j) {
        ZZ combined_ct_c2s = ZZ(1); 
        for(size_t i=0; i<num_clients_t; ++i) { 
            combined_ct_c2s = MulMod(combined_ct_c2s, client_ebfs[i][j].c2, keys.params.p);
        }
        combined_ebf.push_back(combined_ct_c2s);
    }

    // Step 3 - Each client computes their decryption share
    std::vector<std::vector<ZZ>> all_bin_shares;
    all_bin_shares.reserve(num_clients_t);

    for(size_t j = 0; j < combined_ebf.size(); ++j) {
        std::vector<ZZ> shares_for_bin_j;
        for(size_t i=0; i<num_clients_t; ++i) {
            ZZ share = decrypt_share(combined_ebf[j], client_ebfs[i][j].c1, keys.key_pairs[i].sk, keys.params, num_clients_t); 
            shares_for_bin_j.push_back(share);
        }
        all_bin_shares.push_back(shares_for_bin_j);
    }

    // Step 4 - Server computes combined BF
    BloomFilter final_bf(bf_params); 
    for(long j=0; j<m_bits; ++j) {
        std::vector<ZZ> shares_for_bin_j;
        for(size_t i=0; i<num_clients_t; ++i) {
            shares_for_bin_j.push_back(all_bin_shares[j][i]);
        }
        ZZ plaintext = combine_decryption_shares(shares_for_bin_j, keys.params);
        if (plaintext == 1) {
            final_bf.set_bit_manually(j, true); 
        }
    }

    // Step 5 - Server computes intersection
    std::vector<size_t> intersection;
    for(size_t elem : server_set) {
        if(final_bf.contains(elem)) {
            intersection.push_back(elem);
        }
    }

    return intersection;
}