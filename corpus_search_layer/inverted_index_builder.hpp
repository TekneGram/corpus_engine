#pragma once

#include <string>

#include "../corpus_shared_layer/build_options.hpp"

namespace teknegram {

class InvertedIndexBuilder {
    public:
        void build_lemma_index(const std::string& corpus_dir,
                               PostingEncodingMode mode) const;
        void build_pos_index(const std::string& corpus_dir,
                             PostingEncodingMode mode) const;
        void build_word_index(const std::string& corpus_dir,
                              PostingEncodingMode mode) const;

    private:
        void build_index_from_file(const std::string& input_path,
                                   const std::string& header_path,
                                   const std::string& positions_path,
                                   bool is_byte_sized,
                                   PostingEncodingMode mode) const;
    };

}
