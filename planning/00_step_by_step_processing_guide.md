# Corpus Builder Step-by-Step Processing Guide

This guide documents the full data-transformation pipeline starting at `RunCorpusPipeline` in `native/corpus_builder/main.cpp` and continuing through `CorpusEngine::run` in `native/corpus_builder/orchestration_layer/corpus_engine.cpp`.

The focus is on:
- how data changes shape from step to step,
- which layer/function performs each transformation,
- why the step exists,
- which artifacts are produced.

Path/storage location details are intentionally minimized; artifact filenames are included.

## 1) External build request -> typed pipeline invocation

### 1) Entry request normalization

Layer: Entry/Interface layer (`main.cpp`)
Function: `RunJsonMode` / `RunCliMode` -> `RunCorpusPipeline`

Input (example):

```json
{
  "command": "buildCorpus",
  "modelPath": "english.udpipe",
  "inputPath": "/texts",
  "outputDir": "/out",
  "semanticsRulesPath": "/rules.tsv",
  "postingFormat": "raw",
  "emitNgramPositions": true
}
```

Output to next layer (typed call):

```cpp
CorpusEngine::run(
  model_path,
  input_text_path,
  output_dir,
  semantics_rules_path,
  BuildOptions,
  progress_emitter
)
```

Purpose: Convert JSON/CLI payloads into typed runtime options and dispatch a validated corpus build command.

Outputs:
- None (control handoff only)

## 2) Input path resolution -> ordered file list

### 2) Input discovery

Layer: Orchestration layer (`corpus_engine.cpp`)
Function: `ResolveInputFiles` (uses `CollectFilesRecursive`, `IsDirectory`, `IsRegularFile`)

Input:
- `input_text_path` (file or directory)

Output shape:

```cpp
std::vector<InputFile>
```

Example:

```cpp
[
  { absolute_path: ".../S_courtroom/doc1.txt", relative_path: "S_courtroom/doc1.txt" },
  { absolute_path: ".../S_tutorial/doc2.txt",  relative_path: "S_tutorial/doc2.txt"  }
]
```

Purpose: Build a deterministic, recursively discovered file list so doc IDs and token offsets are reproducible.

Outputs:
- None (in-memory file list)

## 3) Semantic rules text -> compiled rule objects

### 3) Semantic rules compilation

Layer: Orchestration layer (`corpus_engine.cpp`)
Function: `LoadSemanticRules`

Input (TSV example):

```text
register	depth	0
genre	regex	^(S_[^/]+)	1
```

Output shape:

```cpp
std::vector<SemanticRule>
```

Example:
- `{ type: kDepth, key_name: "register", depth: 0 }`
- `{ type: kRegex, key_name: "genre", pattern: "^(S_[^/]+)", capture_group: 1 }`

Purpose: Pre-compile semantic grouping logic (depth/regex) used for document-group metadata and semantic filter artifacts.

Outputs:
- None (in-memory rule objects)

## 4) File bytes -> raw document string

### 4) Raw file read

Layer: Parse layer (`text_file_reader.cpp`)
Function: `TextFileReader::read_file`

Input:
- `InputFile.absolute_path`

Output shape:

```cpp
std::string
```

Example:

```text
"The quick brown fox jumps over the dog."
```

Purpose: Load source text into memory for linguistic parsing.

Outputs:
- None (in-memory text)

## 5) Raw text -> parsed linguistic document

### 5) UDPipe parsing

Layer: Parse layer (`udpipe_parser.cpp`)
Function: `UDPipeParser::parse`

Input:
- raw text string
- `document_id`

Output shape:

```cpp
ParsedDocument {
  document_id: DocId,
  sentence_starts: std::vector<uint32_t>,
  tokens: std::vector<ParsedToken>
}
```

Example:

```cpp
ParsedDocument {
  document_id: 0,
  sentence_starts: [0],
  tokens: [
    {word:"The",   lemma:"the",   pos_id:6,  head:2, deprel_id:16},
    {word:"quick", lemma:"quick", pos_id:1,  head:4, deprel_id:4},
    {word:"fox",   lemma:"fox",   pos_id:8,  head:4, deprel_id:27},
    {word:"jumps", lemma:"jump",  pos_id:16, head:0, deprel_id:35}
  ]
}
```

Purpose: Convert plain text into tokenized, tagged, lemmatized, dependency-parsed structures that can be encoded as numeric corpus binaries.

Outputs:
- None (in-memory parsed document)

## 6) Parsed tokens -> core numeric token streams

### 6) Core token encoding and dictionary assignment

Layer: Representation layer (`core_token_layer.cpp` + `dictionary_builder.cpp`)
Function: `CoreTokenLayer::append_document` + `DictionaryBuilder::get_word_id` / `get_lemma_id`

Input:
- `ParsedDocument`

Output shape:
- aligned position-based arrays where each token position has numeric attributes:
  - `word_id`
  - `lemma_id`
  - `pos_id`
  - `global_head`
  - `deprel_id`

Example transformation for one token:

Input token:

```cpp
{word:"jumps", lemma:"jump", pos_id:16, head:0, deprel_id:35}
```

