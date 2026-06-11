# Understanding Group Betweenness Centrality (GBC)

A plain-language guide to what GBC is, where it came from, how it's used, and the
network-science ideas connected to it.

---

## 1. The big idea in one sentence

> **Group Betweenness Centrality tells you how much of a network's "traffic"
> flows through a chosen *team* of nodes — not just one node, but a group acting
> together.**

If you imagine a road map, GBC answers: *"If these particular intersections were
the only ones I could put toll booths on, how much of all the shortest-route
traffic in the city would I catch?"*

---

## 2. Starting from the simpler idea: betweenness

To understand the *group* version, first picture the original, single-node idea.

### Shortest paths

In a network (people, computers, cities…), information usually travels along
the **shortest path** — the route with the fewest hops. Between any two nodes
there may be one shortest path, or several equally short ones.

### A node "in between"

A node has high **betweenness centrality** if it sits on *many* of those shortest
paths. It's a **bridge** — a go-between. Remove it, and lots of pairs suddenly
have to take a longer detour (or get cut off).

> **Everyday analogy.** Think of a friend group made of two cliques that barely
> know each other, joined only by one mutual friend. That mutual friend has huge
> betweenness: almost every "message" passing between the two cliques has to go
> through them. They're a *broker*.

Formally, the betweenness of a node *v* is:

```
        Σ      (number of shortest s–t paths passing through v)
   s ≠ v ≠ t   ───────────────────────────────────────────────
               (total number of shortest s–t paths)
```

You add up, over every pair of other nodes, the **fraction** of their shortest
paths that go through *v*. A fraction is used because if there are 4 equally
short routes and *v* is on 1 of them, *v* only deserves "¼ of the credit" for
that pair.

---

## 3. From one node to a *group*

Now the leap. Instead of asking *"how important is this single node?"*, GBC asks:

> **"How important is this *set* of nodes, considered as a whole?"**

A path is counted if it passes through **at least one** member of the group. The
group gets credit collectively — it doesn't matter *which* member catches the
path, only that *someone* on the team does.

```
            Σ        (number of shortest s–t paths through ANY group member)
   s,t ∉ group       ──────────────────────────────────────────────────────
     s ≠ t           (total number of shortest s–t paths)
```

Two important details:

- **Endpoints are excluded.** We only count traffic *between outsiders* (`s` and
  `t` are not in the group). A path "starting" inside the group isn't traffic the
  group is brokering.
- **Each path counts once for the team.** A route that happens to pass through two
  group members isn't double-counted — the group either catches a path or it
  doesn't.

### Why a group can be more (or less) than the sum of its parts

This is the subtle, interesting part. **GBC is not just the sum of each member's
individual betweenness.**

- **Overlap (redundancy).** If two nodes are bridges over the *same* paths, adding
  the second one to the group barely helps — those paths were already covered.
  Two great brokers who broker the *same* connections are partly wasted together.
- **Complementarity (synergy).** Two mediocre nodes that each cover *different*
  regions can form a powerful group, covering far more together than their
  individual scores suggest.

So choosing the *best group of size k* is a genuine optimization problem, not just
"pick the top-k individual nodes." (More on that in §7.)

---

## 4. A tiny worked picture

Consider a "barbell" graph: two clusters joined by a short chain `A — X — Y — B`.

```
 cluster 1            cluster 2
  (o o o) —— A — X — Y — B —— (o o o)
```

- Node **X** alone has high betweenness — most cross-cluster traffic uses it.
- Node **Y** alone *also* has high betweenness — but it covers *the same* traffic
  as X (every cross-cluster path uses both X and Y).
- So the **group {X, Y}** is barely better than {X} alone: huge redundancy.
- The **group {X, A-cluster-hub}** might cover cross-cluster traffic *and* a lot
  of internal cluster-1 traffic — less redundant, higher GBC.

This is exactly the intuition GBC is built to quantify.

---

## 5. A short history

