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

void set_blinding(const std::vector<long>& set, 
                const Keys& keys, 
                std::vector<ZZ>& r_js, 
                std::vector<Ciphertext>& w_js) {
    for (long x : set) {
        ZZ r = RandomBnd(keys.params.p - 1) + 1;
        r_js.push_back(r);
        Ciphertext enc_x = encrypt(to_ZZ(x + 1), keys.params); // avoid encrypting 0
        Ciphertext enc_r = encrypt(r, keys.params);
        w_js.push_back({MulMod(enc_x.c1, enc_r.c1, keys.params.p), 
                        MulMod(enc_x.c2, enc_r.c2, keys.params.p)});
    }
}

std::vector<Ciphertext> aggregate_ciphertexts(const std::vector<long>& server_set, 
                        const std::vector<std::vector<Ciphertext>>& clients_erbfs, 
                        const std::vector<Ciphertext>& w_js,
                        const BloomFilterParams& bf_params,
                        const Keys& keys) {
    std::vector<Ciphertext> combined_ciphertexts;
    for(long j = 0; j < server_set.size(); j++) {
        Ciphertext c_j = w_js[j];
        for (const auto& erbf : clients_erbfs) {
            for (uint64_t seed : bf_params.seeds) {
                size_t idx = hash_element(server_set[j], seed) % bf_params.bin_count;
                c_j.c1 = MulMod(c_j.c1, erbf[idx].c1, keys.params.p);
                c_j.c2 = MulMod(c_j.c2, erbf[idx].c2, keys.params.p);
            }
        }
        combined_ciphertexts.push_back(c_j);
    }
    return combined_ciphertexts;
}

std::vector<ZZ> compute_decryption_shares(const std::vector<ZZ>& combined_ciphertexts_c1, 
                            const std::vector<long>& server_set, 
                            const ZZ& key_share,
                            const ZZ& p,
                            const ZZ& q,
                            int i,
                            int total_parties) {
    std::vector<ZZ> shares;
    for(long j = 0; j < server_set.size(); j++) {
        ZZ delta = compute_delta(i + 1, total_parties, q);
        ZZ share = compute_share(combined_ciphertexts_c1[j], key_share, delta, p);
        shares.push_back(share);
    }
    return shares;
}

std::vector<long> decrypt_intersection(const std::vector<std::vector<ZZ>>& decryption_shares, 
                                    const std::vector<Ciphertext>& combined_ciphertexts,
                                    const std::vector<long>& server_set,
                                    const std::vector<ZZ>& r_js,
                                    const int total_parties,
                                    const Keys& keys) {
    std::vector<long> intersection;
    for(long j = 0; j < server_set.size(); j++) {
        ZZ combined_shares = to_ZZ(1);
        for (int i = 0; i < total_parties; i++) 
            combined_shares = MulMod(combined_shares, decryption_shares[i][j], keys.params.p);
        ZZ decrypted = MulMod(combined_ciphertexts[j].c2, InvMod(combined_shares, keys.params.p), keys.params.p);
        if (decrypted == MulMod(to_ZZ(server_set[j] + 1), r_js[j], keys.params.p)) 
            intersection.push_back(server_set[j]);
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
    double* server_prep_time,
    double* server_online_time,
    size_t* server_sent_bytes, 
    size_t* server_received_bytes,
    size_t* client_sent_bytes,
    size_t* client_received_bytes
) {
    using namespace std::chrono;
    int n_clients = client_sets.size();
    int total_parties = n_clients + 1; 

    // Pre-processing stage
    // Clients compute their ERBFs
    auto start = high_resolution_clock::now();
    std::vector<std::vector<Ciphertext>> all_erbfs;
    for (const auto& set : client_sets) {
        all_erbfs.push_back(compute_erbf(set, bf_params, keys));
    }
    auto stop = high_resolution_clock::now();
    *client_prep_time = duration<double, std::milli>(stop - start).count() / n_clients;

    // Server blinding
    start = high_resolution_clock::now();
    std::vector<ZZ> r_js;
    std::vector<Ciphertext> w_js;
    set_blinding(server_set, keys, r_js, w_js);
    stop = high_resolution_clock::now();
    *server_prep_time = duration<double, std::milli>(stop - start).count();
    

    // Online stage
    // Clients send ERBFs to server
    size_t all_erbfs_size_bytes = 0;
    for (const auto& erbf : all_erbfs) 
        for (const auto& ct : erbf) 
            all_erbfs_size_bytes += get_ciphertext_size(ct);
    *client_sent_bytes += all_erbfs_size_bytes / n_clients;
    *server_received_bytes += all_erbfs_size_bytes;

    // Server computes ciphertexts for each of its elements
    start = high_resolution_clock::now();
    std::vector<Ciphertext> combined_ciphertexts = aggregate_ciphertexts(server_set, 
                                                                        all_erbfs, w_js, 
                                                                        bf_params, keys);
    stop = high_resolution_clock::now();
    *server_online_time += duration<double, std::milli>(stop - start).count();

    // Server sends c_j to all clients (but only the c1's are needed for creating decryption shares)
    std::vector<ZZ> combined_ciphertexts_c1;
    size_t combined_ciphertexts_size_bytes = 0;
    for (const auto& ct : combined_ciphertexts) {  
        combined_ciphertexts_c1.push_back(ct.c1);
        combined_ciphertexts_size_bytes += NumBytes(ct.c1);
    }
    *server_sent_bytes += combined_ciphertexts_size_bytes * n_clients;
    *client_received_bytes += combined_ciphertexts_size_bytes;

    // Each client computes decryption shares
    ZZ q = (keys.params.p - 1) / 2;
    std::vector<std::vector<ZZ>> decryption_shares(total_parties);
    start = high_resolution_clock::now();
    for (int i = 0; i < total_parties; i++) 
        decryption_shares[i] = compute_decryption_shares(combined_ciphertexts_c1, server_set, 
                                            keys.key_shares[i], keys.params.p, q, 
                                            i, total_parties);
    stop = high_resolution_clock::now();
    *client_online_time += duration<double, std::milli>(stop - start).count() / n_clients;

    // Clients send shares to server
    size_t all_shares_size_bytes = 0;
    for (int i = 0; i < n_clients; i++) 
        for (const auto& share : decryption_shares[i]) 
            all_shares_size_bytes += NumBytes(share);
    *client_sent_bytes += all_shares_size_bytes / n_clients;    
    *server_received_bytes += all_shares_size_bytes;
    
    // Server combines shares and decrypts
    start = high_resolution_clock::now();
    std::vector<long> intersection = decrypt_intersection(decryption_shares, 
                                                        combined_ciphertexts,
                                                        server_set, r_js,
                                                        total_parties, keys);
    stop = high_resolution_clock::now();
    *server_online_time += duration<double, std::milli>(stop - start).count();

    return intersection;
}