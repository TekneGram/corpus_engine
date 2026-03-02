#include "docfreq_builder.hpp"

#include <cstdint>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../corpus_shared_layer/binary_header.hpp"

namespace teknegram {

    void DocFreqBuilder::build_lemma_docfreq(const std::string& corpus_dir) const {
        std::ifstream lemma_in((corpus_dir + "/lemma.bin").c_str(), std::ios::binary | std::ios::in);
        std::ifstream doc_ranges_in((corpus_dir + "/doc_ranges.bin").c_str(), std::ios::binary | std::ios::in);

        if (!lemma_in || !doc_ranges_in) {
            throw std::runtime_error("Failed to open inputs for lemma docfreq build.");
        }

        std::vector<std::uint32_t> lemmas;
        std::uint32_t lemma = 0;
        while (lemma_in.read(reinterpret_cast<char*>(&lemma), sizeof(lemma))) {
            lemmas.push_back(lemma);
        }

        std::vector<std::pair<std::uint32_t, std::uint32_t> > doc_ranges;
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
        while (doc_ranges_in.read(reinterpret_cast<char*>(&begin), sizeof(begin))) {
            if (!doc_ranges_in.read(reinterpret_cast<char*>(&end), sizeof(end))) {
                throw std::runtime_error("Malformed doc_ranges.bin (odd uint32 count).");
            }
            doc_ranges.push_back(std::make_pair(begin, end));
        }

        std::vector<std::vector<DocFreqEntry> > postings;

        for (std::uint32_t doc_id = 0; doc_id < static_cast<std::uint32_t>(doc_ranges.size()); ++doc_id) {
            const std::uint32_t doc_start = doc_ranges[doc_id].first;
            const std::uint32_t doc_end = doc_ranges[doc_id].second;
            std::map<std::uint32_t, std::uint32_t> local_counts;

            for (std::uint32_t i = doc_start; i < doc_end; ++i) {
                ++local_counts[lemmas[i]];
            }

            for (std::map<std::uint32_t, std::uint32_t>::const_iterator it = local_counts.begin(); it != local_counts.end(); ++it) {
                const std::size_t fid = static_cast<std::size_t>(it->first);
                if (postings.size() <= fid) {
                    postings.resize(fid + 1);
                }
                DocFreqEntry entry;
                entry.doc_id = doc_id;
                entry.count = it->second;
                postings[fid].push_back(entry);
            }
        }

        std::vector<IndexHeaderEntry> header(postings.size());
        std::uint64_t offset = 0;
        for (std::size_t i = 0; i < postings.size(); ++i) {
            header[i].offset = offset;
            header[i].length = static_cast<std::uint32_t>(postings[i].size());
            offset += static_cast<std::uint64_t>(postings[i].size());
        }

        std::ofstream header_out((corpus_dir + "/lemma.docfreq.header").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream entries_out((corpus_dir + "/lemma.docfreq.entries").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);

        if (!header_out || !entries_out) {
            throw std::runtime_error("Failed to open outputs for lemma docfreq build.");
        }

        const std::uint32_t size = static_cast<std::uint32_t>(header.size());
        header_out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (!header.empty()) {
            header_out.write(reinterpret_cast<const char*>(header.data()),
                            static_cast<std::streamsize>(header.size() * sizeof(IndexHeaderEntry)));
        }

        for (std::size_t i = 0; i < postings.size(); ++i) {
            if (!postings[i].empty()) {
                entries_out.write(reinterpret_cast<const char*>(postings[i].data()),
                                static_cast<std::streamsize>(postings[i].size() * sizeof(DocFreqEntry)));
            }
        }
    }

    void DocFreqBuilder::build_word_docfreq(const std::string& corpus_dir) const {
        std::ifstream word_in((corpus_dir + "/word.bin").c_str(), std::ios::binary | std::ios::in);
        std::ifstream doc_ranges_in((corpus_dir + "/doc_ranges.bin").c_str(), std::ios::binary | std::ios::in);

        if (!word_in || !doc_ranges_in) {
            throw std::runtime_error("Failed to open inputs for lemma docfreq build.");
        }

        std::vector<std::uint32_t> words;
        std::uint32_t word = 0;
        while (word_in.read(reinterpret_cast<char*>(&word), sizeof(word))) {
            words.push_back(word);
        }

        std::vector<std::pair<std::uint32_t, std::uint32_t> > doc_ranges;
        std::uint32_t begin = 0;
        std::uint32_t end = 0;
        while (doc_ranges_in.read(reinterpret_cast<char*>(&begin), sizeof(begin))) {
            if (!doc_ranges_in.read(reinterpret_cast<char*>(&end), sizeof(end))) {
                throw std::runtime_error("Malformed doc_ranges.bin (odd uint32 count).");
            }
            doc_ranges.push_back(std::make_pair(begin, end));
        }

        std::vector<std::vector<DocFreqEntry> > postings;

        for (std::uint32_t doc_id = 0; doc_id < static_cast<std::uint32_t>(doc_ranges.size()); ++doc_id) {
            const std::uint32_t doc_start = doc_ranges[doc_id].first;
            const std::uint32_t doc_end = doc_ranges[doc_id].second;
            std::map<std::uint32_t, std::uint32_t> local_counts;

            for (std::uint32_t i = doc_start; i < doc_end; ++i) {
                ++local_counts[words[i]];
            }

            for (std::map<std::uint32_t, std::uint32_t>::const_iterator it = local_counts.begin(); it != local_counts.end(); ++it) {
                const std::size_t fid = static_cast<std::size_t>(it->first);
                if (postings.size() <= fid) {
                    postings.resize(fid + 1);
                }
                DocFreqEntry entry;
                entry.doc_id = doc_id;
                entry.count = it->second;
                postings[fid].push_back(entry);
            }
        }

        std::vector<IndexHeaderEntry> header(postings.size());
        std::uint64_t offset = 0;
        for (std::size_t i = 0; i < postings.size(); ++i) {
            header[i].offset = offset;
            header[i].length = static_cast<std::uint32_t>(postings[i].size());
            offset += static_cast<std::uint64_t>(postings[i].size());
        }

        std::ofstream header_out((corpus_dir + "/word.docfreq.header").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream entries_out((corpus_dir + "/word.docfreq.entries").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);

        if (!header_out || !entries_out) {
            throw std::runtime_error("Failed to open outputs for word docfreq build.");
        }

        const std::uint32_t size = static_cast<std::uint32_t>(header.size());
        header_out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (!header.empty()) {
            header_out.write(reinterpret_cast<const char*>(header.data()),
                            static_cast<std::streamsize>(header.size() * sizeof(IndexHeaderEntry)));
        }

        for (std::size_t i = 0; i < postings.size(); ++i) {
            if (!postings[i].empty()) {
                entries_out.write(reinterpret_cast<const char*>(postings[i].data()),
                                static_cast<std::streamsize>(postings[i].size() * sizeof(DocFreqEntry)));
            }
        }
    }

} // namespace corpus
