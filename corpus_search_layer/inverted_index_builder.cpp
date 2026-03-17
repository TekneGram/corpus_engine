#include "inverted_index_builder.hpp"

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "../corpus_shared_layer/artifact_builders.hpp"

namespace teknegram {

    void InvertedIndexBuilder::build_lemma_index(const std::string& corpus_dir,
                                                 PostingEncodingMode mode) const {
        build_index_from_file(corpus_dir + "/lemma.bin",
                              corpus_dir + "/lemma.index.header",
                              corpus_dir + "/lemma.index.positions",
                              false,
                              mode);
    }

    void InvertedIndexBuilder::build_pos_index(const std::string& corpus_dir,
                                               PostingEncodingMode mode) const {
        build_index_from_file(corpus_dir + "/pos.bin",
                              corpus_dir + "/pos.index.header",
                              corpus_dir + "/pos.index.positions",
                              true,
                              mode);
    }

    void InvertedIndexBuilder::build_word_index(const std::string& corpus_dir,
                                                PostingEncodingMode mode) const {
        build_index_from_file(corpus_dir + "/word.bin",
                              corpus_dir + "/word.index.header",
                              corpus_dir + "/word.index.positions",
                              false,
                              mode);
    }

    void InvertedIndexBuilder::build_index_from_file(const std::string& input_path,
                                                     const std::string& header_path,
                                                     const std::string& positions_path,
                                                     bool is_byte_sized,
                                                     PostingEncodingMode mode) const {
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

        WritePositionIndex(header_path, positions_path, postings, mode);
    }

} // namespace corpus
