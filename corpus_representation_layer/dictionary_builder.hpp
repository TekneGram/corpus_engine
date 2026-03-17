#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../corpus_shared_layer/build_options.hpp"

namespace teknegram {
    class DictionaryBuilder {
        public:
            std::uint32_t get_word_id(const std::string& word);
            std::uint32_t get_lemma_id(const std::string& lemma);
            std::size_t num_words() const;
            std::size_t num_lemmas() const;
            void write_lexicons(const std::string& output_dir,
                               PostingEncodingMode mode) const;
        
        private:
            static std::uint32_t get_or_create_id(const std::string& key, std::unordered_map<std::string, std::uint32_t>* map, std::vector<std::string>* reverse);
            std::unordered_map<std::string, std::uint32_t> word_map_;
            std::unordered_map<std::string, std::uint32_t> lemma_map_;
            std::vector<std::string> word_reverse_;
            std::vector<std::string> lemma_reverse_;
    };
}
