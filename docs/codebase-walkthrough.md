# Codebase Walkthrough

How this project is organized, how data flows through it, what happens step by
step when you run it, and the exact algorithm — finished off with a **fully
hand-traced example** whose numbers match the program's real output.

> Companion reading: [`group-betweenness-centrality.md`](group-betweenness-centrality.md)
> explains *what GBC is* conceptually. This document explains *how the code
> computes it*.

---

## 1. What the program does

Given a graph and a chosen **group** of vertices, it computes the group's
**Group Betweenness Centrality** — how much of the network's shortest-path
"traffic" flows through that group — using two interchangeable engines:

- a **serial** engine, and
- a **multithreaded** engine,

which always produce the same number (the program checks this for you).

---

## 2. Repository map

```
src/
  lib.rs                 Library root: re-exports the csr + gbc modules
  csr.rs                 Graph data structure (Compressed Sparse Row) + loader
  gbc.rs                 THE CORE: BFS-based GBC, shared by both engines
  main.rs                Command-line front end (arg parsing, I/O, timing, reporting)
  bin/normalize_graph.rs Standalone helper to canonicalize a graph file
tests/
  test_gbc.rs            Correctness suite: brute-force cross-check + serial==parallel
datas/                   Sample graphs (data4, dolphins, football, facebook, wiki, …)
Cargo.toml               Crate + binary definitions
```

Two binaries are built: **`gbc`** (the main app) and **`normalize-graph`** (the
utility); the correctness suite runs via `cargo test`. The `gbc` binary and the
tests share the `gbc` library crate (`csr` + `gbc` modules).

---

## 3. The data model: Compressed Sparse Row (CSR)

A graph could be stored as a 2-D adjacency matrix, but for sparse graphs that
wastes huge amounts of memory. Instead we use **CSR**, which stores only the
edges that exist, in two flat arrays.

```rust
pub struct Csr {
    pub v_count: usize,        // number of vertices
    pub e_count: usize,        // number of directed entries (2 × undirected edges)
    pub row_ptr: Vec<usize>,   // size v_count + 1
    pub col_idx: Vec<u32>,     // size e_count
}
```

The trick: **the neighbours of vertex `v` are exactly**

```
col_idx[ row_ptr[v] ]  ...  col_idx[ row_ptr[v+1] - 1 ]
```

`row_ptr` has `v_count + 1` entries so the **last** vertex needs no special case,
and an **isolated** vertex simply gets `row_ptr[v] == row_ptr[v+1]` (an empty
range).

### Example: `datas/data4.txt`

The file:

```
7            ← vertex count
7            ← undirected edge count
0,2  1,2  2,0  2,1  2,3  2,4  3,2  3,5  4,2  4,5  5,3  5,4  5,6  6,5   (one per line)
```

describes this graph:

```
        0     1
         \   /
          \ /
           2
          / \
         3   4
          \ /
           5
           |
           6
```

Built into CSR:

```
vertex:   0    1    2          3      4      5        6
row_ptr: [0,   1,   2,         6,     8,     10,      13,   14]
                                                            ^ sentinel
col_idx: [2 | 2 | 0  1  3  4 | 2  5 | 2  5 | 3  4  6 | 5]
          └0┘ └1┘ └─── 2 ───┘ └─3─┘ └─4─┘ └── 5 ───┘ └6┘
```

To read vertex 5's neighbours: `row_ptr[5]=10`, `row_ptr[6]=13`, so
`col_idx[10..12] = {3, 4, 6}` — correct.

---

## 4. End-to-end data flow

