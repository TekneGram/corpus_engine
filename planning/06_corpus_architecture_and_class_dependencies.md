# Corpus Architecture: File Responsibilities and Class Dependency Graph

------------------------------------------------------------------------

# High-Level Architectural View

Your system has five major runtime subsystems:

1.  Parse Layer → Produces `ParsedDocument`
2.  Corpus Representation Layer → Writes core binary corpus
3.  Search Layer → Builds inverted indexes
4.  Sparse Matrix Layer (Analytics Engine) → Builds CSR matrices
5.  Shared Layer → Binary format primitives

The architecture enforces strict separation between feature-first
(search) and document-first (scientific computing) layers.

------------------------------------------------------------------------

# 1. parse_layer/

This layer converts raw text into structured token data.

It produces one core structure:

    struct ParsedDocument;

That struct is the only thing downstream layers depend on.

------------------------------------------------------------------------

## parsed_document.hpp

### Responsibility

Defines the in-memory representation of a parsed document before binary
writing.

It contains:

-   Flat token vectors
-   Sentence boundaries
-   Document metadata
-   Dependency relations

Example structure:

``` cpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct ParsedToken {
    std::string word;
    std::string lemma;
    uint8_t pos_id;
    uint32_t head;        // local sentence index
    uint8_t deprel_id;
};

struct ParsedDocument {
    std::vector<ParsedToken> tokens;
    std::vector<uint32_t> sentence_starts;
    uint32_t document_id;
};
```

Important:

-   This layer still uses strings.
-   Downstream layers convert to integer IDs.

No .cpp is needed unless helper methods are added.

------------------------------------------------------------------------

## text_file_reader.hpp / .cpp

### Responsibility

Reads raw UTF-8 text files.

Minimal class:

``` cpp
class TextFileReader {
public:
    std::string read_file(const std::string& path);
};
```

Implementation:

-   Uses std::ifstream
-   Returns full file content

This layer does NOT parse.

------------------------------------------------------------------------

## udpipe_parser.hpp / .cpp

### Responsibility

Wrap UDPipe model and produce `ParsedDocument`.

Header:

``` cpp
class UDPipeParser {
public:
    explicit UDPipeParser(const std::string& model_path);

    ParsedDocument parse(const std::string& text,
                         uint32_t document_id);

private:
    void* model_; // or actual UD model type
};
```

Implementation:

-   Loads model once
-   Converts UDPipe output into tokens, sentence boundaries, and local
    dependency heads

Dependency heads must later be converted to global token positions.

------------------------------------------------------------------------

# 2. corpus_representation_layer/

This layer writes the core binary corpus:

-   word.bin
-   lemma.bin
-   pos.bin
-   deprel.bin
-   head.bin
-   sentence_bounds.bin
-   doc_ranges.bin
-   token_doc.bin

------------------------------------------------------------------------

## core_token_layer.hpp / .cpp

### Responsibility

Writes parallel token arrays:

-   word_id
-   lemma_id
-   pos_id
-   head (global)
-   deprel_id

Design:

``` cpp
class CoreTokenLayer {
public:
    CoreTokenLayer(const std::string& output_dir);

    void append_document(const ParsedDocument& doc);

    void finalize();

private:
    std::ofstream word_out_;
    std::ofstream lemma_out_;
    std::ofstream pos_out_;
    std::ofstream head_out_;
    std::ofstream deprel_out_;

    uint32_t global_token_counter_;
};
```

Important:

-   Maintain running global_token_counter\_
-   Convert local sentence heads → global positions
-   Write each token in aligned order across all files

------------------------------------------------------------------------

## dictionary_builder.hpp

### Responsibility

Builds:

-   word.lexicon.bin
-   lemma.lexicon.bin
-   pos.lexicon.bin
-   deprel.lexicon.bin

Structure:

``` cpp
class DictionaryBuilder {
public:
    uint32_t get_word_id(const std::string&);
    uint32_t get_lemma_id(const std::string&);

    void write_lexicons(const std::string& output_dir);

private:
    std::unordered_map<std::string, uint32_t> word_map_;
    std::vector<std::string> word_reverse_;
};
```

------------------------------------------------------------------------

## structural_layer.hpp

### Responsibility

Writes:

-   sentence_bounds.bin
-   doc_ranges.bin
-   token_doc.bin

Structure:

``` cpp
class StructuralLayer {
public:
    void append_document(uint32_t doc_id,
                         uint32_t token_start,
                         uint32_t token_end,
                         const std::vector<uint32_t>& sentence_starts);

    void finalize();
};
```

------------------------------------------------------------------------

## metadata_writer.hpp

### Responsibility

Writes SQLite database:

-   corpus.db

It is isolated from binary logic.

------------------------------------------------------------------------

# 3. corpus_search_layer/

This layer builds inverted indexes.

------------------------------------------------------------------------

## inverted_index_builder.hpp

### Responsibility

Builds:

-   lemma.index.header
-   lemma.index.positions

Structure:

``` cpp
class InvertedIndexBuilder {
public:
    void build_lemma_index(const std::string& corpus_dir);
};
```

Algorithm:

1.  Read lemma.bin
2.  Accumulate lemma_id → vector`<token_position>`{=html}
3.  Sort vectors
4.  Write header and flattened positions array

------------------------------------------------------------------------

## dependency_index_builder.hpp

### Responsibility

Build reverse dependency index:

head → child positions

Algorithm:

