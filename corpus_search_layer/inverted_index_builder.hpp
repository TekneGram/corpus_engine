#pragma once

#include <string>

namespace teknegram {

class InvertedIndexBuilder {
    public:
        void build_lemma_index(const std::string& corpus_dir) const;
        void build_pos_index(const std::string& corpus_dir) const;
        void build_word_index(const std::string& corpus_dir) const;

    private:
        void build_index_from_file(const std::string& input_path,
                                const std::string& header_path,
                                const std::string& positions_path,
                                bool is_byte_sized) const;
    };

}
