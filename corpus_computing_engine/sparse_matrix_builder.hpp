#pragma once

#include <string>

#include "../corpus_shared_layer/artifact_builders.hpp"

namespace teknegram {
    class SparseMatrixBuilder {
        public:
            void build_matrix(const std::string& corpus_dir,
                              const std::string& feature_stem,
                              const FeatureRowsResult& result) const;
    };
}
