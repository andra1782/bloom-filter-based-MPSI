#include "benchmarking.hpp" 
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <random>
#include <unistd.h>
#include <fstream>
#include <filesystem>
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

double sample_mean_computation(const std::vector<long>& measurements) {
    double sum = 0;
    for (long measurement : measurements)
        sum += measurement;
    return sum / measurements.size();
}

double sample_mean_communication(const std::vector<size_t>& measurements) {
    double sum = 0;
    for (size_t measurement : measurements)
        sum += measurement;
    return sum / measurements.size();
}

double sample_std_computation(const std::vector<long>& measurements, double mean) {
    if (measurements.size() <= 1) return 0.0;
    double sum = 0;
    for (long measurement : measurements) 
        sum += std::pow(measurement - mean, 2);
    return std::sqrt(sum / (measurements.size() - 1.0));
}

double sample_std_communication(const std::vector<size_t>& measurements, double mean) {
    if (measurements.size() <= 1) return 0.0;
    double sum = 0;
    for (size_t measurement : measurements) 
        sum += std::pow(measurement - mean, 2);
    return std::sqrt(sum / (measurements.size() - 1.0));
}

GT setup_pairings() {
    initPairing(); 

    // generator in G2
    G2 g2_gen;
    mapToG2(g2_gen, 1);

    // deterministic point in G1
    G1 g1_gen;
    Fp t;
    t.setHashOf("mpsi_base_generator");
    mapToG1(g1_gen, t);

    GT base_gt;
    pairing(base_gt, g1_gen, g2_gen);
    return base_gt;
}

void setup_judge_keys(mcl::bn::G2& judge_pk, mcl::bn::Fr& judge_sk) {
    judge_sk.setByCSPRNG();
    mcl::bn::G2 g2_gen;
    mapToG2(g2_gen, 1);
    mcl::bn::G2::mul(judge_pk, g2_gen, judge_sk);
}

void generate_clients_and_server_sets(
    long n_clients,
    size_t set_size_clients,
    long set_size_server,
    long universe_size,
    long forced_intersection_size,
    std::vector<std::vector<long>>& client_sets,
    std::vector<long>& server_set
) {
    std::vector<long> intersection = sample_set(forced_intersection_size, universe_size);

    client_sets.clear();
    for (long i = 0; i < n_clients; ++i) {
        std::vector<long> client_set = sample_set(set_size_clients  - intersection.size(), universe_size);
        client_set.insert(client_set.end(), intersection.begin(), intersection.end());
        std::sort(client_set.begin(), client_set.end());
        client_sets.push_back(client_set);
    }

    server_set.clear();
    server_set = sample_set(set_size_server - intersection.size(), universe_size);
    server_set.insert(server_set.end(), intersection.begin(), intersection.end());
    std::sort(server_set.begin(), server_set.end());
}

