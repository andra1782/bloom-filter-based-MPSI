#include "benchmarking.hpp" 
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include <unistd.h>
#include "mpsi_protocol.hpp" 
#include "experiments.hpp"

double sample_mean(const std::vector<long>& measurements) {
    double sum = 0;
    for (long measurement : measurements) {
        sum += measurement;
    }
    return sum / measurements.size();
}

double sample_std(const std::vector<long>& measurements, double mean) {
    if (measurements.size() <= 1) return 0.0;
    double sum = 0;
    for (long measurement : measurements) {
        sum += std::pow(measurement - mean, 2);
    }
    return std::sqrt(sum / (measurements.size() - 1.0));
}

void benchmark(std::vector<long> parties_list, std::vector<long> set_size_exponents) {
    size_t universe_size = 256; 

    for (long t : parties_list) {
        std::cout << "\nBenchmarking " << t << " parties" << std::endl;
        std::cout << "Format: (Mean Time ms, Std Dev ms)" << std::endl;
        
        for (long exp : set_size_exponents) {
            std::cout << "Results for set size 2^" << exp << ": ";
            size_t set_size = (1 << exp); 
            
            std::vector<std::vector<std::vector<size_t>>> experiment_client_sets;
            std::vector<std::vector<size_t>> experiment_server_sets;

            // Generate data for 10 trials
            for (int i = 0; i < 10; ++i) {
                std::vector<std::vector<size_t>> client_sets;
                // t = #parties, so num_clients = t - 1
                for (int j = 0; j < t - 1; ++j) {
                    std::vector<size_t> set;
                    set.reserve(set_size);
                    for (size_t k = 0; k < set_size; ++k) set.push_back(rand() % universe_size);
                    std::sort(set.begin(), set.end());
                    set.erase(std::unique(set.begin(), set.end()), set.end());
                    client_sets.push_back(set);
                }
                experiment_client_sets.push_back(client_sets);

                std::vector<size_t> s_set;
                s_set.reserve(set_size);
                for (size_t k = 0; k < set_size; ++k) s_set.push_back(rand() % universe_size);
                std::sort(s_set.begin(), s_set.end());
                s_set.erase(std::unique(s_set.begin(), s_set.end()), s_set.end());
                experiment_server_sets.push_back(s_set);
            }

            BloomFilterParams params(set_size, -30);
            Keys keys;
            // threshold t, parties n = t.
            key_gen(&keys, 1024, t, t); 

            std::vector<long> times;
            for (int i = 0; i < 10; ++i) {

                auto start = std::chrono::high_resolution_clock::now();
                // std::cout << "CC " << experiment_client_sets[i][0].size() << " " << experiment_server_sets[i].size() << std::endl;
                // for (size_t c = 0; c < experiment_client_sets[i].size(); ++c) {
                //     std::cout << "Client " << c << ": ";
                //     for( size_t v = 0; v < experiment_client_sets[i][c].size(); ++v)
                //         std::cout << experiment_client_sets[i][c][v] << " ";
                //     std::cout << std::endl;
                // }
                // std::cout << "Server: ";
                // for(size_t v = 0; v < experiment_server_sets[i].size(); ++v)
                //     std::cout << experiment_server_sets[i][v] << " ";
                // std::cout << std::endl;
                for(size_t c = 0; c < experiment_client_sets[i].size(); ++c) {
                    std::cout << "Client " << c << " set size: " << experiment_client_sets[i][c].size() << ", ";
                }
                std::cout << "Server set size: " << experiment_server_sets[i].size() << std::endl;

                std::vector<size_t> result = multiparty_psi(experiment_client_sets[i], experiment_server_sets[i], params, keys);
                std::vector<size_t> expected = compute_intersection_non_private(experiment_client_sets[i], experiment_server_sets[i]);
                std::cout << "Expected size: " << expected.size() << ", MPSI size: " << result.size() << std::endl;
                if (result != expected) {
                    std::cout << "Error: MPSI result does not match expected intersection! "
                         << result.size() - expected.size() << " false positives." << std::endl;
                    print_set("MPSI Result", result);
                    print_set("Expected", expected); 
                }
                auto stop = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
            }

            double mean = sample_mean(times);
            double std_dev = sample_std(times, mean);
            std::cout << "(" << std::fixed << mean << ", " << std_dev << ")" << std::endl;
        }
    }
}