#include "mpsi_protocol.hpp"
#include <iostream>

std::vector<Ciphertext> initialization(
    const std::vector<size_t>& client_set, 
    long m_bits, 
    long k_hashes, 
    const ZZ& pk, 
    const PublicParameters& params) 
{
    BloomFilterParams bf_params(1, -1); // TODO: pass params
    bf_params.bin_count = m_bits;
    bf_params.seeds.clear();
    for(size_t i=0; i<k_hashes; ++i) 
        bf_params.seeds.push_back(i);
    
    BloomFilter bf(bf_params);
    for (size_t element : client_set) {
        bf.insert(element);
    }

    std::vector<Ciphertext> encrypted_bf;
    encrypted_bf.reserve(m_bits);

    for(size_t j=0; j<m_bits; ++j) {
        ZZ message;
        // if bit is 1: encrypt(1), else: encrypt(random > 1??)
        if (bf.contains_bit(j)) { 
             message = 1;
        } else {
             message = // TODO: choose random
        }
        encrypted_bf.push_back(encrypt(message, pk, params));
    }

    return encrypted_bf;
}

std::vector<ZZ> client_compute_decryption_share(
    const std::vector<Ciphertext>& combined_ebf, 
    const std::vector<Ciphertext>& my_ciphertexts,
    const ZZ& sk,
    const PublicParameters& params)
{
    std::vector<ZZ> shares;
    shares.reserve(combined_ebf.size());

    for(size_t j = 0; j < combined_ebf.size(); ++j) {
        // TODO: compute share
    }
    return shares;
}

std::vector<size_t> multiparty_psi(
    const std::vector<std::vector<size_t>>& client_sets,
    const std::vector<size_t>& server_set,
    long m_bits,
    long k_hashes,
    const Keys& keys) 
{
    size_t num_clients = client_sets.size();
    
    // Initialization - clients generate encrypted Bloom Filters
    std::vector<std::vector<Ciphertext>> client_ebfs;
    client_ebfs.reserve(num_clients);

    for(size_t i=0; i<num_clients; ++i) {
        client_ebfs.push_back(initialization(
            client_sets[i], 
            m_bits, 
            k_hashes, 
            keys.key_pairs[i].pk, 
            keys.params
        ));
    }

    // Computation Intersection
    // Steps 1 & 2 - Server computes combined encrypted BF
    std::vector<Ciphertext> combined_ebf;
    combined_ebf.reserve(m_bits);

    for(long j=0; j<m_bits; ++j) {
        Ciphertext combined_ct;
        combined_ct.c1 = 1;
        combined_ct.c2 = 1;

        for(size_t i=0; i<num_clients; ++i) { // TODO: only c2's need to be multiplied
            combined_ct.c1 = MulMod(combined_ct.c1, client_ebfs[i][j].c1, keys.params.p);
            combined_ct.c2 = MulMod(combined_ct.c2, client_ebfs[i][j].c2, keys.params.p);
        }
        combined_ebf.push_back(combined_ct);
    }

    // Step 3 - Each client computes their decryption share
    std::vector<std::vector<ZZ>> all_client_shares;
    all_client_shares.reserve(num_clients);

    for(size_t i=0; i<num_clients; ++i) {
        std::vector<ZZ> shares = client_compute_decryption_share(
            combined_ebf,  // sent by server
            client_ebfs[i],         
            keys.key_pairs[i].sk,   
            keys.params
        );
        all_client_shares.push_back(shares);
    }

    // Step 4 - Server computes combined BF
    BloomFilterParams bf_params(1, -1); // TODO: pass params
    bf_params.bin_count = m_bits;
    bf_params.seeds.clear();
    for(size_t i=0; i<k_hashes; ++i) 
        bf_params.seeds.push_back(i);
    BloomFilter final_bf(bf_params);

    for(long j=0; j<m_bits; ++j) {
        std::vector<ZZ> shares_for_bin_j;
        for(size_t i=0; i<num_clients; ++i) {
            shares_for_bin_j.push_back(all_client_shares[i][j]);
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