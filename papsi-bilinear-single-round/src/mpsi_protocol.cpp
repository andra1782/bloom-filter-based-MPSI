#include "mpsi_protocol.hpp"
#include <chrono>
#include <string>
#include <random>
#include <mcl/bn.hpp>

using namespace std;
using namespace mcl::bn;
static cybozu::RandomGenerator rg;

/*
    The code contains elements inspired from:
    https://github.com/markatou/Partial-APSI/blob/main/c_code/protocols/apsi.cpp
*/

template <typename T>
size_t get_element_size(const T& element) {
    char buffer[sizeof(T)]; 
    return element.serialize(buffer, sizeof(buffer)); 
}

// https://inversepalindrome.com/blog/how-to-create-a-random-string-in-cpp
string random_string(size_t length) {
    const string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    random_device random_device;
    mt19937 generator(random_device());
    uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

    string random_string;
    for (size_t i = 0; i < length; ++i)
        random_string += CHARACTERS[distribution(generator)];
    return random_string;
}

G1 hash_to_G1(long element) {
    G1 h;
    std::string s = std::to_string(element) + "_ID_S";
    hashAndMapToG1(h, s.c_str(), s.size());
    return h;
}

// H2: G1 -> G1
G1 h2(const G1& element) {
    G1 h;
    string s = element.serializeToHexStr();
    hashAndMapToG1(h, s.c_str(), s.size());
    return h;
}

Fr server_blinding(const vector<long>& server_set, vector<G1>& blind_xs) {
    Fr r;
    r.setHashOf(random_string(32)); 
    
    Fr inv_r;
    Fr::inv(inv_r, r);
    blind_xs.resize(server_set.size());

    // Server blinds its elements with r
    for (size_t i = 0; i < server_set.size(); i++) {
        G1 h1_x = hash_to_G1(server_set[i]);
        G1::mul(blind_xs[i], h1_x, r); // H1(x)^r
    }
    return r;
}

unordered_set<int> judge_challenge(const vector<long>& server_set, int p_fraction) {
    unordered_set<int> I;
    for (size_t i = 0; i < server_set.size(); i++) {
        if (rand() % 100 < p_fraction) {
            I.insert(i);
        }
    }
    return I;
}

pair<Fr, Fr> generate_eea_proof(const vector<long>& server_set, Fr r, const unordered_set<int>& I, vector<G1>& tis) {
    Fr random_eea;
    random_eea.setHashOf(random_string(32));
    int c_eea = 0;
    
    for (size_t i = 0; i < server_set.size(); i++) {
        if (I.count(i)) {
            G1 h1_x = hash_to_G1(server_set[i]); // H1(x)
            G1::mul(tis[i], h1_x, random_eea); // t_i = H1(x)^z
        }
    }

    hash<string> std_hash;
    for (size_t i = 0; i < server_set.size(); i++) {
        string long_str = tis[i].serializeToHexStr() + to_string(server_set[i]); // H1(x)^z + x
        c_eea = std_hash(long_str + to_string(c_eea)); // c = H(t_i, x_i)
    }

    Fr c_tmp = c_eea; // c = H(t_i, x_i)
    Fr s_eea = random_eea + c_tmp * r; // s = z + c * r
    return make_pair(c_tmp, s_eea);
}

void verify_eea_proof(const vector<long>& server_set, 
                      const unordered_set<int>& I, 
                      pair<Fr, Fr> eea_proof, 
                      const vector<G1>& blind_xs,
                      const vector<G1>& tis) {
    hash<string> std_hash;
    for (size_t i = 0; i < server_set.size(); i++) {
        if (I.count(i)) {
            G1 h1_x = hash_to_G1(server_set[i]); // H1(x)
            Fr c_tmp = eea_proof.first;
            Fr s_eea = eea_proof.second;

            G1 right; 
            G1::mul(right, h1_x, s_eea); // H1(x)^s

            G1 check;
            G1::mul(check, blind_xs[i], c_tmp); // H1(x)^{c*r}
            G1 left = tis[i] + check; // t_i + H1(x)^{c*r}
            
            if (left.serializeToHexStr() != right.serializeToHexStr()) {
                cout << "Judge: EEA Verification fails for item " << i << "\n";
            }
        }
    }
}

vector<G1> judge_sign(const vector<G1>& blind_xs, const Fr& judge_sk) {
    vector<G1> signatures(blind_xs.size());
    for (size_t i = 0; i < blind_xs.size(); i++) {
        G1 h2_blind = h2(blind_xs[i]); // H2(H1(x)^r)
        G1 sign;
        G1::mul(sign, h2_blind, judge_sk); // sigma = H2(H1(x)^r)^{sk}
        signatures[i] = sign;
    }
    return signatures;
}

