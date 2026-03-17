#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../corpus_shared_layer/artifact_builders.hpp"
#include "../corpus_shared_layer/build_options.hpp"
#include "../orchestration_layer/progress_emitter.hpp"

namespace teknegram {

    template <std::size_t N>
    class NGramBuilder {
        public:
            FeatureRowsResult build(const std::string& corpus_dir,
                                    const BuildOptions& options,
                                    const ProgressEmitter* progress_emitter = 0,
                                    int progress_percent = 0) const {
                NullProgressEmitter default_emitter;
                const ProgressEmitter* emitter = progress_emitter ? progress_emitter : &default_emitter;

                emitter->emit("Building " + stem() + ": loading token arrays", progress_percent);
                const std::vector<std::uint32_t> words =
                    load_uint32_file(corpus_dir + "/word.bin");
                const std::vector<std::uint8_t> pos =
                    load_uint8_file(corpus_dir + "/pos.bin");
                std::vector<std::uint32_t> sentence_starts =
                    load_uint32_file(corpus_dir + "/sentence_bounds.bin");
                const std::vector<std::uint32_t> word_doc =
                    load_uint32_file(corpus_dir + "/word_doc.bin");
                const std::vector<std::uint32_t> doc_ranges =
                    load_uint32_file(corpus_dir + "/doc_ranges.bin");

                if (word_doc.size() != words.size()) {
                    throw std::runtime_error("word_doc.bin length does not match word.bin.");
                }
                if (pos.size() != words.size()) {
                    throw std::runtime_error("pos.bin length does not match word.bin.");
                }
                if (doc_ranges.size() % 2U != 0U) {
                    throw std::runtime_error("doc_ranges.bin is malformed.");
                }

                sentence_starts.push_back(static_cast<std::uint32_t>(words.size()));
                const std::size_t num_docs = doc_ranges.size() / 2U;

                emitter->emit("Building " + stem() + ": collecting bundles", progress_percent);

                std::unordered_map<NGramFeatureKey, std::uint32_t, NGramFeatureKeyHash> ngram_to_id;
                std::vector<WordNGramKey> word_lexicon;
                std::vector<PosNGramKey> pos_lexicon;
                std::vector<std::uint32_t> freq;
                std::vector<std::vector<std::uint32_t> > positions;
                std::vector<std::unordered_map<std::uint32_t, std::uint32_t> > row_maps(num_docs);

                for (std::size_t s = 0; s + 1U < sentence_starts.size(); ++s) {
                    const std::uint32_t sent_begin = sentence_starts[s];
                    const std::uint32_t sent_end = sentence_starts[s + 1U];
                    if (sent_end <= sent_begin || sent_end - sent_begin < N) {
                        continue;
                    }

                    for (std::uint32_t i = sent_begin; i + N <= sent_end; ++i) {
                        NGramFeatureKey key;
                        for (std::size_t j = 0; j < N; ++j) {
                            key.word_values[j] = words[i + j];
                            key.pos_values[j] = pos[i + j];
                        }

                        typename std::unordered_map<NGramFeatureKey, std::uint32_t, NGramFeatureKeyHash>::iterator it =
                            ngram_to_id.find(key);
                        std::uint32_t id = 0U;
                        if (it == ngram_to_id.end()) {
                            id = static_cast<std::uint32_t>(word_lexicon.size());
                            ngram_to_id[key] = id;
                            word_lexicon.push_back(key.words());
                            pos_lexicon.push_back(key.pos());
                            freq.push_back(0U);
                            if (options.emit_ngram_positions) {
                                positions.push_back(std::vector<std::uint32_t>());
                            }
                        } else {
                            id = it->second;
                        }

                        ++freq[id];
                        if (options.emit_ngram_positions) {
                            positions[id].push_back(i);
                        }
                        if (word_doc[i] >= num_docs) {
                            throw std::runtime_error("word_doc.bin contains out-of-range document ids.");
                        }
                        ++row_maps[word_doc[i]][id];
                    }
                }

                emitter->emit("Building " + stem() + ": writing outputs", progress_percent);
                return write_outputs(corpus_dir,
                                     word_lexicon,
                                     pos_lexicon,
                                     freq,
                                     positions,
                                     row_maps,
                                     options);
            }

        private:
            struct WordNGramKey {
                std::array<std::uint32_t, N> values;

                bool operator==(const WordNGramKey& other) const {
                    return values == other.values;
                }
            };

            struct PosNGramKey {
                std::array<std::uint8_t, N> values;

                bool operator==(const PosNGramKey& other) const {
                    return values == other.values;
                }
            };

            struct NGramFeatureKey {
                std::array<std::uint32_t, N> word_values;
                std::array<std::uint8_t, N> pos_values;

                bool operator==(const NGramFeatureKey& other) const {
                    return word_values == other.word_values &&
                           pos_values == other.pos_values;
                }

                WordNGramKey words() const {
                    WordNGramKey key;
                    key.values = word_values;
                    return key;
                }

                PosNGramKey pos() const {
                    PosNGramKey key;
                    key.values = pos_values;
                    return key;
                }
            };

