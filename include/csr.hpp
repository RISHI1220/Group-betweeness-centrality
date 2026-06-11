#ifndef GBC_CSR_HPP
#define GBC_CSR_HPP

#include <string>
#include <vector>

// Compressed Sparse Row (CSR) representation of an undirected graph.
//
// Neighbours of vertex v live in col_idx over the half-open index range
//   [ row_ptr[v], row_ptr[v + 1] ).
// row_ptr has v_count + 1 entries so the last vertex needs no special case,
// and isolated vertices simply get an empty range.
struct CSR
{
    int v_count = 0;                 // number of vertices
    long long e_count = 0;           // number of *directed* entries (2 * undirected edges)
    std::vector<long long> row_ptr;  // size v_count + 1
    std::vector<int> col_idx;        // size e_count
};

// Load a graph from `path`.
//
// File format:
//   line 1: vertex count
//   line 2: undirected edge count
//   remaining lines: one edge per line as "u,v" or "u v"
//
// Construction is order-independent and handles isolated vertices. If the file
// lists each undirected edge only once, the reverse direction is added
// automatically. Throws std::runtime_error on a missing/malformed file or an
// out-of-range endpoint.
CSR load_csr(const std::string& path);

#endif  // GBC_CSR_HPP
