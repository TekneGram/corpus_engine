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
builds `corpus_build_pipeline`.

## Run

```bash
./corpus_build_pipeline \
  models/english-partut-ud-2.5-191206.udpipe \
  _demo_corpus \
  corpus_output
```

Outputs include core token binaries, structure files, dictionaries,
indexes, docfreq, and sparse-matrix files in `corpus_output/`.

The pipeline now emits:

- Core token binaries:
  - `word.bin`, `lemma.bin`, `pos.bin`, `head.bin`, `deprel.bin`
- Structure binaries:
  - `sentence_bounds.bin`, `doc_ranges.bin`, `word_doc.bin`
- Dictionaries:
  - `word.lexicon.bin`, `lemma.lexicon.bin`, `pos.lexicon.bin`, `deprel.lexicon.bin`
- Search/docfreq families:
  - `word`, `lemma`, `2gram`, `3gram`, `4gram`
- Sparse matrices:
  - `word.spm.*`, `lemma.spm.*`, `2gram.spm.*`, `3gram.spm.*`, `4gram.spm.*`

`2gram`, `3gram`, and `4gram` feature IDs are generated from aligned
windows over `word.bin` and `pos.bin`, not `lemma.bin`.

Each n-gram family emits:

- `*gram.lexicon.bin` for the word tuple
- `*gram.pos.lexicon.bin` for the aligned POS tuple

`*gram.freq.bin`, `*gram.docfreq.*`, `*gram.index.*`, and `*gram.spm.*`
all refer to the combined word+POS feature ID space.

Optional semantic mapping rules can be supplied as a 4th argument:

```bash
./corpus_build_pipeline \
  models/english-partut-ud-2.5-191206.udpipe \
  _demo_corpus \
  corpus_output \
  /path/to/semantic_rules.tsv
```

When semantic rules are provided, metadata and binary filter artifacts
for document groups are generated (for example: `semantic.key.lexicon.bin`,
`semantic.value.lexicon.bin`, `semantic.value_doc.*`,
`semantic.doc_groups.*`).

Optional output controls:

- 5th CLI arg: posting/docfreq format, `raw` or `compressed`
- 6th CLI arg: emit n-gram positions, anything except `false` means enabled

Example:

```bash
./corpus_build_pipeline \
  models/english-partut-ud-2.5-191206.udpipe \
  _demo_corpus \
  corpus_output \
  "" \
  compressed \
  true
```

The JSON mode also accepts:

- `postingFormat`: `"raw"` or `"compressed"`
- `emitNgramPositions`: `true` or `false`

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