| Era | Contribution | Who |
|-----|--------------|-----|
| 1940s–50s | First intuitions of "centrality" in small-group communication experiments | Alex Bavelas, Harold Leavitt, Marvin Shaw |
| 1971 | The "rush" in a network — an early betweenness-like flow measure | Jac M. Anthonisse |
| **1977** | **Betweenness centrality formally defined and popularized** | **Linton C. Freeman** |
| 1987–94 | Variants: flow betweenness, random-walk betweenness | Freeman, Borgatti, Newman |
| **1999** | **Group centrality measures introduced** (group degree, closeness, **betweenness**) | **Martin Everett & Stephen Borgatti** |
| **2001** | **Fast O(V·E) algorithm** making betweenness practical on large graphs | **Ulrik Brandes** |
| 2007+ | Efficient *group* betweenness computation and group *selection* | Rami Puzis, Yuval Elovici, Shlomi Dolev, and others |

A few notes:

- **Freeman (1977)**, *"A set of measures of centrality based on betweenness,"*
  Sociometry — the canonical reference for node betweenness.
- **Everett & Borgatti (1999)**, *"The centrality of groups and classes,"* Journal
  of Mathematical Sociology — this is the paper that lifted centrality from single
  nodes to **groups and classes** of nodes, defining GBC.
- **Brandes (2001)**, *"A faster algorithm for betweenness centrality,"* Journal of
  Mathematical Sociology — before this, betweenness was too slow for big networks;
  Brandes' *dependency accumulation* trick changed that. Most modern GBC code
  (including this project) is a descendant of Brandes' BFS-based approach.
- **Puzis et al.** later showed how to compute GBC for many candidate groups
  efficiently and how to *search* for high-GBC groups, which matters because the
  optimization is hard.

---

## 6. Where GBC is actually used

GBC shines whenever you must pick a **small team of "control points"** to cover as
much of a network's flow as possible.

| Field | What the "group" is | What GBC tells you |
|-------|---------------------|--------------------|
| **Network security** | A set of monitors / intrusion-detection sensors | How much network traffic the sensor placement can observe |
| **Epidemiology / public health** | A set of people to vaccinate or quarantine | How many transmission routes the intervention breaks |
| **Transportation** | A set of intersections, stations, or hubs | How much travel depends on that infrastructure set |
| **Counter-terrorism / policing** | A set of key individuals | Which small group most controls information/resource flow in a covert network |
| **Communication / the Internet** | A set of routers or autonomous systems | Bottlenecks and critical relays for routing |
| **Biology** | A set of proteins or genes | Groups of molecules that mediate the most signaling pathways |
| **Marketing / influence** | A seed set of influencers | Which team best sits "between" audiences to spread a message |
| **Infrastructure resilience** | A set of nodes whose loss is most damaging | Where the network is most fragile if the whole group fails |

The common thread: **placement and protection decisions** — *"of all possible
small sets, which one sits across the most of what matters?"*

---

## 7. Related concepts and terms (a glossary)

These ideas all sit near GBC. Understanding them makes GBC click.

### Building blocks

- **Geodesic / shortest path** — the minimum-hop route between two nodes. GBC is
  built entirely on these.
- **Node (vertex) betweenness centrality** — the single-node ancestor of GBC.
- **Edge betweenness centrality** — the same idea for *edges* instead of nodes;
  famously used by the **Girvan–Newman algorithm** to detect communities (cut the
  highest-betweenness edges repeatedly).
- **Dependency (Brandes)** — the clever quantity Brandes accumulates during a
  backward sweep of a BFS tree so betweenness can be computed in one pass per
  source instead of by examining every pair.

### Other centrality measures (different notions of "important")

- **Degree centrality** — simply how many connections a node has. Local, cheap;
  ignores global structure.
- **Closeness centrality** — how *near* a node is, on average, to everyone else
  (short total distance). Good for "who can reach the network fastest."
- **Eigenvector centrality / PageRank / Katz** — "you're important if your
  neighbors are important." Captures influence/prestige rather than brokerage.
- **Betweenness vs. the rest:** degree/closeness/eigenvector measure *being
  well-connected or well-positioned*; betweenness measures *being a gatekeeper
  between others*. A node can be a powerful broker while having very few
  connections.

### Group-level cousins (the Everett–Borgatti family)

- **Group degree centrality** — how many *outside* nodes the group is directly
  connected to.
