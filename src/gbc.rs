//! Serial and multithreaded Group Betweenness Centrality core.

use std::collections::VecDeque;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::thread;

use crate::csr::Csr;

/// Result of a Group Betweenness Centrality computation for a group `S`.
///
/// `raw` is the ordered-pair sum
/// `Σ σ_st(S) / σ_st` over ordered pairs `(s, t)` with `s, t ∉ S` and `s ≠ t`,
/// where `σ_st` is the number of shortest `s`–`t` paths and `σ_st(S)` is how many
/// of them have at least one *internal* vertex in `S`.
#[derive(Debug, Default, Clone, Copy, PartialEq)]
pub struct GbcResult {
    /// Ordered-pair sum (each undirected pair counted twice).
    pub raw: f64,
    /// `raw / 2` — the undirected GBC value.
    pub rescaled: f64,
    /// `raw / ((n - g)(n - g - 1))` — in `[0, 1]`.
    pub normalized: f64,
}

/// Contribution of a single source `s` (assumed not in the group) to the ordered
/// GBC sum: the total, over every valid target `t`, of the fraction of shortest
/// `s`–`t` paths that pass through the group.
///
/// One BFS computes, on the shortest-path DAG rooted at `s`:
/// * `sigma[v]` = number of shortest `s → v` paths
/// * `avoid[v]` = number of shortest `s → v` paths whose internal vertices avoid the group
///
/// The group fraction for a target `t` is then `1 - avoid[t] / sigma[t]`.
fn source_contribution(g: &Csr, in_group: &[bool], s: usize) -> f64 {
    let n = g.v_count;
    let mut dist = vec![-1i32; n];
    let mut sigma = vec![0u64; n];
    let mut avoid = vec![0u64; n];
    let mut order: Vec<u32> = Vec::with_capacity(n); // vertices in non-decreasing distance order

    dist[s] = 0;
    sigma[s] = 1;
    avoid[s] = 1; // s is not in the group, so the empty path "avoids" it

    let mut q: VecDeque<u32> = VecDeque::new();
    q.push_back(s as u32);
    while let Some(v) = q.pop_front() {
        let v = v as usize;
        order.push(v as u32);
        let dv = dist[v];
        for &w in g.neighbours(v) {
            let w = w as usize;
            if dist[w] < 0 {
                dist[w] = dv + 1;
                q.push_back(w as u32);
            }
            if dist[w] == dv + 1 {
                // v is a predecessor of w on the DAG
                sigma[w] += sigma[v];
                if !in_group[v] {
                    // a group vertex blocks the "avoid" flow
                    avoid[w] += avoid[v];
                }
            }
        }
    }

    let mut contribution = 0.0;
    for &t in &order {
        let t = t as usize;
        if t == s || in_group[t] {
            continue; // GBC excludes group members as endpoints
        }
        contribution += 1.0 - avoid[t] as f64 / sigma[t] as f64;
    }
    contribution
}

fn finalize(raw: f64, n: usize, group_size: usize) -> GbcResult {
    let pairs = (n - group_size) as f64 * (n - group_size).saturating_sub(1) as f64;
    GbcResult {
        raw,
        rescaled: raw / 2.0, // undirected: every unordered pair is counted twice
        normalized: if pairs > 0.0 { raw / pairs } else { 0.0 },
    }
}

/// Compute GBC for `in_group` serially. `in_group[v]` marks membership; `group_size`
/// is the number of distinct members.
pub fn gbc_serial(graph: &Csr, in_group: &[bool], group_size: usize) -> GbcResult {
    let mut raw = 0.0;
    for s in 0..graph.v_count {
        if in_group[s] {
            continue; // sources in the group are excluded
        }
        raw += source_contribution(graph, in_group, s);
    }
    finalize(raw, graph.v_count, group_size)
}

/// Compute GBC for `in_group` using `num_threads` workers. Produces the same value
/// as [`gbc_serial`].
///
/// Dynamic work-stealing over sources: BFS cost varies per vertex, so a shared
/// atomic counter balances load better than a static split. Each worker keeps a
/// private partial sum, so there is no contention on the result.
pub fn gbc_parallel(graph: &Csr, in_group: &[bool], group_size: usize, num_threads: usize) -> GbcResult {
    let num_threads = num_threads.max(1);
    let next_source = AtomicUsize::new(0);
    let n = graph.v_count;

    let raw: f64 = thread::scope(|scope| {
        let handles: Vec<_> = (0..num_threads)
            .map(|_| {
                let next_source = &next_source;
                scope.spawn(move || {
                    let mut local = 0.0;
                    loop {
                        let s = next_source.fetch_add(1, Ordering::Relaxed);
                        if s >= n {
                            break;
                        }
                        if in_group[s] {
                            continue;
                        }
                        local += source_contribution(graph, in_group, s);
                    }
                    local
                })
            })
            .collect();

        handles.into_iter().map(|h| h.join().unwrap()).sum()
    });

    finalize(raw, graph.v_count, group_size)
}
