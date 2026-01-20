#include "el_gamal.hpp"

void key_gen(Keys* keys, long key_length, long t, long n) {
    ZZ q;
    GenGermainPrime(q, key_length - 1);
    keys->params.p = 2 * q + 1;
    
    ZZ g = to_ZZ(2);
    while (PowerMod(g, q, keys->params.p) == 1 || PowerMod(g, 2, keys->params.p) == 1) {
        g++;
    }
    keys->params.g = g;

    ZZ sk = RandomBnd(keys->params.p - 2) + 1;
    keys->params.pk = PowerMod(g, sk, keys->params.p);

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
            val = AddMod(val, MulMod(poly[j], x_pow, keys->params.p - 1), keys->params.p - 1);
            x_pow = MulMod(x_pow, to_ZZ(i), keys->params.p - 1);
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

ZZ compute_delta(int i, int t, const ZZ& p) {
    ZZ num = to_ZZ(1);
    ZZ den = to_ZZ(1);
    ZZ phi = p - 1; 

    for (int j = 1; j <= t; j++) {
        if (i == j) continue;
        num = MulMod(num, to_ZZ(j), phi);
        den = MulMod(den, to_ZZ(j - i), phi);
    }

    ZZ d, x, y;
    XGCD(d, x, y, den, phi);
    if (d == 1) {
        return MulMod(num, InvMod(den, phi), phi);
    } else {
        // if GCD != 1, solve the congruence den * delta = num (mod phi)
        if (num % d == 0) {
            ZZ new_den = den / d;
            ZZ new_num = num / d;
            ZZ new_phi = phi / d;
            return MulMod(new_num % new_phi, InvMod(new_den % new_phi, new_phi), phi);
        }
        return to_ZZ(0); 
    }
}

// sh_{j,i} = c_{j,1} ^ {delta_i * sk_i} mod p
ZZ compute_share(const ZZ& c1, const ZZ& sk_i, const ZZ& delta_i, const ZZ& p) {
    ZZ exponent = MulMod(delta_i, sk_i, p - 1);
    return PowerMod(c1, exponent, p);
}