GarbledBloomFilter compute_gbf(const vector<long>& set, 
                                     const BloomFilterParams& bf_params, 
                                     const Fr& r,
                                     const Fr& s_i,
                                     const G2& pk) {
    vector<GT> targets;
    for (size_t i = 0; i < set.size(); i++) {
        G1 h1_x = hash_to_G1(set[i]); // H1(x)
        G1 h1_x_blinded;
        G1::mul(h1_x_blinded, h1_x, r); //
        G1 h2_result = h2(h1_x_blinded); // H2(H1(x)^r)
        
        G1 h2_si;
        G1::mul(h2_si, h2_result, s_i); // H2(H1(x)^r)^{s_i}
        
        GT target;
        pairing(target, h2_si, pk); // e(H2(H1(x)^r)^{s_i}, pk)
        targets.push_back(target);
    }

    GarbledBloomFilter gbf(bf_params);
    gbf.insert_set(set, targets);
    return gbf;
}

std::vector<GT> aggregate_gbfs(const std::vector<GarbledBloomFilter>& gbfs, 
                               int n_clients, 
                               const BloomFilterParams& bf_params) {
    std::vector<GT> aggregated_gbf(bf_params.bin_count);
    for (size_t v = 0; v < bf_params.bin_count; v++) {
        aggregated_gbf[v].clear(); // set to 0
        GT::add(aggregated_gbf[v], aggregated_gbf[v], 1); // set to 1
        for (int i = 0; i < n_clients; i++) {
            GT::mul(aggregated_gbf[v], aggregated_gbf[v], gbfs[i].bins[v]);
        }
    }
    return aggregated_gbf;
}

std::vector<long> intersect(const std::vector<long>& server_set,
                            const std::vector<G1>& judge_signatures,
                            const G2& S_agg,
                            const std::vector<GT>& aggregated_gbf, 
                            const BloomFilterParams& bf_params) {
    std::vector<long> intersection;
    
    for (size_t j = 0; j < server_set.size(); j++) {
        // \hat{x}_j = e(\sigma_j, S_{agg})
        GT expected_target;
        pairing(expected_target, judge_signatures[j], S_agg);

        // \hat{y}_j = \prod GBF_{agg}[h_u(x)]
        GT actual_target;
        actual_target.clear(); 
        GT::add(actual_target, actual_target, 1); // init to 1
        
        std::unordered_set<size_t> visited_bins;
        for (uint64_t seed : bf_params.seeds) {
            size_t v = hash_element(server_set[j], seed) % bf_params.bin_count;
            if(visited_bins.count(v) > 0) 
                continue;
            visited_bins.insert(v);
            
            GT::mul(actual_target, actual_target, aggregated_gbf[v]);
        }

        if (expected_target == actual_target) 
            intersection.push_back(server_set[j]);
    }
    return intersection;
}

