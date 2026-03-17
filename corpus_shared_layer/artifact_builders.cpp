#include "artifact_builders.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace teknegram {

    namespace {

        struct SparseMatrixEntry {
            std::uint32_t feature_id;
            std::uint32_t count;
        };

        void AppendVarByte(std::uint32_t value, std::vector<std::uint8_t>* out) {
            do {
                std::uint8_t byte = static_cast<std::uint8_t>(value & 0x7FU);
                value >>= 7U;
                if (value != 0U) {
                    byte = static_cast<std::uint8_t>(byte | 0x80U);
                }
                out->push_back(byte);
            } while (value != 0U);
        }

        std::vector<std::uint8_t> EncodeDeltaPositions(const std::vector<std::uint32_t>& values) {
            std::vector<std::uint8_t> encoded;
            encoded.reserve(values.size() * 2U);
            std::uint32_t previous = 0U;
            for (std::size_t i = 0; i < values.size(); ++i) {
                const std::uint32_t delta = (i == 0U) ? values[i] : (values[i] - previous);
                AppendVarByte(delta, &encoded);
                previous = values[i];
            }
            return encoded;
        }

        std::vector<std::uint8_t> EncodeDocFreqPosting(const std::vector<DocFreqEntry>& postings) {
            std::vector<std::uint8_t> encoded;
            encoded.reserve(postings.size() * 3U);
            std::uint32_t previous_doc = 0U;
            for (std::size_t i = 0; i < postings.size(); ++i) {
                const std::uint32_t doc_delta = (i == 0U)
                    ? postings[i].doc_id
                    : (postings[i].doc_id - previous_doc);
                AppendVarByte(doc_delta, &encoded);
                AppendVarByte(postings[i].count, &encoded);
                previous_doc = postings[i].doc_id;
            }
            return encoded;
        }

        void WriteHeaderFile(const std::string& path,
                             const std::vector<IndexHeaderEntry>& header) {
            std::ofstream out(path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Failed to open header output: " + path);
            }

            const std::uint32_t header_size = static_cast<std::uint32_t>(header.size());
            out.write(reinterpret_cast<const char*>(&header_size), sizeof(header_size));
            if (!header.empty()) {
                out.write(reinterpret_cast<const char*>(header.data()),
                          static_cast<std::streamsize>(header.size() * sizeof(IndexHeaderEntry)));
            }
        }

    } // namespace

    void WritePositionIndex(const std::string& header_path,
                            const std::string& positions_path,
                            const std::vector<std::vector<std::uint32_t> >& postings,
                            PostingEncodingMode mode) {
        std::ofstream positions_out(positions_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        if (!positions_out) {
            throw std::runtime_error("Failed to open index positions output: " + positions_path);
        }

        std::vector<IndexHeaderEntry> header(postings.size());
        std::uint64_t offset = 0U;

        for (std::size_t i = 0; i < postings.size(); ++i) {
            header[i].offset = offset;
            if (mode == PostingEncodingMode::kCompressed) {
                const std::vector<std::uint8_t> encoded = EncodeDeltaPositions(postings[i]);
                header[i].length = static_cast<std::uint32_t>(encoded.size());
                if (!encoded.empty()) {
                    positions_out.write(reinterpret_cast<const char*>(encoded.data()),
                                        static_cast<std::streamsize>(encoded.size()));
                }
                offset += static_cast<std::uint64_t>(encoded.size());
            } else {
                const std::uint32_t byte_length = static_cast<std::uint32_t>(postings[i].size() * sizeof(std::uint32_t));
                header[i].length = byte_length;
                if (!postings[i].empty()) {
                    positions_out.write(reinterpret_cast<const char*>(postings[i].data()),
                                        static_cast<std::streamsize>(byte_length));
                }
                offset += static_cast<std::uint64_t>(byte_length);
            }
        }

        WriteHeaderFile(header_path, header);
    }

    void WriteDocFreqIndex(const std::string& header_path,
                           const std::string& entries_path,
                           const std::vector<std::vector<DocFreqEntry> >& postings,
                           PostingEncodingMode mode) {
        std::ofstream entries_out(entries_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        if (!entries_out) {
            throw std::runtime_error("Failed to open docfreq entries output: " + entries_path);
        }

        std::vector<IndexHeaderEntry> header(postings.size());
        std::uint64_t offset = 0U;

        for (std::size_t i = 0; i < postings.size(); ++i) {
            header[i].offset = offset;
            if (mode == PostingEncodingMode::kCompressed) {
                const std::vector<std::uint8_t> encoded = EncodeDocFreqPosting(postings[i]);
                header[i].length = static_cast<std::uint32_t>(encoded.size());
                if (!encoded.empty()) {
                    entries_out.write(reinterpret_cast<const char*>(encoded.data()),
                                      static_cast<std::streamsize>(encoded.size()));
                }
                offset += static_cast<std::uint64_t>(encoded.size());
            } else {
                const std::uint32_t byte_length = static_cast<std::uint32_t>(postings[i].size() * sizeof(DocFreqEntry));
                header[i].length = byte_length;
                if (!postings[i].empty()) {
                    entries_out.write(reinterpret_cast<const char*>(postings[i].data()),
                                      static_cast<std::streamsize>(byte_length));
                }
                offset += static_cast<std::uint64_t>(byte_length);
            }
        }

        WriteHeaderFile(header_path, header);
    }

    void WriteSparseMatrixArtifacts(const std::string& meta_path,
                                    const std::string& offsets_path,
                                    const std::string& entries_path,
                                    const FeatureRowsResult& result) {
        std::vector<std::uint64_t> doc_offsets(result.rows.size() + 1U, 0U);
        std::vector<SparseMatrixEntry> flat_entries;

        std::uint64_t running = 0U;
        for (std::size_t doc_id = 0; doc_id < result.rows.size(); ++doc_id) {
            doc_offsets[doc_id] = running;
            const std::vector<std::pair<std::uint32_t, std::uint32_t> >& row = result.rows[doc_id];
            for (std::size_t i = 0; i < row.size(); ++i) {
                SparseMatrixEntry entry;
                entry.feature_id = row[i].first;
                entry.count = row[i].second;
                flat_entries.push_back(entry);
                ++running;
            }
        }
        doc_offsets[result.rows.size()] = running;

        SparseMatrixMeta meta;
        meta.magic = kSparseMatrixMagic;
        meta.version = kSparseMatrixVersion;
        meta.num_docs = static_cast<std::uint64_t>(result.rows.size());
        meta.num_features = result.num_features;
        meta.num_nonzero = static_cast<std::uint64_t>(flat_entries.size());
        meta.entry_size_bytes = sizeof(SparseMatrixEntry);
        meta.reserved = 0U;
        for (std::size_t i = 0; i < sizeof(meta.padding); ++i) {
            meta.padding[i] = 0U;
        }

        std::ofstream meta_out(meta_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream offsets_out(offsets_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream entries_out(entries_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        if (!meta_out || !offsets_out || !entries_out) {
            throw std::runtime_error("Failed to open sparse matrix outputs.");
        }

        meta_out.write(reinterpret_cast<const char*>(&meta), sizeof(meta));
        if (!doc_offsets.empty()) {
            offsets_out.write(reinterpret_cast<const char*>(doc_offsets.data()),
                              static_cast<std::streamsize>(doc_offsets.size() * sizeof(std::uint64_t)));
        }
        if (!flat_entries.empty()) {
            entries_out.write(reinterpret_cast<const char*>(flat_entries.data()),
                              static_cast<std::streamsize>(flat_entries.size() * sizeof(SparseMatrixEntry)));
        }
    }

} // namespace teknegram
