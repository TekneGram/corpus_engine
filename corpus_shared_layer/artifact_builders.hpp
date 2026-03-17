#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "binary_header.hpp"
#include "build_options.hpp"

namespace teknegram {

    struct FeatureRowsResult {
        std::uint64_t num_features;
        std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t> > > rows;

        FeatureRowsResult()
            : num_features(0) {}
    };

    void WritePositionIndex(const std::string& header_path,
                            const std::string& positions_path,
                            const std::vector<std::vector<std::uint32_t> >& postings,
                            PostingEncodingMode mode);

    void WriteDocFreqIndex(const std::string& header_path,
                           const std::string& entries_path,
                           const std::vector<std::vector<DocFreqEntry> >& postings,
                           PostingEncodingMode mode);

    void WriteSparseMatrixArtifacts(const std::string& meta_path,
                                    const std::string& offsets_path,
                                    const std::string& entries_path,
                                    const FeatureRowsResult& result);

} // namespace teknegram
