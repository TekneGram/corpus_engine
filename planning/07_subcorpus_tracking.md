# 07 Subcorpus and Semantic Tracking

## 1. Current implementation summary

The metadata layer is now fully generic and no longer hardcodes corpus-specific fields such as `language` and `level`.

### 1.1 Generic folder path metadata (Phase 1)

The pipeline stores folder structure as ordered path segments per document.

SQLite tables:

- `document(document_id, title, author, relative_path)`
- `folder_segment(segment_id, segment_text)`
- `document_segment(document_id, depth, segment_id)`

Key properties:

- `relative_path` is persisted for every document.
- Folder hierarchy is represented as `(document_id, depth, segment_id)`.
- Works with arbitrary directory depths and names.
- No assumptions about meaning of folders.

### 1.2 User-defined semantics from folder names (Phase 2)

The pipeline now supports rule-driven semantic mapping at build time and materializes results into normalized tables.

SQLite tables:

- `semantic_key(key_id, key_name)`
- `semantic_value(value_id, key_id, value_text)`
- `document_group(document_id, key_id, value_id)`

Rule file support:

- Optional 4th CLI argument: `semantics_rules_path`
- Rule format (tab-separated):
  - `key<TAB>depth<TAB>N`
  - `key<TAB>regex<TAB>pattern<TAB>capture_group`
- First matching rule per key is assigned for a given document.

Result:

- Semantics are user-defined and derived from folder names/patterns.
- Query/sampling layers can filter by `key=value` without corpus-specific schema changes.

## 2. Binary metadata filter layer for scientific-computing speed (Phase 3)

To keep Monte Carlo hot paths free of SQL, semantic filters are now exported to binary artifacts during corpus build.

Generated binary files:

- `semantic.key.lexicon.bin`
  - `key_id -> key_name`
- `semantic.value.lexicon.bin`
  - `value_id -> (key_id, value_text)`
- `semantic.value_doc.header`
- `semantic.value_doc.entries`
  - inverted postings: `value_id -> [doc_id...]`
- `semantic.doc_groups.offsets.bin`
- `semantic.doc_groups.entries.bin`
  - CSR-style per-document semantic assignments: `doc_id -> [(key_id, value_id)...]`

### Runtime intent

- Build candidate document sets via memory-mapped postings/CSR data.
- Perform intersections and sampling in memory.
- Keep SQLite in control-plane usage (metadata management, inspection), not in Monte Carlo inner loops.

This aligns semantic filtering with the same integer-array, cache-friendly design principles used in the sparse matrix analytics layer.
