#ifndef EL_GAMAL_HPP
#define EL_GAMAL_HPP

#include <vector>
#include <NTL/ZZ.h>

using namespace NTL;

struct PublicParameters {
    ZZ p; 
    ZZ g; 
    ZZ pk; 
};

struct Keys {
    PublicParameters params;
    std::vector<ZZ> key_shares; 
};

struct Ciphertext {
    ZZ c1; 
    ZZ c2; 
};

void key_gen(Keys* keys, long key_length, long t, long n);
Ciphertext encrypt(const ZZ& message, const PublicParameters& params);
ZZ compute_delta(int i, int t, const ZZ& p);
ZZ compute_share(const ZZ& c1, const ZZ& sk_i, const ZZ& delta_i, const ZZ& p);

#endif