Output row at token position `i`:

```cpp
word[i]=42, lemma[i]=17, pos[i]=16, head[i]=4294967295, deprel[i]=35
```

(`4294967295` = `UINT32_MAX`, used for root)

Purpose: Build the primary position-oriented corpus representation and convert strings to stable integer IDs for compact storage and indexing.

Outputs:
- `word.bin`
- `lemma.bin`
- `pos.bin`
- `head.bin`
- `deprel.bin`

## 7) Document/sentence structure -> structural binaries

### 7) Structural encoding

Layer: Representation layer (`structural_layer.cpp`)
Function: `StructuralLayer::append_document`

Input:
- `doc_id`
- `token_start`, `token_end`
- `sentence_starts` (document-local)
- `global_token_base`

Output shape:
- doc ranges: `(start,end)` per document
- sentence starts: global token start of each sentence
- token->document map: one doc ID per token position

Example:

```text
doc_ranges:      [(0,5), (5,10)]
sentence_bounds: [0,5]
word_doc:        [0,0,0,0,0,1,1,1,1,1]
```

Purpose: Preserve corpus structure required by doc-level counts, sentence-safe n-gram extraction, and per-doc analytics.

Outputs:
- `doc_ranges.bin`
- `sentence_bounds.bin`
- `word_doc.bin`

## 8) Relative paths + rules -> metadata/semantic assignments

### 8) Metadata and semantic group persistence

Layer: Representation + Orchestration (`metadata_writer.cpp` + semantic logic in `corpus_engine.cpp`)
Function:
- `MetadataWriter::upsert_document`
- `MetadataWriter::upsert_folder_segment`
- `MetadataWriter::upsert_document_segment`
- `AssignSemanticFromRules`
- `MetadataWriter::upsert_semantic_key`
- `MetadataWriter::upsert_semantic_value`
- `MetadataWriter::upsert_document_group`

Input:
- `document_id`
- `relative_path` and path segments
- semantic rules

Output shape:
- normalized relational metadata:
  - documents
  - folder segments and doc->segment(depth) links
  - semantic keys/values
  - doc->(semantic key,value) links

Example semantic assignment:

```cpp
semantic_assignments = {
  "register": "S_courtroom",
  "genre": "S_courtroom"
}
```

Converted ID example:

```cpp
key_id("register") = 0
value_id((0, "S_courtroom")) = 0
```

Purpose: Persist hierarchy and semantic grouping metadata that supports grouping/filtering and later metadata-driven pipelines.

Outputs:
- `corpus.db`

## 9) Semantic maps -> semantic filter binary indexes

### 9) Semantic filter artifact build

Layer: Orchestration layer helper (`corpus_engine.cpp`)
Function: `WriteSemanticFilterArtifacts`

Input:
- key lexicon map
- value lexicon map
- value postings (`value_id -> [doc_ids]`)
- doc groups (`doc_id -> [(key_id,value_id)]`)

Output shape:
- semantic key lexicon
- semantic value lexicon
- value->doc postings index
- doc->(key,value) group rows

Purpose: Materialize semantic filtering structures as fast binary artifacts instead of requiring SQLite joins during query-time filtering.

Outputs:
- `semantic.key.lexicon.bin`
- `semantic.value.lexicon.bin`
- `semantic.value_doc.header`
- `semantic.value_doc.entries`
- `semantic.doc_groups.offsets.bin`
- `semantic.doc_groups.entries.bin`

## 10) Dictionary state -> persisted lexicons

### 10) Lexicon artifact writing

Layer: Representation layer (`dictionary_builder.cpp`)
Function: `DictionaryBuilder::write_lexicons`

Input:
- `word_reverse_` (`word_id -> string`)
- `lemma_reverse_` (`lemma_id -> string`)
- static POS and deprel label arrays

Output shape:
- id->label lexicon files

Purpose: Make numeric core arrays interpretable and searchable by preserving reverse dictionaries.

Outputs:
- `word.lexicon.bin`
- `lemma.lexicon.bin`
- `pos.lexicon.bin`
- `deprel.lexicon.bin`

## 11) Position arrays -> inverted indexes

### 11) Word/Lemma/POS inverted index build

Layer: Search layer (`inverted_index_builder.cpp`)
Function:
- `InvertedIndexBuilder::build_word_index`
- `InvertedIndexBuilder::build_lemma_index`
- `InvertedIndexBuilder::build_pos_index`

Input:
- one dense value stream (`word.bin`, `lemma.bin`, or `pos.bin`)

Output shape:
- postings: `feature_id -> [token_positions]`

Example:

```text
value stream by token pos: [7,3,7,9]
postings:
  3 -> [1]
  7 -> [0,2]
  9 -> [3]
```

Purpose: Build reverse lookup structures needed for fast concordance/search retrieval.

Outputs:
- `word.index.header`
- `word.index.positions`
- `lemma.index.header`
- `lemma.index.positions`
- `pos.index.header`
- `pos.index.positions`

## 12) Head array -> dependency child index

### 12) Dependency index build

Layer: Search layer (`dependency_index_builder.cpp`)
Function: `DependencyIndexBuilder::build`

