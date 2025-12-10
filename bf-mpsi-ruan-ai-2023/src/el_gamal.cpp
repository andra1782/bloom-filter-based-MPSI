#include "el_gamal.hpp"

void key_gen(Keys* keys, long key_length, long num_parties) {
    keys->params.p = NTL::GenPrime_ZZ(key_length);
    keys->params.g = NTL::RandomBnd(keys->params.p); // TODO: g should be a generator

    keys->key_pairs.clear();
    keys->key_pairs.reserve(num_parties);

    for (int i = 0; i < num_parties; ++i) {
        KeyPair kp;
        kp.sk = NTL::RandomBnd(keys->params.p - 2) + 2; // random in [2, p-1]
        kp.pk = PowerMod(keys->params.g, kp.sk, keys->params.p);
        keys->key_pairs.push_back(kp);
    }
}

Ciphertext encrypt(ZZ message, const ZZ& pk, const PublicParameters& params) {
    Ciphertext ct;
    ZZ r = NTL::RandomBnd(params.p);

    // y_{i,1} = g^r mod p
    ct.c1 = PowerMod(params.g, r, params.p);

    // y_{i,2} = m * (pk^r) mod p
    ct.c2 = MulMod(message, PowerMod(pk, r, params.p), params.p);

    return ct;
}

ZZ join_encrypted_data(const std::vector<ZZ>& c2_values, const PublicParameters& params) {
    ZZ Y = to_ZZ(1);
    for (const auto& c2 : c2_values) {
        Y = MulMod(Y, c2, params.p);
    }
    return Y;
}

ZZ decrypt_share(const ZZ& Y, const ZZ& c1, const ZZ& sk, const PublicParameters& params, long num_parties) {
    // Y / (c1^(sk * t)) mod p
    ZZ denominator = PowerMod(c1, sk * num_parties, params.p); 
    ZZ denominator_inv = InvMod(denominator, params.p);
    return MulMod(Y, denominator_inv, params.p);
}

ZZ combine_decryption_shares(const std::vector<ZZ>& shares, const PublicParameters& params) {
    ZZ combined_bf = to_ZZ(1);
    for (const auto& s : shares) {
        combined_bf = MulMod(combined_bf, s, params.p);
    }
    return combined_bf;
}