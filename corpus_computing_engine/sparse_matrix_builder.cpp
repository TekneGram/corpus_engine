#include "sparse_matrix_builder.hpp"

#include "../corpus_shared_layer/artifact_builders.hpp"

namespace teknegram {

    void SparseMatrixBuilder::build_matrix(const std::string& corpus_dir,
                                           const std::string& feature_stem,
                                           const FeatureRowsResult& result) const {
        WriteSparseMatrixArtifacts(corpus_dir + "/" + feature_stem + ".spm.meta.bin",
                                   corpus_dir + "/" + feature_stem + ".spm.offsets.bin",
                                   corpus_dir + "/" + feature_stem + ".spm.entries.bin",
                                   result);
    }

} // namespace teknegram
