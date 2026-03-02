#pragma once

#include <cstddef>
#include <cstdint>

namespace teknegram {

    /*
        Metadata record for one block in a binary file.
        offset: byte position where that section starts in the file
        length: size of that section (bytes)
        Allows reader to jump directory to a section (seek to offset) and know how much data to read (length) instead of scanning the whole binary file
    */
    struct IndexHeaderEntry {
        std::uint64_t offset;
        std::uint32_t length;
    };

    // Total comes to 64 bytes. See 05_cpu_memory_and_monte_carlo_reference.md, from line 77.
    struct SparseMatrixMeta {
        std::uint32_t magic;            // a magic number to identify the file format, 4 bytes
        std::uint32_t version;          // version of the file format, 4 bytes
        std::uint64_t num_docs;         // number of documents / rows, 8 bytes
        std::uint64_t num_features;     // number of features / columns, 8 bytes
        std::uint64_t num_nonzero;      // number of non-zero entries in the matrix, 8 bytes
        std::uint32_t entry_size_bytes; // how many total entries are stored, 4 bytes
        std::uint32_t reserved;         // reserved for future use (kept for alignment with CPU reading length), 4 bytes
        std::uint8_t padding[24];       // extra padding by bytes, 24 bytes
    };

    /*
        A posting item for a term in a document-frequency list.
        Used in an inverted index
        A term maps to many DocFreqEntry record so we can answer
            - which documents contain this term?
            - how frequeny is it per document?
    */
    struct DocFreqEntry {
        std::uint32_t doc_id;   // which document contains the term
        std::uint32_t count;    // how many times that term appears in that document
    };

    static const std::uint32_t kSparseMatrixMagic = 0x53504D31U; // "SPM1" - fixed signature written in the file header; reader checks it to confirm this is the required file type.
    static const std::uint32_t kSparseMatrixVersion = 1U; // ensures compatibility.
    static_assert(sizeof(SparseMatrixMeta) == 64U, "SparseMatrixMeta must by 64 bytes"); // fail at compile time to protect binary layout stability.

} // teknegram