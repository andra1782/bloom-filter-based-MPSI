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
    ZZ sk;
};

struct Ciphertext {
    ZZ c1; 
    ZZ c2; 
};

void key_gen(Keys* keys, long key_length, long t, long n);
Ciphertext encrypt(const ZZ& message, const PublicParameters& params);
ZZ decrypt(const Ciphertext& ct, const ZZ& sk, const ZZ& p);

#endif