```
  graph file (datas/*.txt)
            │
            ▼
   ┌──────────────────┐   load_csr()           reads header + all directed edges,
   │   src/csr.rs     │ ─────────────────────►  counting-sort build (order-independent,
   └──────────────────┘                         handles isolated vertices)
            │
            ▼
        CSR struct  ──────────────┐
            │                     │
   group choice (CLI / prompt)    │
            │                     │
            ▼                     ▼
   ┌──────────────────┐    in_group[] (true = member), group_size
   │   src/main.rs    │
   └──────────────────┘
            │  calls
            ├──────────────► gbc_serial(...)   ┐
            └──────────────► gbc_parallel(...) ┤  src/gbc.rs  (shared core)
                                               │
                              both call source_contribution() per source vertex
                                               │
                                               ▼
                                       finalize() → GBCResult { raw, rescaled, normalized }
            ┌──────────────────────────────────┘
            ▼
   timing + results printed; serial vs parallel agreement checked
```

---

## 5. Execution steps (what a run actually does)

Take this command:

```sh
./target/release/gbc datas/data4.txt --group 2
```

1. **Parse arguments** (`main.rs`). First positional arg = graph file. Flags:
   `--group`, `--threads` (default = hardware thread count), `--mode`
   (`both`/`serial`/`parallel`, default `both`).
2. **Load the graph** → `load_csr("datas/data4.txt")` returns the CSR struct.
   Errors (missing file, bad header, out-of-range endpoint) return an `Err` and
   exit cleanly.
3. **Print graph stats** (vertices, directed/undirected edge counts).
4. **Resolve the group.** From `--group 2`, or by prompting interactively if the
   flag is absent. Build `in_group` (a `Vec<bool>`, `true` at each member) and
   count distinct members → `group_size`. Validate ids are in range and that at
   least two non-group vertices remain.
5. **Run the engine(s).** For each requested mode, time it with
   `std::time::Instant` and call `gbc_serial` / `gbc_parallel`.
6. **Report.** Print `rescaled` and `normalized` GBC plus elapsed time. In `both`
   mode, also print the **speedup** and assert the two results agree to `1e-6`.

---

## 6. The algorithm, in depth

All the real work lives in **`source_contribution()`** in `src/gbc.rs`. It runs
**one Breadth-First Search per source vertex** and is the heart of everything.

### 6.1 What we want from each source `s`

For a fixed source `s` (which must *not* be in the group), we want:

```
contribution(s) = Σ   ( fraction of shortest s→t paths that pass through the group )
                 t valid
```

where a *valid* target `t` is any vertex with `t ≠ s`, `t ∉ group`, and reachable.

### 6.2 Two path counts from one BFS

The clever part: that fraction can be computed from **two counts** that a single
forward BFS accumulates layer by layer.

| array | meaning |
|-------|---------|
| `sigma[v]` | number of shortest `s → v` paths (the usual path count) |
| `avoid[v]` | number of shortest `s → v` paths whose **internal** vertices avoid the group |

Both follow the same recurrence over the shortest-path DAG: a vertex `v` is a
**predecessor** of `w` when `dist[w] == dist[v] + 1`. Then:

```
sigma[w] += sigma[v]                     for every predecessor v
avoid[w] += avoid[v]   ONLY IF v ∉ group     ← the one extra rule
```

That single guard — *don't let path-flow pass through a group vertex* — is the
entire trick. `avoid[t]` ends up counting exactly the shortest paths that dodge
the group, so:

```
fraction through group for target t  =  1 − avoid[t] / sigma[t]
```

> **Why one forward pass is enough.** Because BFS pops vertices in
> non-decreasing distance order, *all* predecessors of `w` are processed before
> `w` itself. So by the time `w` is finalized, both `sigma[w]` and `avoid[w]` are
> complete. (This is simpler than full node-betweenness, which needs a second,
> backward "dependency" sweep — here we only need forward-accumulated counts.)

### 6.3 The procedure

