#pragma once

#include <string>

namespace teknegram {

    class TextFileReader {
        public:
            std::string read_file(const std::string& path) const;
    };

}
