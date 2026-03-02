#pragma once

#include <string>

namespace teknegram {
    class SparseMatrixBuilder {
        public:
            void build_lemma_matrix(const std::string& corpus_dir) const;
    };
}
