//! Correctness test: cross-checks the optimized GBC implementation against an
//! independent brute-force reference on small graphs, and verifies that the serial
//! and parallel paths agree.

use std::collections::VecDeque;

use gbc::csr::{load_csr, Csr};
use gbc::gbc::{gbc_parallel, gbc_serial};

fn bfs_dist(g: &Csr, s: usize) -> Vec<i32> {
    let mut dist = vec![-1i32; g.v_count];
    let mut q = VecDeque::new();
    dist[s] = 0;
    q.push_back(s as u32);
    while let Some(v) = q.pop_front() {
        let v = v as usize;
        for &w in g.neighbours(v) {
            let w = w as usize;
            if dist[w] < 0 {
                dist[w] = dist[v] + 1;
                q.push_back(w as u32);
            }
        }
    }
    dist
}

/// Independent path counter: memoized recursion over predecessors on the DAG. When
/// `avoid` is set, steps through a group vertex are forbidden, so the count is the
/// number of shortest paths whose internal vertices skip the group.
fn count_paths(
    g: &Csr,
    dist: &[i32],
    s: usize,
    t: usize,
    avoid: Option<&[bool]>,
    memo: &mut [f64],
) -> f64 {
    if t == s {
        return 1.0;
    }
    if memo[t] >= 0.0 {
        return memo[t];
    }
    let mut total = 0.0;
    for &u in g.neighbours(t) {
        let u = u as usize;
        if dist[u] == dist[t] - 1 {
            if let Some(av) = avoid {
                if av[u] {
                    continue; // path would pass through a group vertex
                }
            }
            total += count_paths(g, dist, s, u, avoid, memo);
        }
    }
    memo[t] = total;
    total
}

/// Reference GBC (rescaled), computed pair-by-pair with the recursion above.
fn brute_gbc_rescaled(g: &Csr, in_group: &[bool]) -> f64 {
    let mut raw = 0.0;
    for s in 0..g.v_count {
        if in_group[s] {
            continue;
        }
        let dist = bfs_dist(g, s);
        for t in 0..g.v_count {
            if t == s || in_group[t] || dist[t] < 0 {
                continue;
            }
            let mut memo_total = vec![-1.0; g.v_count];
            let mut memo_avoid = vec![-1.0; g.v_count];
            let total = count_paths(g, &dist, s, t, None, &mut memo_total);
            let avoided = count_paths(g, &dist, s, t, Some(in_group), &mut memo_avoid);
            raw += 1.0 - avoided / total;
        }
    }
    raw / 2.0
}

fn check(g: &Csr, name: &str, members: &[usize]) -> bool {
    let mut in_group = vec![false; g.v_count];
    let mut group_size = 0;
    for &v in members {
        if !in_group[v] {
            in_group[v] = true;
            group_size += 1;
        }
    }

    let reference = brute_gbc_rescaled(g, &in_group);
    let serial = gbc_serial(g, &in_group, group_size);
    let parallel = gbc_parallel(g, &in_group, group_size, 4);

    let ok_ref = (serial.rescaled - reference).abs() < 1e-6;
    let ok_par = (serial.rescaled - parallel.rescaled).abs() < 1e-6;

    if !(ok_ref && ok_par) {
        eprintln!(
            "  FAIL  {name} group {members:?}  serial={} brute={reference} parallel={}",
            serial.rescaled, parallel.rescaled
        );
    }
    ok_ref && ok_par
}

// A small deterministic LCG, so the test needs no external rng crate.
struct Lcg(u64);
impl Lcg {
    fn next_u32(&mut self) -> u32 {
        // Numerical Recipes constants.
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn range(&mut self, lo: u32, hi: u32) -> u32 {
        lo + self.next_u32() % (hi - lo + 1)
    }
}

fn run_random_groups(path: &str, name: &str, trials: usize, failures: &mut usize) {
    let g = load_csr(path).expect("load graph");
    let mut rng = Lcg(12345);
    for _ in 0..trials {
        let k = rng.range(1, 3) as usize;
        let members: Vec<usize> =
            (0..k).map(|_| rng.range(0, g.v_count as u32 - 1) as usize).collect();
        if !check(&g, name, &members) {
            *failures += 1;
        }
    }
}

#[test]
fn gbc_matches_brute_force_and_parallel() {
    let mut failures = 0;

    // data4: the tiny hand-traceable graph.
    {
        let g = load_csr("datas/data4.txt").expect("load data4");
        for members in [vec![2], vec![5], vec![2, 5], vec![0, 6]] {
            if !check(&g, "data4", &members) {
                failures += 1;
            }
        }
    }

    // Larger small graphs with random groups.
    run_random_groups("datas/data4.txt", "data4", 10, &mut failures);
    run_random_groups("datas/dolphins.txt", "dolphins", 15, &mut failures);

    assert_eq!(failures, 0, "{failures} GBC test failure(s)");
}
