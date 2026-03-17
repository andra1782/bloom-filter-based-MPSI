#include "el_gamal.hpp"

void key_gen(Keys* keys, long key_length, long t, long n) {
    ZZ q;
    GenGermainPrime(q, key_length - 1);
    keys->params.p = 2 * q + 1;
    
    ZZ h = to_ZZ(2);
    while (PowerMod(h, q, keys->params.p) == 1 || PowerMod(h, 2, keys->params.p) == 1) {
        h++;
    }
    keys->params.g = PowerMod(h, 2, keys->params.p);

    keys->sk = RandomBnd(q - 1) + 1;
    keys->params.pk = PowerMod(keys->params.g, keys->sk, keys->params.p);
}

Ciphertext encrypt(const ZZ& message, const PublicParameters& params) {
    ZZ r = RandomBnd(params.p - 2) + 1;
    Ciphertext ct;
    ct.c1 = PowerMod(params.g, r, params.p);
    ct.c2 = MulMod(message, PowerMod(params.pk, r, params.p), params.p);
    return ct;
}

ZZ decrypt(const Ciphertext& ct, const ZZ& sk, const ZZ& p) {
    return MulMod(ct.c2, InvMod(PowerMod(ct.c1, sk, p), p), p);
}