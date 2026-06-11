//! Compressed Sparse Row (CSR) representation of an undirected graph.

use std::fs;
use std::path::Path;

/// CSR representation of an undirected graph.
///
/// Neighbours of vertex `v` live in [`col_idx`](Csr::col_idx) over the half-open
/// index range `[ row_ptr[v], row_ptr[v + 1] )`. `row_ptr` has `v_count + 1`
/// entries so the last vertex needs no special case, and isolated vertices simply
/// get an empty range.
#[derive(Debug, Default, Clone)]
pub struct Csr {
    /// Number of vertices.
    pub v_count: usize,
    /// Number of *directed* entries (`2 * undirected edges`).
    pub e_count: usize,
    /// Size `v_count + 1`: start offset of each vertex's neighbour range.
    pub row_ptr: Vec<usize>,
    /// Size `e_count`: neighbour vertex ids, grouped by source vertex.
    pub col_idx: Vec<u32>,
}

impl Csr {
    /// Neighbours of vertex `v` as a slice.
    #[inline]
    pub fn neighbours(&self, v: usize) -> &[u32] {
        &self.col_idx[self.row_ptr[v]..self.row_ptr[v + 1]]
    }
}

/// Load a graph from `path`.
///
/// File format:
/// * line 1: vertex count
/// * line 2: undirected edge count
/// * remaining lines: one edge per line as `u,v` or `u v`
///
/// Construction is order-independent and handles isolated vertices. If the file
/// lists each undirected edge only once, the reverse direction is added
/// automatically. Returns an error string on a missing/malformed file or an
/// out-of-range endpoint.
pub fn load_csr<P: AsRef<Path>>(path: P) -> Result<Csr, String> {
    let path = path.as_ref();
    let text = fs::read_to_string(path)
        .map_err(|e| format!("cannot open graph file: {} ({e})", path.display()))?;

    let mut tokens = text.split(|c: char| c == ',' || c.is_whitespace()).filter(|t| !t.is_empty());

    let v_count: i64 = tokens
        .next()
        .and_then(|t| t.parse().ok())
        .filter(|&v| v >= 0)
        .ok_or_else(|| format!("malformed header in: {}", path.display()))?;
    let undirected_edges: i64 = tokens
        .next()
        .and_then(|t| t.parse().ok())
        .ok_or_else(|| format!("malformed header in: {}", path.display()))?;
    let v_count = v_count as usize;

    // Read every directed entry. Separators may be commas or whitespace, so we
    // simply pull integer tokens two at a time.
    let mut edges: Vec<(u32, u32)> = Vec::with_capacity((undirected_edges.max(0) as usize) * 2);
    while let (Some(u), Some(v)) = (tokens.next(), tokens.next()) {
        let u: i64 = u
            .parse()
            .map_err(|_| format!("malformed edge endpoint in: {}", path.display()))?;
        let v: i64 = v
            .parse()
            .map_err(|_| format!("malformed edge endpoint in: {}", path.display()))?;
        if u < 0 || u >= v_count as i64 || v < 0 || v >= v_count as i64 {
            return Err(format!("edge endpoint out of range in: {}", path.display()));
        }
        edges.push((u as u32, v as u32));
    }

    // If the file listed each undirected edge only once, add the reverse so the
    // graph is symmetric for traversal.
    if edges.len() as i64 == undirected_edges {
        let original = edges.len();
        for i in 0..original {
            let (u, v) = edges[i];
            edges.push((v, u));
        }
    }

    let e_count = edges.len();

    // Counting-sort style construction: degree histogram -> prefix sums -> fill.
    let mut row_ptr = vec![0usize; v_count + 1];
    for &(u, _) in &edges {
        row_ptr[u as usize + 1] += 1;
    }
    for i in 0..v_count {
        row_ptr[i + 1] += row_ptr[i];
    }

    let mut col_idx = vec![0u32; e_count];
    let mut cursor = row_ptr.clone(); // running insert position per vertex
    for &(u, v) in &edges {
        col_idx[cursor[u as usize]] = v;
        cursor[u as usize] += 1;
    }

    Ok(Csr { v_count, e_count, row_ptr, col_idx })
}