```
source_contribution(g, in_group, s):
    dist[*]  = -1 ; dist[s]  = 0
    sigma[*] = 0  ; sigma[s] = 1
    avoid[*] = 0  ; avoid[s] = 1        # s ∉ group, so the empty path "avoids" it
    BFS queue = [s]

    while queue not empty:
        v = pop()
        record v in visit order
        for each neighbour w of v (via row_ptr / col_idx):
            if dist[w] == -1:                  # first sighting
                dist[w] = dist[v] + 1 ; enqueue w
            if dist[w] == dist[v] + 1:         # v is a DAG-predecessor of w
                sigma[w] += sigma[v]
                if v ∉ group:  avoid[w] += avoid[v]

    contribution = 0
    for each visited t, where t ≠ s and t ∉ group:
        contribution += 1 − avoid[t] / sigma[t]
    return contribution
```

### 6.4 Combining sources and finishing up (`finalize`)

Both engines sum `source_contribution(s)` over every non-group source `s` into
`raw`, then:

```
raw        = Σ contribution(s)                       # ordered pairs (s, t)
rescaled   = raw / 2                                  # undirected: each pair counted twice
normalized = raw / ((n − g)(n − g − 1))              # in [0, 1]; 0 if < 2 outsiders
```

- `rescaled` is the GBC value (each unordered `{s,t}` pair was counted once as
  `(s,t)` and once as `(t,s)`).
- `normalized` divides by the number of ordered outsider pairs — the maximum
  possible raw value — giving a size-independent score in `[0, 1]`.

### 6.5 Complexity

One BFS is `O(V + E)`; we run one per source, so the whole computation is
`O(V·(V + E))` — the same order as Brandes' classic betweenness algorithm, and
optimal for this exact-counting approach.

---

## 7. Fully worked example

Let's compute GBC for **`datas/data4.txt` with group = {2}** entirely by hand,
then check it against the program.

Recall the graph (2 is the central hub; 0, 1, 6 are leaves):

```
   0   1
    \ /
     2
    / \
   3   4
    \ /
     5
     |
     6
```

Sources to process: every vertex except the group member 2 → `{0, 1, 3, 4, 5, 6}`.

### Source s = 0  (a leaf hanging off 2)

BFS distances: `0→2→{1,3,4}→5→6`. Filling `sigma` / `avoid`:

| t | sigma | avoid | `1 − avoid/sigma` |
|---|------:|------:|------------------:|
| 1 | 1 | 0 | 1 |
| 3 | 1 | 0 | 1 |
| 4 | 1 | 0 | 1 |
| 5 | 2 | 0 | 1 |
| 6 | 2 | 0 | 1 |

Every route out of leaf 0 must cross hub 2, so `avoid` is 0 everywhere →
**contribution = 5**. By symmetry, **source 1 also = 5**.

### Source s = 3

BFS distances: `3→{2,5}→{0,1,4(via 2)}, 4&6(via 5)…`. The interesting target is 4,
reachable two ways — `3→2→4` (through the group) and `3→5→4` (avoiding it):

| t | sigma | avoid | `1 − avoid/sigma` |
|---|------:|------:|------------------:|
| 0 | 1 | 0 | 1     |
| 1 | 1 | 0 | 1     |
| 4 | 2 | 1 | **0.5** |
| 5 | 1 | 1 | 0     |
| 6 | 1 | 1 | 0     |

Reaching 0 and 1 forces a trip through 2 (→1 each); reaching 4 is half-brokered
(one of two shortest paths uses 2); 5 and 6 avoid the group entirely.
**contribution = 2.5**. By symmetry, **source 4 also = 2.5**.

### Source s = 5

| t | sigma | avoid | `1 − avoid/sigma` |
|---|------:|------:|------------------:|
| 3 | 1 | 1 | 0 |
| 4 | 1 | 1 | 0 |
| 6 | 1 | 1 | 0 |
| 0 | 2 | 0 | 1 |
| 1 | 2 | 0 | 1 |

Only the far leaves 0 and 1 (behind hub 2) are brokered → **contribution = 2**.

### Source s = 6  (a leaf hanging off 5)

Same shape as source 5 once you reach 5; only 0 and 1 sit behind the group →
**contribution = 2**.

### Add it all up

