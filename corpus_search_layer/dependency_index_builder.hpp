#pragma once

#include <string>

namespace teknegram {

    class DependencyIndexBuilder {
        public:
            void build(const std::string& corpus_dir) const;
    };

}
