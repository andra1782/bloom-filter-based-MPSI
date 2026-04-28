#ifndef EXPERIMENTS_HPP
#define EXPERIMENTS_HPP

#include <vector>

void print_set(const std::string& name, const std::vector<long>& set);
std::vector<long> compute_intersection_non_private(const std::vector<std::vector<long>>& client_sets, 
                                                     const std::vector<long>& server_set);
std::vector<long> run_experiment(const std::vector<std::vector<long>>& client_sets, 
                    const std::vector<long>& server_set);
void run_random_experiment_and_compare(long num_clients, long set_size, long universe_size);

#endif