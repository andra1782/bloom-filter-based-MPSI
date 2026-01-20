#ifndef EXPERIMENTS_HPP
#define EXPERIMENTS_HPP

#include <vector>

void print_set(const std::string& name, const std::vector<size_t>& set);
std::vector<size_t> compute_intersection_non_private(const std::vector<std::vector<size_t>>& client_sets, 
                                                     const std::vector<size_t>& server_set);
std::vector<size_t> run_experiment(const std::vector<std::vector<size_t>>& client_sets, 
                    const std::vector<size_t>& server_set);
void run_random_experiment_and_compare(size_t num_clients, size_t set_size, size_t universe_size);

#endif

