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

    // Clients agree on a key k_OPRF (one client distributes to all in a star topology)
    ZZ q = (keys.params.p - 1) / 2;
    ZZ k_oprf = RandomBnd(q - 1) + 1;
    *client_sent_bytes += NumBytes(k_oprf) * (n_clients - 1);

    // Initialization stage
    auto start = high_resolution_clock::now();
    size_t all_erbfs_size_bytes = 0;
    std::vector<std::vector<Ciphertext>> all_erbfs;
    for (const auto& set : client_sets) {
        BloomFilter bf(bf_params);
        for (size_t x : set) {
            unsigned char hash_buf[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char*>(&x), sizeof(x), hash_buf);

            ZZ inner_hash = ZZFromBytes(hash_buf, SHA256_DIGEST_LENGTH);
            inner_hash = PowerMod(inner_hash, 2, keys.params.p); 
            ZZ oprf_result = PowerMod(inner_hash, k_oprf, keys.params.p); 

            size_t bf_element = 0;
            BytesFromZZ(reinterpret_cast<unsigned char*>(&bf_element), oprf_result, sizeof(size_t));

            bf.insert(bf_element);
        };

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
    // Clients send ERBFs to server
    *client_sent_bytes += all_erbfs_size_bytes / n_clients;
    *server_received_bytes += all_erbfs_size_bytes;

    // Server blinding
    start = high_resolution_clock::now();
    std::vector<ZZ> r_js;
    std::vector<Ciphertext> w_js;
    for (long x : server_set) {
        ZZ r = RandomBnd(keys.params.p - 1) + 1;
        r_js.push_back(r);
        Ciphertext enc_x = encrypt(to_ZZ(x + 1), keys.params); // avoid encrypting 0
        Ciphertext enc_r = encrypt(r, keys.params);
        w_js.push_back({MulMod(enc_x.c1, enc_r.c1, keys.params.p), 
                        MulMod(enc_x.c2, enc_r.c2, keys.params.p)});
    }
    stop = high_resolution_clock::now();
    *server_prep_time = duration<double, std::milli>(stop - start).count();

    // Online stage
    std::vector<ZZ> t_blinds(server_set.size());
    std::vector<ZZ> blinded_queries(server_set.size());

    // OPRF
    // Server Request
    start = high_resolution_clock::now();
    for (size_t j = 0; j < server_set.size(); j++) {
        unsigned char hash_buf[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(&server_set[j]), sizeof(server_set[j]), hash_buf);
        ZZ inner_hash = ZZFromBytes(hash_buf, SHA256_DIGEST_LENGTH);
        inner_hash = PowerMod(inner_hash, 2, keys.params.p); // quadratic residues

        t_blinds[j] = RandomBnd(q - 1) + 1;
        blinded_queries[j] = PowerMod(inner_hash, t_blinds[j], keys.params.p);
        *server_sent_bytes += NumBytes(blinded_queries[j]);
        *client_received_bytes += NumBytes(blinded_queries[j]);
    }
    stop = high_resolution_clock::now();
    *server_online_time += duration<double, std::milli>(stop - start).count();

    // Clients Eval (one client is enough since they all have the same k_OPRF)
    start = high_resolution_clock::now();
    std::vector<ZZ> evaluated_queries(server_set.size());
    for (size_t j = 0; j < server_set.size(); j++) {
        evaluated_queries[j] = PowerMod(blinded_queries[j], k_oprf, keys.params.p);
        *client_sent_bytes += NumBytes(evaluated_queries[j]);
        *server_received_bytes += NumBytes(evaluated_queries[j]);
    }
    stop = high_resolution_clock::now();
    *client_online_time += duration<double, std::milli>(stop - start).count();

    // Server Recover
    start = high_resolution_clock::now();
    std::vector<size_t> server_bf_elements(server_set.size());
    for (size_t j = 0; j < server_set.size(); j++) {
        ZZ t_inv = InvMod(t_blinds[j], q);
        ZZ oprf_output = PowerMod(evaluated_queries[j], t_inv, keys.params.p);

        size_t bf_element = 0;
        BytesFromZZ(reinterpret_cast<unsigned char*>(&bf_element), oprf_output, sizeof(size_t));
        server_bf_elements[j] = bf_element;
    }
    stop = high_resolution_clock::now();
    *server_online_time += duration<double, std::milli>(stop - start).count();

    // Intersection Computation
    std::vector<long> result;
    for (long j = 0; j < server_set.size(); j++) {
        start = high_resolution_clock::now();
        Ciphertext c_j = w_js[j];
        for (const auto& erbf : all_erbfs) {
            for (uint64_t seed : bf_params.seeds) {
                size_t idx = hash_element(server_bf_elements[j], seed) % bf_params.bin_count;
                c_j.c1 = MulMod(c_j.c1, erbf[idx].c1, keys.params.p);
                c_j.c2 = MulMod(c_j.c2, erbf[idx].c2, keys.params.p);
            }
        }
        stop = high_resolution_clock::now();
        *server_online_time += duration<double, std::milli>(stop - start).count();
        // Server sends c_j to all clients
        *server_sent_bytes += get_ciphertext_size(c_j) * n_clients;
        *client_received_bytes += get_ciphertext_size(c_j);

        ZZ combined_shares = to_ZZ(1);
        double current_item_client_time_sum = 0.0;
        size_t all_shares_size_bytes = 0;
        for (int i = 1; i <= total_parties; i++) {
            start = high_resolution_clock::now();
            ZZ delta = compute_delta(i, total_parties, q);
            // i-1 since the vector is 0-indexed, but 'delta' uses 1-indexing
            ZZ share = compute_share(c_j.c1, keys.key_shares[i-1], delta, keys.params.p);
            stop = high_resolution_clock::now();
            double share_time = duration<double, std::milli>(stop - start).count();

            if (i <= n_clients) {
                current_item_client_time_sum += share_time;
                // Client sends share to server
                all_shares_size_bytes += NumBytes(share);
            } else 
                *server_online_time += share_time;

            start = high_resolution_clock::now();
            combined_shares = MulMod(combined_shares, share, keys.params.p);
            stop = high_resolution_clock::now();
            *server_online_time += duration<double, std::milli>(stop - start).count();
        }
        *client_online_time += current_item_client_time_sum / n_clients;
        *client_sent_bytes += all_shares_size_bytes / n_clients;
        *server_received_bytes += all_shares_size_bytes;
        
        start = high_resolution_clock::now();
        ZZ decrypted = MulMod(c_j.c2, InvMod(combined_shares, keys.params.p), keys.params.p);
        if (decrypted == MulMod(to_ZZ(server_set[j] + 1), r_js[j], keys.params.p)) {
            result.push_back(server_set[j]);
        }
        stop = high_resolution_clock::now();
        *server_online_time += duration<double, std::milli>(stop - start).count();
    }
    return result;
}