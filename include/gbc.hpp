#ifndef GBC_GBC_HPP
#define GBC_GBC_HPP

#include <vector>

#include "csr.hpp"

// Result of a Group Betweenness Centrality (GBC) computation for a group S.
//
// GBC(S) measures how many shortest paths between non-group vertices are routed
// through S:
//
//   raw = sum over ordered pairs (s, t), with s, t not in S and s != t, of
//             sigma_st(S) / sigma_st
//
// where sigma_st is the number of shortest s-t paths and sigma_st(S) is the
// number of those paths having at least one *internal* vertex in S.
struct GBCResult
{
    double raw = 0.0;         // ordered-pair sum (each undirected pair counted twice)
    double rescaled = 0.0;    // raw / 2 — the undirected GBC value
    double normalized = 0.0;  // raw / ((n - g)(n - g - 1)) — in [0, 1]
};

// `in_group[v] != 0` marks vertex v as a member of the group; `group_size` is
// the number of distinct members. Both functions compute the same value.
GBCResult gbc_serial(const CSR& graph, const std::vector<char>& in_group, int group_size);

GBCResult gbc_parallel(const CSR& graph, const std::vector<char>& in_group, int group_size,
                       unsigned num_threads);

#endif  // GBC_GBC_HPP
