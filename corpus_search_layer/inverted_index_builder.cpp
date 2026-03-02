#include "inverted_index_builder.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "../corpus_shared_layer/binary_header.hpp"

namespace teknegram {

    void InvertedIndexBuilder::build_lemma_index(const std::string& corpus_dir) const {
        build_index_from_file(corpus_dir + "/lemma.bin",
                            corpus_dir + "/lemma.index.header",
                            corpus_dir + "/lemma.index.positions",
                            false);
    }

    void InvertedIndexBuilder::build_pos_index(const std::string& corpus_dir) const {
        build_index_from_file(corpus_dir + "/pos.bin",
                            corpus_dir + "/pos.index.header",
                            corpus_dir + "/pos.index.positions",
                            true);
    }

    void InvertedIndexBuilder::build_word_index(const std::string& corpus_dir) const {
        build_index_from_file(corpus_dir + "/word.bin",
                            corpus_dir + "/word.index.header",
                            corpus_dir + "/word.index.positions",
                            true);
    }

    void InvertedIndexBuilder::build_index_from_file(const std::string& input_path,
                                                    const std::string& header_path,
                                                    const std::string& positions_path,
                                                    bool is_byte_sized) const {
        std::ifstream in(input_path.c_str(), std::ios::binary | std::ios::in);
        if (!in) {
            throw std::runtime_error("Failed to open inverted-index input: " + input_path);
        }

        std::vector<std::vector<std::uint32_t> > postings;
        std::uint32_t token_pos = 0;

        if (is_byte_sized) {
            std::uint8_t value = 0;
            while (in.read(reinterpret_cast<char*>(&value), sizeof(value))) {
                const std::size_t idx = static_cast<std::size_t>(value);
                if (postings.size() <= idx) {
                    postings.resize(idx + 1);
                }
                postings[idx].push_back(token_pos++);
            }
        } else {
            std::uint32_t value = 0;
            while (in.read(reinterpret_cast<char*>(&value), sizeof(value))) {
                const std::size_t idx = static_cast<std::size_t>(value);
                if (postings.size() <= idx) {
                    postings.resize(idx + 1);
                }
                postings[idx].push_back(token_pos++);
            }
        }

        std::vector<IndexHeaderEntry> header;
        header.resize(postings.size());

        std::ofstream out_header(header_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream out_positions(positions_path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        if (!out_header || !out_positions) {
            throw std::runtime_error("Failed to open inverted-index output files.");
        }

        std::uint64_t offset = 0;
        for (std::size_t i = 0; i < postings.size(); ++i) {
            header[i].offset = offset;
            header[i].length = static_cast<std::uint32_t>(postings[i].size());
            offset += static_cast<std::uint64_t>(postings[i].size());
        }

        const std::uint32_t header_size = static_cast<std::uint32_t>(header.size());
        out_header.write(reinterpret_cast<const char*>(&header_size), sizeof(header_size));
        out_header.write(reinterpret_cast<const char*>(header.data()),
                        static_cast<std::streamsize>(header.size() * sizeof(IndexHeaderEntry)));

        for (std::size_t i = 0; i < postings.size(); ++i) {
            if (!postings[i].empty()) {
                out_positions.write(reinterpret_cast<const char*>(postings[i].data()),
                                    static_cast<std::streamsize>(postings[i].size() * sizeof(std::uint32_t)));
            }
        }
    }

} // namespace corpus
