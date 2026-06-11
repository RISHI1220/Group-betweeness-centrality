//! Group Betweenness Centrality (GBC).
//!
//! Compute, for a set of vertices `S` in an undirected graph, how many shortest
//! paths between the remaining vertices are routed *through* the group. Both a
//! serial and a multithreaded implementation are provided; they compute the same
//! value.

pub mod csr;
pub mod gbc;

pub use csr::Csr;
pub use gbc::{gbc_parallel, gbc_serial, GbcResult};
