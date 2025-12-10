#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include "mpsi_protocol.hpp"

void print_set(const std::string& name, const std::vector<size_t>& set) {
    std::cout << name << ": { ";
    for (size_t i = 0; i < set.size(); ++i) {
        std::cout << set[i] << (i < set.size() - 1 ? ", " : "");
    }
    std::cout << " }" << std::endl;
}

int main() {
    size_t num_clients = 2;
    std::vector<size_t> client1_set = {1, 2, 3};
    std::vector<size_t> client2_set = {1, 3, 4};
    std::vector<size_t> server_set = {1, 3, 5};
    std::vector<std::vector<size_t>> client_sets = {client1_set, client2_set};

    size_t max_set_size = server_set.size();
    for(const auto& set : client_sets) {
        if(set.size() > max_set_size) max_set_size = set.size();
    }

    long e_pow = -30; // false positive rate = 1 / 2^30
    BloomFilterParams global_params(max_set_size, e_pow);
    size_t m_bits = global_params.bin_count;
    size_t k_hashes = global_params.seeds.size();

    std::cout << "Bloom Filter params: ";
    std::cout << "n (capacity) = " << max_set_size;
    std::cout << ", m (bits) = " << global_params.bin_count;
    std::cout << ", k (hashes) = " << global_params.seeds.size() << std::endl;

    long key_bits = 1024;
    Keys keys;
    key_gen(&keys, key_bits, num_clients); 

    print_set("Client 1", client1_set);
    print_set("Client 2", client2_set);
    print_set("Server", server_set);

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<size_t> result = multiparty_psi(
        client_sets, 
        server_set, 
        global_params,
        keys
    );
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);

    print_set("Calculated Intersection", result);
    std::cout << "Total Time: " << duration.count() << " ms" << std::endl;

    return 0;
}