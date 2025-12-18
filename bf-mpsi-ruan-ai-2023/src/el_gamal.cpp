#include "el_gamal.hpp"

void key_gen(Keys* keys, long key_length, long num_parties) {
    // Inspired by https://github.com/TNO-MPC/encryption_schemes.elgamal/blob/main/src/tno/mpc/encryption_schemes/elgamal/elgamal_base.py
    // a safe prime p = 2q + 1 so Z_p^* is a cyclic group of order p-1 
    // a generator g of this group (g^q mod p != 1  AND  g^2 mod p != 1)
    // a random value sk in [1, ..., p - 2], used to calculate pk = g^x
    ZZ q, g;
    GenGermainPrime(q, key_length - 1);
    keys->params.p = 2 * q + 1;

    for (g = 2; g < keys->params.p - 1; g++) {
        ZZ check_q, check_2;

        PowerMod(check_q, g, q, keys->params.p);
        if (check_q == 1) continue;

        PowerMod(check_2, g, 2, keys->params.p);
        if (check_2 == 1) continue; 

        break;
    }
    keys->params.g = g;

    keys->key_pairs.clear();
    keys->key_pairs.reserve(num_parties);

    for (int i = 0; i < num_parties; ++i) {
        KeyPair kp;
        kp.sk = NTL::RandomBnd(keys->params.p - 2) + 1; // random in [1, p-2]
        kp.pk = PowerMod(keys->params.g, kp.sk, keys->params.p);
        keys->key_pairs.push_back(kp);
    }
}

Ciphertext encrypt(ZZ message, const ZZ& pk, const PublicParameters& params) {
    Ciphertext ct;
    ZZ r = NTL::RandomBnd(params.p - 1) + 1; // random in [1, p-1]

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