#include "dictionary_builder.hpp"

#include <cstddef>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace teknegram {

    namespace {

        void WriteRawStringLexicon(const std::string& path, const std::vector<std::string>& values) {
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

        void WriteFrontCodedLexicon(const std::string& path, const std::vector<std::string>& values) {
            std::ofstream out(path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Failed to open lexicon output: " + path);
            }

            const std::uint32_t count = static_cast<std::uint32_t>(values.size());
            out.write(reinterpret_cast<const char*>(&count), sizeof(count));

            std::string previous;
            for (std::size_t i = 0; i < values.size(); ++i) {
                std::size_t prefix = 0U;
                while (prefix < previous.size() &&
                       prefix < values[i].size() &&
                       previous[prefix] == values[i][prefix]) {
                    ++prefix;
                }

                const std::uint32_t prefix_len = static_cast<std::uint32_t>(prefix);
                const std::uint32_t suffix_len =
                    static_cast<std::uint32_t>(values[i].size() - prefix);
                out.write(reinterpret_cast<const char*>(&prefix_len), sizeof(prefix_len));
                out.write(reinterpret_cast<const char*>(&suffix_len), sizeof(suffix_len));
                if (suffix_len > 0U) {
                    out.write(values[i].data() + prefix,
                              static_cast<std::streamsize>(suffix_len));
                }
                previous = values[i];
            }
        }

        void WriteStaticLexicon(const std::string& path,
                                const char* const* values,
                                std::size_t count) {
            std::ofstream out(path.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
            if (!out) {
                throw std::runtime_error("Failed to open lexicon output: " + path);
            }

            const std::uint32_t item_count = static_cast<std::uint32_t>(count);
            out.write(reinterpret_cast<const char*>(&item_count), sizeof(item_count));
            for (std::size_t i = 0; i < count; ++i) {
                const std::uint32_t len = static_cast<std::uint32_t>(std::strlen(values[i]));
                out.write(reinterpret_cast<const char*>(&len), sizeof(len));
                out.write(values[i], static_cast<std::streamsize>(len));
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

    void DictionaryBuilder::write_lexicons(const std::string& output_dir,
                                           PostingEncodingMode mode) const {
        static const char* const kPosLexicon[] = {
            "UNKNOWN", "ADJ", "ADP", "ADV", "AUX", "CCONJ", "DET", "INTJ",
            "NOUN", "NUM", "PART", "PRON", "PROPN", "PUNCT", "SCONJ",
            "SYM", "VERB", "X"
        };
        static const char* const kDeprelLexicon[] = {
            "UNKNOWN", "acl", "advcl", "advmod", "amod", "appos", "aux",
            "case", "cc", "ccomp", "clf", "compound", "conj", "cop",
            "csubj", "dep", "det", "discourse", "dislocated", "expl",
            "fixed", "flat", "goeswith", "iobj", "list", "mark", "nmod",
            "nsubj", "nummod", "obj", "obl", "orphan", "parataxis",
            "punct", "reparandum", "root", "vocative", "xcomp"
        };

        if (mode == PostingEncodingMode::kCompressed) {
            WriteFrontCodedLexicon(output_dir + "/word.lexicon.bin", word_reverse_);
            WriteFrontCodedLexicon(output_dir + "/lemma.lexicon.bin", lemma_reverse_);
        } else {
            WriteRawStringLexicon(output_dir + "/word.lexicon.bin", word_reverse_);
            WriteRawStringLexicon(output_dir + "/lemma.lexicon.bin", lemma_reverse_);
        }
        WriteStaticLexicon(output_dir + "/pos.lexicon.bin",
                           kPosLexicon,
                           sizeof(kPosLexicon) / sizeof(kPosLexicon[0]));
        WriteStaticLexicon(output_dir + "/deprel.lexicon.bin",
                           kDeprelLexicon,
                           sizeof(kDeprelLexicon) / sizeof(kDeprelLexicon[0]));
    }

} // namespace corpus