Input:
- `head.bin` (global head pointer per token; root sentinel for no head)

Output shape:
- postings: `head_token_pos -> [child_token_positions]`

Example:

```text
head: [MAX, 0, 1, 1]
children:
  0 -> [1]
  1 -> [2,3]
```

Purpose: Enable dependency traversal queries by indexing inverse head-child relations.

Outputs:
- `dep.index.header`
- `dep.index.positions`

## 13) Sentence-scoped token windows -> n-gram feature families

### 13) N-gram extraction and n-gram docfreq derivation

Layer: Search layer (`ngram_builder.hpp`)
Function:
- `NGramBuilder<2>::build`
- `NGramBuilder<3>::build`
- `NGramBuilder<4>::build`

Input:
- `word.bin`
- `pos.bin`
- `sentence_bounds.bin`
- `word_doc.bin`
- `doc_ranges.bin`

Output shape:
- n-gram lexicons (word tuple and POS tuple)
- n-gram frequencies
- optional n-gram position postings
- `FeatureRowsResult` per n-gram family:

```text
rows[doc_id] = [(feature_id,count), ...]
```

Example feature:
- 2-gram window: word IDs `[42,17]`, POS IDs `[6,8]` -> `feature_id=5`

Purpose: Create higher-order lexical features (2/3/4-grams) and their per-document statistics for robust corpus analysis.

Outputs (for each `N` in `2,3,4`):
- `Ngram.lexicon.bin`
- `Ngram.pos.lexicon.bin`
- `Ngram.freq.bin`
- `Ngram.docfreq.header`
- `Ngram.docfreq.entries`
- Optional if `emitNgramPositions=true`:
  - `Ngram.index.header`
  - `Ngram.index.positions`

## 14) Unigram streams + doc ranges -> unigram docfreq indexes

### 14) Word/Lemma docfreq build

Layer: Search layer (`docfreq_builder.cpp`)
Function:
- `DocFreqBuilder::build_word_docfreq`
- `DocFreqBuilder::build_lemma_docfreq`

Input:
- `word.bin` or `lemma.bin`
- `doc_ranges.bin`

Output shape:
- postings: `feature_id -> [{doc_id,count}, ...]`
- plus `FeatureRowsResult` rows:

```text
rows[doc_id] = [(feature_id,count), ...]
```

Example:

```text
doc0 words: [7,7,3], doc1 words:[3,9]
postings:
  3 -> [{doc:0,count:1}, {doc:1,count:1}]
  7 -> [{doc:0,count:2}]
  9 -> [{doc:1,count:1}]
```

Purpose: Build per-document unigram count indexes for statistical analysis and sparse matrix generation.

Outputs:
- `word.docfreq.header`
- `word.docfreq.entries`
- `lemma.docfreq.header`
- `lemma.docfreq.entries`

## 15) Feature rows -> sparse matrix artifacts

### 15) Feature rows -> sparse matrix artifacts

Layer: Computing layer (`sparse_matrix_builder.cpp` + shared `artifact_builders.cpp`)
Function: `SparseMatrixBuilder::build_matrix` -> `WriteSparseMatrixArtifacts`

Input: `FeatureRowsResult`

```text
rows[doc_id] = [(feature_id,count), ...]
```

Output shape (CSR-like):

- metadata: `num_docs`, `num_features`, `nnz`
- offsets: `doc -> start/end in flat entries`
- entries: flat list of `(feature_id, count)`

Example:

```text
rows:
  doc0: [(3,2),(8,1)]
  doc1: [(3,1)]

offsets: [0,2,3]
entries: [(3,2),(8,1),(3,1)]
```

Purpose: Convert per-document feature rows into compact sparse matrix artifacts for fast downstream numerical analysis.

Outputs (for each feature family):

- Word:
  - `word.spm.meta.bin`
  - `word.spm.offsets.bin`
  - `word.spm.entries.bin`
- Lemma:
  - `lemma.spm.meta.bin`
  - `lemma.spm.offsets.bin`
  - `lemma.spm.entries.bin`
- 2-gram:
  - `2gram.spm.meta.bin`
  - `2gram.spm.offsets.bin`
  - `2gram.spm.entries.bin`
- 3-gram:
  - `3gram.spm.meta.bin`
  - `3gram.spm.offsets.bin`
  - `3gram.spm.entries.bin`
- 4-gram:
  - `4gram.spm.meta.bin`
  - `4gram.spm.offsets.bin`
  - `4gram.spm.entries.bin`

## Final pipeline outcome

The complete build transforms raw text corpora into:
- core position-oriented binaries (`word.bin`, `lemma.bin`, `pos.bin`, `head.bin`, `deprel.bin`),
- structural binaries (`doc_ranges.bin`, `sentence_bounds.bin`, `word_doc.bin`),
- lexical dictionaries (`*.lexicon.bin`),
- reverse indexes (`*.index.*`, `*.docfreq.*`, `dep.index.*`),
- n-gram feature artifacts (`2gram/3gram/4gram.*`),
- sparse matrix artifacts (`*.spm.*`),
- and relational metadata (`corpus.db`).
