#pragma once

#include <string>

#include "../corpus_shared_layer/artifact_builders.hpp"
#include "../corpus_shared_layer/build_options.hpp"

namespace teknegram {

    class DocFreqBuilder {
        public:
            FeatureRowsResult build_lemma_docfreq(const std::string& corpus_dir,
                                                  PostingEncodingMode mode) const;
            FeatureRowsResult build_word_docfreq(const std::string& corpus_dir,
                                                 PostingEncodingMode mode) const;
        };

}
