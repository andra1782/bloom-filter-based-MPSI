#include "mpsi_protocol.hpp"
#include <chrono>

size_t get_ciphertext_size(const Ciphertext& ct) {
    return NumBytes(ct.c1) + NumBytes(ct.c2);
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
    size_t all_erbfs_size_bytes = 0;
    std::vector<std::vector<Ciphertext>> all_erbfs;
    for (const auto& set : client_sets) {
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
        all_erbfs.push_back(erbf);
        all_erbfs_size_bytes += erbf_size_bytes;
    }
    auto stop = high_resolution_clock::now();
    *client_prep_time = duration<double, std::milli>(stop - start).count() / n_clients;

    // Server sends their elements to the Judge
    *server_sent_bytes += server_set.size() * sizeof(long);
    *judge_received_bytes += server_set.size() * sizeof(long);

    // Judge creates the ERBF of the server
    auto judge_start = high_resolution_clock::now();
    BloomFilter bf(bf_params);
    for (size_t x : server_set) 
        bf.insert(x);

    std::vector<Ciphertext> erbf_server;
    size_t erbf_size_bytes = 0;
    for (size_t l = 0; l < bf_params.bin_count; l++) {
        ZZ m = bf.contains_bit(l) ? to_ZZ(1) : (RandomBnd(keys.params.p - 2) + 2);
        Ciphertext ciphertext = encrypt(m, keys.params);
        erbf_server.push_back(ciphertext);
        erbf_size_bytes += get_ciphertext_size(ciphertext);
    }

    // Judge creates shares of the ERBF
    std::vector<std::vector<Ciphertext>> server_erbf_shares(client_sets.size()); // sh[i][v] = share of server's ERBF at index v for party i
    for (size_t v = 0; v < bf_params.bin_count; v++) {
        Ciphertext final_share = erbf_server[v];
        for (int i = 0; i < n_clients - 1; i++) {
            ZZ new_share_c1 = RandomBnd(keys.params.p - 1) + 1; // [1, p-1]
            ZZ new_share_c2 = RandomBnd(keys.params.p - 1) + 1; // [1, p-1]
            server_erbf_shares[i].push_back(Ciphertext{new_share_c1, new_share_c2});
            final_share.c1 = MulMod(final_share.c1, InvMod(new_share_c1, keys.params.p), keys.params.p);
            final_share.c2 = MulMod(final_share.c2, InvMod(new_share_c2, keys.params.p), keys.params.p);
        }
        server_erbf_shares[n_clients - 1].push_back(final_share); 
    }
    auto judge_stop = high_resolution_clock::now();
    *judge_computation_time = duration<double, std::milli>(judge_stop - judge_start).count();

    // Judge sends shares to clients
    size_t share_size_bytes = 0;
    for (size_t v = 0; v < bf_params.bin_count; v++)
        for (int i = 0; i < n_clients; i++) 
            share_size_bytes += get_ciphertext_size(server_erbf_shares[i][v]);
    *judge_sent_bytes += share_size_bytes;
    *client_received_bytes += share_size_bytes / n_clients;

    // Each client computes a ciphertext for each ERBF bin
    auto start_client = high_resolution_clock::now();
    std::vector<std::vector<Ciphertext>> clients_ciphertexts(client_sets.size()); // c[i][v] = ciphertext computed by client i for index v
    for (int i = 0; i < n_clients; i++) {
        for(size_t v = 0; v < bf_params.bin_count; v++) {
            ZZ c_iv_c1 = MulMod(all_erbfs[i][v].c1, server_erbf_shares[i][v].c1, keys.params.p);
            ZZ c_iv_c2 = MulMod(all_erbfs[i][v].c2, server_erbf_shares[i][v].c2, keys.params.p);
            clients_ciphertexts[i].push_back(Ciphertext{c_iv_c1, c_iv_c2});
        }
    }
    auto stop_client = high_resolution_clock::now();
    *client_online_time += duration<double, std::milli>(stop_client - start_client).count() / n_clients;

    // Clients send their ciphertexts to the Judge
    size_t ciphertext_size_bytes = 0;
    for (size_t v = 0; v < bf_params.bin_count; v++)
        for (int i = 0; i < client_sets.size(); i++) 
            ciphertext_size_bytes += get_ciphertext_size(clients_ciphertexts[i][v]);
    *client_sent_bytes += ciphertext_size_bytes / n_clients;
    *judge_received_bytes += ciphertext_size_bytes;

    // Judge selects the bins corresponding to the server's elements and aggregates the ciphertexts
    auto judge_aggregation_start = high_resolution_clock::now();
    std::vector<Ciphertext> combined_ciphertexts; // c[j] = aggregated ciphertext for server element j
    for (size_t x : server_set) {
        Ciphertext aggregated_c = {to_ZZ(1), to_ZZ(1)};
        for (int i = 0; i < client_sets.size(); i++) {
            for (uint64_t seed : bf_params.seeds) {
                size_t v = hash_element(x, seed) % bf_params.bin_count; // bin
                aggregated_c.c1 = MulMod(aggregated_c.c1, clients_ciphertexts[i][v].c1, keys.params.p);
                aggregated_c.c2 = MulMod(aggregated_c.c2, clients_ciphertexts[i][v].c2, keys.params.p);
            }
        }
        combined_ciphertexts.push_back(aggregated_c);
    }
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
    std::vector<long> intersection;
    for (size_t j = 0; j < server_set.size(); j++) {
        ZZ decrypted = decrypt(combined_ciphertexts[j], keys.sk, keys.params.p); 
        if (decrypted == to_ZZ(1)) {
            intersection.push_back(server_set[j]);
        }
    }
    auto server_decryption_stop = high_resolution_clock::now();
    *server_computation_time = duration<double, std::milli>(server_decryption_stop - server_decryption_start).count();

    return intersection;
}