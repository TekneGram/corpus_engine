#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace teknegram {
    class StructuralLayer {
        public:
            explicit StructuralLayer(const std::string& output_dir);

            void append_document(std::uint32_t doc_id, std::uint32_t token_start, std::uint32_t token_end, const std::vector<std::uint32_t>& sentence_starts, std::uint32_t global_token_base);
            void finalize();
        private:
            std::ofstream sentence_bounds_out_;
            std::ofstream doc_ranges_out_;
            std::ofstream word_doc_out_;
    };
}
