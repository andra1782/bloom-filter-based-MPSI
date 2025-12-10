#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <string>
#include "mpsi_protocol.hpp"

void print_set(const std::string& name, const std::vector<size_t>& set) {
    std::cout << name << ": { ";
    for (size_t i = 0; i < set.size(); ++i) {
        std::cout << set[i] << (i < set.size() - 1 ? ", " : "");
    }
    std::cout << " }" << std::endl;
}

void run_experiment(const std::vector<std::vector<size_t>>& client_sets, 
                    const std::vector<size_t>& server_set) {

    size_t max_set_size = server_set.size();
    for(const auto& set : client_sets) {
        if(set.size() > max_set_size) max_set_size = set.size();
    }

    BloomFilterParams global_params(max_set_size, -30);

    Keys keys;
    key_gen(&keys, 1024, client_sets.size()); 

    for(size_t i = 0; i < client_sets.size(); ++i) {
        print_set("Client " + std::to_string(i + 1), client_sets[i]);
    }
    print_set("Server", server_set);
    std::cout << "[Params] n=" << max_set_size 
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
}

int main() {
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

    return 0;
}