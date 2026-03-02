# Dual-Layer Architecture Summary

Generated: 2026-03-01

A dual-layer architecture is necessary because search and large-scale
Monte Carlo simulation have fundamentally different computational access
patterns: search requires a feature-first inverted index (feature →
positions) for fast lookup, concordance retrieval, CQL matching, and
dependency traversal, while repeated statistical resampling requires a
document-first sparse matrix layout (doc → feature counts) to enable
sequential, cache-friendly summation without rescanning posting lists or
reconstructing tokens. Keeping these two layers separate aligns memory
layout with computation type, prevents cache-thrashing and redundant
work during tens of thousands of simulations, and transforms the system
from a text-processing tool into a scalable numerical corpus analytics
engine.
