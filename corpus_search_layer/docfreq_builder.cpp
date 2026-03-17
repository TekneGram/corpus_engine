#include "docfreq_builder.hpp"

#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../corpus_shared_layer/artifact_builders.hpp"

namespace teknegram {

    namespace {

        std::vector<std::pair<std::uint32_t, std::uint32_t> > LoadDocRanges(const std::string& path) {
            std::ifstream in(path.c_str(), std::ios::binary | std::ios::in);
            if (!in) {
                throw std::runtime_error("Failed to open doc ranges: " + path);
            }

            std::vector<std::pair<std::uint32_t, std::uint32_t> > doc_ranges;
            std::uint32_t begin = 0U;
            std::uint32_t end = 0U;
            while (in.read(reinterpret_cast<char*>(&begin), sizeof(begin))) {
                if (!in.read(reinterpret_cast<char*>(&end), sizeof(end))) {
                    throw std::runtime_error("Malformed doc_ranges.bin (odd uint32 count).");
                }
                doc_ranges.push_back(std::make_pair(begin, end));
            }
            return doc_ranges;
        }

        std::vector<std::uint32_t> LoadUint32File(const std::string& path) {
            std::ifstream in(path.c_str(), std::ios::binary | std::ios::in);
            if (!in) {
                throw std::runtime_error("Failed to open token binary: " + path);
            }

            std::vector<std::uint32_t> values;
            std::uint32_t value = 0U;
            while (in.read(reinterpret_cast<char*>(&value), sizeof(value))) {
                values.push_back(value);
            }
            return values;
        }

        FeatureRowsResult BuildFeatureRowsFromValues(
            const std::vector<std::uint32_t>& values,
            const std::vector<std::pair<std::uint32_t, std::uint32_t> >& doc_ranges,
            std::vector<std::vector<DocFreqEntry> >* postings_out) {

            FeatureRowsResult result;
            result.rows.resize(doc_ranges.size());

            std::vector<std::vector<DocFreqEntry> > postings;

            for (std::uint32_t doc_id = 0U; doc_id < static_cast<std::uint32_t>(doc_ranges.size()); ++doc_id) {
                const std::uint32_t doc_start = doc_ranges[doc_id].first;
                const std::uint32_t doc_end = doc_ranges[doc_id].second;
                std::map<std::uint32_t, std::uint32_t> local_counts;

                for (std::uint32_t i = doc_start; i < doc_end; ++i) {
                    ++local_counts[values[i]];
                }

                for (std::map<std::uint32_t, std::uint32_t>::const_iterator it = local_counts.begin();
                     it != local_counts.end();
                     ++it) {
                    if (postings.size() <= it->first) {
                        postings.resize(static_cast<std::size_t>(it->first) + 1U);
                    }
                    DocFreqEntry entry;
                    entry.doc_id = doc_id;
                    entry.count = it->second;
                    postings[it->first].push_back(entry);
                    result.rows[doc_id].push_back(std::make_pair(it->first, it->second));
                }
            }

            result.num_features = postings.size();
            *postings_out = postings;
            return result;
        }

    } // namespace

    FeatureRowsResult DocFreqBuilder::build_word_docfreq(const std::string& corpus_dir,
                                                         PostingEncodingMode mode) const {
        std::vector<std::vector<DocFreqEntry> > postings;
        const FeatureRowsResult result = BuildFeatureRowsFromValues(
            LoadUint32File(corpus_dir + "/word.bin"),
            LoadDocRanges(corpus_dir + "/doc_ranges.bin"),
            &postings);

        WriteDocFreqIndex(corpus_dir + "/word.docfreq.header",
                          corpus_dir + "/word.docfreq.entries",
                          postings,
                          mode);
        return result;
    }

    FeatureRowsResult DocFreqBuilder::build_lemma_docfreq(const std::string& corpus_dir,
                                                          PostingEncodingMode mode) const {
        std::vector<std::vector<DocFreqEntry> > postings;
        const FeatureRowsResult result = BuildFeatureRowsFromValues(
            LoadUint32File(corpus_dir + "/lemma.bin"),
            LoadDocRanges(corpus_dir + "/doc_ranges.bin"),
            &postings);

        WriteDocFreqIndex(corpus_dir + "/lemma.docfreq.header",
                          corpus_dir + "/lemma.docfreq.entries",
                          postings,
                          mode);
        return result;
    }

} // namespace teknegram
