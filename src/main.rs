//! Command-line runner for Group Betweenness Centrality.

use std::io::{self, Read, Write};
use std::process::ExitCode;
use std::time::Instant;

use gbc::csr::load_csr;
use gbc::gbc::{gbc_parallel, gbc_serial, GbcResult};

fn usage(prog: &str) {
    eprintln!(
        "Usage: {prog} <graph-file> [options]\n\n\
         Options:\n\
        \x20 --group v1,v2,...   group vertices (if omitted, you are prompted)\n\
        \x20 --threads N         worker threads for the parallel run (default: hardware)\n\
        \x20 --mode MODE         both | serial | parallel (default: both)"
    );
}

fn parse_group(csv: &str) -> Result<Vec<i64>, String> {
    csv.split(',')
        .filter(|t| !t.is_empty())
        .map(|t| t.trim().parse::<i64>().map_err(|_| format!("invalid vertex id: {t}")))
        .collect()
}

fn print_result(label: &str, r: &GbcResult, seconds: f64) {
    println!(
        "{label}:\n\
        \x20 GBC (rescaled):   {:.6}\n\
        \x20 GBC (normalized): {:.6}\n\
        \x20 time:             {:.6} s",
        r.rescaled, r.normalized, seconds
    );
}

fn hardware_concurrency() -> usize {
    thread_count_or(1)
}

fn thread_count_or(default: usize) -> usize {
    std::thread::available_parallelism().map(|n| n.get()).unwrap_or(default)
}

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        usage(&args[0]);
        return Err(String::new());
    }

    let graph_path = args[1].clone();
    let mut group_csv: Option<String> = None;
    let mut mode = "both".to_string();
    let mut threads = hardware_concurrency().max(1);

    let mut i = 2;
    while i < args.len() {
        let a = &args[i];
        let mut next = || -> Result<String, String> {
            i += 1;
            args.get(i).cloned().ok_or_else(|| {
                usage(&args[0]);
                String::from("missing argument")
            })
        };
        match a.as_str() {
            "--group" => group_csv = Some(next()?),
            "--threads" => {
                let v: i64 = next()?.parse().map_err(|_| "invalid --threads value".to_string())?;
                threads = v.max(1) as usize;
            }
            "--mode" => mode = next()?,
            other => {
                eprintln!("Unknown option: {other}");
                usage(&args[0]);
                return Err(String::new());
            }
        }
        i += 1;
    }

    let graph = load_csr(&graph_path)?;

    println!("Graph: {graph_path}");
    println!("  vertices:       {}", graph.v_count);
    println!(
        "  directed edges: {} ({} undirected)",
        graph.e_count,
        graph.e_count / 2
    );

    // Resolve the group, either from --group or interactively.
    let members: Vec<i64> = match &group_csv {
        Some(csv) => parse_group(csv)?,
        None => read_group_interactively()?,
    };

    let mut in_group = vec![false; graph.v_count];
    for &v in &members {
        if v < 0 || v >= graph.v_count as i64 {
            return Err(format!("Group vertex out of range: {v}"));
        }
        in_group[v as usize] = true;
    }
    let group_size = in_group.iter().filter(|&&b| b).count();

    if graph.v_count.saturating_sub(group_size) < 2 {
        return Err("Group leaves fewer than two non-group vertices; GBC is undefined.".to_string());
    }

    println!("  group size:     {group_size}\n");

    let run_serial = mode == "both" || mode == "serial";
    let run_parallel = mode == "both" || mode == "parallel";
    if !run_serial && !run_parallel {
        return Err(format!("Invalid --mode: {mode}"));
    }

    let mut serial_res = GbcResult::default();
    let mut parallel_res = GbcResult::default();
    let mut serial_time = 0.0;
    let mut parallel_time = 0.0;

    if run_serial {
        let t0 = Instant::now();
        serial_res = gbc_serial(&graph, &in_group, group_size);
        serial_time = t0.elapsed().as_secs_f64();
        print_result("Serial", &serial_res, serial_time);
    }

    if run_parallel {
        if run_serial {
            println!();
        }
        let t0 = Instant::now();
        parallel_res = gbc_parallel(&graph, &in_group, group_size, threads);
        parallel_time = t0.elapsed().as_secs_f64();
        print_result(&format!("Parallel ({threads} threads)"), &parallel_res, parallel_time);
    }

    if run_serial && run_parallel {
        let speedup = if parallel_time > 0.0 { serial_time / parallel_time } else { 0.0 };
        let diff = (serial_res.rescaled - parallel_res.rescaled).abs();
        println!("\nSpeedup: {speedup:.6}x");
        println!(
            "Serial/parallel agree: {} (|diff| = {diff:.6})",
            if diff < 1e-6 { "yes" } else { "NO" }
        );
    }

    Ok(())
}

fn read_group_interactively() -> Result<Vec<i64>, String> {
    print!("Enter group size: ");
    io::stdout().flush().ok();

    // Read all of stdin once and pull integer tokens, matching the C++ `cin >>`
    // behaviour (whitespace-separated, robust to newlines).
    let mut input = String::new();
    io::stdin().read_to_string(&mut input).map_err(|e| e.to_string())?;
    let mut tokens = input.split_whitespace();

    let gs: i64 = tokens
        .next()
        .and_then(|t| t.parse().ok())
        .filter(|&v| v >= 0)
        .ok_or_else(|| "Invalid group size.".to_string())?;
    println!("Enter {gs} vertex id(s):");

    let mut members = Vec::with_capacity(gs as usize);
    for _ in 0..gs {
        let v: i64 = tokens
            .next()
            .and_then(|t| t.parse().ok())
            .ok_or_else(|| "Invalid vertex id.".to_string())?;
        members.push(v);
    }
    Ok(members)
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(msg) => {
            if !msg.is_empty() {
                eprintln!("Error: {msg}");
            }
            ExitCode::FAILURE
        }
    }
}
