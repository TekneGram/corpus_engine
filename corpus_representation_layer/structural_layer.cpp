#include "structural_layer.hpp"

#include <stdexcept>

namespace teknegram {
    StructuralLayer::StructuralLayer(const std::string& output_dir)
    : sentence_bounds_out_((output_dir + "/sentence_bounds.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        doc_ranges_out_((output_dir + "/doc_ranges.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        word_doc_out_((output_dir + "/word_doc.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc) {
            if (!sentence_bounds_out_ || !doc_ranges_out_ || !word_doc_out_) {
                throw std::runtime_error("Failed to open one or more structural layer output files.");
            }
        }
    
    void StructuralLayer::append_document(std::uint32_t doc_id, std::uint32_t token_start, std::uint32_t token_end, const std::vector<std::uint32_t>& sentence_starts, std::uint32_t global_token_base) {
        doc_ranges_out_.write(reinterpret_cast<const char*>(&token_start), sizeof(token_start));
        doc_ranges_out_.write(reinterpret_cast<const char*>(&token_end), sizeof(token_end));

        for (std::size_t i = 0; i < sentence_starts.size(); ++i) {
            const std::uint32_t global_start = global_token_base + sentence_starts[i];
            sentence_bounds_out_.write(reinterpret_cast<const char*>(&global_start), sizeof(global_start));
        }

        for (std::uint32_t token = token_start; token < token_end; ++token) {
            (void)token;
            word_doc_out_.write(reinterpret_cast<const char*>(&doc_id), sizeof(doc_id));
        }
    }
}

/*
append_document writes boundaries and for documents and sentences and stores tokens per document

EXAMPLE
doc_1: "The dog ran fast." (5 tokens including punctuation)
doc_2: "The man ran slowly." (5 tokens including punctuation)

doc_1 tokens: 0..4 (token_start=0, token_end=5)
doc_2 tokens: 5..9 'token_start=5, token_end=10)
global_token_base is 0 for doc_1, 5 for doc_2

doc_ranges.bin
    doc_1 (0, 5)
    doc_2 (5, 10)
So logically: [0, 5, 5, 10] as uint32_t

sentence_bounds.bin (global sentence starts)
doc_1 sentence start: 0 + 0 = 0
doc_2 sentence start: 5 + 0 = 5
So logically: [0, 5] as uint32_t

word_doc.bin (one doc_id per token position)
token positions 0..4 -> doc_id 1
token positions 5..9 -> doc_id 2
So logically [1, 1, 1, 1, 1, 2, 2, 2, 2, 2] as uint32_t

*/
