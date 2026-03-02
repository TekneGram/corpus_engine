#pragma once

#include <string>

namespace teknegram {

    class DocFreqBuilder {
        public:
            void build_lemma_docfreq(const std::string& corpus_dir) const;
            void build_word_docfreq(const std::string& corpus_dir) const;
        };

}
