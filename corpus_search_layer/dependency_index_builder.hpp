#pragma once

#include <string>

#include "../corpus_shared_layer/build_options.hpp"

namespace teknegram {

    class DependencyIndexBuilder {
        public:
            void build(const std::string& corpus_dir,
                       PostingEncodingMode mode) const;
    };

}