1.  Read head.bin
2.  For each token position i:
    -   head = head\[i\]
    -   push i into children\[head\]
3.  Write inverted structure

------------------------------------------------------------------------

## ngram_builder.hpp

### Responsibility

Build:

-   n-gram lexicon
-   n-gram postings
-   optional n-gram docfreq

Input source:

-   `word.bin` (word IDs, sentence-bounded sliding windows)

Template structure:

``` cpp
template <size_t N>
class NGramBuilder {
public:
    void build(const std::string& corpus_dir);
};
```

------------------------------------------------------------------------

## docfreq_builder.hpp

### Responsibility

Build:

feature → (doc_id, count)

Operates document-by-document.

Public interface:

``` cpp
class DocFreqBuilder {
public:
    void build_lemma_docfreq(const std::string& corpus_dir) const;
    void build_word_docfreq(const std::string& corpus_dir) const;
};
```

------------------------------------------------------------------------

# 4. corpus_computing_engine/

Scientific computing core.

------------------------------------------------------------------------

## sparse_matrix_builder.hpp

### Responsibility

Build:

-   .spm.meta.bin
-   .spm.offsets.bin
-   .spm.entries.bin

Structure:

``` cpp
class SparseMatrixBuilder {
public:
    void build_lemma_matrix(const std::string& corpus_dir);

private:
    void write_meta(uint64_t num_docs,
                    uint64_t num_features,
                    uint64_t nnz);
};
```

Core logic:

1.  Read docfreq postings
2.  Build CSR layout
3.  Write meta header, offsets\[D+1\], entries\[NNZ\]

No compression. Raw 8-byte entries.

------------------------------------------------------------------------

# 5. corpus_shared_layer/

Shared binary primitives.

------------------------------------------------------------------------

## binary_header.hpp

``` cpp
struct IndexHeaderEntry {
    uint64_t offset;
    uint32_t length;
};

struct SparseMatrixMeta {
    uint32_t magic;
    uint32_t version;
    uint64_t num_docs;
    uint64_t num_features;
    uint64_t num_nonzero;
    uint32_t entry_size_bytes;
    uint32_t reserved;
};
```

------------------------------------------------------------------------

## binary_writer.hpp

``` cpp
class BinaryWriter {
public:
    explicit BinaryWriter(const std::string& path);

    template<typename T>
    void write(const T& value);

    void write_raw(const void* data, size_t size);
};
```

------------------------------------------------------------------------

## corpus_types.hpp

``` cpp
using token_id_t = uint32_t;
using lemma_id_t = uint32_t;
using pos_id_t   = uint8_t;
```

------------------------------------------------------------------------

# Class Dependency Graph

------------------------------------------------------------------------

## Orchestration Layer

    CorpusEngine
        ├── UDPipeParser
        ├── TextFileReader
        ├── DictionaryBuilder
        ├── CoreTokenLayer
        ├── StructuralLayer
        ├── MetadataWriter
        ├── InvertedIndexBuilder
        ├── DependencyIndexBuilder
        ├── NGramBuilder<N>
        ├── DocFreqBuilder
        └── SparseMatrixBuilder

CorpusEngine is the only class allowed to see everything.

------------------------------------------------------------------------

## Parse Layer

    TextFileReader
        (no dependencies)

    UDPipeParser
        └── ParsedDocument

    ParsedDocument
        (pure data struct)

------------------------------------------------------------------------

## Corpus Representation Layer

    CoreTokenLayer
        ├── DictionaryBuilder
        ├── StructuralLayer
        └── BinaryWriter

    StructuralLayer
        └── BinaryWriter

    DictionaryBuilder
        └── BinaryWriter

    MetadataWriter
        └── SQLite

------------------------------------------------------------------------

## Search Layer

    InvertedIndexBuilder
        ├── BinaryWriter
        ├── BinaryHeader
        └── corpus_types

    DependencyIndexBuilder
        ├── BinaryWriter
        └── BinaryHeader

    NGramBuilder<N>
        ├── BinaryWriter
        └── BinaryHeader

    DocFreqBuilder
        ├── BinaryWriter
        └── corpus_types

------------------------------------------------------------------------

## Sparse Matrix Layer

    SparseMatrixBuilder
        ├── DocFreqBuilder (read output)
        ├── BinaryWriter
        ├── BinaryHeader
        └── corpus_types

------------------------------------------------------------------------

## Shared Layer

    BinaryWriter
        └── <fstream>

    BinaryHeader
        └── corpus_types

    corpus_types
        (no dependencies)

------------------------------------------------------------------------

# Compile-Time Dependency Rules

1.  Search layer must NOT include sparse matrix headers.
2.  Sparse matrix layer must NOT include inverted index headers.
3.  Parse layer must not include representation, search, or computing
    layers.
4.  Only CorpusEngine includes everything.

------------------------------------------------------------------------

# Runtime Data Flow

    TEXT FILE
        ↓
    UDPipeParser
        ↓
    ParsedDocument
        ↓
    CoreTokenLayer + StructuralLayer
        ↓
    Columnar binaries
        ↓
    Search Builders
        ↓
    DocFreqBuilder
        ↓
    SparseMatrixBuilder
        ↓
    MonteCarloEngine

Search and SparseMatrix are siblings, not parent-child.

------------------------------------------------------------------------

# Core Conceptual Separation

Feature-first world:

    feature → positions

Document-first world:

    doc → (feature, count)

These two worlds must never mix at class level.

Only CorpusEngine coordinates them.
