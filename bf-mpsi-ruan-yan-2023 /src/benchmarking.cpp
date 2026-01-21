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

// https://github.com/jellevos/bitset_mpsi/blob/master/main.cpp
std::vector<long> sample_set(long set_size, long domain_size) {
    std::default_random_engine generator(rand());
    std::uniform_int_distribution<long> distribution(0, domain_size-1);

    std::vector<long> set;
    set.reserve(set_size);

    while (set.size() < set_size) {
        long element = distribution(generator);
        bool duplicate = false;

        for (long other_element : set) {
            if (element == other_element) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            set.push_back(element);
        }
    }

    return set;
}

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

void generate_clients_and_server_sets(
    long n_clients,
    size_t set_size,
    long universe_size,
    std::vector<std::vector<long>>& client_sets,
    std::vector<long>& server_set
) {
    client_sets.clear();
    for (long i = 0; i < n_clients; ++i) {
        std::vector<long> client_set = sample_set(set_size, universe_size);
        std::sort(client_set.begin(), client_set.end());
        client_sets.push_back(client_set);
    }

    server_set.clear();
    server_set = sample_set(set_size, universe_size);
    std::sort(server_set.begin(), server_set.end());
}

void benchmark(long repetitions, std::vector<long> number_of_parties_list, long set_size, double intersection_ratio) {
    for (long t : number_of_parties_list) {
        // Calculate the domain size to achieve the target intersection to set size ratio
        // D = n / (ratio)^(1/t)
        double exponent = 1.0 / t;
        double domain_size_double = set_size / std::pow(intersection_ratio, exponent);
        long domain_size = static_cast<long>(std::ceil(domain_size_double));
        
        // Generate data
        std::vector<std::vector<std::vector<long>>> experiment_client_sets;
        std::vector<std::vector<long>> experiment_server_sets;
        for (int i = 0; i < repetitions; ++i) {
            std::vector<std::vector<long>> client_sets;
            std::vector<long> server_set;
            generate_clients_and_server_sets(t - 1, set_size, domain_size, client_sets, server_set);
            experiment_client_sets.push_back(client_sets);
            experiment_server_sets.push_back(server_set);
        }

        BloomFilterParams params(set_size, -30); 
        Keys keys;
        // threshold t, parties n = t.
        key_gen(&keys, 1024, t, t); 

        std::cout << "\nBenchmarking " << t << " parties, Set size " << set_size << ", Domain size " << domain_size;
        std::cout << ", Params: m=" << params.bin_count << ", k=" << params.seeds.size();
        std::cout << ", Format: (Mean Time ms, Std Dev ms)" << ": " << std::endl;


        std::vector<long> times;
        for (int i = 0; i < repetitions; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            std::vector<long> result = multiparty_psi(
                experiment_client_sets[i], 
                experiment_server_sets[i], 
                params, 
                keys
            );
            auto stop = std::chrono::high_resolution_clock::now();

            std::vector<long> expected = compute_intersection_non_private(
                experiment_client_sets[i], 
                experiment_server_sets[i]
            );
            std::cout << "Expected size: " << expected.size() << ", MPSI size: " << result.size();
            if (result != expected) {
                std::vector<long> difference;
                std::set_difference(result.begin(), result.end(),
                    expected.begin(), expected.end(),
                    std::back_inserter(difference));
                std::cout << "; " << result.size() - expected.size() << " False positives: ";
                print_set("", difference);
            } else
                std::cout << std::endl;                       
            times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count());
        }

        double mean = sample_mean(times);
        double std_dev = sample_std(times, mean);
        std::cout << "(" << std::fixed << mean << ", " << std_dev << ")" << std::endl;
    }
}