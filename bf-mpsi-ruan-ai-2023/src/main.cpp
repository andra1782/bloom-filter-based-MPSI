#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <string>
#include <algorithm>
#include <random>
#include "mpsi_protocol.hpp"
#include "benchmarking.hpp"

void print_set(const std::string& name, const std::vector<size_t>& set) {
    std::cout << name << ": { ";
    for (size_t i = 0; i < set.size(); ++i) {
        std::cout << set[i] << (i < set.size() - 1 ? ", " : "");
    }
    std::cout << " }" << std::endl;
}

std::vector<size_t> compute_intersection_non_private(const std::vector<std::vector<size_t>>& client_sets, 
                                                     const std::vector<size_t>& server_set) {
    if (client_sets.empty()) 
        return {};

    std::vector<size_t> current_intersection = client_sets[0];
    std::sort(current_intersection.begin(), current_intersection.end());
    
    for (size_t i = 1; i < client_sets.size(); ++i) {
        std::vector<size_t> next_set = client_sets[i];
        std::sort(next_set.begin(), next_set.end());
        
        std::vector<size_t> temp;
        std::set_intersection(current_intersection.begin(), current_intersection.end(),
                              next_set.begin(), next_set.end(),
                              std::back_inserter(temp));
        current_intersection = temp;
    }

    std::vector<size_t> server_sorted = server_set;
    std::sort(server_sorted.begin(), server_sorted.end());
    
    std::vector<size_t> final_intersection;
    std::set_intersection(current_intersection.begin(), current_intersection.end(),
                          server_sorted.begin(), server_sorted.end(),
                          std::back_inserter(final_intersection));
    
    return final_intersection;
}

std::vector<size_t> run_experiment(const std::vector<std::vector<size_t>>& client_sets, 
                    const std::vector<size_t>& server_set) {

    size_t max_set_size = server_set.size();
    for(const auto& set : client_sets) {
        if(set.size() > max_set_size) 
            max_set_size = set.size();
    }

    BloomFilterParams global_params(max_set_size, -30);

    Keys keys;
    key_gen(&keys, 1024, client_sets.size()); 

    for(size_t i = 0; i < client_sets.size(); ++i) {
        print_set("Client " + std::to_string(i + 1), client_sets[i]);
    }
    print_set("Server", server_set);
    std::cout << "Params: n=" << max_set_size 
              << ", m=" << global_params.bin_count 
              << ", k=" << global_params.seeds.size() << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<size_t> result = multiparty_psi(
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

void run_random_experiment_and_compare(size_t num_clients, size_t set_size, size_t universe_size) {
    std::cout << "Random Experiment (Clients: " << num_clients 
              << ", Set Size: " << set_size << ")" << std::endl;

    std::vector<std::vector<size_t>> client_sets;
    for(size_t i = 0; i < num_clients; ++i) {
        std::vector<size_t> set;
        for(size_t j = 0; j < set_size; ++j)
            set.push_back(rand() % universe_size);
        std::sort(set.begin(), set.end());
        set.erase(std::unique(set.begin(), set.end()), set.end());
        client_sets.push_back(set);
    }

    std::vector<size_t> server_set;
    for(size_t j = 0; j < set_size; ++j) 
        server_set.push_back(rand() % universe_size);
    std::sort(server_set.begin(), server_set.end());
    server_set.erase(std::unique(server_set.begin(), server_set.end()), server_set.end());

    std::vector<size_t> expected = compute_intersection_non_private(client_sets, server_set);
    print_set("Expected Intersection (Non-Private)", expected);

    std::vector<size_t> actual = run_experiment(client_sets, server_set);
    
    std::sort(actual.begin(), actual.end());
    if (actual == expected) {
        std::cout << "SUCCESS" << std::endl;
    } else {
        std::cout << "FAILURE" << std::endl;
    }
}

int main() {
    srand(time(NULL));

    run_experiment({ {1, 2, 3}, {1, 3, 4} }, // Clients
        {1, 3, 5} // Server
    );

    run_experiment({ {10, 11}, {12, 13} }, // Clients
        {14, 15} // Server
    );

    run_experiment({ {7, 8, 9}, {7, 8, 9} }, // Clients
        {7, 8, 9} // Server
    );

    run_experiment({ 
          {1, 2, 3, 4, 5}, // Client 1
          {5, 6, 7, 8, 9}, // Client 2
          {2, 5, 8, 10, 12} // Client 3
        },
        {5, 12, 100, 200} // Server
    );

    run_random_experiment_and_compare(10, 40, 50);

    std::cout << "\nRunning benchmarks: ";
    // 2, 5 and 10 parties, set sizes 2^4, 2^6 and 2^8
    benchmark({2, 5, 10}, {4, 6, 8});

    return 0;
}