#include "benchmarking.hpp" 
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include "mpsi_protocol.hpp" 

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
    size_t universe_size = 1000000; 

    for (long t : parties_list) {
        std::cout << "\nBenchmarking " << t << " parties" << std::endl;
        std::cout << "Format: (Mean Time ms, Std Dev ms)" << std::endl;
        
        for (long exp : set_size_exponents) {
            std::cout << "Results for set size 2^" << exp << ": ";
            size_t set_size = 1 << exp; 
            
            std::vector<std::vector<std::vector<size_t>>> experiment_client_sets;
            std::vector<std::vector<size_t>> experiment_server_sets;
            
            for (int i = 0; i < 10; ++i) { // 10 iterations
                std::vector<std::vector<size_t>> client_sets;
                for (int j = 0; j < t; ++j) {
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
            key_gen(&keys, 1024, t); 

            std::vector<long> times;
            for (int i = 0; i < 10; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                multiparty_psi(experiment_client_sets[i], experiment_server_sets[i], params, keys);
                auto stop = std::chrono::high_resolution_clock::now();
                times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
            }

            double mean = sample_mean(times);
            double std = sample_std(times, mean);

            std::cout << "(" << mean << ", " << std << "),\n";
            std::cout.flush(); 
        }
    }
}