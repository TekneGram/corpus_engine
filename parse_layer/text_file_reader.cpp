#include "text_file_reader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace teknegram {

    std::string TextFileReader::read_file(const std::string& path) const {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

}
