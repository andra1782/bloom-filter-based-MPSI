#include "mpsi_protocol.hpp"
#include <chrono>

size_t get_ciphertext_size(const Ciphertext& ct) {
    return NumBytes(ct.c1) + NumBytes(ct.c2);
}

std::vector<Ciphertext> compute_erbf(const std::vector<long>& set, 
                                    const BloomFilterParams& bf_params, 
                                    const Keys& keys) {
    BloomFilter bf(bf_params);
    for (size_t x : set) 
        bf.insert(x);

    std::vector<Ciphertext> erbf;
    size_t erbf_size_bytes = 0;
    for (size_t l = 0; l < bf_params.bin_count; l++) {
        ZZ m = bf.contains_bit(l) ? to_ZZ(1) : (RandomBnd(keys.params.p - 2) + 2);
        Ciphertext ciphertext = encrypt(m, keys.params);
        erbf.push_back(ciphertext);
        erbf_size_bytes += get_ciphertext_size(ciphertext);
    }
    return erbf;
}

std::vector<Ciphertext> aggegate_ciphertexts(const std::vector<long>& server_set, 
                                            const std::vector<std::vector<Ciphertext>>& erbfs, 
                                            int n_clients, 
                                            const BloomFilterParams& bf_params, 
                                            const Keys& keys) {
    std::vector<Ciphertext> combined_ciphertexts; // c[j] = aggregated ciphertext for server element j
    for (size_t x : server_set) {
        Ciphertext aggregated_c = {to_ZZ(1), to_ZZ(1)};
        for (int i = 0; i < n_clients; i++) {
            for (uint64_t seed : bf_params.seeds) {
                size_t v = hash_element(x, seed) % bf_params.bin_count; // bin
                aggregated_c.c1 = MulMod(aggregated_c.c1, erbfs[i][v].c1, keys.params.p);
                aggregated_c.c2 = MulMod(aggregated_c.c2, erbfs[i][v].c2, keys.params.p);
            }
        }
        combined_ciphertexts.push_back(aggregated_c);
    }
    return combined_ciphertexts;
}

std::vector<long> decrypt_intersection(const std::vector<Ciphertext>& combined_ciphertexts, 
                                    const std::vector<long>& server_set, 
                                    const Keys& keys) {
    std::vector<long> intersection;
    for (size_t j = 0; j < server_set.size(); j++) {
        ZZ decrypted = decrypt(combined_ciphertexts[j], keys.sk, keys.params.p); 
        if (decrypted == to_ZZ(1)) {
            intersection.push_back(server_set[j]);
        }
    }
    return intersection;
}

std::vector<long> multiparty_psi(
    const std::vector<std::vector<long>>& client_sets,
    const std::vector<long>& server_set,
    BloomFilterParams& bf_params,
    const Keys& keys,
    double* client_prep_time,
    double* client_online_time,
    double* server_computation_time,
    double* judge_computation_time,
    size_t* server_sent_bytes, 
    size_t* server_received_bytes,
    size_t* client_sent_bytes,
    size_t* client_received_bytes,
    size_t* judge_sent_bytes,
    size_t* judge_received_bytes
) {
    using namespace std::chrono;
    int n_clients = client_sets.size();
    int total_parties = n_clients + 1; 

    // Clients compute their ERBFs
    auto start = high_resolution_clock::now();
    std::vector<std::vector<Ciphertext>> all_erbfs;
    for (const auto& set : client_sets) {
        all_erbfs.push_back(compute_erbf(set, bf_params, keys));
    }
    auto stop = high_resolution_clock::now();
    *client_prep_time = duration<double, std::milli>(stop - start).count() / n_clients;

    // Server sends their elements to the Judge
    *server_sent_bytes += server_set.size() * sizeof(long);
    *judge_received_bytes += server_set.size() * sizeof(long);

    // Clients send their ERBFs to the Judge
    size_t total_erbf_size_bytes = 0;
    for (const auto& erbfs : all_erbfs) 
        for (const auto& ct : erbfs) 
            total_erbf_size_bytes += get_ciphertext_size(ct);
    *client_sent_bytes += total_erbf_size_bytes / n_clients; 
    *judge_received_bytes += total_erbf_size_bytes;

    // Judge selects the bins corresponding to the server's elements and aggregates the ciphertexts
    auto judge_aggregation_start = high_resolution_clock::now();
    std::vector<Ciphertext> combined_ciphertexts = aggegate_ciphertexts(server_set, all_erbfs, n_clients, bf_params, keys);
    auto judge_aggregation_stop = high_resolution_clock::now();
    *judge_computation_time += duration<double, std::milli>(judge_aggregation_stop - judge_aggregation_start).count();

    // Judge sends the aggregated ciphertexts to the server
    size_t combined_ciphertext_size_bytes = 0;
    for (const auto& ct : combined_ciphertexts)        
        combined_ciphertext_size_bytes += get_ciphertext_size(ct);
    *judge_sent_bytes += combined_ciphertext_size_bytes;
    *server_received_bytes += combined_ciphertext_size_bytes;

    // Server decrypts and computes the intersection
    auto server_decryption_start = high_resolution_clock::now();
    std::vector<long> intersection = decrypt_intersection(combined_ciphertexts, server_set, keys);
    auto server_decryption_stop = high_resolution_clock::now();
    *server_computation_time = duration<double, std::milli>(server_decryption_stop - server_decryption_start).count();

    return intersection;
}