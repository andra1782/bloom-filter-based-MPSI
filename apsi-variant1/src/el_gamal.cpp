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

    ZZ sk = RandomBnd(q - 1) + 1;
    keys->params.pk = PowerMod(keys->params.g, sk, keys->params.p);

    // f(x) = sk + a_1 * x + a_2 * x^2 + ... + a_{t-1} * x^{t-1}
    // poly[i] = a_i
    std::vector<ZZ> poly(t);
    poly[0] = sk;
    for(int i = 1; i < t; i++) 
        poly[i] = RandomBnd(keys->params.p - 1);

    keys->key_shares.clear();
    for(int i = 1; i <= n; i++) {
        ZZ val = to_ZZ(0); // f(i)
        ZZ x_pow = to_ZZ(1);
        for(int j = 0; j < t; j++) {
            val = AddMod(val, MulMod(poly[j], x_pow, q), q);
            x_pow = MulMod(x_pow, to_ZZ(i), q);
        }
        keys->key_shares.push_back(val);
    }
}

Ciphertext encrypt(const ZZ& message, const PublicParameters& params) {
    ZZ r = RandomBnd(params.p - 2) + 1;
    Ciphertext ct;
    ct.c1 = PowerMod(params.g, r, params.p);
    ct.c2 = MulMod(message, PowerMod(params.pk, r, params.p), params.p);
    return ct;
}

ZZ compute_delta(int i, int t, const ZZ& q) {
    ZZ num = to_ZZ(1);
    ZZ den = to_ZZ(1);

    for (int j = 1; j <= t; j++) {
        if (i == j) continue;
        num = MulMod(num, to_ZZ(j), q);

        long diff = j - i;
        ZZ diff_zz = to_ZZ(diff);
        if (diff < 0) 
            diff_zz = AddMod(diff_zz, q, q);
        den = MulMod(den, diff_zz, q);
    }
    return MulMod(num, InvMod(den, q), q);
}

// sh_{j,i} = c_{j,1} ^ {delta_i * sk_i} mod p
ZZ compute_share(const ZZ& c1, const ZZ& sk_i, const ZZ& delta_i, const ZZ& p) {
    ZZ q = (p - 1) / 2;
    ZZ exponent = MulMod(delta_i, sk_i, q);
    return PowerMod(c1, exponent, p);
}