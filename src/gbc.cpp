#include "gbc.hpp"

#include <atomic>
#include <queue>
#include <thread>
#include <vector>

namespace
{

// Contribution of a single source `s` (assumed not in the group) to the ordered
// GBC sum: the total, over every valid target t, of the fraction of shortest
// s-t paths that pass through the group.
//
// One BFS computes, on the shortest-path DAG rooted at s:
//   sigma[v] = number of shortest s-v paths
//   avoid[v] = number of shortest s-v paths whose internal vertices avoid the group
// The group fraction for a target t is then  1 - avoid[t] / sigma[t].
double source_contribution(const CSR& g, const std::vector<char>& in_group, int s)
{
    const int n = g.v_count;
    std::vector<int> dist(n, -1);
    std::vector<unsigned long long> sigma(n, 0);
    std::vector<unsigned long long> avoid(n, 0);
    std::vector<int> order;  // vertices in non-decreasing distance order
    order.reserve(n);

    dist[s] = 0;
    sigma[s] = 1;
    avoid[s] = 1;  // s is not in the group, so the empty path "avoids" it

    std::queue<int> q;
    q.push(s);
    while (!q.empty())
    {
        int v = q.front();
        q.pop();
        order.push_back(v);
        for (long long e = g.row_ptr[v]; e < g.row_ptr[v + 1]; ++e)
        {
            int w = g.col_idx[e];
            if (dist[w] < 0)
            {
                dist[w] = dist[v] + 1;
                q.push(w);
            }
            if (dist[w] == dist[v] + 1)  // v is a predecessor of w on the DAG
            {
                sigma[w] += sigma[v];
                if (!in_group[v])  // a group vertex blocks the "avoid" flow
                    avoid[w] += avoid[v];
            }
        }
    }

    double contribution = 0.0;
    for (int t : order)
    {
        if (t == s || in_group[t])  // GBC excludes group members as endpoints
            continue;
        contribution += 1.0 - static_cast<double>(avoid[t]) / static_cast<double>(sigma[t]);
    }
    return contribution;
}

GBCResult finalize(double raw, int n, int group_size)
{
    GBCResult r;
    r.raw = raw;
    r.rescaled = raw / 2.0;  // undirected: every unordered pair is counted twice
    double pairs = static_cast<double>(n - group_size) * (n - group_size - 1);
    r.normalized = pairs > 0.0 ? raw / pairs : 0.0;
    return r;
}

}  // namespace

GBCResult gbc_serial(const CSR& graph, const std::vector<char>& in_group, int group_size)
{
    double raw = 0.0;
    for (int s = 0; s < graph.v_count; ++s)
    {
        if (in_group[s])  // sources in the group are excluded
            continue;
        raw += source_contribution(graph, in_group, s);
    }
    return finalize(raw, graph.v_count, group_size);
}

GBCResult gbc_parallel(const CSR& graph, const std::vector<char>& in_group, int group_size,
                       unsigned num_threads)
{
    if (num_threads == 0)
        num_threads = 1;

    std::atomic<int> next_source{0};
    std::vector<double> partial(num_threads, 0.0);

    // Dynamic work-stealing over sources: BFS cost varies per vertex, so a
    // shared atomic counter balances load better than a static split.
    auto worker = [&](unsigned tid) {
        double local = 0.0;
        int s;
        while ((s = next_source.fetch_add(1, std::memory_order_relaxed)) < graph.v_count)
        {
            if (in_group[s])
                continue;
            local += source_contribution(graph, in_group, s);
        }
        partial[tid] = local;
    };

    std::vector<std::thread> pool;
    pool.reserve(num_threads);
    for (unsigned t = 0; t < num_threads; ++t)
        pool.emplace_back(worker, t);
    for (auto& th : pool)
        th.join();

    double raw = 0.0;
    for (double p : partial)
        raw += p;
    return finalize(raw, graph.v_count, group_size);
}
