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

    BloomFilterParams global_params(max_set_size, -30);

    Keys keys;
    // n = clients + 1 (server), t = n 
    int n = client_sets.size() + 1;
    key_gen(&keys, 1024, n, n); 

    // for(size_t i = 0; i < client_sets.size(); ++i) {
    //     print_set("Client " + std::to_string(i + 1), client_sets[i]);
    // }
    // print_set("Server", server_set);
    std::cout << "Params: n=" << max_set_size 
              << ", m=" << global_params.bin_count 
              << ", k=" << global_params.seeds.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<long> result = multiparty_psi(
        client_sets, 
        server_set, 
        global_params,
        keys
    );
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    print_set("Result", result);
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << std::endl;

    return result;
}

void run_random_experiment_and_compare(long num_clients, long set_size, long universe_size) {
    std::cout << "Random Experiment (Clients: " << num_clients 
              << ", Set Size: " << set_size << ")" << std::endl;

    std::vector<std::vector<long>> client_sets;
    for(size_t i = 0; i < num_clients; ++i) {
        std::vector<long> set;
        for(size_t j = 0; j < set_size; ++j)
            set.push_back(rand() % universe_size);
        std::sort(set.begin(), set.end());
        set.erase(std::unique(set.begin(), set.end()), set.end());
        client_sets.push_back(set);
    }

    std::vector<long> server_set;
    for(size_t j = 0; j < set_size; ++j) 
        server_set.push_back(rand() % universe_size);
    std::sort(server_set.begin(), server_set.end());
    server_set.erase(std::unique(server_set.begin(), server_set.end()), server_set.end());

    std::vector<long> expected = compute_intersection_non_private(client_sets, server_set);
    print_set("Expected Intersection (Non-Private)", expected);

    std::vector<long> actual = run_experiment(client_sets, server_set);
    
    std::sort(actual.begin(), actual.end());
    if (actual == expected) {
        std::cout << "SUCCESS" << std::endl;
    } else {
        std::cout << "FAILURE" << std::endl;
    }
}