- **Group closeness centrality** — how close the group (as a whole) is to all
  outside nodes.
- **Group betweenness centrality** — *this document.*

### Structural / social ideas

- **Broker / brokerage** — a node (or group) that connects otherwise separated
  parts. High betweenness ≈ strong brokerage.
- **Structural holes (Ronald Burt)** — gaps between groups; whoever bridges a
  structural hole gains brokerage advantage. GBC quantifies a team's grip on these.
- **Bridge / cut vertex (articulation point)** — an edge/node whose removal
  disconnects the graph; an extreme, all-or-nothing form of high betweenness.

### Computational ideas behind choosing a group

- **Normalization** — raw GBC depends on network size; dividing by the maximum
  possible value rescales it to `[0, 1]` so networks can be compared.
- **NP-hardness** — finding the size-*k* group with the *maximum* GBC is
  computationally hard; you can't just check every possible group on a big network.
- **Submodularity & greedy approximation** — GBC has a "diminishing returns"
  property (each added member helps less as the group grows). This lets a simple
  **greedy** algorithm — repeatedly add the node that boosts GBC the most — come
  with a provable quality guarantee (~63% of optimal), which is why greedy
  selection is the standard practical approach.

---

## 8. The engine that finds shortest paths: BFS

Everything about betweenness rests on **shortest paths**, so we need a way to find
them. For graphs where every edge counts the same (one "hop"), the right tool is
**Breadth-First Search (BFS)**.

### The ripple analogy

> Drop a stone in a pond. The ripple reaches everything **one ring at a time** —
> first the points 1 unit away, then everything 2 units away, then 3, and so on.

BFS explores a graph the same way. Starting from a source node `s`, it visits:

1. `s` itself (distance 0),
2. then **all** of `s`'s direct neighbours (distance 1),
3. then all of *their* not-yet-seen neighbours (distance 2),
4. …and so on, layer by layer.

Because it always finishes a whole "ring" before moving outward, **the first time
BFS reaches any node, it has arrived by a shortest path.** That's the property
that makes BFS perfect for betweenness.

### How it works mechanically

BFS uses a **queue** (a first-in-first-out line):

```
put s in the queue, mark its distance as 0
while the queue is not empty:
    take the front node v out of the queue
    for each neighbour w of v:
        if w has not been seen yet:
            distance[w] = distance[v] + 1     # one ring further out
            put w in the queue
```

The queue guarantees the layer-by-layer order: nodes 1 hop away are all dequeued
before any node 2 hops away, and so on.

### From "shortest distance" to "how many shortest paths"

Plain BFS gives the shortest *distance*. Betweenness also needs the **number** of
shortest paths (because credit is shared as a fraction). That takes one small
addition: while exploring, whenever we find that `v` sits one step *before* `w` on
a shortest route (a **predecessor**), we add `v`'s path-count onto `w`'s:

```
shortest_paths[w] += shortest_paths[v]      # for every predecessor v of w
```

A node reachable by two different shortest routes ends up with a count of 2, and
so on. This is exactly the `σ` count used by GBC — and the same mechanism, with
one extra rule (*don't let the count flow through a group member*), produces the
`avoid` count. One sweep of BFS gives us both.

> **Why BFS and not Dijkstra?** Dijkstra's algorithm handles edges with different
> *weights* (distances/costs). Our graphs are **unweighted** — every edge is one
> hop — and for that case BFS is both correct and faster.

---

## 9. The storage format that makes it fast: CSR

BFS has to ask, over and over, *"who are this node's neighbours?"* The graph needs
to answer that question **quickly** and **without wasting memory**. That's the job
of **Compressed Sparse Row (CSR)**.

### Why not a grid?

The obvious way to store a graph is an **adjacency matrix**: an N×N grid where
cell `(i, j)` is 1 if an edge connects `i` and `j`. Simple — but for a network
like Facebook (4,039 nodes) that's 4039 × 4039 ≈ **16 million cells**, almost all
of them 0, because real networks are **sparse** (each node touches only a handful
of others). That's enormous waste.

### The CSR idea: a contacts book, not a grid

> Think of a phone's contact list. You don't keep a giant table of "me vs. every
> human on Earth" with a tick next to the few I know. You keep a **compact list**
> of just the people I actually know.

CSR does this with **two flat arrays**:

- **`col_idx`** — one long list of all neighbours, with each node's neighbours
  sitting together in a contiguous run.
- **`row_ptr`** — a small index that says *where each node's run begins* in that
  long list.

To get node `v`'s neighbours, you read `col_idx` from `row_ptr[v]` up to (but not
including) `row_ptr[v+1]`. Two array lookups, then a tidy slice — no scanning, no
wasted space.

