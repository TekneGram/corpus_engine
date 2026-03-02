#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace teknegram {

    template <std::size_t N>
    class NGramBuilder {
        public:
            void build(const std::string& corpus_dir) const {
                std::ifstream word_in((corpus_dir + "/word.bin").c_str(), std::ios::binary | std::ios::in);
                std::ifstream sentence_in((corpus_dir + "/sentence_bounds.bin").c_str(), std::ios::binary | std::ios::in);
                if (!word_in || !sentence_in) {
                    throw std::runtime_error("NGramBuilder failed to open input files.");
                }

                std::vector<std::uint32_t> words;
                std::uint32_t word_id = 0;
                while (word_in.read(reinterpret_cast<char*>(&word_id), sizeof(word_id))) {
                    words.push_back(word_id);
                }

                std::vector<std::uint32_t> sentence_starts;
                std::uint32_t start = 0;
                while (sentence_in.read(reinterpret_cast<char*>(&start), sizeof(start))) {
                    sentence_starts.push_back(start);
                }
                sentence_starts.push_back(static_cast<std::uint32_t>(words.size()));

                typedef std::vector<std::uint32_t> NGramKey;
                std::map<NGramKey, std::uint32_t> ngram_to_id;
                std::vector<NGramKey> lexicon;
                std::vector<std::uint32_t> freq;

                for (std::size_t s = 0; s + 1 < sentence_starts.size(); ++s) {
                    const std::uint32_t sent_begin = sentence_starts[s];
                    const std::uint32_t sent_end = sentence_starts[s + 1];
                    if (sent_end <= sent_begin || sent_end - sent_begin < N) {
                        continue;
                    }

                    for (std::uint32_t i = sent_begin; i + N <= sent_end; ++i) {
                        NGramKey key;
                        key.reserve(N);
                        for (std::size_t j = 0; j < N; ++j) {
                            key.push_back(words[i + j]);
                        }

                        typename std::map<NGramKey, std::uint32_t>::iterator it = ngram_to_id.find(key);
                        if (it == ngram_to_id.end()) {
                            const std::uint32_t id = static_cast<std::uint32_t>(lexicon.size());
                            ngram_to_id[key] = id;
                            lexicon.push_back(key);
                            freq.push_back(1U);
                        } else {
                            ++freq[it->second];
                        }
                    }
                }

                std::ofstream lexicon_out((corpus_dir + "/" + std::to_string(static_cast<unsigned long long>(N)) + "g.lexicon.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
                std::ofstream freq_out((corpus_dir + "/" + std::to_string(static_cast<unsigned long long>(N)) + "g.freq.bin").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
                if (!lexicon_out || !freq_out) {
                    throw std::runtime_error("NGramBuilder failed to open outputs.");
                }

                for (std::size_t i = 0; i < lexicon.size(); ++i) {
                    for (std::size_t j = 0; j < lexicon[i].size(); ++j) {
                        lexicon_out.write(reinterpret_cast<const char*>(&lexicon[i][j]), sizeof(std::uint32_t));
                    }
                    freq_out.write(reinterpret_cast<const char*>(&freq[i]), sizeof(std::uint32_t));
                }
            }
    };

} // namespace corpus