            struct NGramFeatureKeyHash {
                std::size_t operator()(const NGramFeatureKey& key) const {
                    std::size_t hash = 0U;
                    for (std::size_t i = 0; i < N; ++i) {
                        hash ^= static_cast<std::size_t>(key.word_values[i]) +
                            0x9e3779b9U + (hash << 6U) + (hash >> 2U);
                        hash ^= static_cast<std::size_t>(key.pos_values[i]) +
                            0x9e3779b9U + (hash << 6U) + (hash >> 2U);
                    }
                    return hash;
                }
            };

            static std::string stem() {
                return std::to_string(static_cast<unsigned long long>(N)) + "gram";
            }

            static std::vector<std::uint8_t> load_uint8_file(const std::string& path) {
                std::ifstream in(path.c_str(), std::ios::binary | std::ios::in);
                if (!in) {
                    throw std::runtime_error("NGramBuilder failed to open input file: " + path);
                }

                std::vector<std::uint8_t> values;
                std::uint8_t value = 0U;
                while (in.read(reinterpret_cast<char*>(&value), sizeof(value))) {
                    values.push_back(value);
                }
                return values;
            }

            static std::vector<std::uint32_t> load_uint32_file(const std::string& path) {
                std::ifstream in(path.c_str(), std::ios::binary | std::ios::in);
                if (!in) {
                    throw std::runtime_error("NGramBuilder failed to open input file: " + path);
                }

                std::vector<std::uint32_t> values;
                std::uint32_t value = 0U;
                while (in.read(reinterpret_cast<char*>(&value), sizeof(value))) {
                    values.push_back(value);
                }
                return values;
            }

            static FeatureRowsResult write_outputs(
                const std::string& corpus_dir,
                const std::vector<WordNGramKey>& word_lexicon,
                const std::vector<PosNGramKey>& pos_lexicon,
                const std::vector<std::uint32_t>& freq,
                const std::vector<std::vector<std::uint32_t> >& positions,
                const std::vector<std::unordered_map<std::uint32_t, std::uint32_t> >& row_maps,
                const BuildOptions& options) {

                const std::string stem_name = stem();

                std::ofstream lexicon_out((corpus_dir + "/" + stem_name + ".lexicon.bin").c_str(),
                                          std::ios::binary | std::ios::out | std::ios::trunc);
                std::ofstream pos_lexicon_out((corpus_dir + "/" + stem_name + ".pos.lexicon.bin").c_str(),
                                              std::ios::binary | std::ios::out | std::ios::trunc);
                std::ofstream freq_out((corpus_dir + "/" + stem_name + ".freq.bin").c_str(),
                                       std::ios::binary | std::ios::out | std::ios::trunc);
                if (!lexicon_out || !pos_lexicon_out || !freq_out) {
                    throw std::runtime_error("NGramBuilder failed to open outputs.");
                }

                std::vector<std::uint32_t> old_to_new;
                if (N == 4U) {
                    old_to_new.assign(freq.size(), static_cast<std::uint32_t>(-1));
                }

                std::vector<std::vector<std::uint32_t> > filtered_positions;
                std::uint32_t kept_count = 0U;
                for (std::size_t i = 0; i < word_lexicon.size(); ++i) {
                    if (N == 4U && freq[i] < 2U) {
                        continue;
                    }
                    if (N == 4U) {
                        old_to_new[i] = kept_count;
                    }

                    for (std::size_t j = 0; j < N; ++j) {
                        lexicon_out.write(reinterpret_cast<const char*>(&word_lexicon[i].values[j]),
                                          sizeof(std::uint32_t));
                        pos_lexicon_out.write(reinterpret_cast<const char*>(&pos_lexicon[i].values[j]),
                                              sizeof(std::uint8_t));
                    }
                    freq_out.write(reinterpret_cast<const char*>(&freq[i]),
                                   sizeof(std::uint32_t));
                    if (options.emit_ngram_positions) {
                        filtered_positions.push_back(positions[i]);
                    }
                    ++kept_count;
                }

                if (options.emit_ngram_positions) {
                    WritePositionIndex(corpus_dir + "/" + stem_name + ".index.header",
                                       corpus_dir + "/" + stem_name + ".index.positions",
                                       filtered_positions,
                                       options.posting_encoding);
                }

                FeatureRowsResult result;
                result.num_features = kept_count;
                result.rows.resize(row_maps.size());

                std::vector<std::vector<DocFreqEntry> > docfreq_postings(kept_count);

                for (std::size_t doc_id = 0; doc_id < row_maps.size(); ++doc_id) {
                    for (typename std::unordered_map<std::uint32_t, std::uint32_t>::const_iterator it =
                             row_maps[doc_id].begin();
                         it != row_maps[doc_id].end();
                         ++it) {
                        const std::uint32_t new_id = (N == 4U)
                            ? old_to_new[it->first]
                            : it->first;
                        if (new_id == static_cast<std::uint32_t>(-1)) {
                            continue;
                        }
                        result.rows[doc_id].push_back(std::make_pair(new_id, it->second));
                        DocFreqEntry entry;
                        entry.doc_id = static_cast<std::uint32_t>(doc_id);
                        entry.count = it->second;
                        docfreq_postings[new_id].push_back(entry);
                    }
                    std::sort(result.rows[doc_id].begin(), result.rows[doc_id].end());
                }

                WriteDocFreqIndex(corpus_dir + "/" + stem_name + ".docfreq.header",
                                  corpus_dir + "/" + stem_name + ".docfreq.entries",
                                  docfreq_postings,
                                  options.posting_encoding);

                return result;
            }
    };

} // namespace teknegram
