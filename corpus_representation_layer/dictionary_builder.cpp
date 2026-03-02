#include "dictionary_builder.hpp"

#include <fstream>
#include <stdexcept>

namespace teknegram {

    namespace {

        void WriteStringLexicon(const std::string& path, const std::vector<std::string>& values) {
            std::ofstream out(path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Failed to open lexicon output: " + path);
            }

            const std::uint32_t count = static_cast<std::uint32_t>(values.size());
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));

            for (std::size_t i = 0; i < values.size(); ++i) {
                const std::uint32_t len = static_cast<std::uint32_t>(values[i].size());
                out.write(reinterpret_cast<const char*>(&len), sizeof(len));
                out.write(values[i].data(), static_cast<std::streamsize>(values[i].size()));
            }
        }

    } // namespace

    std::uint32_t DictionaryBuilder::get_or_create_id(const std::string& key,
                                                    std::unordered_map<std::string, std::uint32_t>* map,
                                                    std::vector<std::string>* reverse) {
        std::unordered_map<std::string, std::uint32_t>::const_iterator it = map->find(key);
        if (it != map->end()) {
            return it->second;
        }

        const std::uint32_t next_id = static_cast<std::uint32_t>(reverse->size());
        (*map)[key] = next_id;
        reverse->push_back(key);
        return next_id;
    }

    std::uint32_t DictionaryBuilder::get_word_id(const std::string& word) {
        return get_or_create_id(word, &word_map_, &word_reverse_);
    }

    std::uint32_t DictionaryBuilder::get_lemma_id(const std::string& lemma) {
        return get_or_create_id(lemma, &lemma_map_, &lemma_reverse_);
    }

    std::size_t DictionaryBuilder::num_words() const {
        return word_reverse_.size();
    }

    std::size_t DictionaryBuilder::num_lemmas() const {
        return lemma_reverse_.size();
    }

    void DictionaryBuilder::write_lexicons(const std::string& output_dir) const {
        WriteStringLexicon(output_dir + "/word.lexicon.bin", word_reverse_);
        WriteStringLexicon(output_dir + "/lemma.lexicon.bin", lemma_reverse_);
    }

} // namespace corpus
