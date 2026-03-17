#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../corpus_shared_layer/artifact_builders.hpp"
#include "../corpus_shared_layer/build_options.hpp"

namespace teknegram {

    template <std::size_t N>
    class NGramBuilder {
        public:
            FeatureRowsResult build(const std::string& corpus_dir,
                                    const BuildOptions& options) const {
                const std::vector<std::uint32_t> words =
                    load_uint32_file(corpus_dir + "/word.bin");
                std::vector<std::uint32_t> sentence_starts =
                    load_uint32_file(corpus_dir + "/sentence_bounds.bin");
                const std::vector<std::uint32_t> word_doc =
                    load_uint32_file(corpus_dir + "/word_doc.bin");
                const std::vector<std::uint32_t> doc_ranges =
                    load_uint32_file(corpus_dir + "/doc_ranges.bin");

                if (word_doc.size() != words.size()) {
                    throw std::runtime_error("word_doc.bin length does not match word.bin.");
                }

                sentence_starts.push_back(static_cast<std::uint32_t>(words.size()));
                const std::size_t num_docs = doc_ranges.size() / 2U;

                typedef std::vector<std::uint32_t> NGramKey;

                std::map<NGramKey, std::uint32_t> ngram_to_id;
                std::vector<NGramKey> lexicon;
                std::vector<std::uint32_t> freq;
                std::vector<std::vector<std::uint32_t> > positions;
                std::vector<std::map<std::uint32_t, std::uint32_t> > row_maps(num_docs);

                for (std::size_t s = 0; s + 1U < sentence_starts.size(); ++s) {
                    const std::uint32_t sent_begin = sentence_starts[s];
                    const std::uint32_t sent_end = sentence_starts[s + 1U];
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
                        std::uint32_t id = 0U;
                        if (it == ngram_to_id.end()) {
                            id = static_cast<std::uint32_t>(lexicon.size());
                            ngram_to_id[key] = id;
                            lexicon.push_back(key);
                            freq.push_back(0U);
                            positions.push_back(std::vector<std::uint32_t>());
                        } else {
                            id = it->second;
                        }

                        ++freq[id];
                        if (options.emit_ngram_positions) {
                            positions[id].push_back(i);
                        }
                        ++row_maps[word_doc[i]][id];
                    }
                }

                return write_outputs(corpus_dir,
                                     lexicon,
                                     freq,
                                     positions,
                                     row_maps,
                                     options);
            }

        private:
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
                const std::vector<std::vector<std::uint32_t> >& lexicon,
                const std::vector<std::uint32_t>& freq,
                const std::vector<std::vector<std::uint32_t> >& positions,
                const std::vector<std::map<std::uint32_t, std::uint32_t> >& row_maps,
                const BuildOptions& options) {

                const std::string stem =
                    std::to_string(static_cast<unsigned long long>(N)) + "gram";

                std::vector<std::uint32_t> old_to_new(freq.size(), static_cast<std::uint32_t>(-1));
                std::vector<std::vector<std::uint32_t> > filtered_lexicon;
                std::vector<std::uint32_t> filtered_freq;
                std::vector<std::vector<std::uint32_t> > filtered_positions;

                for (std::size_t i = 0; i < freq.size(); ++i) {
                    if (N == 4U && freq[i] < 2U) {
                        continue;
                    }
                    old_to_new[i] = static_cast<std::uint32_t>(filtered_lexicon.size());
                    filtered_lexicon.push_back(lexicon[i]);
                    filtered_freq.push_back(freq[i]);
                    filtered_positions.push_back(positions[i]);
                }

                std::ofstream lexicon_out((corpus_dir + "/" + stem + ".lexicon.bin").c_str(),
                                          std::ios::binary | std::ios::out | std::ios::trunc);
                std::ofstream freq_out((corpus_dir + "/" + stem + ".freq.bin").c_str(),
                                       std::ios::binary | std::ios::out | std::ios::trunc);
                if (!lexicon_out || !freq_out) {
                    throw std::runtime_error("NGramBuilder failed to open outputs.");
                }

                for (std::size_t i = 0; i < filtered_lexicon.size(); ++i) {
                    for (std::size_t j = 0; j < filtered_lexicon[i].size(); ++j) {
                        lexicon_out.write(reinterpret_cast<const char*>(&filtered_lexicon[i][j]),
                                          sizeof(std::uint32_t));
                    }
                    freq_out.write(reinterpret_cast<const char*>(&filtered_freq[i]),
                                   sizeof(std::uint32_t));
                }

                if (options.emit_ngram_positions) {
                    WritePositionIndex(corpus_dir + "/" + stem + ".index.header",
                                       corpus_dir + "/" + stem + ".index.positions",
                                       filtered_positions,
                                       options.posting_encoding);
                }

                FeatureRowsResult result;
                result.num_features = filtered_lexicon.size();
                result.rows.resize(row_maps.size());

                std::vector<std::vector<DocFreqEntry> > docfreq_postings(filtered_lexicon.size());

                for (std::size_t doc_id = 0; doc_id < row_maps.size(); ++doc_id) {
                    for (std::map<std::uint32_t, std::uint32_t>::const_iterator it = row_maps[doc_id].begin();
                         it != row_maps[doc_id].end();
                         ++it) {
                        const std::uint32_t new_id = old_to_new[it->first];
                        if (new_id == static_cast<std::uint32_t>(-1)) {
                            continue;
                        }
                        result.rows[doc_id].push_back(std::make_pair(new_id, it->second));
                        DocFreqEntry entry;
                        entry.doc_id = static_cast<std::uint32_t>(doc_id);
                        entry.count = it->second;
                        docfreq_postings[new_id].push_back(entry);
                    }
                }

                WriteDocFreqIndex(corpus_dir + "/" + stem + ".docfreq.header",
                                  corpus_dir + "/" + stem + ".docfreq.entries",
                                  docfreq_postings,
                                  options.posting_encoding);

                return result;
            }
    };

} // namespace teknegram
