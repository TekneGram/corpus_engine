#include "sparse_matrix_builder.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "../corpus_shared_layer/binary_header.hpp"

namespace teknegram {

    namespace {

        struct SparseEntry {
            std::uint32_t feature_id;
            std::uint32_t count;
        };

    } // namespace

    void SparseMatrixBuilder::build_lemma_matrix(const std::string& corpus_dir) const {
        std::ifstream header_in((corpus_dir + "/lemma.docfreq.header").c_str(), std::ios::binary | std::ios::in);
        std::ifstream entries_in((corpus_dir + "/lemma.docfreq.entries").c_str(), std::ios::binary | std::ios::in);
        std::ifstream doc_ranges_in((corpus_dir + "/doc_ranges.bin").c_str(), std::ios::binary | std::ios::in);

        if (!header_in || !entries_in || !doc_ranges_in) {
            throw std::runtime_error("SparseMatrixBuilder failed to open docfreq/doc_ranges inputs.");
        }

        std::vector<std::uint64_t> doc_offsets(1, 0);
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
        while (doc_ranges_in.read(reinterpret_cast<char*>(&begin), sizeof(begin))) {
            if (!doc_ranges_in.read(reinterpret_cast<char*>(&end), sizeof(end))) {
                throw std::runtime_error("Malformed doc_ranges.bin.");
            }
            doc_offsets.push_back(0);
        }
        const std::uint64_t num_docs = static_cast<std::uint64_t>(doc_offsets.size() - 1);

        std::uint32_t feature_count = 0;
        header_in.read(reinterpret_cast<char*>(&feature_count), sizeof(feature_count));
        std::vector<IndexHeaderEntry> header(feature_count);
        if (feature_count > 0) {
            header_in.read(reinterpret_cast<char*>(header.data()),
                        static_cast<std::streamsize>(header.size() * sizeof(IndexHeaderEntry)));
        }

        std::vector<DocFreqEntry> all_docfreq_entries;
        DocFreqEntry dfe;
        while (entries_in.read(reinterpret_cast<char*>(&dfe), sizeof(dfe))) {
            all_docfreq_entries.push_back(dfe);
        }

        std::vector<std::vector<SparseEntry> > rows(static_cast<std::size_t>(num_docs));

        for (std::uint32_t feature_id = 0; feature_id < feature_count; ++feature_id) {
            const IndexHeaderEntry& he = header[feature_id];
            const std::uint64_t start = he.offset;
            const std::uint64_t finish = he.offset + he.length;

            for (std::uint64_t i = start; i < finish; ++i) {
                const DocFreqEntry& e = all_docfreq_entries[static_cast<std::size_t>(i)];
                SparseEntry se;
                se.feature_id = feature_id;
                se.count = e.count;
                rows[e.doc_id].push_back(se);
            }
        }

        std::vector<SparseEntry> flat_entries;
        flat_entries.reserve(all_docfreq_entries.size());

        std::uint64_t offset = 0;
        doc_offsets[0] = 0;
        for (std::size_t d = 0; d < rows.size(); ++d) {
            offset += static_cast<std::uint64_t>(rows[d].size());
            doc_offsets[d + 1] = offset;
            flat_entries.insert(flat_entries.end(), rows[d].begin(), rows[d].end());
        }

        SparseMatrixMeta meta;
        meta.magic = kSparseMatrixMagic;
        meta.version = kSparseMatrixVersion;
        meta.num_docs = num_docs;
        meta.num_features = feature_count;
        meta.num_nonzero = flat_entries.size();
        meta.entry_size_bytes = sizeof(SparseEntry);
        meta.reserved = 0;
        for (std::size_t i = 0; i < sizeof(meta.padding); ++i) {
            meta.padding[i] = 0;
        }

        std::ofstream meta_out((corpus_dir + "/lemma.spm.meta.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream offsets_out((corpus_dir + "/lemma.spm.offsets.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream entries_out((corpus_dir + "/lemma.spm.entries.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);

        if (!meta_out || !offsets_out || !entries_out) {
            throw std::runtime_error("SparseMatrixBuilder failed to open outputs.");
        }

        meta_out.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
        offsets_out.write(reinterpret_cast<const char*>(doc_offsets.data()),
                        static_cast<std::streamsize>(doc_offsets.size() * sizeof(std::uint64_t)));
        if (!flat_entries.empty()) {
            entries_out.write(reinterpret_cast<const char*>(flat_entries.data()),
                            static_cast<std::streamsize>(flat_entries.size() * sizeof(SparseEntry)));
        }
    }

} // namespace corpus