std::vector<long> multiparty_psi(
    const std::vector<std::vector<long>>& client_sets,
    const std::vector<long>& server_set,
    BloomFilterParams& bf_params,
    const mcl::bn::G2& judge_pk,  
    const mcl::bn::Fr& judge_sk, 
    int p_fraction, 
    double* client_computation_time,
    double* leader_client_computation_time,
    double* server_authorize_time,
    double* server_intersect_time,
    double* judge_computation_time,
    size_t* server_sent_bytes, 
    size_t* server_received_bytes,
    size_t* client_sent_bytes,
    size_t* client_received_bytes,
    size_t* leader_client_sent_bytes,
    size_t* leader_client_received_bytes,
    size_t* judge_sent_bytes,
    size_t* judge_received_bytes
) {
    using namespace std::chrono;
    int n_clients = client_sets.size();

    // Authorization Phase
    // Server blinds its set 
    auto server_blind_start = high_resolution_clock::now();
    vector<G1> blind_xs;
    Fr r = server_blinding(server_set, blind_xs);
    auto server_blind_stop = high_resolution_clock::now();
    *server_authorize_time += duration<double, std::milli>(server_blind_stop - server_blind_start).count();

    // Server sends blinded set to judge
    for (const auto& blind_x : blind_xs) {
        *server_sent_bytes += get_element_size(blind_x); 
        *judge_received_bytes += get_element_size(blind_x);
    }

    // Judge creates a challenge subset of the blinded set
    auto judge_challenge_start = high_resolution_clock::now();
    unordered_set<int> challenge_indices = judge_challenge(server_set, p_fraction);
    auto judge_challenge_stop = high_resolution_clock::now();
    *judge_computation_time += duration<double, std::milli>(judge_challenge_stop - judge_challenge_start).count();

    // Judge sends the challenge to the server (indices of the challenged items)
    *judge_sent_bytes += challenge_indices.size() * sizeof(int);
    *server_received_bytes += challenge_indices.size() * sizeof(int);

    //Server generates EEA proof for the challenged items
    vector<G1> tis(server_set.size());
    auto server_eea_start = high_resolution_clock::now();
    pair<Fr, Fr> eea_proof = generate_eea_proof(server_set, r, challenge_indices, tis);
    auto server_eea_stop = high_resolution_clock::now();
    *server_authorize_time += duration<double, std::milli>(server_eea_stop - server_eea_start).count();

    // Server sends EEA proof to judge
    *server_sent_bytes += get_element_size(eea_proof.first) + get_element_size(eea_proof.second);
    *judge_received_bytes += get_element_size(eea_proof.first) + get_element_size(eea_proof.second);

    // Judge verifies EEA proof and computes signatures for the blinded set if the proof is valid
    auto judge_verify_and_sign_start = high_resolution_clock::now();
    vector<G1> judge_signatures;
    verify_eea_proof(server_set, challenge_indices, eea_proof, blind_xs, tis);
    judge_signatures = judge_sign(blind_xs, judge_sk);
    auto judge_verify_and_sign_stop = high_resolution_clock::now();
    *judge_computation_time += duration<double, std::milli>(judge_verify_and_sign_stop - judge_verify_and_sign_start).count();

    // Judge sends signatures to server
    for(const auto& sigma : judge_signatures) {
        *judge_sent_bytes += get_element_size(sigma); 
        *server_received_bytes += get_element_size(sigma);
    }


    // Intersect Phase
    // Server sends r to each client
    for (int i = 0; i < n_clients; i++) {
        *server_sent_bytes += get_element_size(r);
        *client_received_bytes += get_element_size(r);
        *leader_client_received_bytes += get_element_size(r);
    }

    // Each client generates a secret, computes their S value and their GBF
    auto client_start = high_resolution_clock::now();
    std::vector<G2> S_values;
    std::vector<GarbledBloomFilter> gbfs;
    G2 g2_gen; 
    mapToG2(g2_gen, 1);

    for (size_t i = 0; i < client_sets.size(); i++) {
        // secret
        Fr s_i; 
        s_i.setHashOf(random_string(32));

        // S value
        G2 S_i; 
        G2::mul(S_i, g2_gen, s_i);
        S_values.push_back(S_i);
        
        // GBF
        gbfs.push_back(compute_gbf(client_sets[i], bf_params, r, s_i, judge_pk));
    }
    auto client_stop = high_resolution_clock::now();
    *client_computation_time = duration<double, std::milli>(client_stop - client_start).count() / n_clients;
    *leader_client_computation_time = duration<double, std::milli>(client_stop - client_start).count() / n_clients;

    // Each client sends their S value and GBF to the leader client (client 1)
    size_t total_gbf_and_s_values_bytes = 0;
    for (size_t i = 1; i < n_clients; i++) {
        total_gbf_and_s_values_bytes += get_element_size(S_values[i]);
        for (const auto& bin : gbfs[i].bins) 
            total_gbf_and_s_values_bytes += get_element_size(bin);
    }
    if (n_clients > 1) {
        *client_sent_bytes += total_gbf_and_s_values_bytes / (n_clients - 1);
        *leader_client_received_bytes += total_gbf_and_s_values_bytes;
    }
    
    // The leader client aggregates the S values and GBFs
    auto leader_start = high_resolution_clock::now();
    std::vector<GT> aggregated_gbf = aggregate_gbfs(gbfs, n_clients, bf_params);
    G2 S_agg; 
    S_agg.clear(); 
    for (const auto& S_i : S_values) {
        G2::add(S_agg, S_agg, S_i); // exponent multiplications
    }
    auto leader_stop = high_resolution_clock::now();
    *leader_client_computation_time = duration<double, std::milli>(leader_stop - leader_start).count();

    // The leader client sends the aggregated S and GBFs to the server
    for (const auto& bin : aggregated_gbf) {
        *leader_client_sent_bytes += get_element_size(bin);
        *server_received_bytes += get_element_size(bin);
    }
    *leader_client_sent_bytes += get_element_size(S_agg);
    *server_received_bytes += get_element_size(S_agg);

    // The server computes the intersection
    auto server_start = high_resolution_clock::now();
    std::vector<long> intersection = intersect(server_set, judge_signatures, S_agg, aggregated_gbf, bf_params);
    auto server_stop = high_resolution_clock::now();
    *server_intersect_time = duration<double, std::milli>(server_stop - server_start).count();

    return intersection;
}