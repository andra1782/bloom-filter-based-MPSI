#include "mpsi_protocol.hpp"

std::vector<long> multiparty_psi(
    const std::vector<std::vector<long>>& client_sets,
    const std::vector<long>& server_set,
    BloomFilterParams& bf_params,
    const Keys& keys) 
{
    int n_clients = client_sets.size();
    int total_parties = n_clients + 1; 

    // Initialization stage
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

    // Server blinding
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

    // Online stage
    std::vector<long> result;
    for (long j = 0; j < server_set.size(); j++) {
        Ciphertext c_j = w_js[j];
        for (const auto& erbf : all_erbfs) {
            for (uint64_t seed : bf_params.seeds) {
                size_t idx = hash_element(server_set[j], seed) % bf_params.bin_count;
                c_j.c1 = MulMod(c_j.c1, erbf[idx].c1, keys.params.p);
                c_j.c2 = MulMod(c_j.c2, erbf[idx].c2, keys.params.p);
            }
        }

        ZZ q = (keys.params.p - 1) / 2;
        ZZ combined_shares = to_ZZ(1);
        for (int i = 1; i <= total_parties; i++) {
            ZZ delta = compute_delta(i, total_parties, q);
            // i-1 since the vector is 0-indexed, but 'delta' uses 1-indexing
            combined_shares = MulMod(combined_shares, compute_share(c_j.c1, keys.key_shares[i-1], delta, keys.params.p), keys.params.p);
        }   

        ZZ decrypted = MulMod(c_j.c2, InvMod(combined_shares, keys.params.p), keys.params.p);
        if (decrypted == MulMod(to_ZZ(server_set[j] + 1), r_js[j], keys.params.p)) {
            result.push_back(server_set[j]);
        }
    }
    return result;
}