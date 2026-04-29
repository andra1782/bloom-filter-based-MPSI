#include "benchmarking.hpp" 
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include "mpsi_protocol.hpp" 

void print_set(const std::string& name, const std::vector<long>& set) {
    std::cout << name << ": { ";
    for (size_t i = 0; i < set.size(); ++i) {
        std::cout << set[i] << (i < set.size() - 1 ? ", " : "");
    }
    std::cout << " }" << std::endl;
}

std::vector<long> compute_intersection_non_private(const std::vector<std::vector<long>>& client_sets, 
                                                     const std::vector<long>& server_set) {
    if (client_sets.empty()) 
        return {};

    std::vector<long> current_intersection = client_sets[0];
    std::sort(current_intersection.begin(), current_intersection.end());
    
    for (size_t i = 1; i < client_sets.size(); ++i) {
        std::vector<long> next_set = client_sets[i];
        std::sort(next_set.begin(), next_set.end());
        
        std::vector<long> temp;
        std::set_intersection(current_intersection.begin(), current_intersection.end(),
                              next_set.begin(), next_set.end(),
                              std::back_inserter(temp));
        current_intersection = temp;
    }

    std::vector<long> server_sorted = server_set;
    std::sort(server_sorted.begin(), server_sorted.end());
    
    std::vector<long> final_intersection;
    std::set_intersection(current_intersection.begin(), current_intersection.end(),
                          server_sorted.begin(), server_sorted.end(),
                          std::back_inserter(final_intersection));
    
    return final_intersection;
}

std::vector<long> run_experiment(const std::vector<std::vector<long>>& client_sets, 
                    const std::vector<long>& server_set) {

    size_t max_set_size = server_set.size();
    for(const auto& set : client_sets) {
        if(set.size() > max_set_size) 
            max_set_size = set.size();
    }

    Keys keys;
    // t = clients + 1 (server)
    int t = client_sets.size() + 1;
    key_gen(&keys, 1024, t, t); 
    GT base_gt = setup_pairings();
    BloomFilterParams global_params(max_set_size, -10, keys.params.p, base_gt);
    mcl::bn::G2 judge_pk;
    mcl::bn::Fr judge_sk;
    setup_judge_keys(judge_pk, judge_sk);

    std::cout << "Params: " << "t=" << t
              << ", n=" << max_set_size 
              << ", m=" << global_params.bin_count 
              << ", k=" << global_params.seeds.size() << std::endl;

    double client_prep_time = 0.0;
    double client_online_time = 0.0;
    double server_computation_time = 0.0;
    double judge_computation_time = 0.0;
    size_t server_sent_bytes = 0;
    size_t server_received_bytes = 0;
    size_t client_sent_bytes = 0;
    size_t leader_client_sent_bytes = 0;
    size_t leader_client_received_bytes = 0;
    size_t judge_sent_bytes = 0;
    size_t judge_received_bytes = 0;

    std::vector<long> result = multiparty_psi(
        client_sets, 
        server_set, 
        global_params,
        keys,
        judge_pk,
        judge_sk,
        &client_prep_time,
        &client_online_time,
        &server_computation_time,
        &judge_computation_time,
        &server_sent_bytes,
        &server_received_bytes,
        &client_sent_bytes,
        &leader_client_sent_bytes,
        &leader_client_received_bytes,
        &judge_sent_bytes,
        &judge_received_bytes
    );
    std::cout << "Result: ";
    print_set("MPSI", result);

    std::cout << "Client prep time: " << client_prep_time << " ms";
    std::cout << ", Client online time: " << client_online_time << " ms";
    std::cout << ", Server computation time: " << server_computation_time << " ms";
    std::cout << ", Judge computation time: " << judge_computation_time << " ms";
    std::cout << "Server sent bytes: " << server_sent_bytes;
    std::cout << ", Server received bytes: " << server_received_bytes;
    std::cout << ", Client sent bytes: " << client_sent_bytes;
    std::cout << ", Leader Client sent bytes: " << leader_client_sent_bytes;
    std::cout << ", Leader Client received bytes: " << leader_client_received_bytes;
    std::cout << ", Judge sent bytes: " << judge_sent_bytes;
    std::cout << ", Judge received bytes: " << judge_received_bytes << std::endl;
    std::cout << std::endl;
    
    return result;
}