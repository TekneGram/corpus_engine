# Corpus Architecture Pipeline (C++11)

This project follows the layered architecture in
`planning/06_corpus_architecture_and_class_dependencies.md`.

## Layout

- `parse_layer/`
- `corpus_shared_layer/`
- `corpus_representation_layer/`
- `corpus_search_layer/`
- `corpus_computing_engine/`
- `orchestration_layer/`

## Requirements

- UDPipe source in `./udpipe`
- UDPipe model in `./models/english-partut-ud-2.5-191206.udpipe`
- SQLite3 development library

## Getting UDPipe and Model Files

This repository does not include `udpipe/` source or model binaries.

- UDPipe source code:
  - https://github.com/ufal/udpipe
  - Clone/copy into `./udpipe`
- Example English UDPipe model:
  - https://lindat.mff.cuni.cz/repository/items/41f05304-629f-4313-b9cf-9eeb0a2ca7c6
  - Place the model file at:
    `./models/english-partut-ud-2.5-191206.udpipe`

## Build

```bash
./tools/build.sh
```

This script builds UDPipe (`udpipe/src/libudpipe.a`) if needed and then
builds `corpus_pipeline`.

## Run

```bash
./corpus_pipeline \
  models/english-partut-ud-2.5-191206.udpipe \
  _demo_corpus \
  corpus_output
```

Outputs include core token binaries, structure files, indexes, docfreq,
and sparse-matrix files in `corpus_output/`.

Optional semantic mapping rules can be supplied as a 4th argument:

```bash
./corpus_pipeline \
  models/english-partut-ud-2.5-191206.udpipe \
  _demo_corpus \
  corpus_output \
  /path/to/semantic_rules.tsv
```

When semantic rules are provided, metadata and binary filter artifacts
for document groups are generated (for example: `semantic.key.lexicon.bin`,
`semantic.value.lexicon.bin`, `semantic.value_doc.*`,
`semantic.doc_groups.*`).

## CI And Commit Checks

Run all checks manually:

```bash
./tools/run_ci_checks.sh
```

Install local git hooks once per clone:

```bash
./tools/install_git_hooks.sh
```

This sets `core.hooksPath=.githooks` and enables the starter hook set:
`pre-commit`, `pre-push`, `commit-msg`, and `post-merge`.

Optional heavier checks in hooks can be enabled with:

- `RUN_SMOKE_CHECKS=1` for `pre-commit` smoke pipeline run
- `RUN_INTEGRATION_CHECKS=1` for `pre-push` integration pipeline run
