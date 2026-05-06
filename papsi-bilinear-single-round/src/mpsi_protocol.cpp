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

GarbledBloomFilter compute_gbf(const std::vector<long>& set, 
                            const BloomFilterParams& bf_params, 
                            const Fr& s_i,
                            const G2& pk) {
    std::vector<GT> targets;
    for (size_t x : set) {
        G1 h1_x = hash_to_G1(x);
        
        G1 h1_x_si;
        G1::mul(h1_x_si, h1_x, s_i); 
        
        GT target;
        pairing(target, h1_x_si, pk); // e(H1(x)^{s_i}, pk)
        
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

std::vector<G1> authorize(const std::vector<long>& server_set, const Fr& judge_sk) {
    std::vector<G1> judge_signatures;
    for (long x : server_set) {
        G1 h1_x = hash_to_G1(x);
        G1 sigma;
        G1::mul(sigma, h1_x, judge_sk); // \sigma = H1(x||ID_S)^{sk}
        judge_signatures.push_back(sigma);
    }
    return judge_signatures;
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
    double* client_prep_time,
    double* client_online_time,
    double* server_computation_time,
    double* judge_computation_time,
    size_t* server_sent_bytes, 
    size_t* server_received_bytes,
    size_t* client_sent_bytes,
    size_t* leader_client_sent_bytes,
    size_t* leader_client_received_bytes,
    size_t* judge_sent_bytes,
    size_t* judge_received_bytes
) {
    using namespace std::chrono;
    int n_clients = client_sets.size();

    // Authorization Phase
    // Server sends its set to the judge
    *server_sent_bytes += server_set.size() * sizeof(long);
    *judge_received_bytes += server_set.size() * sizeof(long);

    // Judge computes signatures (if they approve the set)
    auto judge_start = high_resolution_clock::now();
    std::vector<G1> judge_signatures = authorize(server_set, judge_sk);
    auto judge_stop = high_resolution_clock::now();
    *judge_computation_time += duration<double, std::milli>(judge_stop - judge_start).count();

    // Judge sends signatures back to server
    for(const auto& sigma : judge_signatures) {
        *judge_sent_bytes += get_element_size(sigma); 
        *server_received_bytes += get_element_size(sigma);
    }


    // Intersect Phase
    // Offline Phase
    // Each client generates a secret, computes their S value and their GBF
    auto client_start = high_resolution_clock::now();
    std::vector<G2> S_values;
    std::vector<GarbledBloomFilter> gbfs;
    G2 g2_gen; 
    mapToG2(g2_gen, 1);

    for (const auto& set : client_sets) {
        // secret
        Fr s_i; 
        s_i.setHashOf(random_string(32));

        // S value
        G2 S_i; 
        G2::mul(S_i, g2_gen, s_i);
        S_values.push_back(S_i);
        
        // GBF
        gbfs.push_back(compute_gbf(set, bf_params, s_i, judge_pk));
    }
    auto client_stop = high_resolution_clock::now();
    *client_prep_time = duration<double, std::milli>(client_stop - client_start).count() / n_clients;

    // Online Phase
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
    *client_online_time = duration<double, std::milli>(leader_stop - leader_start).count();

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
    *server_computation_time = duration<double, std::milli>(server_stop - server_start).count();

    return intersection;
}