### A picture (the `data4` graph)

```
        0     1
         \   /
          \ /
           2          ← vertex 2 has neighbours {0, 1, 3, 4}
          / \
         3   4
          \ /
           5
           |
           6
```

stored as:

```
vertex:   0    1    2          3      4      5        6
row_ptr: [0,   1,   2,         6,     8,     10,      13,   14]   ← where each run starts
col_idx: [2 | 2 | 0  1  3  4 | 2  5 | 2  5 | 3  4  6 | 5]        ← all neighbours, grouped
          └0┘ └1┘ └─── 2 ───┘ └─3─┘ └─4─┘ └── 5 ───┘ └6┘
```

To list vertex 2's neighbours: `row_ptr[2] = 2` and `row_ptr[3] = 6`, so read
`col_idx[2..5] = {0, 1, 3, 4}`. ✅

### Why the extra `+1` slot

`row_ptr` holds **N + 1** numbers (note the trailing `14` above). That final
"sentinel" entry means even the *last* vertex's run has a clear end
(`row_ptr[v+1]`), with no special case. It also lets an **isolated** node (no
edges) have `row_ptr[v] == row_ptr[v+1]` — an empty run — naturally.

### Why it matters for GBC

CSR makes the inner loop of BFS — "visit each neighbour of `v`" — a simple, fast
walk over a contiguous slice of memory. On the large datasets that's the
difference between a quick answer and an impractically slow one, and it's why the
millions of edge-visits in a full GBC run stay cheap.

---

## 10. How this idea connects to the code in this repository

This project computes GBC with a **Brandes-style breadth-first search** from each
source node:

1. A BFS finds, for every other node, the shortest distance **and** the number of
   shortest paths to it (`σ`).
2. A parallel count (`avoid`) tracks shortest paths that *dodge* the group
   entirely — by simply refusing to let path-counts flow *through* a group member.
3. The fraction of `s–t` traffic the group catches is then just
   `1 − avoid[t] / σ[t]`, summed over all valid targets.

Because each undirected pair is counted from both ends, the total is halved
("rescaled"), and dividing by the number of valid pairs gives the normalized
score in `[0, 1]`. A serial and a multithreaded version compute the same value;
see the repository [README](../README.md) for build and usage instructions.

---

## 11. Quick recap

- **Betweenness** = how much a *single* node acts as a bridge on shortest paths.
- **Group betweenness (GBC)** = how much a *team* of nodes does so together.
- A group's value depends on **coverage**, not headcount — overlap wastes,
  complementarity multiplies.
- Born from **Freeman (1977)** → generalized by **Everett & Borgatti (1999)** →
  made fast by **Brandes (2001)** and later group-specific methods.
- Used wherever you must place a few **monitors, vaccines, or control points** to
  cover the most of a network's flow.
- Sits in a family of measures — degree, closeness, eigenvector — each capturing a
  different flavor of "important."
- **Computed** by exploring the network with **BFS** (shortest paths, ring by ring)
  over a graph stored compactly as **CSR** (a contacts-book layout that keeps the
  neighbour lookups fast).

---

### Suggested reading

- L. C. Freeman, *"A set of measures of centrality based on betweenness,"*
  Sociometry, 1977.
- M. G. Everett & S. P. Borgatti, *"The centrality of groups and classes,"*
  Journal of Mathematical Sociology, 1999.
- U. Brandes, *"A faster algorithm for betweenness centrality,"* Journal of
  Mathematical Sociology, 2001.
- R. Puzis, Y. Elovici, S. Dolev, *"Fast algorithm for successive computation of
  group betweenness centrality,"* Physical Review E, 2007.
