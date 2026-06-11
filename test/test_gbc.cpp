// Correctness test: cross-checks the optimized GBC implementation against an
// independent brute-force reference on small graphs, and verifies that the
// serial and parallel paths agree.
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <vector>

#include "csr.hpp"
#include "gbc.hpp"

namespace
{

std::vector<int> bfs_dist(const CSR& g, int s)
{
    std::vector<int> dist(g.v_count, -1);
    std::queue<int> q;
    dist[s] = 0;
    q.push(s);
    while (!q.empty())
    {
        int v = q.front();
        q.pop();
        for (long long e = g.row_ptr[v]; e < g.row_ptr[v + 1]; ++e)
        {
            int w = g.col_idx[e];
            if (dist[w] < 0)
            {
                dist[w] = dist[v] + 1;
                q.push(w);
            }
        }
    }
    return dist;
}

// Independent path counter: memoized recursion over predecessors on the DAG.
// When `avoid` is set, steps through a group vertex are forbidden, so the count
// is the number of shortest paths whose internal vertices skip the group.
double count_paths(const CSR& g, const std::vector<int>& dist, int s, int t,
                   const std::vector<char>* avoid, std::vector<double>& memo)
{
    if (t == s)
        return 1.0;
    if (memo[t] >= 0.0)
        return memo[t];
    double total = 0.0;
    for (long long e = g.row_ptr[t]; e < g.row_ptr[t + 1]; ++e)
    {
        int u = g.col_idx[e];
        if (dist[u] == dist[t] - 1)
        {
            if (avoid && (*avoid)[u])
                continue;  // path would pass through a group vertex
            total += count_paths(g, dist, s, u, avoid, memo);
        }
    }
    memo[t] = total;
    return total;
}

// Reference GBC (rescaled), computed pair-by-pair with the recursion above.
double brute_gbc_rescaled(const CSR& g, const std::vector<char>& in_group)
{
    double raw = 0.0;
    for (int s = 0; s < g.v_count; ++s)
    {
        if (in_group[s])
            continue;
        std::vector<int> dist = bfs_dist(g, s);
        for (int t = 0; t < g.v_count; ++t)
        {
            if (t == s || in_group[t] || dist[t] < 0)
                continue;
            std::vector<double> memo_total(g.v_count, -1.0), memo_avoid(g.v_count, -1.0);
            double total = count_paths(g, dist, s, t, nullptr, memo_total);
            double avoided = count_paths(g, dist, s, t, &in_group, memo_avoid);
            raw += 1.0 - avoided / total;
        }
    }
    return raw / 2.0;
}

int failures = 0;

void check(const CSR& g, const std::string& name, const std::vector<int>& members)
{
    std::vector<char> in_group(g.v_count, 0);
    int group_size = 0;
    for (int v : members)
        if (!in_group[v])
        {
            in_group[v] = 1;
            ++group_size;
        }

    double reference = brute_gbc_rescaled(g, in_group);
    GBCResult serial = gbc_serial(g, in_group, group_size);
    GBCResult parallel = gbc_parallel(g, in_group, group_size, 4);

    bool ok_ref = std::fabs(serial.rescaled - reference) < 1e-6;
    bool ok_par = std::fabs(serial.rescaled - parallel.rescaled) < 1e-6;

    std::cout << (ok_ref && ok_par ? "  PASS  " : "  FAIL  ") << name << " group {";
    for (size_t i = 0; i < members.size(); ++i)
        std::cout << members[i] << (i + 1 < members.size() ? "," : "");
    std::cout << "}  serial=" << serial.rescaled << " brute=" << reference
              << " parallel=" << parallel.rescaled << "\n";

    if (!ok_ref || !ok_par)
        ++failures;
}

void run_random_groups(const std::string& path, const std::string& name, int trials)
{
    CSR g = load_csr(path);
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> vertex(0, g.v_count - 1);
    std::uniform_int_distribution<int> size(1, 3);

    for (int i = 0; i < trials; ++i)
    {
        std::vector<int> members;
        int k = size(rng);
        for (int j = 0; j < k; ++j)
            members.push_back(vertex(rng));
        check(g, name, members);
    }
}

}  // namespace

int main()
{
    std::cout.setf(std::ios::fixed);
    std::cout.precision(6);

    // data4: the tiny hand-traceable graph.
    {
        CSR g = load_csr("datas/data4.txt");
        check(g, "data4", {2});
        check(g, "data4", {5});
        check(g, "data4", {2, 5});
        check(g, "data4", {0, 6});
    }

    // Larger small graphs with random groups.
    run_random_groups("datas/data4.txt", "data4", 10);
    run_random_groups("datas/dolphins.txt", "dolphins", 15);

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << " (" << failures << " failure(s))\n";
    return failures == 0 ? 0 : 1;
}
