// Utility: read a graph edge list and emit it in the canonical format expected
// by this project — a "v_count / e_count" header followed by every directed
// edge (both directions), deduplicated and sorted by source then target.
//
// Accepts input whether each undirected edge is listed once or twice, with
// commas or whitespace as separators.
//
//   Usage: normalize-graph <input-file> [output-file]   (default output: stdout)
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <utility>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <input-file> [output-file]\n";
        return 1;
    }

    std::ifstream fin(argv[1]);
    if (!fin)
    {
        std::cerr << "Error: cannot open " << argv[1] << "\n";
        return 1;
    }

    int v_count = 0;
    long long edge_count = 0;  // header value; recomputed from the actual edges
    fin >> v_count >> edge_count;

    std::set<std::pair<int, int>> directed;  // sorted + deduplicated
    std::string line;
    std::getline(fin, line);  // finish the header line
    while (std::getline(fin, line))
    {
        for (char& c : line)
            if (c == ',')
                c = ' ';
        std::istringstream ss(line);
        int u, v;
        if (!(ss >> u >> v) || u == v)
            continue;
        directed.insert({u, v});
        directed.insert({v, u});  // enforce symmetry
    }

    std::ostream* out = &std::cout;
    std::ofstream fout;
    if (argc >= 3)
    {
        fout.open(argv[2]);
        if (!fout)
        {
            std::cerr << "Error: cannot write " << argv[2] << "\n";
            return 1;
        }
        out = &fout;
    }

    *out << v_count << "\n" << directed.size() / 2 << "\n";
    for (const auto& [u, v] : directed)
        *out << u << "," << v << "\n";

    std::cerr << "Wrote " << directed.size() << " directed edges (" << directed.size() / 2
              << " undirected).\n";
    return 0;
}