```
raw = 5 (s0) + 5 (s1) + 2.5 (s3) + 2.5 (s4) + 2 (s5) + 2 (s6) = 19

rescaled   = 19 / 2                       = 9.5
normalized = 19 / ((7−1)(7−1−1)) = 19/30  = 0.633333…
```

### Check against the program

```sh
$ ./target/release/gbc datas/data4.txt --group 2 --mode serial
...
  GBC (rescaled):   9.500000
  GBC (normalized): 0.633333
```

✅ The hand trace matches exactly. (Intuitively, `9.5 / 15 = 0.633`: the hub node
2 brokers about 63% of all shortest-path traffic between the other six vertices —
high, as expected for the graph's only central connector.)

---

## 8. Serial vs. parallel — same math, different driver

Both engines call the identical `source_contribution()`; they differ only in
*who runs which source*.

**Serial** (`gbc_serial`): a plain loop over sources, accumulating into one `raw`.

**Parallel** (`gbc_parallel`):

```
AtomicUsize next_source = 0
thread::scope → spawn N scoped workers, each:
    local = 0.0
    loop:
        s = next_source.fetch_add(1)
        if s >= v_count: break
        if s ∉ group: local += source_contribution(g, in_group, s)
    return local
join all; raw = Σ locals
```

Key design choices:

- **Dynamic load balancing** via a shared atomic counter — each worker grabs the
  next source whenever it's free. BFS cost varies per source, so this beats a
  fixed split.
- **No lock on the hot path** — every worker accumulates into its own `local`
  variable and returns it from the scoped thread. The single reduction happens
  after the join, so there's no contention while computing.
- **Scoped threads** (`std::thread::scope`) let the workers borrow the graph and
  `in_group` directly, with no `Arc`/cloning and no `unsafe`.

This is why the results are **bit-for-bit comparable** and the program can assert
`|serial − parallel| < 1e-6`.

---

## 9. The correctness test (`tests/test_gbc.rs`)

The test guards against silent algorithm regressions using an **independent**
method:

1. A separate **brute-force** GBC computes the same value pair-by-pair with a
   memoized recursive path counter (different code path from the BFS engine).
2. For `data4` and `dolphins`, with hand-picked and random groups, it checks:
   - `serial == brute force` (the algorithm is correct), and
   - `serial == parallel` (the two engines agree).

`cargo test` runs it; all 29 cases must pass (the test fails otherwise).

---

## 10. The `normalize-graph` utility

`src/bin/normalize_graph.rs` is a small standalone tool to put an arbitrary edge
list into the canonical format the loader expects: it reads any
comma/whitespace-separated edges, **deduplicates**, **adds both directions**
(symmetry via a `BTreeSet`, which also keeps them sorted), then writes the
`v_count / e_count / edges` format. Handy when adding a new dataset.

```sh
./target/release/normalize-graph raw_edges.txt clean_graph.txt
```

---

## 11. Building it

```sh
cargo build --release    # builds target/release/{gbc, normalize-graph}
cargo test --release     # runs the correctness suite
cargo run --release --bin gbc -- datas/data4.txt --group 2,5   # demo
```

Cargo compiles the `csr` + `gbc` modules into the `gbc` library crate once and
links it into the `gbc` binary and the integration test. The release profile uses
`opt-level = 3` with LTO; threading is the standard library's `std::thread`, so
there are no external dependencies.

---

## 12. One-paragraph summary

A graph file is parsed into a compact **CSR** structure. You name a **group** of
vertices. For every other vertex as a source, a **single BFS** counts both the
total shortest paths (`sigma`) and the shortest paths that *avoid* the group
(`avoid`); `1 − avoid/sigma` is the fraction of that source's traffic the group
intercepts. Summed over all sources and rescaled, that's the **Group Betweenness
Centrality** — computed identically by a serial loop and a lock-free pool of
scoped `std::thread` workers, and verified against a brute-force reference.
