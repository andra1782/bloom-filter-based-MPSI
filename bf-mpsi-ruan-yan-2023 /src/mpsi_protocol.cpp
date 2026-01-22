#include "mpsi_protocol.hpp"
#include <chrono>

std::vector<long> multiparty_psi(
    const std::vector<std::vector<long>>& client_sets,
    const std::vector<long>& server_set,
    BloomFilterParams& bf_params,
    const Keys& keys,
    double* client_prep_time,
    double* client_online_time,
    double* server_prep_time,
    double* server_online_time) 
{
    using namespace std::chrono;
    int n_clients = client_sets.size();
    int total_parties = n_clients + 1; 

    // Initialization stage
    auto start = high_resolution_clock::now();
    std::vector<std::vector<Ciphertext>> all_erbfs;
    for (const auto& set : client_sets) {
        BloomFilter bf(bf_params);
        for (size_t x : set) 
            bf.insert(x);

        std::vector<Ciphertext> erbf;
        for (size_t l = 0; l < bf_params.bin_count; l++) {
            ZZ m = bf.contains_bit(l) ? to_ZZ(1) : (RandomBnd(keys.params.p - 2) + 2);
            erbf.push_back(encrypt(m, keys.params));
        }
        all_erbfs.push_back(erbf);
    }
    auto stop = high_resolution_clock::now();
    *client_prep_time = duration<double, std::milli>(stop - start).count() / n_clients;

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
    ZZ q = (keys.params.p - 1) / 2;
    std::vector<long> result;
    for (long j = 0; j < server_set.size(); j++) {
        start = high_resolution_clock::now();
        Ciphertext c_j = w_js[j];
        for (const auto& erbf : all_erbfs) {
            for (uint64_t seed : bf_params.seeds) {
                size_t idx = hash_element(server_set[j], seed) % bf_params.bin_count;
                c_j.c1 = MulMod(c_j.c1, erbf[idx].c1, keys.params.p);
                c_j.c2 = MulMod(c_j.c2, erbf[idx].c2, keys.params.p);
            }
        }
        stop = high_resolution_clock::now();
        *server_online_time += duration<double, std::milli>(stop - start).count();

        ZZ combined_shares = to_ZZ(1);
        double current_item_client_time_sum = 0.0;
        for (int i = 1; i <= total_parties; i++) {
            start = high_resolution_clock::now();
            ZZ delta = compute_delta(i, total_parties, q);
            // i-1 since the vector is 0-indexed, but 'delta' uses 1-indexing
            ZZ share = compute_share(c_j.c1, keys.key_shares[i-1], delta, keys.params.p);
            stop = high_resolution_clock::now();
            double share_time = duration<double, std::milli>(stop - start).count();

            if (i <= n_clients) 
                current_item_client_time_sum += share_time;
            else 
                *server_online_time += share_time;

            start = high_resolution_clock::now();
            combined_shares = MulMod(combined_shares, share, keys.params.p);
            stop = high_resolution_clock::now();
            *server_online_time += duration<double, std::milli>(stop - start).count();
        }   
        *client_online_time += current_item_client_time_sum / n_clients;
        
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