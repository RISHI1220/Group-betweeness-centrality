#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "csr.hpp"
#include "gbc.hpp"

namespace
{

void usage(const char* prog)
{
    std::cerr
        << "Usage: " << prog << " <graph-file> [options]\n\n"
        << "Options:\n"
        << "  --group v1,v2,...   group vertices (if omitted, you are prompted)\n"
        << "  --threads N         worker threads for the parallel run (default: hardware)\n"
        << "  --mode MODE         both | serial | parallel (default: both)\n";
}

std::vector<int> parse_group(const std::string& csv)
{
    std::vector<int> members;
    std::stringstream ss(csv);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty())
            members.push_back(std::stoi(tok));
    return members;
}

void print_result(const std::string& label, const GBCResult& r, double seconds)
{
    std::cout << label << ":\n"
              << "  GBC (rescaled):   " << r.rescaled << "\n"
              << "  GBC (normalized): " << r.normalized << "\n"
              << "  time:             " << seconds << " s\n";
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        usage(argv[0]);
        return 1;
    }

    std::string graph_path = argv[1];
    std::string group_csv;
    std::string mode = "both";
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());

    for (int i = 2; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc)
            {
                usage(argv[0]);
                std::exit(1);
            }
            return argv[++i];
        };
        if (a == "--group")
            group_csv = next();
        else if (a == "--threads")
            threads = static_cast<unsigned>(std::max(1, std::stoi(next())));
        else if (a == "--mode")
            mode = next();
        else
        {
            std::cerr << "Unknown option: " << a << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    CSR graph;
    try
    {
        graph = load_csr(graph_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Graph: " << graph_path << "\n"
              << "  vertices:       " << graph.v_count << "\n"
              << "  directed edges: " << graph.e_count << " (" << graph.e_count / 2
              << " undirected)\n";

    // Resolve the group, either from --group or interactively.
    std::vector<int> members;
    if (group_csv.empty())
    {
        std::cout << "Enter group size: ";
        int gs = 0;
        if (!(std::cin >> gs) || gs < 0)
        {
            std::cerr << "Invalid group size.\n";
            return 1;
        }
        std::cout << "Enter " << gs << " vertex id(s):\n";
        for (int i = 0; i < gs; ++i)
        {
            int v;
            if (!(std::cin >> v))
            {
                std::cerr << "Invalid vertex id.\n";
                return 1;
            }
            members.push_back(v);
        }
    }
    else
    {
        members = parse_group(group_csv);
    }

    std::vector<char> in_group(graph.v_count, 0);
    for (int v : members)
    {
        if (v < 0 || v >= graph.v_count)
        {
            std::cerr << "Group vertex out of range: " << v << "\n";
            return 1;
        }
        in_group[v] = 1;
    }
    int group_size = 0;
    for (char c : in_group)
        group_size += c;

    if (graph.v_count - group_size < 2)
    {
        std::cerr << "Group leaves fewer than two non-group vertices; GBC is undefined.\n";
        return 1;
    }

    std::cout << "  group size:     " << group_size << "\n\n";
    std::cout << std::fixed << std::setprecision(6);

    bool run_serial = (mode == "both" || mode == "serial");
    bool run_parallel = (mode == "both" || mode == "parallel");
    if (!run_serial && !run_parallel)
    {
        std::cerr << "Invalid --mode: " << mode << "\n";
        return 1;
    }

    GBCResult serial_res, parallel_res;
    double serial_time = 0.0, parallel_time = 0.0;

    if (run_serial)
    {
        auto t0 = std::chrono::high_resolution_clock::now();
        serial_res = gbc_serial(graph, in_group, group_size);
        auto t1 = std::chrono::high_resolution_clock::now();
        serial_time = std::chrono::duration<double>(t1 - t0).count();
        print_result("Serial", serial_res, serial_time);
    }

    if (run_parallel)
    {
        if (run_serial)
            std::cout << "\n";
        auto t0 = std::chrono::high_resolution_clock::now();
        parallel_res = gbc_parallel(graph, in_group, group_size, threads);
        auto t1 = std::chrono::high_resolution_clock::now();
        parallel_time = std::chrono::duration<double>(t1 - t0).count();
        print_result("Parallel (" + std::to_string(threads) + " threads)", parallel_res,
                     parallel_time);
    }

    if (run_serial && run_parallel)
    {
        double speedup = parallel_time > 0.0 ? serial_time / parallel_time : 0.0;
        double diff = std::fabs(serial_res.rescaled - parallel_res.rescaled);
        std::cout << "\nSpeedup: " << speedup << "x\n"
                  << "Serial/parallel agree: " << (diff < 1e-6 ? "yes" : "NO")
                  << " (|diff| = " << diff << ")\n";
    }

    return 0;
}