void benchmark(long repetitions, std::vector<long> number_of_parties_list, long set_size_clients, long set_size_server, int false_positive_exponent=-30) {
    long long domain_size = (1LL << 32) - 1;
    long forced_intersection_size = set_size_clients / 4;

    std::filesystem::create_directory("../data"); 
    std::ofstream comp_csv("../data/computation.csv");
    comp_csv << "Parties,Client Prep,Client Online,Server,Judge\n";
    std::ofstream comm_csv("../data/communication.csv");
    comm_csv << "Parties,Client Sent,Leader Client Sent,Leader Client Received,Server Sent,Server Received,Judge Sent,Judge Received\n";
    std::ofstream sim_csv("../data/simulation.csv");
    sim_csv << "Parties,LAN (2.5 GBps),125 MBps,25 MBps,6.25 MBps,625 KBps\n";
    std::ofstream fp_csv("../data/false_positives.csv");
    fp_csv << "Parties,R1,R2,R3,R4,R5,R6,R7,R8,R9,R10\n";

    for (long t : number_of_parties_list) {
        // Generate data
        std::vector<std::vector<std::vector<long>>> experiment_client_sets;
        std::vector<std::vector<long>> experiment_server_sets;
        for (int i = 0; i < repetitions; ++i) {
            std::vector<std::vector<long>> client_sets;
            std::vector<long> server_set;
            generate_clients_and_server_sets(t - 1, set_size_clients, set_size_server, 
                domain_size, forced_intersection_size, client_sets, server_set);
            experiment_client_sets.push_back(client_sets);
            experiment_server_sets.push_back(server_set);
        }

        // setup keys and params
        Keys keys;
        key_gen(&keys, 1024, t, t); // threshold t, parties n = t.
        GT base_gt = setup_pairings();
        BloomFilterParams params(set_size_clients, false_positive_exponent, keys.params.p, base_gt); 
        mcl::bn::G2 judge_pk;
        mcl::bn::Fr judge_sk;
        setup_judge_keys(judge_pk, judge_sk);

        std::cout << "\nBenchmarking " << t << " parties, ";
        std::cout << "Set size clients " << set_size_clients << ", Set size server " << set_size_server;
        std::cout << ", Domain size " << domain_size;
        std::cout << ", Params: m=" << params.bin_count << ", k=" << params.seeds.size() << std::endl;
        fp_csv << t;

        std::vector<long> client_prep_times;
        std::vector<long> client_online_times;
        std::vector<long> server_computation_times;
        std::vector<long> judge_computation_times;
        std::vector<size_t> server_sent_bytes_all;
        std::vector<size_t> server_received_bytes_all;
        std::vector<size_t> client_sent_bytes_all;
        std::vector<size_t> leader_client_sent_bytes_all;
        std::vector<size_t> leader_client_received_bytes_all;
        std::vector<size_t> judge_sent_bytes_all;
        std::vector<size_t> judge_received_bytes_all;

        for (int i = 0; i < repetitions; ++i) {
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
                experiment_client_sets[i], 
                experiment_server_sets[i], 
                params, 
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

            std::vector<long> expected = compute_intersection_non_private(
                experiment_client_sets[i], 
                experiment_server_sets[i]
            );
            
            std::cout << "Expected size: " << expected.size() << ", MPSI size: " << result.size();
            long fp_count = result.size() - expected.size();
            fp_csv << "," << fp_count;

            if (result != expected) {
                std::vector<long> difference;
                std::set_difference(result.begin(), result.end(),
                    expected.begin(), expected.end(),
                    std::back_inserter(difference));
                std::cout << "; " << fp_count << " False positives";
                print_set("", difference);
            } else {
                std::cout << std::endl; 
            }

            client_prep_times.push_back(static_cast<long>(client_prep_time));
            client_online_times.push_back(static_cast<long>(client_online_time));
            server_computation_times.push_back(static_cast<long>(server_computation_time));
            judge_computation_times.push_back(static_cast<long>(judge_computation_time));
            server_sent_bytes_all.push_back(server_sent_bytes);
            server_received_bytes_all.push_back(server_received_bytes);
            client_sent_bytes_all.push_back(client_sent_bytes);
            leader_client_sent_bytes_all.push_back(leader_client_sent_bytes);
            leader_client_received_bytes_all.push_back(leader_client_received_bytes);
            judge_sent_bytes_all.push_back(judge_sent_bytes);
            judge_received_bytes_all.push_back(judge_received_bytes);
        }
        fp_csv << "\n";

        double mean_client_prep = sample_mean_computation(client_prep_times);
        double std_dev = sample_std_computation(client_prep_times, mean_client_prep);
        std::cout << "Client prep time (ms): mean " << std::fixed << mean_client_prep << ", std dev " << std_dev << std::endl;

        double mean_client_online = sample_mean_computation(client_online_times);
        std_dev = sample_std_computation(client_online_times, mean_client_online);
        std::cout << "Client online time (ms): mean " << std::fixed << mean_client_online << ", std dev " << std_dev << std::endl;

        double mean_server_computation = sample_mean_computation(server_computation_times);
        std_dev = sample_std_computation(server_computation_times, mean_server_computation);
        std::cout << "Server computation time (ms): mean " << std::fixed << mean_server_computation << ", std dev " << std_dev << std::endl;
        
        double mean_judge_computation = sample_mean_computation(judge_computation_times);
        std_dev = sample_std_computation(judge_computation_times, mean_judge_computation);
        std::cout << "Judge computation time (ms): mean " << std::fixed << mean_judge_computation << ", std dev " << std_dev << std::endl;
        
        double mean_server_sent = sample_mean_communication(server_sent_bytes_all);
        std_dev = sample_std_communication(server_sent_bytes_all, mean_server_sent);
        std::cout << "Server sent bytes: mean " << std::fixed << mean_server_sent << ", std dev " << std_dev << std::endl;

        double mean_server_received = sample_mean_communication(server_received_bytes_all);
        std_dev = sample_std_communication(server_received_bytes_all, mean_server_received);
        std::cout << "Server received bytes: mean " << std::fixed << mean_server_received << ", std dev " << std_dev << std::endl;

        double mean_client_sent = sample_mean_communication(client_sent_bytes_all);
        std_dev = sample_std_communication(client_sent_bytes_all, mean_client_sent);
        std::cout << "Client sent bytes: mean " << std::fixed << mean_client_sent << ", std dev " << std_dev << std::endl;

        double mean_leader_client_sent = sample_mean_communication(leader_client_sent_bytes_all);
        std_dev = sample_std_communication(leader_client_sent_bytes_all, mean_leader_client_sent);
        std::cout << "Leader Client sent bytes: mean " << std::fixed << mean_leader_client_sent << ", std dev " << std_dev << std::endl;

        double mean_leader_client_received = sample_mean_communication(leader_client_received_bytes_all);
        std_dev = sample_std_communication(leader_client_received_bytes_all, mean_leader_client_received);
        std::cout << "Leader Client received bytes: mean " << std::fixed << mean_leader_client_received << ", std dev " << std_dev << std::endl;

        double mean_judge_sent = sample_mean_communication(judge_sent_bytes_all);
        std_dev = sample_std_communication(judge_sent_bytes_all, mean_judge_sent);
        std::cout << "Judge sent bytes: mean " << std::fixed << mean_judge_sent << ", std dev " << std_dev << std::endl;

        double mean_judge_received = sample_mean_communication(judge_received_bytes_all);
        std_dev = sample_std_communication(judge_received_bytes_all, mean_judge_received);
        std::cout << "Judge received bytes: mean " << std::fixed << mean_judge_received << ", std dev " << std_dev << std::endl;
        
        comp_csv << t << "," 
                << mean_client_prep << ","   
                << mean_client_online << "," 
                << mean_server_computation << "," 
                << mean_judge_computation << "\n";

        comm_csv << t << "," 
                << mean_client_sent << "," 
                << mean_leader_client_sent << "," 
                << mean_leader_client_received << "," 
                << mean_server_sent << "," 
                << mean_server_received << ","
                << mean_judge_sent << "," 
                << mean_judge_received << "\n";

        // Network Simulation 
        size_t bandwidth_lan = 2500000000; // 2.5 GBps
        size_t bandwidth_0 = 125000000; // 125 MBps
        size_t bandwidth_1 = 25000000; // 25 MBps
        size_t bandwidth_2 = 6250000; // 6.25 MBps
        size_t bandwidth_3 = 625000; // 625 KBps
        
        const double LATENCY_LAN = 0.1; // ms
        const double LATENCY_WAN = 80.0; // ms
        const size_t MESSAGES = 5;

        double total_comp_time = mean_client_prep + mean_client_online + mean_server_computation + mean_judge_computation;
        double bandwidth_total_bytes = (mean_server_sent + mean_server_received + mean_client_sent + mean_leader_client_sent + mean_leader_client_received + mean_judge_sent + mean_judge_received) / 2;
        std::cout << "\nTotal Computation Time (ms): " << total_comp_time << std::endl;
        std::cout << "Total Communication (bytes): " << bandwidth_total_bytes << std::endl;

        auto calc_net_time = [&](double latency_ms, double bandwidth_bps) {
            // latency + communication_time + computation_time
            return (latency_ms * MESSAGES) + (bandwidth_total_bytes / bandwidth_bps)  + total_comp_time;
        };

        size_t messages = t - 1 + 3; // 2 for authorize, t - 1 to send to leader, 1 leader to server
        size_t time_lan = calc_net_time(LATENCY_LAN, bandwidth_lan);
        double time_network_0 = calc_net_time(LATENCY_WAN, bandwidth_0);
        double time_network_1 = calc_net_time(LATENCY_WAN, bandwidth_1);
        double time_network_2 = calc_net_time(LATENCY_WAN, bandwidth_2);
        double time_network_3 = calc_net_time(LATENCY_WAN, bandwidth_3);

        std::cout << "\nSimulated Total Times (Communication + Computation):" << std::endl;
        std::cout << "Bandwidth 2.5 GBps, Latency " << time_lan << " ms (LAN)" << std::endl; 
        std::cout << "Bandwidth 125 MBps, Latency " << time_network_0 << " ms (1 Gbps)" << std::endl;
        std::cout << "Bandwidth 25 MBps, Latency " << time_network_1 << " ms (200 Mbps)" << std::endl;
        std::cout << "Bandwidth 6.25 MBps, Latency " << time_network_2 << " ms (20 Mbps)" << std::endl;
        std::cout << "Bandwidth 625 KBps, Latency " << time_network_3 << " ms (5 Mbps)" << std::endl;

        sim_csv << t << "," 
                << time_lan << "," 
                << time_network_0 << "," 
                << time_network_1 << ","
                << time_network_2 << "," 
                << time_network_3 << "\n";
    }
}