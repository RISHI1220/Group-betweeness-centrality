# Group Betweenness Centrality

Compute the **Group Betweenness Centrality (GBC)** of a set of vertices in an
undirected graph, with both a **serial** and a **multithreaded** implementation
for side-by-side comparison.

Given a group of vertices *S*, GBC measures how many shortest paths between the
remaining vertices are routed *through* the group:

```
GBC(S) = Σ  σ_st(S) / σ_st       over ordered pairs (s, t), s, t ∉ S, s ≠ t
```

where `σ_st` is the number of shortest *s*–*t* paths and `σ_st(S)` is how many of
them use at least one **internal** vertex from *S*.

## Algorithm

For every source `s ∉ S`, a single BFS over the shortest-path DAG computes:

| quantity   | meaning                                                            |
|------------|-------------------------------------------------------------------|
| `σ[v]`     | number of shortest `s → v` paths                                  |
| `avoid[v]` | number of those paths whose internal vertices **avoid** the group |

`avoid` is accumulated like `σ`, except flow through a group vertex is blocked.
The fraction of shortest `s → t` paths passing through the group is then simply
`1 − avoid[t] / σ[t]`, summed over all valid targets. This is `O(V·(V+E))`
overall — the same complexity as Brandes' betweenness — and exact.

The result is reported three ways:

- **rescaled** — the undirected GBC value (`raw / 2`, since each unordered pair
  is counted from both ends).
- **normalized** — `raw / ((n−g)(n−g−1))`, in `[0, 1]`.

## Layout

```
include/        csr.hpp, gbc.hpp           public interfaces
src/csr.cpp     graph loading into CSR
src/gbc.cpp     serial + parallel GBC core (shared)
src/main.cpp    command-line runner
src/normalize_graph.cpp   dataset normalizer utility
test/test_gbc.cpp         brute-force correctness suite
datas/          sample graphs (dolphins, football, facebook, wiki, …)
```

The graph is held in **Compressed Sparse Row** form; construction is
order-independent (counting sort) and handles isolated vertices.

## Build

Requires a C++17 compiler.

```sh
make            # builds build/gbc, build/normalize-graph, build/test-gbc
make test       # runs the correctness suite
make run        # quick demo on datas/data4.txt
make clean
```

## Usage

```sh
./build/gbc <graph-file> [options]

  --group v1,v2,...   group vertices (if omitted, you are prompted)
  --threads N         worker threads for the parallel run (default: hardware)
  --mode MODE         both | serial | parallel  (default: both)
```

Example:

```sh
./build/gbc datas/facebook.txt --group 0,107,1684 --threads 8
```

```
Graph: datas/facebook.txt
  vertices:       4039
  directed edges: 176468 (88234 undirected)
  group size:     3

Serial:
  GBC (rescaled):   5558107.654712
  GBC (normalized): 0.682594
  time:             0.865980 s

Parallel (8 threads):
  GBC (rescaled):   5558107.654712
  GBC (normalized): 0.682594
  time:             0.164655 s

Speedup: 5.259350x
Serial/parallel agree: yes (|diff| = 0.000000)
```

### Graph file format

```
<vertex-count>
<undirected-edge-count>
u,v          # one edge per line; "u v" also accepted
...
```

Both edge directions may be listed (as in the bundled datasets); if each edge
appears only once, the reverse is added automatically. To canonicalize an
arbitrary edge list, use:

```sh
./build/normalize-graph input.txt output.txt
```

## Parallelism

The parallel path uses `std::thread` workers that pull sources from a shared
`std::atomic` counter (dynamic load balancing, since per-source BFS cost
varies). Each worker accumulates a private partial sum, so there is no
contention on the result — the totals are reduced once at the end. The test
suite asserts the serial and parallel results are identical.
