#include "csr.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

CSR load_csr(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin)
        throw std::runtime_error("cannot open graph file: " + path);

    int v_count = 0;
    long long undirected_edges = 0;
    if (!(fin >> v_count >> undirected_edges) || v_count < 0)
        throw std::runtime_error("malformed header in: " + path);

    // Read every directed entry. Separators may be commas or whitespace.
    std::vector<std::pair<int, int>> edges;
    edges.reserve(static_cast<std::size_t>(undirected_edges) * 2);

    std::string line;
    std::getline(fin, line);  // consume the remainder of the header line
    while (std::getline(fin, line))
    {
        for (char& c : line)
            if (c == ',')
                c = ' ';
        std::istringstream ss(line);
        int u, v;
        if (!(ss >> u >> v))
            continue;  // skip blank lines / trailing junk
        if (u < 0 || u >= v_count || v < 0 || v >= v_count)
            throw std::runtime_error("edge endpoint out of range in: " + path);
        edges.emplace_back(u, v);
    }

    // If the file listed each undirected edge only once, add the reverse so the
    // graph is symmetric for traversal.
    if (static_cast<long long>(edges.size()) == undirected_edges)
    {
        std::size_t original = edges.size();
        for (std::size_t i = 0; i < original; ++i)
            edges.emplace_back(edges[i].second, edges[i].first);
    }

    CSR g;
    g.v_count = v_count;
    g.e_count = static_cast<long long>(edges.size());

    // Counting-sort style construction: degree histogram -> prefix sums -> fill.
    g.row_ptr.assign(v_count + 1, 0);
    for (const auto& [u, v] : edges)
        g.row_ptr[u + 1]++;
    for (int i = 0; i < v_count; ++i)
        g.row_ptr[i + 1] += g.row_ptr[i];

    g.col_idx.resize(static_cast<std::size_t>(g.e_count));
    std::vector<long long> cursor(g.row_ptr);  // running insert position per vertex
    for (const auto& [u, v] : edges)
        g.col_idx[cursor[u]++] = v;

    return g;
}
