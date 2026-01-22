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
    size_t set_size_clients,
    long set_size_server,
    long universe_size,
    std::vector<std::vector<long>>& client_sets,
    std::vector<long>& server_set
) {
    client_sets.clear();
    for (long i = 0; i < n_clients; ++i) {
        std::vector<long> client_set = sample_set(set_size_clients, universe_size);
        std::sort(client_set.begin(), client_set.end());
        client_sets.push_back(client_set);
    }

    server_set.clear();
    server_set = sample_set(set_size_server, universe_size);
    std::sort(server_set.begin(), server_set.end());
}

void benchmark(long repetitions, std::vector<long> number_of_parties_list, long set_size_clients, long set_size_server) {
    for (long t : number_of_parties_list) {
        long domain_size = static_cast<long>(std::ceil(set_size_server * 1.25));
        
        // Generate data
        std::vector<std::vector<std::vector<long>>> experiment_client_sets;
        std::vector<std::vector<long>> experiment_server_sets;
        for (int i = 0; i < repetitions; ++i) {
            std::vector<std::vector<long>> client_sets;
            std::vector<long> server_set;
            generate_clients_and_server_sets(t - 1, set_size_clients, set_size_server, domain_size, client_sets, server_set);
            experiment_client_sets.push_back(client_sets);
            experiment_server_sets.push_back(server_set);
        }

        BloomFilterParams params(set_size_server, -30); 
        Keys keys;
        // threshold t, parties n = t.
        key_gen(&keys, 1024, t, t); 

        std::cout << "\nBenchmarking " << t << " parties, ";
        std::cout << "Set size clients " << set_size_clients << ", Set size server " << set_size_server;
        std::cout << ", Domain size " << domain_size;
        std::cout << ", Params: m=" << params.bin_count << ", k=" << params.seeds.size() << std::endl;

        std::vector<long> client_prep_times;
        std::vector<long> client_online_times;
        std::vector<long> server_prep_times;
        std::vector<long> server_online_times;
        for (int i = 0; i < repetitions; ++i) {
            double client_prep_time = 0.0;
            double client_online_time = 0.0;
            double server_prep_time = 0.0;
            double server_online_time = 0.0;

            std::vector<long> result = multiparty_psi(
                experiment_client_sets[i], 
                experiment_server_sets[i], 
                params, 
                keys,
                &client_prep_time,
                &client_online_time,
                &server_prep_time,
                &server_online_time
            );

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
            client_prep_times.push_back(static_cast<long>(client_prep_time));
            client_online_times.push_back(static_cast<long>(client_online_time));
            server_prep_times.push_back(static_cast<long>(server_prep_time));
            server_online_times.push_back(static_cast<long>(server_online_time));                      
        }

        double mean = sample_mean(client_prep_times);
        double std_dev = sample_std(client_prep_times, mean);
        std::cout << "Client prep time (ms): mean " << std::fixed << mean << ", std dev " << std_dev << std::endl;
        
        mean = sample_mean(client_online_times);
        std_dev = sample_std(client_online_times, mean);
        std::cout << "Client online time (ms): mean " << std::fixed << mean << ", std dev " << std_dev << std::endl;
        
        mean = sample_mean(server_prep_times);
        std_dev = sample_std(server_prep_times, mean);
        std::cout << "Server prep time (ms): mean " << std::fixed << mean << ", std dev " << std_dev << std::endl;
        
        mean = sample_mean(server_online_times);
        std_dev = sample_std(server_online_times, mean);
        std::cout << "Server online time (ms): mean " << std::fixed << mean << ", std dev " << std_dev << std::endl;
    }
}