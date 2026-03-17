#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "../parse_layer/parsed_document.hpp"

namespace teknegram {
    class DictionaryBuilder;
    class StructuralLayer;

    class CoreTokenLayer {
        public:
            explicit CoreTokenLayer(const std::string& output_dir);
            void append_document(const ParsedDocument& doc,
                                 DictionaryBuilder* dictionary_builder,
                                 StructuralLayer* structural_layer);
            void finalize();
            std::uint32_t global_token_count() const;

        private:
                std::ofstream word_out_;
                std::ofstream lemma_out_;
                std::ofstream pos_out_;
                std::ofstream head_out_;
                std::ofstream deprel_out_;

                std::uint32_t global_token_counter_;
    };
}
