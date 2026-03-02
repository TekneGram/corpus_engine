#include "core_token_layer.hpp"

#include <stdexcept>

#include "dictionary_builder.hpp"
#include "structural_layer.hpp"

namespace teknegram {
    // 5 file types that will be output to convert text into binary, using dictionary
    // global token counter keeps track of where tokens start in new document
    CoreTokenLayer::CoreTokenLayer(const std::string& output_dir)
        : word_out_((output_dir + "/word.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        lemma_out_((output_dir + "/lemma.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        pos_out_((output_dir + "/pos.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        head_out_((output_dir + "/head.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        deprel_out_((output_dir + "/deprel.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc),
        global_token_counter_(0) {
        if (!word_out_ || !lemma_out_ || !pos_out_ || !head_out_ || !deprel_out_) {
            throw std::runtime_error("Failed to open one or more core token layer output files.");
        }
    }

    void CoreTokenLayer::append_document(const ParsedDocument& doc,
                                        DictionaryBuilder* dictionary_builder,
                                        StructuralLayer* structural_layer) {
        if (!dictionary_builder || !structural_layer) {
            throw std::runtime_error("CoreTokenLayer dependencies cannot be null.");
        }

        const std::uint32_t token_start = global_token_counter_;

        std::size_t sentence_idx = 0;
        std::uint32_t current_sentence_start = doc.sentence_starts.empty() ? 0U : doc.sentence_starts[0];
        std::uint32_t next_sentence_start = doc.sentence_starts.size() > 1 ? doc.sentence_starts[1] : static_cast<std::uint32_t>(doc.tokens.size());

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(doc.tokens.size()); ++i) {
            while (sentence_idx + 1 < doc.sentence_starts.size() && i >= next_sentence_start) {
                ++sentence_idx;
                current_sentence_start = doc.sentence_starts[sentence_idx];
                next_sentence_start = (sentence_idx + 1 < doc.sentence_starts.size())
                    ? doc.sentence_starts[sentence_idx + 1]
                    : static_cast<std::uint32_t>(doc.tokens.size());
            }

            const ParsedToken& token = doc.tokens[i];

            const std::uint32_t word_id = dictionary_builder->get_word_id(token.word);
            const std::uint32_t lemma_id = dictionary_builder->get_lemma_id(token.lemma);

            std::uint32_t global_head = static_cast<std::uint32_t>(-1);
            if (token.head != 0U) {
                const std::uint32_t local_head_zero_based = token.head - 1U;
                const std::uint32_t local_head_global_in_doc = current_sentence_start + local_head_zero_based;
                global_head = token_start + local_head_global_in_doc;
            }

            word_out_.write(reinterpret_cast<const char*>(&word_id), sizeof(word_id));
            lemma_out_.write(reinterpret_cast<const char*>(&lemma_id), sizeof(lemma_id));
            pos_out_.write(reinterpret_cast<const char*>(&token.pos_id), sizeof(token.pos_id));
            head_out_.write(reinterpret_cast<const char*>(&global_head), sizeof(global_head));
            deprel_out_.write(reinterpret_cast<const char*>(&token.deprel_id), sizeof(token.deprel_id));

            ++global_token_counter_;
        }

        const std::uint32_t token_end = global_token_counter_;
        structural_layer->append_document(doc.document_id,
                                        token_start,
                                        token_end,
                                        doc.sentence_starts,
                                        token_start);
    }

    std::uint32_t CoreTokenLayer::global_token_count() const {
        return global_token_counter_;
    }

} // namespace corpus
