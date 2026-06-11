//! Utility: read a graph edge list and emit it in the canonical format expected
//! by this project — a `v_count / e_count` header followed by every directed edge
//! (both directions), deduplicated and sorted by source then target.
//!
//! Accepts input whether each undirected edge is listed once or twice, with
//! commas or whitespace as separators.
//!
//!   Usage: normalize-graph <input-file> [output-file]   (default output: stdout)

use std::collections::BTreeSet;
use std::fs;
use std::io::{self, Write};
use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <input-file> [output-file]", args[0]);
        return ExitCode::FAILURE;
    }

    let text = match fs::read_to_string(&args[1]) {
        Ok(t) => t,
        Err(_) => {
            eprintln!("Error: cannot open {}", args[1]);
            return ExitCode::FAILURE;
        }
    };

    let mut tokens = text.split(|c: char| c == ',' || c.is_whitespace()).filter(|t| !t.is_empty());

    // Header: vertex count, then an edge count we ignore (recomputed below).
    let v_count: i64 = tokens.next().and_then(|t| t.parse().ok()).unwrap_or(0);
    let _edge_count: i64 = tokens.next().and_then(|t| t.parse().ok()).unwrap_or(0);

    let mut directed: BTreeSet<(i64, i64)> = BTreeSet::new(); // sorted + deduplicated
    while let (Some(u), Some(v)) = (tokens.next(), tokens.next()) {
        let (u, v): (i64, i64) = match (u.parse(), v.parse()) {
            (Ok(u), Ok(v)) => (u, v),
            _ => continue,
        };
        if u == v {
            continue;
        }
        directed.insert((u, v));
        directed.insert((v, u)); // enforce symmetry
    }

    let out: Box<dyn Write> = if args.len() >= 3 {
        match fs::File::create(&args[2]) {
            Ok(f) => Box::new(io::BufWriter::new(f)),
            Err(_) => {
                eprintln!("Error: cannot write {}", args[2]);
                return ExitCode::FAILURE;
            }
        }
    } else {
        Box::new(io::BufWriter::new(io::stdout()))
    };
    let mut out = out;

    let _ = writeln!(out, "{}", v_count);
    let _ = writeln!(out, "{}", directed.len() / 2);
    for (u, v) in &directed {
        let _ = writeln!(out, "{u},{v}");
    }
    let _ = out.flush();

    eprintln!(
        "Wrote {} directed edges ({} undirected).",
        directed.len(),
        directed.len() / 2
    );
    ExitCode::SUCCESS
}
