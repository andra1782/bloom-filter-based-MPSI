#ifndef EL_GAMAL_H
#define EL_GAMAL_H

#include <vector>
#include <utility>
#include <NTL/ZZ.h>

using namespace NTL;

struct PublicParameters {
    ZZ p; 
    ZZ g; 
};

struct KeyPair {
    ZZ pk;
    ZZ sk;
};

struct Keys {
    PublicParameters params;
    std::vector<KeyPair> key_pairs;
};

struct Ciphertext {
    ZZ c1; // g^r
    ZZ c2; // m * pk^r
};

void key_gen(Keys* keys, long key_length, long num_parties);

Ciphertext encrypt(ZZ message, const ZZ& pk, const PublicParameters& params);

ZZ join_encrypted_data(const std::vector<ZZ>& c2_values, const PublicParameters& params);

ZZ decrypt_share(const ZZ& Y, const ZZ& c1, const ZZ& sk, const PublicParameters& params, long num_parties);

ZZ combine_decryption_shares(const std::vector<ZZ>& shares, const PublicParameters& params);

#endif